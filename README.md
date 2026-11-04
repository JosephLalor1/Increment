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

- **Forgot your password?** On the sign-in screen, enter your username and Increment emails a 6-digit reset code to your verified address. Enter it with a new password and you're signed in. Reset codes expire after 15 minutes.
- **Recovery codes.** When you create an account you're also shown a one-time recovery code. Write it down: it's the backup way back in if you can't get to your email ("Use a recovery code instead"). Each code works once, and using it gives you a new one. You can also make a new code in Settings (the old one stops working).
- **Settings** (button on the main menu): light or dark theme, accent colour, lesson text size, change email, change password, new recovery code, and delete account.
- **Email verification.** New accounts enter an email address and get a 6-digit code from Increment; the account stays locked until the code is entered. Codes expire after 15 minutes and allow 5 tries. Accounts made before this was added are asked for an address at their next sign-in.

### Setting up email

Emails are sent through [Resend](https://resend.com). The app reads its settings from `email_config.txt`, placed next to `increment.exe` (for example `build\email_config.txt`):

```
api_key re_your_sending_only_key
from Increment <onboarding@resend.dev>
```

Use a **sending-only** API key, never a full-access one, and never commit the file (it's in `.gitignore`). While Resend is in test mode (no verified domain), the `onboarding@resend.dev` sender can only deliver to the email address of the Resend account; verify a domain in Resend to email anyone. Without `email_config.txt` the app still works, just without the email step.

## Controls

- Mouse to click, wheel or arrow keys / Page Up / Page Down to scroll
- `1`-`6` pick a quiz answer, `Enter` continues
- `Esc` or `Backspace` goes back
- In forms, `Tab` / `Shift+Tab` moves between fields, `Enter` goes to the next field or submits, `Ctrl+V` pastes, and `Esc` leaves the field

## Code editor

Open it from the main menu, with **Try it** on any example in a lesson, or from a lesson's **Exercises**. Programs are compiled with the gcc/g++ that comes with raylib, and get whatever you type in the Input box as their input. Exercises check your output against the expected output and remember when you've passed one. Your code is saved per account, so you can leave an exercise and come back to it.

- `F5` or `Ctrl+Enter` runs the program; while it runs, the button becomes **Stop**
- Compiler errors are marked on their lines; crashes are explained in plain English
- Programs are stopped after 5 seconds and limited to 512 MB of memory
- `Ctrl+Z` / `Ctrl+Y` undo and redo (Reset and Show solution can be undone too)
- `Tab` / `Shift+Tab` indent and outdent the selected lines
- `Ctrl+A`, `Ctrl+C`, `Ctrl+X`, `Ctrl+V`; double-click selects a word, triple-click a line

## Adding lessons

Drop a `.txt` file in `assets/lessons/`. See the lesson format in [CLAUDE.md](CLAUDE.md).
