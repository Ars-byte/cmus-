# cmus-
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

> 🇦🇷 Español | [🇬🇧 English](README-EN.md)

---

## ¿Qué es CMUS++?

CMUS++ es un reproductor de música para terminal escrito en C++17. Corre completamente dentro de tu terminal — sin GUI, sin Electron, sin navegador. Solo un reproductor rápido, controlado por teclado, que reproduce tus archivos de música locales.

Empezó como un programa de un solo archivo y después se dividió en una estructura modular con carpetas separadas `backend/` y `frontend/`, más soporte para temas de color personalizados mediante archivos XML.

### ¿Por qué CMUS++?

- **Cero dependencias de runtime** más allá de libsndfile y la capa de audio del OS
- **Rápido** — arranca al instante, sin pantallas de carga
- **Controlado por teclado** — navegación estilo vim (j/k, h/l)
- **Temeable** — 9 temas de color built-in + temas XML ilimitados
- **Multiplataforma** — Linux (ALSA), macOS (CoreAudio), Windows (WinMM)
- **Formatos** — MP3, FLAC, WAV, OGG, OPUS, AIFF, AU

---

## Requisitos

Antes de compilar, asegurate de tener las librerías necesarias instaladas.

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
# Instalá Homebrew primero si no lo tenés: https://brew.sh
brew install libsndfile
# CoreAudio viene incluido con Xcode Command Line Tools
xcode-select --install
```

### Windows (MinGW / MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libsndfile
# WinMM es parte del Windows SDK — no hace falta instalar nada extra
```

---

## Compilación

```bash
# 1. Entrar al directorio del proyecto
cd cmuspp

# 2. Dar permisos de ejecución al script (solo Linux/macOS)
chmod +x bootstrap.sh

# 3. Compilar
./bootstrap.sh

# 4. Ejecutar
./cmuspp
```

El script detecta tu plataforma automáticamente y enlaza las librerías correctas. El binario resultante queda en el mismo directorio.

### Compilación manual (si preferís)

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

## Estructura del proyecto

```
cmuspp/
├── main.cpp                  ← Punto de entrada y event loop principal
├── bootstrap.sh              ← Script de build multiplataforma
├── README.md                 ← Este archivo (español)
├── README-EN.md              ← Documentación en inglés
│
├── backend/                  ← Lógica de audio (sin código de TUI)
│   ├── audio_common.hpp      ← Constantes compartidas, mono_now(), macros de plataforma
│   ├── ring.hpp              ← Ring buffer SPSC lock-free (float estéreo)
│   ├── decoder.hpp           ← Hilo decodificador: libsndfile + resampleo bilineal
│   ├── audio_out.hpp         ← AudioOut: implementaciones ALSA / CoreAudio / WinMM
│   └── player.hpp            ← Player: playlist, seek, volumen, control de reproducción
│
├── frontend/                 ← Renderizado TUI (sin código de audio)
│   ├── theme.hpp             ← struct Theme, 9 built-ins, parser XML, ThemeManager
│   ├── ansi.hpp              ← Namespace A::, apply_theme(), RawTerm, helpers de string
│   └── draw.hpp              ← draw_player(), draw_browser(), browse()
│
└── themes/                   ← Temas XML personalizados (se cargan al arrancar)
    ├── monokai.xml
    └── solarized.xml
```

### Gráfico de dependencias

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
 └── (backend/player.hpp ya incluido arriba)
