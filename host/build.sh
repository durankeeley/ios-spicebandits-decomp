#!/usr/bin/env bash
# Build ra_host (Linux).  Needs: gcc, SDL2 dev, GL dev, python3 (to fetch the Unicorn static lib from the pip wheel).
set -euo pipefail
cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)
[ -d "$ROOT/.venv" ] || python3 -m venv "$ROOT/.venv"
"$ROOT/.venv/bin/python" -c "import unicorn" 2>/dev/null || "$ROOT/.venv/bin/pip" -q install unicorn
UNI=$("$ROOT/.venv/bin/python" -c "import unicorn,os;print(os.path.dirname(unicorn.__file__))")
mkdir -p build
gcc -O2 -g -Wall -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation -std=gnu11 \
    -I"$UNI/include" $(pkg-config --cflags sdl2) \
    cpu.c extra.c cxxabi.c zlib_hle.c macho.c symbols.c libc.c objc.c sdl.c gles.c pvrtc.c audio.c main.c \
    -o build/ra_host "$UNI/lib/libunicorn.a" $(pkg-config --libs sdl2) -lGL -lm -lpthread -lSDL2_ttf -lSDL2_image -lz
echo "built host/build/ra_host"
