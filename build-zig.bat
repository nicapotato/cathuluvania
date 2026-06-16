@echo off
setlocal enableextensions

set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"

set "RAYLIB_DIR=external\raylib-master"
set "RAYLIB_SRC=%RAYLIB_DIR%\src"
set "BIN_DIR=bin"
set "OBJ_DIR=obj"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo [1/5] Vendoring raylib 6.0...
if exist "scripts\vendor_raylib.sh" (
    bash scripts/vendor_raylib.sh
    if errorlevel 1 (
        echo [ERROR] vendor_raylib.sh failed.
        pause
        exit /b 1
    )
) else if not exist "%RAYLIB_DIR%\src\raylib.h" (
    echo [ERROR] Missing scripts\vendor_raylib.sh and %RAYLIB_DIR%
    pause
    exit /b 1
)

echo [2/5] Compiling Cathuluvania with Zig cc...
del /q "%BIN_DIR%\Cathuluvania.exe" 2>nul

zig cc src\main.c src\game.c src\level.c src\tile_config.c src\platform_path.c ^
    "%RAYLIB_SRC%/rcore.c" "%RAYLIB_SRC%/rmodels.c" "%RAYLIB_SRC%/rshapes.c" ^
    "%RAYLIB_SRC%/rtext.c" "%RAYLIB_SRC%/rtextures.c" "%RAYLIB_SRC%/utils.c" ^
    "%RAYLIB_SRC%/raudio.c" "%RAYLIB_SRC%/rglfw.c" ^
    -o "%BIN_DIR%\Cathuluvania.exe" -O2 ^
    -I src -I include -I "%RAYLIB_SRC%" -I "%RAYLIB_SRC%/external/glfw/include" ^
    -DPLATFORM_DESKTOP -lgdi32 -lwinmm -luser32 -lshell32 -lkernel32
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [3/5] Copying resources...
if not exist "%BIN_DIR%\resources" xcopy /E /I /Y resources "%BIN_DIR%\resources"

echo [4/5] Build complete: %BIN_DIR%\Cathuluvania.exe
pause
