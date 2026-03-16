#pragma once
/*
  frontend/draw.hpp
  TUI rendering: draw_player(), draw_browser(), browse().
*/

#include "ansi.hpp"
#include "../backend/player.hpp"

#include <filesystem>
#include <pwd.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── File browser data ────────────────────────────────────────────────────────
struct BD { std::vector<std::string> dirs, songs; };

inline BD scan(const std::string& path) {
    BD d;
    try {
        for (auto& e : fs::directory_iterator(path)) {
            std::string nm = e.path().filename().string();
            if (nm.empty() || nm[0] == '.') continue;
            if (e.is_directory())                          d.dirs.push_back(nm);
            else if (e.is_regular_file() && is_audio(nm)) d.songs.push_back(nm);
        }
    } catch (...) {}
    auto ic = [](const std::string& a, const std::string& b) {
        return icase_sort_key(a) < icase_sort_key(b);
    };
    std::sort(d.dirs.begin(),  d.dirs.end(),  ic);
    std::sort(d.songs.begin(), d.songs.end(), ic);
    return d;
}

inline int count_au(const std::string& p) {
    int n = 0;
    try {
        for (auto& e : fs::directory_iterator(p))
            if (e.is_regular_file() && is_audio(e.path().filename().string())) ++n;
    } catch (...) {}
    return n;
}

// ── Draw: player ─────────────────────────────────────────────────────────────
inline void draw_player(Player& p, const ThemeManager& mgr) {
    auto [cols, rows] = tsz();
    int total = (int)p.songs.size();

    int max_vis = std::max(1, rows - 4);
    int start   = std::max(0, p.row - max_vis / 2);
    int end     = std::min(total, start + max_vis);
    if (end == total) start = std::max(0, total - max_vis);

    std::string o; o.reserve(cols * rows * 6);
    o += "\033[H\033[2J"; o += A::HIDE;

    // ── Header ───────────────────────────────────────────────────────────────
    o += A::BG_HDR; o += A::BOLD;
    o += " "; o += A::GRN; o += A::NOTE; o += " "; o += A::W1; o += "CMUS++";
    {
        std::string dn = fs::path(p.dir).filename().string();
        if (dn.empty()) dn = p.dir;
        int theme_w = 2 + (int)mgr.active().name.size();
        o += A::W2; o += center_in(dn, cols - 9 - theme_w);
        o += A::W3; o += " "; o += mgr.active().name; o += " ";
    }
    o += A::RST; o += "\n";

    // ── Playlist ─────────────────────────────────────────────────────────────
    for (int i = start; i < end; ++i) {
        const std::string& s = p.songs[i];
        bool cur  = (i == p.row), play = (s == p.playing_now);
        if (cur && play) {
            o += A::BG_PLAY; o += A::BOLD;
            o += " "; o += A::PLAY_I; o += "  ";
            o += pad_r(s, cols - 5); o += A::RST;
        } else if (cur) {
            o += A::BG_SEL; o += A::BOLD;
            o += "  "; o += A::ARR; o += "  ";
            o += pad_r(s, cols - 5); o += A::RST;
        } else if (play) {
            o += A::GRN;
            o += " "; o += A::NOTE; o += "  ";
            o += pad_r(s, cols - 5); o += A::RST;
        } else {
            o += A::W3;
            o += "     ";
            o += pad_r(s, cols - 5); o += A::RST;
        }
        o += "\n";
    }
    for (int i = end - start; i < max_vis; ++i) o += "\033[K\n";

    // ── Now playing title + elapsed/duration ─────────────────────────────────
    {
        double el = p.elapsed(), dur = p.duration();
        std::string now = p.playing_now.empty() ? "Stopped" : p.playing_now;
        std::string timestr;
        timestr += A::W2; timestr += fmt_t(el);
        if (dur > 0) { timestr += A::W3; timestr += " / "; timestr += A::W2; timestr += fmt_t(dur); }
        timestr += A::RST;
        int time_vis = (dur > 0) ? (cpw(fmt_t(el)) + 3 + cpw(fmt_t(dur))) : cpw(fmt_t(el));
        int title_w  = cols - time_vis - 1;
        o += A::W0; o += A::BOLD;
        o += pad_r(now, title_w);
        o += A::RST; o += timestr; o += "\n";
    }

    // ── Progress bar ─────────────────────────────────────────────────────────
    {
        double el = p.elapsed(), dur = p.duration();
        int bw = cols;
        if (dur > 0) {
            int f = (int)(std::min(1.0, el / dur) * bw);
            o += A::GRN; o += rep(A::PROG, f);
            o += A::W0;  o += A::DOT;
            o += A::W3;  o += rep(A::TRACK, std::max(0, bw - f - 1));
        } else {
            static const char* sp[] = {"⣾","⣽","⣻","⢿","⡿","⣟","⣯","⣷"};
            o += A::AMB; o += sp[(int)(el * 6) % 8]; o += A::RST;
            o += A::W3; o += rep(A::TRACK, bw - 1);
        }
        o += A::RST; o += "\n";
    }

    // ── Status bar ───────────────────────────────────────────────────────────
    o += A::BG_STAT;
    if (p.paused)                   o += " " + std::string(A::AMB) + A::PAUS_I;
    else if (p.playing_now.empty()) o += " " + std::string(A::W3)  + A::STOP_I;
    else                            o += " " + std::string(A::GRN)  + A::PLAY_I;
    o += A::W2;

    int vn = (int)(p.volume * 8);
    o += " "; o += A::VOL_I; o += " ";
    o += A::GRN; o += rep(A::FULL,  vn);
    o += A::W3;  o += rep(A::EMPTY, 8 - vn);
    o += A::W2;

    char tc[20]; snprintf(tc, sizeof(tc), "  %d/%d", p.row + 1, total);
    o += tc;

    o += " ";
    o += (p.shuffle ? A::GRN : A::W3);
    o += A::SHUF_I; o += (p.shuffle ? " SHUF" : " shuf");
    o += A::W3; o += "  ";
    o += (p.loop_on ? A::GRN : A::W3);
    o += A::LOOP_I; o += (p.loop_on ? " LOOP" : " loop");
    o += A::W2;

    const char* hints = " · ↑↓/jk · Enter · n/p · Space · ←→ seek · +/- vol · t theme · o browse · q quit";
    int left_vis = 2 + 12 + (int)std::string(tc).size() + 7 + 7;
    int fill = std::max(0, cols - left_vis - cpw(hints));
    o += std::string(fill, ' ');
    o += A::W3; o += hints; o += A::RST;

    emit(o); flush_out();
}

