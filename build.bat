@echo off
rem Builds build\increment.exe with the raylib 6.0 install at C:\raylib.
rem Usage: build.bat [release]   (release = optimised, no console window)
setlocal
cd /d "%~dp0"

if "%RAYLIB_DIR%"=="" set RAYLIB_DIR=C:\raylib
set CC=%RAYLIB_DIR%\w64devkit\bin\gcc.exe
set RL_SRC=%RAYLIB_DIR%\raylib\src

if not exist "%CC%" (
    echo Could not find gcc at %CC%. Set RAYLIB_DIR to your raylib install.
    exit /b 1
)

set CFLAGS=-std=c99 -Wall -Wextra -Wno-missing-field-initializers -I"%RL_SRC%"
set LIBS=-L"%RL_SRC%" -lraylib -lopengl32 -lgdi32 -lwinmm

if /i "%1"=="release" (
    set CFLAGS=%CFLAGS% -O2 -DNDEBUG
    set LIBS=%LIBS% -mwindows
) else (
    set CFLAGS=%CFLAGS% -g -O0
)

if not exist build mkdir build
"%CC%" %CFLAGS% src\*.c -o build\increment.exe %LIBS%
if errorlevel 1 exit /b 1
echo Built build\increment.exe
