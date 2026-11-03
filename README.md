# Increment

Learn C and C++, one step at a time. A desktop app with short lessons, syntax-highlighted examples, and quizzes, built in C with [raylib](https://www.raylib.com/).

## Building (Windows)

Requires the raylib 6.0 Windows installer at `C:\raylib`. Set `RAYLIB_DIR` if it lives elsewhere.

```
build.bat            # debug build
build.bat release    # optimised build, no console
build\increment.exe
```

Run `test.bat` to build and run the tests.

## Accounts

Each person on the computer can create a local account with their own saved progress and settings. Accounts live in `accounts.txt` next to the program, and passwords are stored only as salted PBKDF2-SHA256 hashes. Accounts keep everyone's progress separate, but they aren't strong security: anyone who can open the program's folder can read or edit the progress files directly.

- **Recovery codes.** When you create an account you're shown a one-time recovery code. Write it down. If you forget your password, choose "Forgot your password?" on the sign-in screen and enter it. Each code works once, and using it gives you a new one. You can also make a new code in Settings (the old one stops working).
- **Settings** (button on the main menu): light or dark theme, accent colour, lesson text size, change password, new recovery code, and delete account.

## Controls

- Mouse to click, wheel or arrow keys / Page Up / Page Down to scroll
- `1`-`6` pick a quiz answer, `Enter` continues
- `Esc` or `Backspace` goes back
- In forms, `Tab` / `Shift+Tab` moves between fields, `Enter` goes to the next field or submits, `Ctrl+V` pastes, and `Esc` leaves the field

## Adding lessons

Drop a `.txt` file in `assets/lessons/`. See the lesson format in [CLAUDE.md](CLAUDE.md).
