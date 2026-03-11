@echo off
setlocal

echo [BUILD] StarRaster (native)...

if not exist "build" mkdir "build"

gcc -O2 -std=c99 ^
    -I src ^
    -I third_party ^
    src/sr_main.c ^
    src/sr_raster.c ^
    src/sr_texture.c ^
    src/sr_gif.c ^
    -o build/starraster.exe ^
    -lopengl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lwinmm -lm

if %errorlevel% neq 0 (
    echo [FAIL] Build failed.
    pause
    exit /b 1
)

echo [OK] Build succeeded.
echo [RUN] Starting StarRaster...
echo.

cd /d "%~dp0"
build\starraster.exe
