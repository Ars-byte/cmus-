#pragma once
/*
  frontend/theme.hpp
  Theme definitions + XML theme loader.

  Built-in themes are compiled in as defaults.
  Custom themes can be loaded from XML files at runtime.

  XML format  (themes/<name>.xml):
  ────────────────────────────────
  <?xml version="1.0" encoding="UTF-8"?>
  <theme name="mytheme">
    <!-- Foreground colors: bright → muted -->
    <fg0 r="230" g="230" b="220"/>
    <fg1 r="180" g="178" b="168"/>
    <fg2 r="130" g="128" b="118"/>
    <fg3 r="85"  g="83"  b="75"/>
    <!-- Accent and warning -->
    <acc  r="140" g="200" b="140"/>
    <warn r="210" g="170" b="70"/>
    <!-- Background combos: bg + fg for each zone -->
    <bghdr bgr="22"  bgg="22"  bgb="18"  fgr="190" fgg="185" fgb="175"/>
    <bgsel bgr="50"  bgg="50"  bgb="44"  fgr="255" fgg="255" fgb="245"/>
    <bgplay bgr="20" bgg="42"  bgb="20"  fgr="140" fgg="200" fgb="140"/>
    <bgstat bgr="14" bgg="14"  bgb="12"  fgr="140" fgg="135" fgb="125"/>
  </theme>
*/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── ANSI helpers ────────────────────────────────────────────────────────────
// We store the escape sequences as std::string inside each Theme at load time.
inline std::string make_fg(int r, int g, int b) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
    return buf;
}
inline std::string make_bg(int r, int g, int b) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[48;2;%d;%d;%dm", r, g, b);
    return buf;
}
inline std::string make_combo(int bgr, int bgg, int bgb,
                               int fgr, int fgg, int fgb) {
    return make_bg(bgr, bgg, bgb) + make_fg(fgr, fgg, fgb);
}

// ── Theme struct ────────────────────────────────────────────────────────────
struct Theme {
    std::string name;
    // Foreground
    std::string fg0, fg1, fg2, fg3;
    // Accent / warning
    std::string acc, warn;
    // Background combos (bg escape + fg escape concatenated)
    std::string bghdr, bgsel, bgplay, bgstat;
};

