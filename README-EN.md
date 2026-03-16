# CMUS++

```
 ██████╗███╗   ███╗██╗   ██╗███████╗    ██╗  ██╗
██╔════╝████╗ ████║██║   ██║██╔════╝    ╚██╗██╔╝
██║     ██╔████╔██║██║   ██║███████╗     ╚███╔╝
██║     ██║╚██╔╝██║██║   ██║╚════██║     ██╔██╗
╚██████╗██║ ╚═╝ ██║╚██████╔╝███████║ ██╗██╔╝ ██╗
 ╚═════╝╚═╝     ╚═╝ ╚═════╝ ╚══════╝ ╚═╝╚═╝  ╚═╝
                   ++ C++ Terminal Music Player
```

> 🇬🇧 English | [🇦🇷 Español](README.md)

---

## What is CMUS++?

CMUS++ is a terminal music player written in C++17. It runs entirely inside your terminal — no GUI, no Electron, no browser. Just a fast, keyboard-driven player that plays your local music files.

It was built as a single-file program and later split into a clean modular structure with separate `backend/` and `frontend/` directories, plus support for custom color themes via XML files.

### Why CMUS++?

- **Zero runtime dependencies** beyond libsndfile and your OS audio layer
- **Fast** — starts instantly, no splash screens, no loading bars
- **Keyboard-driven** — vim-style navigation (j/k, h/l)
- **Themeable** — 9 built-in color themes + unlimited custom XML themes
- **Cross-platform** — Linux (ALSA), macOS (CoreAudio), Windows (WinMM)
- **Formats** — MP3, FLAC, WAV, OGG, OPUS, AIFF, AU

---

## Requirements

Before building, make sure you have the required libraries installed.

### Linux (Ubuntu / Debian)

```bash
sudo apt install g++ libsndfile1-dev libasound2-dev
```

### Linux (Fedora / RHEL)

```bash
sudo dnf install gcc-c++ libsndfile-devel alsa-lib-devel
```

### Linux (Arch)

```bash
sudo pacman -S gcc libsndfile alsa-lib
```

### macOS

```bash
# Install Homebrew first if you don't have it: https://brew.sh
brew install libsndfile
# CoreAudio is included with Xcode Command Line Tools
xcode-select --install
```

### Windows (MinGW / MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libsndfile
# WinMM is part of the Windows SDK — no extra install needed
```

---

## Building

```bash
# 1. Enter the project directory
cd cmuspp

# 2. Make the build script executable (Linux/macOS only)
chmod +x bootstrap.sh

# 3. Build
./bootstrap.sh

# 4. Run
./cmuspp
```

The script auto-detects your platform and links the correct libraries. The resulting binary is placed in the same directory.

### Manual build (if you prefer)

```bash
# Linux
g++ -std=c++17 -O2 main.cpp -I. -lasound -lsndfile -o cmuspp

# macOS
g++ -std=c++17 -O2 main.cpp -I. -lsndfile \
    -framework AudioToolbox -framework CoreAudio -o cmuspp

# Windows (MinGW)
g++ -std=c++17 -O2 main.cpp -I. -lsndfile -lwinmm -o cmuspp.exe
```

---

## Project Structure

```
cmuspp/
├── main.cpp                  ← Entry point and main event loop
├── bootstrap.sh              ← Cross-platform build script
├── README.md                 ← Documentation in Spanish
├── README-EN.md              ← This file (English)
│
├── backend/                  ← Audio logic (no TUI code here)
│   ├── audio_common.hpp      ← Shared constants, mono_now(), platform macros
│   ├── ring.hpp              ← Lock-free SPSC ring buffer (float stereo)
│   ├── decoder.hpp           ← Decoder thread: libsndfile + bilinear resampling
│   ├── audio_out.hpp         ← AudioOut: ALSA / CoreAudio / WinMM implementations
│   └── player.hpp            ← Player: playlist, seek, volume, playback control
│
├── frontend/                 ← TUI rendering (no audio code here)
│   ├── theme.hpp             ← Theme struct, 9 built-ins, XML parser, ThemeManager
│   ├── ansi.hpp              ← A:: namespace, apply_theme(), RawTerm, string helpers
│   └── draw.hpp              ← draw_player(), draw_browser(), browse()
│
└── themes/                   ← Custom XML themes (loaded at startup)
    ├── monokai.xml
    └── solarized.xml
