# Increment

Learn C and C++, one step at a time. Increment is a desktop app with short lessons, syntax-highlighted examples, quizzes and hands-on coding exercises, built in C with [raylib](https://www.raylib.com/).

## Features

- **Two learning tracks.** A C track (from your first program through pointers and memory management) and a C++ track (references, `new` and `delete`, classes, linked lists).
- **Learning journeys.** Each track is a winding path of lessons. Completed lessons are ticked off, the next one is highlighted, and **Find my place** jumps straight to it. Any lesson can be opened at any time.
- **Quizzes.** Each lesson ends with a quiz; 70% passes. Afterwards, **Review Answers** shows each question with your answer, the correct one and an explanation. Your best score is kept.
- **Pointer diagrams.** The pointer, pass-by-pointer, pointer arithmetic, malloc/free, new/delete and linked list lessons step through their examples visually: click a line of code to run up to it and watch arrows glide to what each pointer points at, or click any box to have it explained. Dangling pointers, freed memory and leaks show up in red.
- **A built-in code editor.** Run any example from a lesson with **Try it**, solve exercises that check your program's output, or write your own code. Programs are compiled with gcc/g++, compiler errors are marked on their lines, and crashes are explained in plain English.
- **Search.** Search every lesson's title and text, filtered by C, C++ or both.
- **Notes.** Every lesson has a notes panel for explanations and reminders in your own words. Notes save as you type and are private to your account.
- **Saved lessons.** Bookmark lessons you found useful or difficult and find them again under **Saved** on the home screen.
- **Statistics.** Completion, exercises, quiz scores, milestones and suggestions for what to revise.
- **Increment Fox.** The mascot guides you along the journey, reacts to your quiz answers and celebrates finished lessons and tracks.
- **Accounts.** Each person on the computer gets their own progress, notes, bookmarks and settings: light or dark theme, accent colour and text size.

## Building (Windows)

Requires the raylib 6.0 Windows installer at `C:\raylib`. Set `RAYLIB_DIR` if it lives elsewhere.

```
build.bat            # debug build
build.bat release    # optimised build, no console
build\increment.exe
```

Run `test.bat` to build and run the tests. Keep the `assets` folder next to the executable, or one level above it, when distributing the app.

## Accounts and email

Accounts are stored locally, with passwords kept only as salted PBKDF2-SHA256 hashes. They keep everyone's progress separate, but they aren't strong security: anyone who can open the program's folder can read or edit the progress files.

New accounts verify an email address with a 6-digit code, and a forgotten password can be reset by email. Each account also gets a one-time recovery code as a backup way back in.

Email is sent through [Resend](https://resend.com). To enable it, copy `email_config.example.txt` to `build\email_config.txt` and add your own **sending-only** API key:

```
api_key re_your_sending_only_key
from Increment <onboarding@resend.dev>
```

Never commit this file. Without it, the app works the same, just without the email step. For local development, `run-dev.bat` writes the emails to files in `build\email-test-outbox` instead of sending them, using separate test accounts.

## Gradual exercise hints

Exercises with optional clues show **Show hint** in the task panel's fixed header. Each click reveals exactly one numbered hint, then the button changes to **Show next hint**. Earlier clues remain in the panel; scroll over the instructions/hints to reread them. The panel brings each new clue into view. After the last hint, the button is disabled and reads **All hints revealed**. Exercises without hints keep their existing layout and have no hint button.

Hints do not edit your program, reveal the separate solution, run code, or change scores/completion. They stay revealed while editing, running, checking output, or resetting code in the current exercise. Opening another exercise (or reopening an exercise after leaving it) starts with no clues revealed. Hint usage is not saved between sessions. When mascot art is available, revealing a clue briefly changes the editor fox to its idea expression; hints also work without the mascot.

Lesson authors can add up to eight `@hint Your clue here` lines inside an `@exercise`, outside code fences. Their file order is their reveal order. Each hint occupies one source line and wraps automatically in the UI. Existing exercise files need no changes. Empty and excess hint lines are ignored with parser warnings; the lesson checker reports them as content problems.

Initial hints cover **Count down to liftoff**, **Find the smallest value**, **swap_chars**, **count_vowels**, **concat_strings**, **InsertAtBeginning**, and **Lab Part 2: DoublyLinkedList**. Each has three clues progressing from a general concept to a more specific approach without supplying a complete implementation.

Manual checks: reveal one clue per click in a C and C++ exercise; reread earlier clues by scrolling, and reach the disabled final button. Edit, run, and reset code to check that revealed clues remain. Switch exercises and confirm the old hint state is cleared. Open an exercise without hints and confirm no hint button appears. Check light/dark themes and 900x640 with large text. Developer exercise screenshots can set `INCREMENT_SHOT_HINTS=1` (or another count) to preview revealed clues without changing progress.

## Controls

- Mouse to click, wheel or arrow keys / Page Up / Page Down to scroll
- `1`-`6` pick a quiz answer, `Enter` continues
- `Esc` or `Backspace` goes back
- `Ctrl+F` searches the lessons
- `F11` switches full screen on and off. The app opens as a window filling the screen; choose maximised, full screen or a normal window under **Settings > Window**, and it's remembered next time
- In the code editor, `F5` or `Ctrl+Enter` runs the program, and the usual editing shortcuts work (undo, redo, copy, paste, `Tab` to indent)

## Adding lessons

Lessons are plain text files in `assets/lessons/`. See the lesson format in [CLAUDE.md](CLAUDE.md).
