@echo off
setlocal

echo ============================================
echo  StarRaster - Install Dependencies
echo ============================================
echo.

:: Check for gcc
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] gcc not found. Please install MinGW-w64 or MSYS2.
    echo     https://www.msys2.org/
    echo     After install: pacman -S mingw-w64-ucrt-x86_64-gcc
    pause
    exit /b 1
)
echo [OK] gcc found

:: Create directories
if not exist "third_party\sokol" mkdir "third_party\sokol"
if not exist "third_party\stb"   mkdir "third_party\stb"
if not exist "build"             mkdir "build"

:: Download sokol headers
echo.
echo Downloading sokol headers...
curl -sL -o "third_party\sokol\sokol_app.h"  "https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h"
curl -sL -o "third_party\sokol\sokol_gfx.h"  "https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h"
curl -sL -o "third_party\sokol\sokol_glue.h"  "https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h"
curl -sL -o "third_party\sokol\sokol_log.h"   "https://raw.githubusercontent.com/floooh/sokol/master/sokol_log.h"

:: Verify downloads
for %%f in (sokol_app.h sokol_gfx.h sokol_glue.h sokol_log.h) do (
    if not exist "third_party\sokol\%%f" (
        echo [FAIL] Failed to download %%f
        exit /b 1
    )
)
echo [OK] sokol headers downloaded

:: Download stb headers
echo.
echo Downloading stb headers...
curl -sL -o "third_party\stb\stb_image.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
if not exist "third_party\stb\stb_image.h" (
    echo [FAIL] Failed to download stb_image.h
    exit /b 1
)
curl -sL -o "third_party\stb\stb_image_write.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
if not exist "third_party\stb\stb_image_write.h" (
    echo [FAIL] Failed to download stb_image_write.h
    exit /b 1
)
echo [OK] stb headers downloaded

echo.
echo ============================================
echo  All dependencies installed successfully!
echo  Run build-run.bat to build and run.
echo ============================================
pause