```

### Dependency graph

```
main.cpp
 ├── frontend/draw.hpp
 │    ├── frontend/ansi.hpp
 │    │    └── frontend/theme.hpp
 │    └── backend/player.hpp
 │         ├── backend/audio_out.hpp
 │         │    └── backend/ring.hpp
 │         │         └── backend/audio_common.hpp
 │         └── backend/decoder.hpp
 │              └── backend/ring.hpp
 └── (backend/player.hpp already included above)
```

---

## Keyboard Controls

### Player view

| Key | Action |
|---|---|
| `↑` or `k` | Move cursor up in playlist |
| `↓` or `j` | Move cursor down in playlist |
| `Enter` | Play selected song |
| `Space` | Pause / resume |
| `n` or `N` | Next song |
| `p` or `P` | Previous song |
| `←` or `h` | Seek backward 5 seconds |
| `→` or `l` | Seek forward 5 seconds |
| `+` or `=` | Increase volume |
| `-` or `_` | Decrease volume |
| `s` | Toggle shuffle |
| `r` | Toggle loop (repeat current song) |
| `t` | Cycle to next color theme |
| `o` | Open file browser |
| `q` | Quit |

### File browser

| Key | Action |
|---|---|
| `↑` or `k` | Move up |
| `↓` or `j` | Move down |
| `→`, `Enter`, or `l` | Enter directory / select folder |
| `←`, `Backspace`, or `h` | Go up one level |
| `o` | Jump to home directory |
| `q` | Cancel and return to player |

---

## Color Themes

### Built-in themes

Press `t` while the player is open to cycle through all available themes. The active theme name is shown in the top-right corner of the header bar.

| Name | Style |
|---|---|
| `default` | Dark green tones, near-black background |
| `catppuccin` | Catppuccin Mocha — soft purples and greens |
| `dracula` | Dracula — purple accents, dark background |
| `nord` | Nord — icy blues and Arctic greens |
| `gruvbox` | Gruvbox — warm browns and muted yellows |
| `rosepine` | Rosé Pine — dusty pinks and deep purples |
| `tokyonight` | Tokyo Night — neon teal on deep navy |
| `everforest` | Everforest — earthy greens and warm beige |
| `cream` | Cream — light theme with soft beige tones |

### Custom XML themes

You can create unlimited custom themes without recompiling. Just drop a `.xml` file into the `themes/` folder next to the `cmuspp` binary.

**The `themes/` folder must be in the same directory as the executable.**

#### XML format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<theme name="my-theme">

  <!-- Text brightness levels: bright → normal → dim → muted -->
  <fg0  r="248" g="248" b="242"/>
  <fg1  r="215" g="210" b="195"/>
  <fg2  r="117" g="113" b="94"/>
  <fg3  r="75"  g="71"  b="60"/>

  <!-- Accent color (playing indicator, progress bar, etc.) -->
  <acc  r="166" g="226" b="46"/>

  <!-- Warning / amber color (pause indicator, loading spinner) -->
  <warn r="230" g="219" b="116"/>

  <!-- UI zone color combos: bgr/bgg/bgb = background, fgr/fgg/fgb = text -->
  <bghdr  bgr="39"  bgg="40"  bgb="34"  fgr="102" fgg="217" fgb="239"/>
  <bgsel  bgr="73"  bgg="72"  bgb="62"  fgr="248" fgg="248" fgb="242"/>
  <bgplay bgr="30"  bgg="44"  bgb="18"  fgr="166" fgg="226" fgb="46"/>
  <bgstat bgr="29"  bgg="29"  bgb="24"  fgr="117" fgg="113" fgb="94"/>

</theme>
```

#### UI zones explained

| Field | Where it appears |
|---|---|
| `bghdr` | Top header bar (directory name, theme name) |
| `bgsel` | Currently selected (highlighted) row in the playlist |
| `bgplay` | The row of the song that is currently playing |
| `bgstat` | Bottom status bar (play/pause icon, volume, shuffle, hints) |

#### Finding color palettes

Good sources for RGB color values to use in your XML themes:

