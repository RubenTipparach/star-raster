@echo off
setlocal

echo [BUILD] StarRaster (WASM)...

:: Check for emcc
where emcc >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] emcc not found. Please install Emscripten SDK:
    echo     https://emscripten.org/docs/getting_started/downloads.html
    echo     Then run: emsdk activate latest
    pause
    exit /b 1
)

if not exist "build\web" mkdir "build\web"

emcc -O2 -std=c99 -g ^
    -msimd128 ^
    -s USE_WEBGL2=1 ^
    -s WASM=1 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s TOTAL_MEMORY=67108864 ^
    -s STACK_SIZE=1048576 ^
    -s ASSERTIONS=2 ^
    -I src ^
    -I third_party ^
    --preload-file assets ^
    --preload-file config ^
    src/sr_main.c ^
    src/sr_raster.c ^
    src/sr_texture.c ^
    src/sr_gif.c ^
    -o build/web/starraster.html ^
    --shell-file web/shell.html

if %errorlevel% neq 0 (
    echo [FAIL] WASM build failed.
    pause
    exit /b 1
)

echo [OK] WASM build succeeded.
echo [INFO] Output in build/web/
echo [INFO] Serve with: python -m http.server 8000 -d build/web
echo.
pause