// ── Built-in themes ─────────────────────────────────────────────────────────
inline std::vector<Theme> make_builtin_themes() {
    std::vector<Theme> v;
    auto add = [&](const char* nm,
                   int r0,int g0,int b0,  int r1,int g1,int b1,
                   int r2,int g2,int b2,  int r3,int g3,int b3,
                   int ar,int ag,int ab,  int wr,int wg,int wb,
                   int hr,int hg,int hb,  int hfr,int hfg,int hfb,
                   int sr,int sg,int sb,  int sfr,int sfg,int sfb,
                   int pr,int pg,int pb,  int pfr,int pfg,int pfb,
                   int tr,int tg,int tb,  int tfr,int tfg,int tfb)
    {
        Theme t;
        t.name   = nm;
        t.fg0    = make_fg(r0,g0,b0);  t.fg1 = make_fg(r1,g1,b1);
        t.fg2    = make_fg(r2,g2,b2);  t.fg3 = make_fg(r3,g3,b3);
        t.acc    = make_fg(ar,ag,ab);  t.warn= make_fg(wr,wg,wb);
        t.bghdr  = make_combo(hr,hg,hb,  hfr,hfg,hfb);
        t.bgsel  = make_combo(sr,sg,sb,  sfr,sfg,sfb);
        t.bgplay = make_combo(pr,pg,pb,  pfr,pfg,pfb);
        t.bgstat = make_combo(tr,tg,tb,  tfr,tfg,tfb);
        v.push_back(std::move(t));
    };

    //         name         fg0              fg1              fg2             fg3
    //         acc          warn
    //         bghdr bg+fg                   bgsel bg+fg
    //         bgplay bg+fg                  bgstat bg+fg
    add("default",
        230,230,220,  180,178,168,  130,128,118,  85,83,75,
        140,200,140,  210,170,70,
        22,22,18,    190,185,175,
        50,50,44,    255,255,245,
        20,42,20,    140,200,140,
        14,14,12,    140,135,125);

    add("catppuccin",
        205,214,244,  166,173,200,  108,112,134,  88,91,112,
        166,227,161,  249,226,175,
        30,30,46,    180,190,254,
        49,50,68,    205,214,244,
        30,45,30,    166,227,161,
        17,17,27,    108,112,134);

    add("dracula",
        248,248,242,  189,147,249,  98,114,164,   68,71,90,
        80,250,123,   241,250,140,
        40,42,54,    189,147,249,
        68,71,90,    248,248,242,
        30,50,35,    80,250,123,
        21,22,30,    98,114,164);

    add("nord",
        236,239,244,  216,222,233,  136,192,208,  76,86,106,
        163,190,140,  235,203,139,
        46,52,64,    136,192,208,
        67,76,94,    236,239,244,
        35,50,40,    163,190,140,
        36,41,51,    76,86,106);

    add("gruvbox",
        235,219,178,  214,196,155,  168,153,132,  102,92,84,
        184,187,38,   215,153,33,
        40,40,40,    168,153,132,
        60,56,54,    235,219,178,
        30,40,20,    184,187,38,
        29,32,33,    102,92,84);

    add("rosepine",
        224,222,244,  196,167,231,  144,122,169,  78,70,100,
        235,188,186,  246,193,119,
        25,23,36,    144,122,169,
        42,39,63,    224,222,244,
        38,25,40,    235,188,186,
        18,16,26,    78,70,100);

    add("tokyonight",
        192,202,245,  169,177,214,  86,95,137,    65,72,104,
        115,218,202,  224,175,104,
        26,27,38,    86,95,137,
        40,42,60,    192,202,245,
        20,40,38,    115,218,202,
        16,17,26,    65,72,104);

    add("everforest",
        211,198,170,  167,192,128,  131,165,152,  92,110,95,
        167,192,128,  219,188,127,
        37,42,37,    131,165,152,
        57,64,55,    211,198,170,
        30,46,30,    167,192,128,
        29,34,29,    92,110,95);

    add("cream",
        60,50,40,     100,88,75,    150,135,120,  185,172,158,
        80,140,80,    190,130,30,
        240,232,220,  100,88,75,
        210,198,182,  40,35,28,
        195,225,195,  50,110,50,
        228,218,206,  150,135,120);

    return v;
}

// ── Minimal XML parser (no extra deps) ──────────────────────────────────────
namespace xml_detail {

// Returns the integer value of attribute `attr` inside `tag_text`, or -1.
inline int attr_int(const std::string& tag_text, const char* attr) {
    std::string pat = std::string(attr) + "=\"";
    size_t pos = tag_text.find(pat);
    if (pos == std::string::npos) {
        // try attr='...'
        pat = std::string(attr) + "='";
        pos = tag_text.find(pat);
        if (pos == std::string::npos) return -1;
    }
    pos += pat.size();
    size_t end = tag_text.find_first_of("\"'", pos);
    if (end == std::string::npos) return -1;
    try { return std::stoi(tag_text.substr(pos, end - pos)); }
    catch (...) { return -1; }
}

// Returns attribute string value, empty if not found.
inline std::string attr_str(const std::string& tag_text, const char* attr) {
    std::string pat = std::string(attr) + "=\"";
    size_t pos = tag_text.find(pat);
    if (pos == std::string::npos) {
        pat = std::string(attr) + "='";
        pos = tag_text.find(pat);
        if (pos == std::string::npos) return {};
    }
    pos += pat.size();
    size_t end = tag_text.find_first_of("\"'", pos);
    if (end == std::string::npos) return {};
    return tag_text.substr(pos, end - pos);
}

// Find opening tag content (everything between < and >) for a given tag name.
inline std::string find_tag(const std::string& xml, const char* tag) {
    std::string open = std::string("<") + tag;
    size_t pos = xml.find(open);
    while (pos != std::string::npos) {
        char next = xml[pos + open.size()];
        if (next == ' ' || next == '/' || next == '>') {
            size_t end = xml.find('>', pos);
            if (end != std::string::npos)
                return xml.substr(pos, end - pos + 1);
        }
        pos = xml.find(open, pos + 1);
    }
    return {};
}

} // namespace xml_detail

