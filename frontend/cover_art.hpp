#pragma once
/*
  frontend/cover_art.hpp
  Album cover art extraction + terminal rendering (ANSI + Kitty Graphics Protocol).
*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef CMUSPP_HAS_JPEG
#  include <jpeglib.h>
#  include <setjmp.h>
#endif

#ifdef CMUSPP_HAS_PNG
#  include <png.h>
#endif

namespace fs = std::filesystem;

struct Pixel { uint8_t r, g, b; };

struct RawImg {
    std::vector<Pixel> px;
    int w = 0, h = 0;
    bool empty() const { return px.empty(); }
};

enum class ImgFmt { NONE, JPEG, PNG };

inline ImgFmt detect_fmt(const uint8_t* d, size_t len) {
    if (len >= 3 && d[0]==0xFF && d[1]==0xD8 && d[2]==0xFF) return ImgFmt::JPEG;
    if (len >= 4 && d[0]==0x89 && d[1]=='P'  && d[2]=='N'  && d[3]=='G') return ImgFmt::PNG;
    return ImgFmt::NONE;
}
inline ImgFmt mime_to_fmt(const std::string& m) {
    if (m.find("jpeg")!=std::string::npos || m.find("jpg")!=std::string::npos) return ImgFmt::JPEG;
    if (m.find("png") !=std::string::npos) return ImgFmt::PNG;
    return ImgFmt::NONE;
}

#ifdef CMUSPP_HAS_JPEG
struct JpegErr { struct jpeg_error_mgr pub; jmp_buf jb; };
static void jpeg_err_exit(j_common_ptr c) { longjmp(((JpegErr*)c->err)->jb, 1); }

inline RawImg decode_jpeg(const uint8_t* data, size_t len) {
    RawImg img;
    struct jpeg_decompress_struct ci{};
    JpegErr je{};
    ci.err = jpeg_std_error(&je.pub);
    je.pub.error_exit = jpeg_err_exit;
    if (setjmp(je.jb)) { jpeg_destroy_decompress(&ci); return {}; }
    jpeg_create_decompress(&ci);
    jpeg_mem_src(&ci, data, (unsigned long)len);
    if (jpeg_read_header(&ci, TRUE) != JPEG_HEADER_OK) { jpeg_destroy_decompress(&ci); return {}; }
    ci.out_color_space = JCS_RGB;
    jpeg_start_decompress(&ci);
    img.w = (int)ci.output_width; img.h = (int)ci.output_height;
    img.px.resize((size_t)img.w * img.h);
    std::vector<uint8_t> row((size_t)img.w * 3);
    JSAMPROW rp = row.data();
    for (int y = 0; (int)ci.output_scanline < img.h; ++y) {
        jpeg_read_scanlines(&ci, &rp, 1);
        for (int x = 0; x < img.w; ++x)
            img.px[(size_t)y*img.w+x] = {row[x*3], row[x*3+1], row[x*3+2]};
    }
    jpeg_finish_decompress(&ci); jpeg_destroy_decompress(&ci);
    return img;
}
#else
inline RawImg decode_jpeg(const uint8_t*, size_t) { return {}; }
#endif

#ifdef CMUSPP_HAS_PNG
inline RawImg decode_png(const uint8_t* data, size_t len) {
    if (len < 8 || png_sig_cmp(data, 0, 8)) return {};
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
    if (!png) return {};
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png,nullptr,nullptr); return {}; }
    struct Src { const uint8_t* p; size_t rem; };
    Src src{data+8, len-8};
    png_set_read_fn(png, &src,[](png_structp ps, png_bytep buf, png_size_t n){
        Src* s = (Src*)png_get_io_ptr(ps);
        size_t r = std::min(n, s->rem);
        memcpy(buf, s->p, r); s->p += r; s->rem -= r;
    });
    png_set_sig_bytes(png, 8);
    if (setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png,&info,nullptr); return {}; }
    png_read_info(png, info);
    int w=(int)png_get_image_width(png,info), h=(int)png_get_image_height(png,info);
    int ct=png_get_color_type(png,info), bd=png_get_bit_depth(png,info);
    if (bd==16) png_set_strip_16(png);
    if (ct==PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (ct==PNG_COLOR_TYPE_GRAY && bd<8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png,info,PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (ct & PNG_COLOR_MASK_ALPHA) png_set_strip_alpha(png);
    if (ct==PNG_COLOR_TYPE_GRAY || ct==PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_read_update_info(png,info);
    RawImg img; img.w=w; img.h=h; img.px.resize((size_t)w*h);
    std::vector<std::vector<uint8_t>> rows(h, std::vector<uint8_t>((size_t)w*3));
    std::vector<png_bytep> rp(h);
    for (int y=0;y<h;++y) rp[y]=rows[y].data();
    png_read_image(png, rp.data());
    png_destroy_read_struct(&png,&info,nullptr);
    for (int y=0;y<h;++y)
        for (int x=0;x<w;++x)
            img.px[(size_t)y*w+x]={rows[y][x*3],rows[y][x*3+1],rows[y][x*3+2]};
    return img;
}
#else
inline RawImg decode_png(const uint8_t*, size_t) { return {}; }
#endif

inline RawImg decode_image(const uint8_t* d, size_t len, ImgFmt hint=ImgFmt::NONE) {
    ImgFmt fmt = detect_fmt(d, len);
    if (fmt == ImgFmt::NONE) fmt = hint;
    if (fmt == ImgFmt::JPEG) return decode_jpeg(d, len);
    if (fmt == ImgFmt::PNG)  return decode_png(d, len);
    return {};
}

// ── Area-averaging resize (fallback para terminales ANSI clásicas) ──────────
inline RawImg resize_img(const RawImg& src, int tw, int th) {
    if (src.w == tw && src.h == th) return src;
    if (tw <= 0 || th <= 0 || src.w <= 0 || src.h <= 0) return {};
    RawImg dst; dst.w = tw; dst.h = th; dst.px.resize((size_t)tw * th);

    for (int y = 0; y < th; ++y) {
        int sy1 = y * src.h / th;
        int sy2 = (y + 1) * src.h / th;
        if (sy2 == sy1) sy2 = sy1 + 1;
        if (sy2 > src.h) sy2 = src.h;

        for (int x = 0; x < tw; ++x) {
            int sx1 = x * src.w / tw;
            int sx2 = (x + 1) * src.w / tw;
            if (sx2 == sx1) sx2 = sx1 + 1;
            if (sx2 > src.w) sx2 = src.w;

            long r = 0, g = 0, b = 0;
            int count = 0;
            for (int cy = sy1; cy < sy2; ++cy) {
                for (int cx = sx1; cx < sx2; ++cx) {
                    const Pixel& p = src.px[(size_t)cy * src.w + cx];
                    r += p.r; g += p.g; b += p.b;
                    count++;
                }
            }
            if (count > 0) {
                dst.px[(size_t)y * tw + x] = {
                    (uint8_t)(r / count),
                    (uint8_t)(g / count),
                    (uint8_t)(b / count)
                };
            }
        }
    }
    return dst;
}

static inline uint32_t be32(const uint8_t* p){return((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static inline uint32_t be24(const uint8_t* p){return((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
static inline uint32_t ss28(const uint8_t* p){return((uint32_t)(p[0]&0x7f)<<21)|((uint32_t)(p[1]&0x7f)<<14)|((uint32_t)(p[2]&0x7f)<<7)|((uint32_t)(p[3]&0x7f));}

inline RawImg extract_mp3_cover(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    uint8_t hdr[10];
    if (fread(hdr,1,10,f)<10 || hdr[0]!='I'||hdr[1]!='D'||hdr[2]!='3') { fclose(f); return {}; }
    int ver=(int)hdr[3];
    bool unsync=(hdr[5]&0x80)!=0;
    uint32_t tag_size=ss28(hdr+6);
    if (tag_size>16*1024*1024) { fclose(f); return {}; }
    std::vector<uint8_t> tag(tag_size);
    size_t got=fread(tag.data(),1,tag_size,f); fclose(f);
    tag.resize(got);

    std::vector<uint8_t> buf;
    if (unsync) {
        buf.reserve(tag_size);
        for (size_t i=0;i<tag.size();++i) {
            buf.push_back(tag[i]);
            if (tag[i]==0xFF && i+1<tag.size() && tag[i+1]==0x00) ++i;
        }
    }
    const uint8_t* p   = unsync ? buf.data() : tag.data();
    size_t         left = unsync ? buf.size() : tag.size();
    bool v22=(ver==2);

    while (left>=(v22?6u:10u)) {
        if (p[0]==0x00) break;
        std::string fid(reinterpret_cast<const char*>(p), v22?3:4);
        uint32_t fsz;
        if (v22) { fsz=be24(p+3); p+=6; left-=6; }
        else     { fsz=(ver>=4)?ss28(p+4):be32(p+4); p+=10; left-=10; }
        if (fsz>left) break;

        bool is_apic=(!v22&&fid=="APIC")||(v22&&fid=="PIC");
        if (is_apic && fsz>4) {
            const uint8_t* fp=p; size_t fl=fsz;
            uint8_t enc=fp[0]; fp++; fl--;
            ImgFmt fmt=ImgFmt::NONE;
            if (v22) {
                if (fl>=3) {
                    std::string s(reinterpret_cast<const char*>(fp),3);
                    if (s=="JPG") fmt=ImgFmt::JPEG;
                    if (s=="PNG") fmt=ImgFmt::PNG;
                    fp+=3; fl-=3;
                }
            } else {
                std::string mime;
                while (fl>0&&*fp!=0){mime+=(char)*fp++;fl--;}
                if (fl>0){fp++;fl--;}
                fmt=mime_to_fmt(mime);
            }
            if (fl<2){p+=fsz;left-=fsz;continue;}
            fp++; fl--;  
            if (enc==1||enc==2) { while(fl>=2&&!(fp[0]==0&&fp[1]==0)){fp+=2;fl-=2;} if(fl>=2){fp+=2;fl-=2;} }
            else                { while(fl>0&&*fp!=0){fp++;fl--;} if(fl>0){fp++;fl--;} }
            if (fl>0) { RawImg img=decode_image(fp,fl,fmt); if(!img.empty()){ return img; } }
        }
        p+=fsz; left-=fsz;
    }
    return {};
}

inline RawImg extract_flac_cover(const std::string& path) {
    FILE* f=fopen(path.c_str(),"rb");
    if (!f) return {};
    uint8_t marker[4];
    if (fread(marker,1,4,f)<4||marker[0]!='f'||marker[1]!='L'||marker[2]!='a'||marker[3]!='C') { fclose(f); return {}; }
    RawImg best;
    while (true) {
        uint8_t bh[4]; if (fread(bh,1,4,f)<4) break;
        bool last=(bh[0]&0x80)!=0; int type=(bh[0]&0x7F); uint32_t sz=be24(bh+1);
        if (type==6 && sz>=32) {
            std::vector<uint8_t> blk(sz);
            if (fread(blk.data(),1,sz,f)!=sz) break;
            const uint8_t* p=blk.data();
            uint32_t pictype=be32(p); p+=4;
            uint32_t mlen=be32(p); p+=4;
            if (p+mlen+20>blk.data()+sz) break;
            std::string mime(reinterpret_cast<const char*>(p),mlen); p+=mlen;
            uint32_t dlen=be32(p); p+=4; p+=dlen;
            p+=16;
            uint32_t datalen=be32(p); p+=4;
            if (p+datalen>blk.data()+sz) break;
            RawImg img=decode_image(p,datalen,mime_to_fmt(mime));
            if (!img.empty()) {
                if (pictype==3) { fclose(f); return img; }
                if (best.empty()) best=std::move(img);
            }
        } else { fseek(f,(long)sz,SEEK_CUR); }
        if (last) break;
    }
    fclose(f); return best;
}

inline std::vector<uint8_t> base64_decode(const std::string& s) {
    static const uint8_t T[256]={
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
    };
    std::vector<uint8_t> out; out.reserve(s.size()*3/4);
    uint32_t acc=0; int bits=0;
    for (unsigned char c:s) { uint8_t v=T[c]; if(v==64) continue; acc=(acc<<6)|v; bits+=6; if(bits>=8){bits-=8;out.push_back((acc>>bits)&0xFF);} }
    return out;
}

inline RawImg extract_ogg_cover(const std::string& path) {
    FILE* f=fopen(path.c_str(),"rb");
    if (!f) return {};
    std::vector<uint8_t> buf(256*1024);
    size_t got=fread(buf.data(),1,buf.size(),f); fclose(f);
    buf.resize(got);

    static const uint8_t magic[]={0x03,'v','o','r','b','i','s'};
    auto it=std::search(buf.begin(),buf.end(),magic,magic+7);
    if (it==buf.end()) return {};
    const uint8_t* p=&*it+7; const uint8_t* end=buf.data()+buf.size();

    if (p+4>end) return {};
    uint32_t vlen=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; p+=4;
    if (p+vlen>end) return {};
    p+=vlen;
    if (p+4>end) return {};
    uint32_t cnt=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; p+=4;

    for (uint32_t i=0;i<cnt&&p+4<=end;++i) {
        uint32_t clen=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; p+=4;
        if (p+clen>end) break;
        std::string tag(reinterpret_cast<const char*>(p),clen); p+=clen;
        std::string lo=tag; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
        const char* key="metadata_block_picture=";
        if (lo.compare(0,strlen(key),key)!=0) continue;
        std::string b64=tag.substr(strlen(key));
        b64.erase(std::remove_if(b64.begin(),b64.end(),::isspace),b64.end());
        auto blk=base64_decode(b64);
        if (blk.size()<32) continue;
        const uint8_t* bp=blk.data();
        be32(bp); bp+=4;
        uint32_t mlen=be32(bp); bp+=4;
        if (bp+mlen+20>blk.data()+blk.size()) continue;
        std::string mime(reinterpret_cast<const char*>(bp),mlen); bp+=mlen;
        uint32_t dlen=be32(bp); bp+=4; bp+=dlen;
        bp+=16;
        uint32_t datalen=be32(bp); bp+=4;
        if (bp+datalen>blk.data()+blk.size()) continue;
        RawImg img=decode_image(bp,datalen,mime_to_fmt(mime));
        if (!img.empty()) return img;
    }
    return {};
}

inline RawImg extract_dir_cover(const std::string& song_path) {
    fs::path dir=fs::path(song_path).parent_path();
    static const char* names[]={
        "cover.jpg","cover.jpeg","cover.png",
        "folder.jpg","folder.jpeg","folder.png",
        "album.jpg","album.jpeg","album.png",
        "front.jpg","front.jpeg","front.png",
        "art.jpg","art.jpeg","art.png", nullptr
    };
    for (int i=0;names[i];++i) {
        fs::path p=dir/names[i]; if (!fs::exists(p)) continue;
        FILE* f=fopen(p.string().c_str(),"rb"); if (!f) continue;
        fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f);
        if (sz<=0){fclose(f);continue;}
        std::vector<uint8_t> data((size_t)sz);
        size_t nr=fread(data.data(),1,sz,f); fclose(f);
        data.resize(nr);
        RawImg img=decode_image(data.data(),data.size());
        if (!img.empty()) return img;
    }
    return {};
}

inline RawImg extract_cover(const std::string& path) {
    std::string lc=path; std::transform(lc.begin(),lc.end(),lc.begin(),::tolower);
    auto ends=[&](const char* e){ size_t el=strlen(e); return lc.size()>=el&&lc.compare(lc.size()-el,el,e)==0; };
    RawImg img;
    if (ends(".mp3"))                         img=extract_mp3_cover(path);
    if (img.empty()&&ends(".flac"))           img=extract_flac_cover(path);
    if (img.empty()&&(ends(".ogg")||ends(".opus"))) img=extract_ogg_cover(path);
    if (img.empty())                          img=extract_dir_cover(path);
    return img;
}

// ════════════════════════════════════════════════════════════════════════════
//  KITTY GRAPHICS PROTOCOL SUPPORT
// ════════════════════════════════════════════════════════════════════════════

// Detecta si la terminal soporta el protocolo avanzado (Kitty, WezTerm, Ghostty, etc)
inline bool is_kitty() {
    if (const char* t = getenv("TERM")) if (strstr(t, "kitty")) return true;
    if (getenv("KITTY_WINDOW_ID")) return true;
    if (const char* t = getenv("TERM_PROGRAM")) {
        if (strstr(t, "kitty") || strstr(t, "WezTerm") || strstr(t, "ghostty")) return true;
    }
    return false;
}

// Codificador Base64 rápido para los píxeles
inline std::string base64_encode(const uint8_t* data, size_t len) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (data[i] << 16) | 
                     ((i + 1 < len ? data[i + 1] : 0) << 8) | 
                     ((i + 2 < len ? data[i + 2] : 0));
        out.push_back(T[(n >> 18) & 63]);
        out.push_back(T[(n >> 12) & 63]);
        out.push_back(i + 1 < len ? T[(n >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? T[n & 63] : '=');
    }
    return out;
}

// Genera la cadena ANSI gigante que sube la imagen cruda a la GPU de la terminal
inline std::string build_kitty_upload(const RawImg& img, int id) {
    std::vector<uint8_t> rgb;
    rgb.reserve((size_t)img.w * img.h * 3);
    for (const auto& p : img.px) {
        rgb.push_back(p.r); rgb.push_back(p.g); rgb.push_back(p.b);
    }
    std::string b64 = base64_encode(rgb.data(), rgb.size());
    std::string seq;
    
    size_t chunk_size = 4096;
    // Pre-alocar memoria para mejorar eficiencia en concatenación
    seq.reserve(b64.size() + (b64.size() / chunk_size + 2) * 64);
    
    size_t pos = 0;
    bool first = true;

    while (pos < b64.size()) {
        size_t take = std::min(chunk_size, b64.size() - pos);
        std::string chunk = b64.substr(pos, take);
        pos += take;
        bool more = (pos < b64.size());

        seq += "\033_G";
        if (first) {
            // "q=2" suprime de la pantalla errores en caso de que ocurran fallas del protocolo 
            seq += "a=t,q=2,f=24,s=" + std::to_string(img.w) + 
                   ",v=" + std::to_string(img.h) + ",i=" + std::to_string(id) + ",";
            first = false;
        }
        seq += "m=" + std::string(more ? "1" : "0") + ";" + chunk + "\033\\";
    }
    return seq;
}

// Renderiza los bloques vacíos para darle espacio a la foto y le manda el comando "mostrar"
inline std::vector<std::string> render_cover_kitty_display(int cols, int rows, int id) {
    if (cols <= 0 || rows <= 0) return {};
    std::vector<std::string> lines(rows, std::string(cols, ' '));
    // a=a (mostrar), q=2 (silencio absoluto), C=1 (no mover cursor), z=1 (dibujar encima del texto)
    lines[0] = "\033_Ga=a,q=2,i=" + std::to_string(id) + 
               ",c=" + std::to_string(cols) + 
               ",r=" + std::to_string(rows) + 
               ",C=1,z=1;\033\\" + lines[0];
    return lines;
}

// ════════════════════════════════════════════════════════════════════════════
//  FALLBACK RENDERING (ANSI HALF-BLOCK)
// ════════════════════════════════════════════════════════════════════════════

inline std::vector<std::string> render_cover(const RawImg& img, int cols, int rows) {
    if (cols <= 0 || rows <= 0) return {};
    RawImg sized=resize_img(img, cols, rows*2);
    std::vector<std::string> lines; lines.reserve(rows);
    char esc[64];
    for (int ty=0;ty<rows;++ty) {
        std::string line; line.reserve((size_t)cols*28);
        for (int tx=0;tx<cols;++tx) {
            Pixel up=sized.px[(size_t)(ty*2)*cols+tx];
            Pixel dn=(ty*2+1<rows*2)?sized.px[(size_t)(ty*2+1)*cols+tx]:Pixel{0,0,0};
            snprintf(esc,sizeof(esc),"\033[48;2;%d;%d;%dm",up.r,up.g,up.b); line+=esc;
            snprintf(esc,sizeof(esc),"\033[38;2;%d;%d;%dm",dn.r,dn.g,dn.b); line+=esc;
            line+="▄";
        }
        line+="\033[0m"; lines.push_back(std::move(line));
    }
    return lines;
}

inline std::vector<std::string> render_no_cover(int cols, int rows,
                                                  const char* fg2,
                                                  const char* fg3,
                                                  const char* acc) {
    if (cols <= 0 || rows <= 0) return {};
    std::vector<std::string> lines(rows);
    static const char* art[]={
        "⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣶⣾⣿⣿⣿⣿⣷⣶⣶⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⣠⢔⣫⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣄⠀⠀⠀⠀⠀",
        "⠀⠀⠀⣠⢊⡴⡫⢚⡽⣟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀",
        "⠀⠀⡴⣱⢫⢎⡔⡩⣚⠵⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⠀⠀",
        "⠀⣼⣽⣳⣣⢯⣞⡜⡱⣫⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀",
        "⢸⣿⣿⣿⣿⣿⣿⣾⡽⣱⣫⠞⠉⠀⠀⠀⠀⠉⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇",
        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⠃⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷",
        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠘⠃⠀⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        "⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⢀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿",
        "⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣤⣀⣀⣀⣠⣴⢟⡵⣳⢯⢿⣿⡟⣿⣿⣿⣿⡇",
        "⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣞⡵⣫⢏⢞⡽⡽⣻⢯⡟⠀",
        "⠀⠀⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣚⢕⡡⢊⠜⡵⣣⠟⠀⠀",
        "⠀⠀⠀⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⢷⣫⢖⡥⢊⡴⠋⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠙⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣞⣭⠞⠋⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠿⠿⢿⣿⣿⣿⣿⡿⠿⠟⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀",
        "      no  artwork, skill issues?         ",
        nullptr
    };
    int art_rows=0; while(art[art_rows]) ++art_rows;
    int last_art = art_rows - 1;  // index of the "no artwork" text line
    int start_row=std::max(0,(rows-art_rows)/2);
    for (int y=0;y<rows;++y) {
        std::string& line=lines[y]; line.reserve(cols*4);
        int ai=y-start_row;
        if (ai>=0&&ai<art_rows) {
            const char* a=art[ai];
            int aw=0;
            for (const char* c=a;*c;) {
                unsigned char b=(unsigned char)*c;
                int sk=(b<0x80)?1:(b<0xE0)?2:(b<0xF0)?3:4;
                c+=sk; ++aw;
            }
            int lpad=std::max(0,(cols-aw)/2);
            int rpad=std::max(0,cols-aw-lpad);
            line += (ai==last_art) ? fg3 : acc;
            line += std::string(lpad,' ');
            line += a;
            line += std::string(rpad,' ');
        } else {
            line+=fg3; line+=std::string(cols,' ');
        }
        line+="\033[0m";
    }
    return lines;
}

// ════════════════════════════════════════════════════════════════════════════
//  COVER CACHE
// ════════════════════════════════════════════════════════════════════════════
struct CoverCache {
    std::string path;
    int cols = 0, rows = 0;
    RawImg cached_img;
    std::vector<std::string> lines;
    
    std::string kitty_upload_seq;
    bool kitty_needs_upload = false;

    const std::vector<std::string>& get(const std::string& song_path,
                                         int c, int r,
                                         const char* fg2,
                                         const char* fg3,
                                         const char* acc)
    {
        if (path == song_path && cols == c && rows == r) return lines;
        
        bool new_song = (path != song_path);
        if (new_song) {
            path = song_path;
            cached_img = path.empty() ? RawImg{} : extract_cover(path);
            
            if (is_kitty()) {
                // Borramos la imagen vieja en RAM para evitar superposiciones (q=2 silencioso)
                kitty_upload_seq = "\033_Ga=d,d=i,q=2,i=1;\033\\";
                
                // Si la canción tiene portada, concatenamos la secuencia para enviarla a RAM
                if (!cached_img.empty()) {
                    kitty_upload_seq += build_kitty_upload(cached_img, 1);
                }
                kitty_needs_upload = true;
            }
        }
        cols = c; rows = r;

        if (cols <= 0 || rows <= 0) {
            lines.clear();
            return lines;
        }

        if (cached_img.empty()) {
            lines = render_no_cover(cols, rows, fg2, fg3, acc);
        } else if (is_kitty()) {
            lines = render_cover_kitty_display(cols, rows, 1);
        } else {
            lines = render_cover(cached_img, cols, rows);
        }

        if (is_kitty() && kitty_needs_upload && !lines.empty()) {
            lines[0] = kitty_upload_seq + lines[0];
            kitty_needs_upload = false;
        }

        return lines;
    }


    std::string consume_kitty_upload() {
        if (kitty_needs_upload) {
            kitty_needs_upload = false;
            return kitty_upload_seq;
        }
        return "";
    }
};
