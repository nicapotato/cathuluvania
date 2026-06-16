@echo off
setlocal enableextensions

set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"

set "RAYLIB_DIR=raylib"
set "RAYLIB_SRC=%RAYLIB_DIR%\src"
set "BIN_DIR=bin"
set "OBJ_DIR=obj"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo [1/4] Checking Raylib...
if not exist "%RAYLIB_DIR%" (
    echo      Cloning raylib...
    git clone --depth 1 --branch 6.0 https://github.com/raysan5/raylib.git "%RAYLIB_DIR%"
    if errorlevel 1 (
        echo [ERROR] Failed to clone raylib.
        pause
        exit /b 1
    )
)

echo [2/4] Compiling Platformer2D with Zig cc...
del /q "%BIN_DIR%\Platformer2D.exe" 2>nul

zig cc src\main.c src\game.c src\level.c src\tile_config.c src\platform_path.c ^
    "%RAYLIB_SRC%/rcore.c" "%RAYLIB_SRC%/rmodels.c" "%RAYLIB_SRC%/rshapes.c" ^
    "%RAYLIB_SRC%/rtext.c" "%RAYLIB_SRC%/rtextures.c" "%RAYLIB_SRC%/utils.c" ^
    "%RAYLIB_SRC%/raudio.c" "%RAYLIB_SRC%/rglfw.c" ^
    -o "%BIN_DIR%\Platformer2D.exe" -O2 ^
    -I src -I include -I "%RAYLIB_SRC%" -I "%RAYLIB_SRC%/external/glfw/include" ^
    -DPLATFORM_DESKTOP -lgdi32 -lwinmm -luser32 -lshell32 -lkernel32
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [3/4] Copying resources...
if not exist "%BIN_DIR%\resources" xcopy /E /I /Y resources "%BIN_DIR%\resources"

echo [4/4] Build complete: %BIN_DIR%\Platformer2D.exe
pause
