@echo off
rem Builds and runs the tests in tests\ against a fresh temp directory.
setlocal
cd /d "%~dp0"

if "%RAYLIB_DIR%"=="" set RAYLIB_DIR=C:\raylib
set CC=%RAYLIB_DIR%\w64devkit\bin\gcc.exe
set RL_SRC=%RAYLIB_DIR%\raylib\src

if not exist build mkdir build
"%CC%" -std=c99 -Wall -Wextra -g -I"%RL_SRC%" tests\test_account.c src\account.c src\platform.c ^
    -o build\test_account.exe -L"%RL_SRC%" -lraylib -lopengl32 -lgdi32 -lwinmm -lbcrypt
if errorlevel 1 exit /b 1

set TESTDIR=%TEMP%\increment_test_%RANDOM%%RANDOM%
mkdir "%TESTDIR%"
build\test_account.exe "%TESTDIR%"
set RESULT=%errorlevel%
rmdir /s /q "%TESTDIR%"
exit /b %RESULT%
