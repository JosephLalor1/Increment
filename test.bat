@echo off
rem Builds and runs the tests in tests\. Takes about 15 seconds (the runner tests compile
rem and run real programs, and one waits for the 5-second time limit).
setlocal
cd /d "%~dp0"

if "%RAYLIB_DIR%"=="" set RAYLIB_DIR=C:\raylib
set CC=%RAYLIB_DIR%\w64devkit\bin\gcc.exe
set RL_SRC=%RAYLIB_DIR%\raylib\src
set CFLAGS=-std=c99 -Wall -Wextra -Wno-missing-field-initializers -g -I"%RL_SRC%"
set LIBS=-L"%RL_SRC%" -lraylib -lopengl32 -lgdi32 -lwinmm -lbcrypt
set FAILED=0

if not exist build mkdir build

"%CC%" %CFLAGS% tests\test_account.c src\account.c src\platform.c -o build\test_account.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_editor.c src\editor.c src\ui.c src\icons.c -o build\test_editor.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_runner.c src\runner.c src\platform.c -o build\test_runner.exe %LIBS%
if errorlevel 1 exit /b 1

set TESTDIR=%TEMP%\increment_test_%RANDOM%%RANDOM%
mkdir "%TESTDIR%"
build\test_account.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
rmdir /s /q "%TESTDIR%"

build\test_editor.exe
if errorlevel 1 set FAILED=1

build\test_runner.exe
if errorlevel 1 set FAILED=1

exit /b %FAILED%