- **[catppuccin.com](https://catppuccin.com)** — huge collection of pastel themes with exact RGB values
- **[terminal.sexy](https://terminal.sexy)** — browser-based theme designer and converter
- **[gogh](https://gogh-theme.github.io/gogh/)** — 200+ terminal color schemes
- **[base16](https://tinted-theming.github.io/base16-gallery/)** — standardized 16-color system used by many editors
- Any Neovim / Alacritty / Kitty theme — they all use RGB hex values you can convert

To convert hex to RGB: `#A6E22E` → `r="166" g="226" b="46"`

#### Rules and notes

- The `name` attribute must be unique. If it matches a built-in theme name, it **overrides** it.
- All 10 fields (`fg0`–`fg3`, `acc`, `warn`, `bghdr`, `bgsel`, `bgplay`, `bgstat`) are required. A file with any missing field is silently ignored.
- Changes to XML files take effect on the **next restart** of the program.
- XML themes appear **after** the built-in themes in the `t` cycle.
- You can have as many XML theme files as you want in the `themes/` folder.

---

## How It Works (Architecture)

### Backend

The backend handles all audio processing and is completely independent from the TUI.

**`Ring`** (`backend/ring.hpp`)
A lock-free single-producer single-consumer circular buffer that holds interleaved stereo float samples. The decoder writes into it; the audio output thread reads from it. `std::atomic` head/tail pointers make the hot path allocation-free. `condition_variable` instances allow each side to sleep efficiently when the buffer is full or empty.

**`Decoder`** (`backend/decoder.hpp`)
Runs on its own thread. Opens audio files using libsndfile, reads them in 4096-frame chunks, performs bilinear resampling to 44100 Hz stereo if needed, and writes the result into the Ring. Blocks when the ring is full, wakes up when space is available.

**`AudioOut`** (`backend/audio_out.hpp`)
Reads from the Ring and sends PCM data to the hardware. There are three implementations inside `#ifdef` guards — ALSA, CoreAudio, and WinMM — but they all expose the same interface: `open()`, `attach(Ring&)`, `swap_ring(Ring&)`, `stop()`. The `swap_ring` method enables glitch-free seeking: a new Decoder is started for the new position and the audio thread seamlessly switches to the new ring without stopping.

**`Player`** (`backend/player.hpp`)
The high-level coordinator. Owns an `AudioOut` and a `Decoder`. Manages the playlist vector, current row, shuffle/loop state, volume, elapsed time tracking, and triggers the next song when the current one ends.

### Frontend

The frontend handles all terminal rendering and is completely independent from audio.

**`ThemeManager`** (`frontend/theme.hpp`)
Holds all themes — built-ins generated at compile time and any XML files loaded at startup. Provides `next()`, `set(idx)`, `active()`. Colors are stored as `std::string` containing pre-built ANSI escape sequences, so switching themes is just updating `const char*` pointers in the `A::` namespace — no string building at render time.

**`ansi.hpp`** (`frontend/ansi.hpp`)
The `A::` namespace holds all active color pointers plus ANSI escape constants. Also provides: `emit()` (unbuffered write to stdout), `RawTerm` (RAII raw terminal mode), `read_key()` (reads single keypresses including escape sequences), and string layout utilities (`pad_r`, `center_in`, `trunc_str`, `fmt_t`, `cpw`).

**`draw.hpp`** (`frontend/draw.hpp`)
Two render functions and one interactive loop. `draw_player()` and `draw_browser()` each build an entire screen's worth of ANSI-colored text into a single `std::string` and call `emit()` once — this avoids flickering caused by partial screen updates. `browse()` is the interactive file navigator loop.

---

## Supported Formats

Support depends on what codecs libsndfile was compiled with on your system. Standard installations cover:

| Format | Extensions | Notes |
|---|---|---|
| MP3 | `.mp3` | Requires libsndfile ≥ 1.1.0 |
| FLAC | `.flac` | Fully supported |
| WAV | `.wav` | Fully supported, all bit depths |
| OGG Vorbis | `.ogg` | Fully supported |
| Opus | `.opus` | Requires libopus |
| AIFF | `.aiff` `.aif` | Fully supported |
| AU / SND | `.au` | Fully supported |

---

## Troubleshooting

**`cannot open audio device`**
On Linux, check that ALSA is working: `aplay -l` should list your sound cards. Make sure no other application has exclusive access to the device.

**MP3 files don't play**
Your libsndfile version may be older than 1.1.0. Check with `pkg-config --modversion sndfile`. On older distros, install `libmpg123-dev` and rebuild libsndfile from source with MP3 support.

**Colors look wrong / no colors**
Make sure your terminal supports truecolor (24-bit color). Test with:
```bash
printf '\033[38;2;255;100;0mThis should be orange\033[0m\n'
```
If it shows orange, truecolor works. Compatible terminals: Alacritty, Kitty, WezTerm, iTerm2, GNOME Terminal ≥ 3.36, Windows Terminal.

**Theme XML not loading**
- The `themes/` folder must be in the **same directory as the `cmuspp` binary**, not the source directory.
- All 10 color fields must be present in the XML.
- The file must have a `.xml` extension.
- Run from the binary's directory: `cd /path/to/cmuspp && ./cmuspp`

---

## License

MIT — do whatever you want with it.