// ── Draw: file browser ───────────────────────────────────────────────────────
inline void draw_browser(const std::string& path, const BD& d, int cur, int scr) {
    auto [cols, rows] = tsz();
    int max_vis = std::max(1, rows - 9);
    int total   = (int)(d.dirs.size() + d.songs.size());

    std::string o;
    o += HOME_NOFLASH; o += A::HIDE;

    // Header
    o += A::BG_HDR; o += A::BOLD;
    o += "  "; o += A::GRN; o += A::NOTE; o += "  ";
    o += A::W1; o += "CMUS++";
    o += A::W3; o += "  "; o += A::DIR_I; o += "  ";
    o += A::W2; o += "File Browser";
    o += std::string(std::max(0, cols - 27), ' ');
    o += A::RST; o += "\n";

    o += A::W3; o += " "; o += trunc_str(path, cols - 2); o += A::RST; o += "\n";

    if (!d.songs.empty()) {
        o += " "; o += A::GRN; o += A::NOTE;
        o += "  " + std::to_string(d.songs.size()) + " track";
        if (d.songs.size() > 1) o += "s";
        o += A::RST;
    } else { o += " "; o += A::W3; o += "no audio files"; o += A::RST; }
    o += "\n";

    o += A::W3; o += rep(A::HL, cols); o += A::RST; o += "\n";

    int end_i = std::min(total, scr + max_vis);
    for (int i = scr; i < end_i; ++i) {
        bool sel = (i == cur);
        if (i < (int)d.dirs.size()) {
            const std::string& nm = d.dirs[i];
            int sc = count_au(path + "/" + nm);
            std::string badge;
            if (sc > 0)
                badge = std::string(" ") + A::GRN + A::NOTE + std::to_string(sc) + A::RST;
            int badge_w = sc > 0 ? (2 + (int)std::to_string(sc).size()) : 0;
            int name_w  = cols - 8 - badge_w;
            if (sel) {
                o += A::BG_SEL; o += A::BOLD;
                o += "  "; o += A::ARR; o += "  "; o += A::DIR_I; o += " ";
                o += pad_r(nm, name_w); o += badge; o += A::RST;
            } else {
                o += A::W3;
                o += "     "; o += A::DIR_I; o += " ";
                o += pad_r(nm, name_w); o += badge; o += A::RST;
            }
        } else {
            const std::string& nm = d.songs[i - d.dirs.size()];
            if (sel) {
                o += A::BG_SEL; o += A::GRN; o += A::BOLD;
                o += "  "; o += A::ARR; o += "  "; o += A::NOTE; o += " ";
                o += pad_r(nm, cols - 9); o += A::RST;
            } else {
                o += A::W2;
                o += "     "; o += A::NOTE; o += " ";
                o += pad_r(nm, cols - 9); o += A::RST;
            }
        }
        o += "\n";
    }
    for (int i = end_i - scr; i < max_vis; ++i) o += "\033[K\n";
    if (total == 0) { o += A::W3; o += "  (empty)"; o += A::RST; o += "\n"; }
    if (total > max_vis) {
        char b[48]; snprintf(b, sizeof(b), "  %d-%d of %d", scr + 1, end_i, total);
        o += A::W3; o += b; o += A::RST; o += "\n";
    }

    o += A::W3; o += rep(A::HL, cols); o += A::RST; o += "\n";
    o += A::BG_STAT;
    std::string kb_line;
    auto kb = [&](const char* k, const char* v) {
        kb_line += std::string(" ") + A::W1 + k + A::W3 + " " + v;
    };
    kb("↑↓","nav"); kb("→/Enter","open"); kb("←/Bksp","up"); kb("o","home"); kb("q","cancel");
    int kb_vis = 47;
    o += kb_line;
    o += std::string(std::max(0, cols - kb_vis), ' ');
    o += A::RST;

    emit(o); flush_out();
}

