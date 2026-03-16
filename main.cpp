/*
  ██████╗███╗   ███╗██╗   ██╗███████╗    ██╗  ██╗
 ██╔════╝████╗ ████║██║   ██║██╔════╝    ╚██╗██╔╝
 ██║     ██╔████╔██║██║   ██║███████╗     ╚███╔╝
 ██║     ██║╚██╔╝██║██║   ██║╚════██║     ██╔██╗
 ╚██████╗██║ ╚═╝ ██║╚██████╔╝███████║ ██╗██╔╝ ██╗
  ╚═════╝╚═╝     ╚═╝ ╚═════╝ ╚══════╝ ╚═╝╚═╝  ╚═╝
                   ++ C++ Terminal Music Player

 Audio decode : libsndfile  (MP3 · FLAC · WAV · OGG · OPUS · AIFF)
 Audio output : ALSA (Linux) · CoreAudio (macOS) · WinMM (Windows)
 TUI          : ANSI/POSIX — zero extra runtime deps
 Themes       : built-in palettes + XML files in ./themes/

 Build:  ./bootstrap.sh
*/

#include "frontend/draw.hpp"   // pulls in ansi.hpp → theme.hpp
#include "backend/player.hpp"  // pulls in audio_out.hpp → decoder.hpp → ring.hpp

#include <csignal>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ── Global theme manager ────────────────────────────────────────────────────
static ThemeManager g_themes;

int main() {
    // Load any XML themes from the themes/ directory next to the binary.
    // Themes with the same name as a built-in will override it.
    g_themes.load_xml_dir("themes");

    init_colors(g_themes);
    signal(SIGPIPE, SIG_IGN);
    RawTerm rt(STDIN_FILENO);

    // Enter alternate screen buffer + enable synchronized output (no flicker)
    emit("\033[?1049h\033[?2026h");
    emit(A::CLS);

    std::string sel = browse(rt);
    if (sel.empty()) {
        emit("\033[?2026l\033[?1049l");
        emit(A::SHOW);
        return 0;
    }

    Player player;
    player.load_dir(sel);
    draw_player(player, g_themes);

    while (true) {
        if (player.songs.empty()) {
            emit(std::string(A::CLS) + A::SHOW); flush_out();
            sel = browse(rt);
            if (!sel.empty()) { player.load_dir(sel); draw_player(player, g_themes); }
            else break;
            continue;
        }

        if (!player.playing_now.empty() && !player.paused && player.is_ended()) {
            player.next_song(); draw_player(player, g_themes);
        }

        std::string key = read_key(rt, 0.12);
        if (key.empty()) {
            if (!player.playing_now.empty() && !player.paused) draw_player(player, g_themes);
            continue;
        }

        bool redraw = true;
        int n = (int)player.songs.size();

        if      (key == "\x1b[A" || key == "k") player.row = (player.row - 1 + n) % n;
        else if (key == "\x1b[B" || key == "j") player.row = (player.row + 1) % n;
        else if (key == "\r")                    player.play_current();
        else if (key == "n" || key == "N")       player.next_song();
        else if (key == "p" || key == "P")       player.prev_song();
        else if (key == " ")                     player.toggle_pause();
        else if (key == "\x1b[D" || key == "h")  player.seek(-5.0);
        else if (key == "\x1b[C" || key == "l")  player.seek(+5.0);
        else if (key == "+" || key == "=")       player.change_vol(+0.05f);
        else if (key == "-" || key == "_")       player.change_vol(-0.05f);
        else if (key == "s" || key == "S")       player.shuffle = !player.shuffle;
        else if (key == "r" || key == "R")       player.loop_on = !player.loop_on;
        else if (key == "t" || key == "T")       apply_theme(g_themes, g_themes.current + 1);
        else if (key == "o" || key == "O") {
            emit(std::string(A::CLS) + A::SHOW); flush_out();
            std::string s2 = browse(rt);
            if (!s2.empty()) player.load_dir(s2);
        }
        else if (key == "q" || key == "Q") break;
        else redraw = false;

        if (redraw) draw_player(player, g_themes);
    }

    player.stop_all();
    emit("\033[?2026l\033[?1049l");
    emit(A::SHOW);
    return 0;
}
