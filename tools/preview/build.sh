#!/bin/sh
# Builds the desktop preview with clang++ (or g++). Needs SDL2 headers (LovyanGFX
# picks its desktop platform from them) and an SDL2 library to link against.
#   SDL_INC=/path/containing/SDL2  SDL_LIB=/path/to/libSDL2.dylib  tools/preview/build.sh
set -e
cd "$(dirname "$0")/../.."
LIBS=.pio/libdeps/esp32-8048s043
[ -d "$LIBS/LovyanGFX" ] || { echo "Run 'pio run' once first so the libraries are downloaded."; exit 1; }
OUT=tools/preview/build
mkdir -p "$OUT"
CXX=${CXX:-clang++}
FLAGS="-std=c++17 -O2 -DLGFX_USE_V1 -Iinclude -I$LIBS/LovyanGFX/src -I$LIBS/ArduinoJson/src ${SDL_INC:+-I$SDL_INC}"
objs=""
for src in $(find "$LIBS/LovyanGFX/src/lgfx" -name '*.cpp' -o -name '*.c' | grep -v '/platforms/') \
           src/scope.cpp src/render.cpp src/mini_core.cpp src/data/large_airports_data.cpp tools/preview/preview.cpp; do
  obj="$OUT/$(echo "$src" | tr '/' '_').o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    case "$src" in *.c) ${CC:-clang} -O2 -DLGFX_USE_V1 -I$LIBS/LovyanGFX/src ${SDL_INC:+-I$SDL_INC} -c "$src" -o "$obj" ;;
                   *) $CXX $FLAGS -c "$src" -o "$obj" ;; esac
  fi
  objs="$objs $obj"
done
$CXX $objs ${SDL_LIB:-$(sdl2-config --libs)} -o "$OUT/preview"
echo "Built $OUT/preview"
