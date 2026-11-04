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

## Learning roadmap

Choose C or C++ from the main menu to see its roadmap. Connected lesson cards show completed lessons, the **Recommended Next** lesson, and upcoming topics. Every card is clickable; preparation suggestions never restrict lessons, quizzes, or exercises. Use **Show lesson list** to return to the compact list.

Completion uses the existing saved best quiz score (70% passes). The recommendation is the first incomplete lesson in filename order within the selected track, even if later lessons are already complete. Totals and the progress bar update from the same scores. Optional preparation relationships are maintained in the lesson-ID table in `src/roadmap.c`; references to missing content are omitted.

Manual roadmap checks:

- Open both tracks, switch between roadmap and list, and open completed and upcoming lessons.
- Pass or fail a quiz, return to the roadmap, and check completion and the first incomplete recommendation. A failed retry should preserve a previous pass.
- Scroll to the final lesson using the wheel and keyboard; check Back navigation and exercise/editor return navigation.
- Check light/dark themes and all text sizes at 900x640 and larger window sizes.
- With a fully completed track, check the congratulations message and that all lessons remain accessible.

## Accounts

Each person on the computer can create a local account with their own saved progress and settings. Accounts live in `accounts.txt` next to the program, and passwords are stored only as salted PBKDF2-SHA256 hashes. Accounts keep everyone's progress separate, but they aren't strong security: anyone who can open the program's folder can read or edit the progress files directly.

- **Recovery codes.** When you create an account you're shown a one-time recovery code. Write it down. If you forget your password, choose "Forgot your password?" on the sign-in screen and enter it. Each code works once, and using it gives you a new one. You can also make a new code in Settings (the old one stops working).
- **Settings** (button on the main menu): light or dark theme, accent colour, lesson text size, change password, new recovery code, and delete account.

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
