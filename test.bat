@echo off
rem Builds and runs the tests in tests\. Takes about 15 seconds (the runner tests compile
rem and run real programs, and one waits for the 5-second time limit).
rem The email tests never use the network unless RESEND_LIVE_TEST=1 is set, which sends one
rem email through Resend to its simulated inbox delivered@resend.dev.
rem A test that fails to build stops the script; one that fails when run is reported and
rem the rest still run. The exit code is 1 if any failed.
setlocal
cd /d "%~dp0"

if "%RAYLIB_DIR%"=="" set RAYLIB_DIR=C:\raylib
set CC=%RAYLIB_DIR%\w64devkit\bin\gcc.exe
set RL_SRC=%RAYLIB_DIR%\raylib\src
set CFLAGS=-std=c99 -Wall -Wextra -Wno-missing-field-initializers -g -I"%RL_SRC%" -Isrc
set LIBS=-L"%RL_SRC%" -lraylib -lopengl32 -lgdi32 -lwinmm -lbcrypt -lwinhttp -lcomdlg32
set FAILED=0

if not exist build mkdir build

rem A throwaway folder for the tests that write files.
set TESTDIR=%TEMP%\increment_test_%RANDOM%%RANDOM%
mkdir "%TESTDIR%"


"%CC%" %CFLAGS% tests\test_code_skills.c src\code\code_skills.c -o build\test_code_skills.exe
if errorlevel 1 exit /b 1
build\test_code_skills.exe
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_completion.c src\code\completion.c src\code\code_text.c -o build\test_completion.exe
if errorlevel 1 exit /b 1
build\test_completion.exe
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_editor.c src\code\editor.c src\code\completion.c src\code\code_text.c -o build\test_editor.exe
if errorlevel 1 exit /b 1
build\test_editor.exe
if errorlevel 1 set FAILED=1

rem Compiles and runs real programs; one waits for the 5-second time limit.
"%CC%" %CFLAGS% tests\test_runner.c src\code\runner.c src\core\platform.c -o build\test_runner.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_runner.exe
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_steps.c src\level\line_diff.c src\level\step_hints.c src\level\step_skills.c src\level\step_check.c src\level\step_cheats.c src\code\code_text.c src\code\code_skills.c src\learning\lesson.c src\learning\skills.c src\code\drafts.c -o build\test_steps.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_steps.exe
if errorlevel 1 set FAILED=1

rem Compiles and runs instrumented C and C++ programs to check what they report.
"%CC%" %CFLAGS% tests\test_probe.c src\level\memory_probe.c src\code\code_text.c -o build\test_probe.exe
if errorlevel 1 exit /b 1
build\test_probe.exe "%RAYLIB_DIR%\w64devkit\bin" "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_roadmap.c src\learning\roadmap.c src\learning\lesson.c src\learning\skills.c src\learning\progress.c -o build\test_roadmap.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_roadmap.exe
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_skills.c src\learning\skills.c src\learning\skill_progress.c src\core\platform.c -o build\test_skills.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_skills.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_review.c src\learning\review.c src\core\platform.c -o build\test_review.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_review.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

rem Compiles a few hundred generated speed round questions with gcc and g++ to check their answers.
"%CC%" %CFLAGS% tests\test_speed_round.c src\speed\speed_round.c src\speed\speed_questions.c src\core\random_stream.c -o build\test_speed_round.exe
if errorlevel 1 exit /b 1
build\test_speed_round.exe "%RAYLIB_DIR%\w64devkit\bin"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_speed_plan.c src\speed\speed_plan.c src\speed\speed_round.c src\speed\speed_questions.c src\core\random_stream.c src\learning\skill_progress.c src\learning\review.c src\learning\progress.c src\learning\roadmap.c src\learning\lesson.c src\learning\skills.c src\core\platform.c -o build\test_speed_plan.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_speed_plan.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_dashboard.c src\home\dashboard_stats.c src\learning\roadmap.c src\learning\progress.c -o build\test_dashboard.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_dashboard.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_journey_view.c src\home\journey_view.c src\ui\mascot.c src\ui\mascot_state.c src\learning\roadmap.c src\learning\lesson.c src\learning\skills.c src\learning\progress.c src\ui\ui.c src\ui\icons.c src\ui\icon_pen.c -o build\test_journey_view.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_journey_view.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_mascot.c src\ui\mascot_state.c -o build\test_mascot.exe
if errorlevel 1 exit /b 1
build\test_mascot.exe
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_code_library.c src\sandbox\code_library.c src\code\code_skills.c src\core\platform.c -o build\test_code_library.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_code_library.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_ranking.c src\rankings\ranking.c src\rankings\ranking_skills.c src\sandbox\code_library.c src\code\code_skills.c src\account\account.c src\account\email_address.c src\core\platform.c src\learning\progress.c -o build\test_ranking.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_ranking.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_window_mode.c src\app\window_mode.c -o build\test_window_mode.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_window_mode.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_backup.c src\account\backup.c -o build\test_backup.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_backup.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_account.c src\account\account.c src\account\email_address.c src\core\platform.c -o build\test_account.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_account.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

"%CC%" %CFLAGS% tests\test_email.c src\account\email.c src\account\email_address.c src\core\platform.c -o build\test_email.exe %LIBS%
if errorlevel 1 exit /b 1
build\test_email.exe "%TESTDIR%"
if errorlevel 1 set FAILED=1

rmdir /s /q "%TESTDIR%"
exit /b %FAILED%