```

---

## Controles de teclado

### Vista del reproductor

| Tecla | Acción |
|---|---|
| `↑` o `k` | Subir cursor en la playlist |
| `↓` o `j` | Bajar cursor en la playlist |
| `Enter` | Reproducir canción seleccionada |
| `Space` | Pausa / reanudar |
| `n` o `N` | Siguiente canción |
| `p` o `P` | Canción anterior |
| `←` o `h` | Retroceder 5 segundos |
| `→` o `l` | Avanzar 5 segundos |
| `+` o `=` | Subir volumen |
| `-` o `_` | Bajar volumen |
| `s` | Toggle shuffle (aleatorio) |
| `r` | Toggle loop (repetir canción actual) |
| `t` | Ciclar al siguiente tema de color |
| `o` | Abrir navegador de archivos |
| `q` | Salir |

### Navegador de archivos

| Tecla | Acción |
|---|---|
| `↑` o `k` | Subir |
| `↓` o `j` | Bajar |
| `→`, `Enter` o `l` | Entrar al directorio / seleccionar carpeta |
| `←`, `Backspace` o `h` | Subir un nivel |
| `o` | Ir al directorio home |
| `q` | Cancelar y volver al reproductor |

---

## Temas de color

### Temas built-in

Presioná `t` mientras el reproductor está abierto para ciclar entre todos los temas disponibles. El nombre del tema activo se muestra en la esquina superior derecha de la barra de header.

| Nombre | Estilo |
|---|---|
| `default` | Tonos verde oscuro, fondo casi negro |
| `catppuccin` | Catppuccin Mocha — púrpuras suaves y verdes |
| `dracula` | Dracula — acentos púrpura, fondo oscuro |
| `nord` | Nord — azules helados y verdes árticos |
| `gruvbox` | Gruvbox — marrones cálidos y amarillos apagados |
| `rosepine` | Rosé Pine — rosas empolvados y púrpuras profundos |
| `tokyonight` | Tokyo Night — cian neón sobre azul marino profundo |
| `everforest` | Everforest — verdes terrosos y beige cálido |
| `cream` | Cream — tema claro con tonos beige suaves |

### Temas XML personalizados

Podés crear temas ilimitados sin recompilar. Solo tirá un archivo `.xml` en la carpeta `themes/` que está al lado del binario `cmuspp`.

**La carpeta `themes/` tiene que estar en el mismo directorio que el ejecutable.**

#### Formato XML

```xml
<?xml version="1.0" encoding="UTF-8"?>
<theme name="mi-tema">

  <!-- Niveles de brillo del texto: brillante → normal → tenue → apagado -->
  <fg0  r="248" g="248" b="242"/>
  <fg1  r="215" g="210" b="195"/>
  <fg2  r="117" g="113" b="94"/>
  <fg3  r="75"  g="71"  b="60"/>

  <!-- Color de acento (indicador de reproducción, barra de progreso, etc.) -->
  <acc  r="166" g="226" b="46"/>

  <!-- Color de advertencia / ámbar (indicador de pausa, spinner de carga) -->
  <warn r="230" g="219" b="116"/>

  <!-- Combos de color por zona: bgr/bgg/bgb = fondo, fgr/fgg/fgb = texto -->
  <bghdr  bgr="39"  bgg="40"  bgb="34"  fgr="102" fgg="217" fgb="239"/>
  <bgsel  bgr="73"  bgg="72"  bgb="62"  fgr="248" fgg="248" fgb="242"/>
  <bgplay bgr="30"  bgg="44"  bgb="18"  fgr="166" fgg="226" fgb="46"/>
  <bgstat bgr="29"  bgg="29"  bgb="24"  fgr="117" fgg="113" fgb="94"/>

</theme>
```

#### Zonas de la UI explicadas

| Campo | Dónde aparece |
|---|---|
| `bghdr` | Barra de header superior (nombre del directorio, nombre del tema) |
| `bgsel` | Fila actualmente seleccionada (resaltada) en la playlist |
| `bgplay` | La fila de la canción que se está reproduciendo en este momento |
| `bgstat` | Barra de estado inferior (ícono play/pausa, volumen, shuffle, hints) |

#### Dónde conseguir paletas de colores

Buenas fuentes de valores RGB para usar en tus temas XML:

- **[catppuccin.com](https://catppuccin.com)** — colección enorme de temas pastel con valores RGB exactos
- **[terminal.sexy](https://terminal.sexy)** — diseñador y convertidor de temas en el navegador
- **[gogh](https://gogh-theme.github.io/gogh/)** — más de 200 esquemas de color para terminal
- **[base16](https://tinted-theming.github.io/base16-gallery/)** — sistema estandarizado de 16 colores usado por muchos editores
- Cualquier tema de Neovim / Alacritty / Kitty — todos usan valores RGB en hex que podés convertir

Para convertir hex a RGB: `#A6E22E` → `r="166" g="226" b="46"`

#### Reglas y notas importantes

- El atributo `name` tiene que ser único. Si coincide con el nombre de un tema built-in, lo **sobreescribe**.
- Los 10 campos (`fg0`–`fg3`, `acc`, `warn`, `bghdr`, `bgsel`, `bgplay`, `bgstat`) son todos obligatorios. Un archivo al que le falte alguno se ignora silenciosamente.
- Los cambios en los archivos XML se aplican al **próximo reinicio** del programa.
- Los temas XML aparecen **después** de los built-ins en el ciclo de la tecla `t`.
- Podés tener tantos archivos XML de temas como quieras en la carpeta `themes/`.

---

## Cómo funciona (Arquitectura)

### Backend

El backend maneja todo el procesamiento de audio y es completamente independiente de la TUI.

**`Ring`** (`backend/ring.hpp`)
Un buffer circular lock-free single-producer single-consumer que guarda muestras float estéreo entrelazadas. El decoder escribe en él; el hilo de salida de audio lee de él. Los punteros head/tail con `std::atomic` hacen que el camino caliente sea libre de allocations. Las `condition_variable` permiten que cada lado duerma eficientemente cuando el buffer está lleno o vacío.

