#pragma once
/*
  frontend/ansi.hpp
  Runtime ANSI palette (populated from ThemeManager),
  terminal size query, raw I/O helpers, string layout utils.
*/

#include "theme.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

// ── ANSI escape constants ────────────────────────────────────────────────────
namespace A {
    inline const char* RST  = "\033[0m";
    inline const char* BOLD = "\033[1m";
    inline const char* DIM  = "\033[2m";
    inline const char* CLS  = "\033[H\033[2J";
    inline const char* HIDE = "\033[?25l";
    inline const char* SHOW = "\033[?25h";

    // Runtime palette — set by apply_theme()
    inline const char* W0="", *W1="", *W2="", *W3="";
    inline const char* GRN="", *AMB="";
    inline const char* BG_HDR="", *BG_SEL="", *BG_PLAY="", *BG_STAT="";

    // Icons
    inline const char* PLAY_I = "▶";
    inline const char* PAUS_I = "⏸";
    inline const char* STOP_I = "■";
    inline const char* NOTE   = "♪";
    inline const char* ARR    = "›";
    inline const char* DIR_I  = "▸";
    inline const char* SHUF_I = "⇄";
    inline const char* LOOP_I = "↺";
    inline const char* VOL_I  = "▐";
    inline const char* PROG   = "━";
    inline const char* TRACK  = "─";
    inline const char* DOT    = "●";
    inline const char* FULL   = "█";
    inline const char* EMPTY  = "░";
    inline const char* HL     = "─";
}

// ── Theme application ────────────────────────────────────────────────────────
// We store the c_str() pointers into a thread-local cache to avoid re-hashing.
// The strings must stay alive as long as the pointers are used (they do —
// they live inside ThemeManager::themes which is alive for the whole program).
inline void apply_theme(ThemeManager& mgr, int idx) {
    mgr.set(idx);
    const Theme& t = mgr.active();
    A::W0      = t.fg0.c_str();   A::W1  = t.fg1.c_str();
    A::W2      = t.fg2.c_str();   A::W3  = t.fg3.c_str();
    A::GRN     = t.acc.c_str();   A::AMB = t.warn.c_str();
    A::BG_HDR  = t.bghdr.c_str(); A::BG_SEL  = t.bgsel.c_str();
    A::BG_PLAY = t.bgplay.c_str();A::BG_STAT = t.bgstat.c_str();
}

inline void init_colors(ThemeManager& mgr) { apply_theme(mgr, 0); }

// ── Terminal size ────────────────────────────────────────────────────────────
struct TSz { int cols, rows; };
inline TSz tsz() {
    struct winsize w{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return { w.ws_col > 10 ? w.ws_col : 80, w.ws_row > 5 ? w.ws_row : 24 };
}

// Move cursor to top-left without clearing (avoids flash)
static const char* HOME_NOFLASH = "\033[H";

// ── Raw I/O ──────────────────────────────────────────────────────────────────
inline void emit(const std::string& s) {
    const char* p = s.data();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = ::write(STDOUT_FILENO, p, left);
        if (n <= 0) break;
        p += n; left -= n;
    }
}
inline void flush_out() { /* write() is unbuffered — no-op */ }

// ── String layout helpers ────────────────────────────────────────────────────
inline std::string rep(const std::string& s, int n) {
    if (n <= 0) return {};
    std::string r; r.reserve(s.size() * n);
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

// Count Unicode codepoints in a plain (no ANSI escapes) string
inline int cpw(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        i += (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        ++w;
    }
    return w;
}

// Truncate a plain string to `lim` visible columns
inline std::string trunc_str(const std::string& s, int lim) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        int sk = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        if (w + 1 >= lim && i + sk < s.size()) return s.substr(0, i) + "…";
        w++; i += sk;
    }
    return s;
}

// Pad a plain string to exactly `w` columns
inline std::string pad_r(const std::string& s, int w) {
    int dw = cpw(s);
    if (dw >= w) return trunc_str(s, w);
    return s + std::string(w - dw, ' ');
}

// Center a plain string within `w` columns
inline std::string center_in(const std::string& s, int w) {
    int dw = cpw(s);
    if (dw >= w) return trunc_str(s, w);
    int lp = (w - dw) / 2;
    return std::string(lp, ' ') + s + std::string(w - dw - lp, ' ');
}

// Format seconds as "M:SS"
inline std::string fmt_t(double s) {
    int v = std::max(0, (int)s);
    char b[12]; snprintf(b, sizeof(b), "%d:%02d", v / 60, v % 60);
    return b;
}

// ── Raw terminal input ───────────────────────────────────────────────────────
struct RawTerm {
    int fd; struct termios sv{};
    RawTerm(int f) : fd(f) { tcgetattr(fd, &sv); }
    void on()  const { struct termios r = sv; cfmakeraw(&r); tcsetattr(fd, TCSADRAIN, &r); }
    void off() const { tcsetattr(fd, TCSADRAIN, &sv); }
    ~RawTerm() { off(); }
};

inline std::string read_key(RawTerm& rt, double timeout) {
    rt.on();
    auto try1 = [&](double t) -> std::optional<char> {
        fd_set fds; FD_ZERO(&fds); FD_SET(rt.fd, &fds);
        struct timeval tv{}, *tvp = (t >= 0) ? &tv : nullptr;
        if (t >= 0) { tv.tv_sec = (long)t; tv.tv_usec = (long)((t - (long)t) * 1e6); }
        if (select(rt.fd + 1, &fds, nullptr, nullptr, tvp) <= 0) return {};
        char c; return (read(rt.fd, &c, 1) == 1) ? std::optional<char>(c) : std::nullopt;
    };
    auto ch = try1(timeout); rt.off();
    if (!ch) return "";
    if (*ch != '\x1b') return std::string(1, *ch);
    rt.on();
    std::string seq(1, *ch);
    for (int i = 0; i < 6; ++i) {
        auto b = try1(0.05); if (!b) break;
        seq += *b; if (isalpha(*b) || *b == '~') break;
    }
    rt.off();
    return seq;
}
