@echo off
rem Builds and runs the tests in tests\. Takes about 15 seconds (the runner tests compile
rem and run real programs, and one waits for the 5-second time limit).
rem The email tests never use the network unless RESEND_LIVE_TEST=1 is set, which sends one
rem email through Resend to its simulated inbox delivered@resend.dev.
setlocal
cd /d "%~dp0"

if "%RAYLIB_DIR%"=="" set RAYLIB_DIR=C:\raylib
set CC=%RAYLIB_DIR%\w64devkit\bin\gcc.exe
set RL_SRC=%RAYLIB_DIR%\raylib\src
set CFLAGS=-std=c99 -Wall -Wextra -Wno-missing-field-initializers -g -I"%RL_SRC%"
set LIBS=-L"%RL_SRC%" -lraylib -lopengl32 -lgdi32 -lwinmm -lbcrypt -lwinhttp
set FAILED=0

if not exist build mkdir build

"%CC%" %CFLAGS% tests\test_practice.c src\practice.c src\quiz_attempt.c src\lesson.c src\progress.c -o build\test_practice.exe %LIBS%
if errorlevel 1 exit /b 1

"%CC%" %CFLAGS% tests\test_exercise_hints.c src\exercise_hints.c src\lesson.c src\progress.c -o build\test_exercise_hints.exe %LIBS%
if errorlevel 1 exit /b 1

"%CC%" %CFLAGS% tests\test_account.c src\account.c src\email_address.c src\platform.c -o build\test_account.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_editor.c src\editor.c src\ui.c src\icons.c -o build\test_editor.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_runner.c src\runner.c src\platform.c -o build\test_runner.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_email.c src\email.c src\email_address.c src\platform.c -o build\test_email.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_dashboard.c src\dashboard_stats.c src\progress.c -o build\test_dashboard.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_quiz_attempt.c src\quiz_attempt.c src\lesson.c src\progress.c -o build\test_quiz_attempt.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_quiz_attempt.exe
if errorlevel 1 set FAILED=1
"%CC%" %CFLAGS% tests\test_journey_view.c src\journey_view.c src\mascot.c src\mascot_state.c src\roadmap.c src\lesson.c src\progress.c src\ui.c src\icons.c -o build\test_journey_view.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_mascot.c src\mascot_state.c src\mascot_events.c src\roadmap.c src\lesson.c src\progress.c -o build\test_mascot.exe %LIBS%
if errorlevel 1 exit /b 1

set TESTDIR=%TEMP%\increment_test_%RANDOM%%RANDOM%
"%CC%" %CFLAGS% tests\test_roadmap.c src\roadmap.c src\lesson.c src\progress.c -o build\test_roadmap.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_roadmap.exe
if errorlevel 1 set FAILED=1
"%CC%" %CFLAGS% tests\test_search.c src\search.c src\lesson.c -o build\test_search.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_search.exe
if errorlevel 1 set FAILED=1
"%CC%" %CFLAGS% tests\test_memory_model.c src\memory_model.c src\lesson.c -o build\test_memory_model.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_memory_model.exe
if errorlevel 1 set FAILED=1
"%CC%" %CFLAGS% tests\test_review.c src\review.c src\platform.c -o build\test_review.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_speed_plan.c src\speed_plan.c src\speed_round.c src\review.c src\progress.c src\quiz_attempt.c src\lesson.c src\platform.c -o build\test_speed_plan.exe %LIBS%
if errorlevel 1 exit /b 1
rem Compiles a few hundred generated speed round questions with gcc and g++ to check their answers.
"%CC%" %CFLAGS% tests\test_speed_round.c src\speed_round.c -o build\test_speed_round.exe
if errorlevel 1 exit /b 1
build\test_speed_round.exe "%RAYLIB_DIR%\w64devkit\bin"
if errorlevel 1 set FAILED=1
"%CC%" %CFLAGS% tests\test_bookmarks.c src\bookmarks.c -o build\test_bookmarks.exe
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_notes.c src\notes.c src\platform.c -o build\test_notes.exe %LIBS%
if errorlevel 1 exit /b 1
"%CC%" %CFLAGS% tests\test_window_mode.c src\window_mode.c -o build\test_window_mode.exe %LIBS%
if errorlevel 1 exit /b 1

mkdir "%TESTDIR%"
build\test_practice.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_exercise_hints.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_review.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_speed_plan.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_bookmarks.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_notes.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_window_mode.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_mascot.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_journey_view.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_dashboard.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

build\test_account.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
build\test_email.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1
rmdir /s /q "%TESTDIR%"

build\test_editor.exe
if errorlevel 1 set FAILED=1

build\test_runner.exe
if errorlevel 1 set FAILED=1

exit /b %FAILED%
