#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] StarRaster (WASM)..."

# Check for emcc
if ! command -v emcc &>/dev/null; then
    echo "[!] emcc not found. Please install Emscripten SDK:"
    echo "    https://emscripten.org/docs/getting_started/downloads.html"
    echo "    Then run: emsdk activate latest"
    exit 1
fi

# Download dependencies if missing
if [ ! -d "third_party/sokol" ] || [ ! -d "third_party/stb" ]; then
    echo "[INFO] Downloading dependencies..."
    mkdir -p third_party/sokol third_party/stb
    curl -sL -o third_party/sokol/sokol_app.h  https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h
    curl -sL -o third_party/sokol/sokol_gfx.h  https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h
    curl -sL -o third_party/sokol/sokol_glue.h https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h
    curl -sL -o third_party/sokol/sokol_log.h  https://raw.githubusercontent.com/floooh/sokol/master/sokol_log.h
    curl -sL -o third_party/stb/stb_image.h    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    curl -sL -o third_party/stb/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
    echo "[OK] Dependencies downloaded."
fi

mkdir -p build/web

emcc -O2 -std=c99 \
    -msimd128 \
    -s USE_WEBGL2=1 \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=67108864 \
    -s STACK_SIZE=1048576 \
    -I src \
    -I third_party \
    --preload-file assets \
    --preload-file config \
    src/sr_main.c \
    src/sr_raster.c \
    src/sr_texture.c \
    src/sr_gif.c \
    -o build/web/index.html \
    --shell-file web/shell.html

if [ $? -ne 0 ]; then
    echo "[FAIL] WASM build failed."
    exit 1
fi

echo "[OK] WASM build succeeded."
echo "[INFO] Output in build/web/"
echo "[INFO] Serve locally with: python3 -m http.server 8000 -d build/web"