// ── Browse: interactive file picker ─────────────────────────────────────────
inline std::string browse(RawTerm& rt, const std::string& start = "") {
    std::string cur = start.empty()
        ? std::string(getpwuid(getuid())->pw_dir)
        : start;
    try { cur = fs::canonical(cur).string(); } catch (...) {}

    int cursor = 0, scroll = 0;
    auto d = scan(cur);
    draw_browser(cur, d, cursor, scroll);

    while (true) {
        auto [cols, rows] = tsz();
        int max_vis = std::max(1, rows - 9);
        int total   = (int)(d.dirs.size() + d.songs.size());
        std::string key = read_key(rt, -1);
        if (key.empty()) continue;
        bool redraw = true;

        if (key == "\x1b[A" || key == "k") {
            if (total) cursor = (cursor - 1 + total) % total;
        } else if (key == "\x1b[B" || key == "j") {
            if (total) cursor = (cursor + 1) % total;
        } else if (key == "\x1b[C" || key == "\r" || key == "l") {
            if (total == 0) { redraw = false; }
            else if (cursor < (int)d.dirs.size()) {
                std::string nxt = cur + "/" + d.dirs[cursor];
                try { nxt = fs::canonical(nxt).string(); } catch (...) {}
                cur = nxt; d = scan(cur); cursor = scroll = 0;
                if (d.dirs.empty() && !d.songs.empty()) {
                    emit(std::string(A::CLS) + A::SHOW); flush_out(); return cur;
                }
            } else {
                emit(std::string(A::CLS) + A::SHOW); flush_out(); return cur;
            }
        } else if (key == "\x1b[D" || key == "\x7f" || key == "h") {
            fs::path pp = fs::path(cur).parent_path();
            if (pp.string() != cur) {
                std::string prev = fs::path(cur).filename().string();
                cur = pp.string(); d = scan(cur);
                auto it = std::find(d.dirs.begin(), d.dirs.end(), prev);
                cursor = (it != d.dirs.end()) ? (int)(it - d.dirs.begin()) : 0;
                scroll = 0;
            }
        } else if (key == "o" || key == "O") {
            cur = std::string(getpwuid(getuid())->pw_dir);
            d = scan(cur); cursor = scroll = 0;
        } else if (key == "q" || key == "Q" || key == "\x03") {
            emit(std::string(A::CLS) + A::SHOW); flush_out(); return "";
        } else { redraw = false; }

        if (redraw) {
            total = (int)(d.dirs.size() + d.songs.size());
            if (cursor < scroll)               scroll = cursor;
            else if (cursor >= scroll + max_vis) scroll = cursor - max_vis + 1;
            draw_browser(cur, d, cursor, scroll);
        }
    }
}
