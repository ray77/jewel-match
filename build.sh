#!/usr/bin/env bash
# Build Jewel Match for the browser.
#
# The game keeps three blocking event loops that call one another - startGame,
# run and endGame - which is the shape emscripten_set_main_loop cannot take.
# ASYNCIFY unwinds and rewinds the stack at the blocking points instead, so
# SDL_WaitEvent, SDL_Delay and SDL_AddTimer all keep working and the game logic
# stays exactly as it was written. It costs binary size, which for a match-3 at
# a few megabytes is not worth a rewrite.
#
# SDL, SDL_image, SDL_ttf and SDL_mixer all come from Emscripten's own ports,
# so nothing has to be built by hand.
#
# Usage: ./build.sh            (needs emsdk_env.sh sourced, or EMSDK set)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/out"

if ! command -v emcc >/dev/null; then
    # shellcheck disable=SC1091
    source "${EMSDK:-$HOME/Developer/emsdk}/emsdk_env.sh" >/dev/null 2>&1 || {
        echo "emcc not found - source emsdk_env.sh or set EMSDK" >&2
        exit 1
    }
fi

mkdir -p "$OUT"
emcc "$ROOT"/src/*.cpp -I"$ROOT/include" \
    -sUSE_SDL=2 \
    -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png"]' \
    -sUSE_SDL_TTF=2 \
    -sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS='["ogg"]' \
    -sASYNCIFY -sASYNCIFY_STACK_SIZE=32768 \
    -sALLOW_MEMORY_GROWTH=1 \
    --preload-file "$ROOT/assets"@assets \
    -O2 -o "$OUT/jewel.js"

echo "built:"
ls -la "$OUT"/jewel.js "$OUT"/jewel.wasm "$OUT"/jewel.data | awk '{printf "  %9d  %s\n", $5, $9}'