**`Decoder`** (`backend/decoder.hpp`)
Corre en su propio hilo. Abre archivos de audio usando libsndfile, los lee en chunks de 4096 frames, hace resampleo bilineal a 44100 Hz estéreo si es necesario, y escribe el resultado en el Ring. Se bloquea cuando el ring está lleno y se despierta cuando hay espacio disponible.

**`AudioOut`** (`backend/audio_out.hpp`)
Lee del Ring y manda datos PCM al hardware. Hay tres implementaciones dentro de guardas `#ifdef` — ALSA, CoreAudio y WinMM — pero todas exponen la misma interfaz: `open()`, `attach(Ring&)`, `swap_ring(Ring&)`, `stop()`. El método `swap_ring` permite hacer seek sin glitches: se arranca un nuevo Decoder en la nueva posición y el hilo de audio cambia al nuevo ring de forma transparente sin detenerse.

**`Player`** (`backend/player.hpp`)
El coordinador de alto nivel. Tiene un `AudioOut` y un `Decoder`. Maneja el vector de playlist, la fila actual, el estado de shuffle/loop, el volumen, el tracking del tiempo transcurrido, y dispara la siguiente canción cuando la actual termina.

### Frontend

El frontend maneja todo el renderizado en terminal y es completamente independiente del audio.

**`ThemeManager`** (`frontend/theme.hpp`)
Contiene todos los temas — los built-ins generados en tiempo de compilación y cualquier archivo XML cargado al arrancar. Provee `next()`, `set(idx)`, `active()`. Los colores se guardan como `std::string` con secuencias de escape ANSI ya armadas, así que cambiar de tema es solo actualizar punteros `const char*` en el namespace `A::` — sin construcción de strings en tiempo de render.

**`ansi.hpp`** (`frontend/ansi.hpp`)
El namespace `A::` contiene todos los punteros de color activos más constantes de escape ANSI. También provee: `emit()` (write sin buffer a stdout), `RawTerm` (modo terminal raw con RAII), `read_key()` (lee teclas individuales incluyendo secuencias de escape), y utilidades de layout de strings (`pad_r`, `center_in`, `trunc_str`, `fmt_t`, `cpw`).

**`draw.hpp`** (`frontend/draw.hpp`)
Dos funciones de render y un loop interactivo. `draw_player()` y `draw_browser()` cada una construye todo el contenido de una pantalla de texto con color ANSI en un solo `std::string` y llama a `emit()` una sola vez — esto evita el flickering que causarían las actualizaciones parciales de pantalla. `browse()` es el loop interactivo del navegador de archivos.

---

## Formatos soportados

El soporte depende de con qué codecs fue compilado libsndfile en tu sistema. Las instalaciones estándar cubren:

| Formato | Extensiones | Notas |
|---|---|---|
| MP3 | `.mp3` | Requiere libsndfile ≥ 1.1.0 |
| FLAC | `.flac` | Soporte completo |
| WAV | `.wav` | Soporte completo, todos los bit depths |
| OGG Vorbis | `.ogg` | Soporte completo |
| Opus | `.opus` | Requiere libopus |
| AIFF | `.aiff` `.aif` | Soporte completo |
| AU / SND | `.au` | Soporte completo |

---

## Solución de problemas

**`cannot open audio device`**
En Linux, verificá que ALSA esté funcionando: `aplay -l` debería listar tus tarjetas de sonido. Asegurate de que ninguna otra aplicación tenga acceso exclusivo al dispositivo.

**Los archivos MP3 no se reproducen**
Tu versión de libsndfile puede ser anterior a 1.1.0. Verificá con `pkg-config --modversion sndfile`. En distros antiguas, instalá `libmpg123-dev` y recompilá libsndfile desde el código fuente con soporte para MP3.

**Los colores se ven mal / no hay colores**
Asegurate de que tu terminal soporte truecolor (24-bit color). Probá con:
```bash
printf '\033[38;2;255;100;0mEsto debería verse naranja\033[0m\n'
```
Si se ve naranja, truecolor funciona. Terminales compatibles: Alacritty, Kitty, WezTerm, iTerm2, GNOME Terminal ≥ 3.36, Windows Terminal.

**El tema XML no carga**
- La carpeta `themes/` tiene que estar en el **mismo directorio que el binario `cmuspp`**, no en el directorio de código fuente.
- Los 10 campos de color tienen que estar presentes en el XML.
- El archivo tiene que tener extensión `.xml`.
- Ejecutá desde el directorio del binario: `cd /ruta/a/cmuspp && ./cmuspp`

---

## Licencia

MIT — hacé lo que quieras con el código.
