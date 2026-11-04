@echo off
rem Offline email verification for development. Environment changes stay in this script.
setlocal
cd /d "%~dp0"

if not exist "build\increment.exe" (
    call build.bat
    if errorlevel 1 exit /b 1
)

rem Keep development accounts separate from normal application accounts.
set "INCREMENT_DATA_DIR=%~dp0build\email-test-data"
set "INCREMENT_EMAIL_OUTBOX=%~dp0build\email-test-outbox"
if not exist "%INCREMENT_DATA_DIR%" mkdir "%INCREMENT_DATA_DIR%"
if not exist "%INCREMENT_DATA_DIR%" exit /b 1
if not exist "%INCREMENT_EMAIL_OUTBOX%" mkdir "%INCREMENT_EMAIL_OUTBOX%"
if not exist "%INCREMENT_EMAIL_OUTBOX%" exit /b 1

echo Development mode: verification emails are saved locally, not sent.
echo Test accounts: %INCREMENT_DATA_DIR%
echo Read verification codes in: %INCREMENT_EMAIL_OUTBOX%
echo Open the text file named after your test username and enter its code in the app.
"%~dp0build\increment.exe" %*
exit /b %errorlevel%
