@echo off
:: ================================================================
::  localTransfer.io  –  build.bat
::  Compiles all source files into a single localTransfer.io.exe
::  Requires: MinGW-w64 / MSYS2  (g++ on PATH)
::
::  Usage:
::    build.bat           — Release build (optimised)
::    build.bat debug     — Debug build (no optimisation, symbols)
::    build.bat clean     — Remove build artefacts
:: ================================================================

setlocal EnableDelayedExpansion

set EXE=localTransfer.io.exe
set CXX=g++
set STD=-std=c++17
set WARN=-Wall -Wextra -Wno-unused-parameter
set LINK=-lws2_32 -lshell32 -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
set SUBSYS=-mconsole

:: ── Source files ──
set SRCS=globals.cpp utils.cpp database.cpp http_server.cpp main.cpp

:: ── Clean target ──
if /i "%1"=="clean" (
    echo [CLEAN] Removing build artefacts...
    if exist *.o del /q *.o
    if exist "%EXE%" del /q "%EXE%"
    echo [CLEAN] Done.
    exit /b 0
)

:: ── Choose optimisation level ──
if /i "%1"=="debug" (
    set OPT=-O0 -g
    echo [BUILD] Mode: DEBUG
) else (
    set OPT=-O2
    echo [BUILD] Mode: RELEASE
)

:: ── Verify g++ is available ──
where %CXX% >nul 2>&1
if errorlevel 1 (
    echo [ERR] g++ not found on PATH.
    echo       Install MinGW-w64 via MSYS2:  pacman -S mingw-w64-ucrt-x86_64-gcc
    exit /b 1
)

echo [BUILD] Compiling %SRCS% -> %EXE%
echo.

%CXX% %STD% %OPT% %WARN% %SUBSYS% ^
    %SRCS% ^
    -o "%EXE%" ^
    %LINK%

if errorlevel 1 (
    echo.
    echo [ERR] Build failed.
    exit /b 1
)

echo.
echo [OK] Build successful: %EXE%
echo      Run with:  %EXE% [--port PORT] [--saving_dir PATH]
exit /b 0