// ── Load one Theme from an XML file ─────────────────────────────────────────
// Returns false and leaves `t` untouched on parse error.
inline bool load_theme_xml(const std::string& path, Theme& t) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    if (sz <= 0) { fclose(f); return false; }
    std::string xml(sz, '\0');
    fread(xml.data(), 1, sz, f);
    fclose(f);

    using namespace xml_detail;

    // <theme name="...">
    std::string root = find_tag(xml, "theme");
    if (root.empty()) return false;
    std::string nm = attr_str(root, "name");
    if (nm.empty()) return false;

    // Helper: parse a <tag r g b /> into a fg escape
    auto read_fg = [&](const char* tag) -> std::string {
        std::string s = find_tag(xml, tag);
        if (s.empty()) return {};
        int r = attr_int(s,"r"), g = attr_int(s,"g"), b = attr_int(s,"b");
        if (r < 0 || g < 0 || b < 0) return {};
        return make_fg(r, g, b);
    };

    // Helper: parse a <tag bgr bgg bgb fgr fgg fgb /> into a combo escape
    auto read_combo = [&](const char* tag) -> std::string {
        std::string s = find_tag(xml, tag);
        if (s.empty()) return {};
        int br=attr_int(s,"bgr"),bg_=attr_int(s,"bgg"),bb=attr_int(s,"bgb");
        int fr=attr_int(s,"fgr"),fg_=attr_int(s,"fgg"),fb=attr_int(s,"fgb");
        if (br<0||bg_<0||bb<0||fr<0||fg_<0||fb<0) return {};
        return make_combo(br,bg_,bb, fr,fg_,fb);
    };

    Theme tmp;
    tmp.name   = nm;
    tmp.fg0    = read_fg("fg0");  if (tmp.fg0.empty())   return false;
    tmp.fg1    = read_fg("fg1");  if (tmp.fg1.empty())   return false;
    tmp.fg2    = read_fg("fg2");  if (tmp.fg2.empty())   return false;
    tmp.fg3    = read_fg("fg3");  if (tmp.fg3.empty())   return false;
    tmp.acc    = read_fg("acc");  if (tmp.acc.empty())   return false;
    tmp.warn   = read_fg("warn"); if (tmp.warn.empty())  return false;
    tmp.bghdr  = read_combo("bghdr");  if (tmp.bghdr.empty())  return false;
    tmp.bgsel  = read_combo("bgsel");  if (tmp.bgsel.empty())  return false;
    tmp.bgplay = read_combo("bgplay"); if (tmp.bgplay.empty()) return false;
    tmp.bgstat = read_combo("bgstat"); if (tmp.bgstat.empty()) return false;

    t = std::move(tmp);
    return true;
}

// ── Theme manager ────────────────────────────────────────────────────────────
// Holds all themes (built-in + XML), tracks the active index.
struct ThemeManager {
    std::vector<Theme> themes;
    int current = 0;

    ThemeManager() : themes(make_builtin_themes()) {}

    // Scan a directory for *.xml files and load them as extra themes.
    void load_xml_dir(const std::string& dir) {
        if (!fs::exists(dir)) return;
        for (auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto& p = entry.path();
            if (p.extension() != ".xml") continue;
            Theme t;
            if (load_theme_xml(p.string(), t)) {
                // Replace existing theme with same name, or append.
                auto it = std::find_if(themes.begin(), themes.end(),
                    [&](const Theme& x){ return x.name == t.name; });
                if (it != themes.end()) *it = std::move(t);
                else                    themes.push_back(std::move(t));
            }
        }
    }

    int count() const { return (int)themes.size(); }

    const Theme& get(int idx) const {
        int n = count();
        idx = ((idx % n) + n) % n;
        return themes[idx];
    }

    void set(int idx) {
        int n = count();
        current = ((idx % n) + n) % n;
    }

    void next() { set(current + 1); }

    const Theme& active() const { return get(current); }
};
