# Increment

Learn C and C++ by writing code. Increment is a desktop app where every level is a sequence of small coding steps: change the program until its output matches the target. It is built in C with [raylib](https://www.raylib.com/).

## Features

- **Two journeys.** A C track (a first program through pointers and memory management) and a C++ track (references, `new` and `delete`, classes, linked lists): 30 levels, 236 steps. Each track is a winding path of levels; **Find my place** jumps to the next one. Any level can be opened at any time.
- **Levels are coding steps.** Early steps tweak a working program, later ones fill a gap, fix a bug, and finally write the program from an empty `main`. The editor sits beside the target output and your program's output, compared line by line. Programs are compiled with gcc/g++; compiler errors are marked on their lines.
- **Hints are code.** A hint highlights the line to work on, then shows its shape, then the line itself. Each one costs some of the step's XP.
- **Honest solutions only.** Printing the answer doesn't pass: the code must use what the step teaches, and a second run with different values must match too.
- **A picture of memory.** Code with a pointer in it gets a diagram under the editor: a box per variable and an arrow per pointer, redrawn from real addresses after each run. Freed blocks, null and wild pointers show up in red.
- **Skills and XP.** The skills in the code you write are detected (28 of them, from loops and strings to pointers, classes and debugging) and earn XP. Statistics shows each skill's level and which ones need work.
- **Speed rounds.** Most levels offer an optional round of ten generated "what is x?" snippets against the clock; ten in a row earns gold on that level. Answers are scheduled like flashcards (SM-2), and a **Challenge** practises the skills that most need it.
- **A free code editor.** Write and run your own C or C++, with input, name suggestions and saved programs.
- **Rankings.** Compares the accounts on this computer by progress, or by the concepts in their saved programs.
- **Accounts.** Each person gets their own progress, code and settings (light or dark theme, accent colour, text size, the journey's animated background). Progress can be exported to a backup file and restored.

## Building (Windows)

Requires the raylib 6.0 Windows installer at `C:\raylib`. Set `RAYLIB_DIR` if it lives elsewhere.

```
build.bat            # debug build
build.bat release    # optimised build, no console
build\increment.exe
```

Run `test.bat` to build and run the tests. Keep the `assets` folder next to the executable, or one level above it, when distributing the app.

## Source layout

The code is in `src/`, one folder per concern:

| Folder | What it holds |
|---|---|
| `app` | Start-up, navigation between screens, the window, the splash and the transitions |
| `core` | The Windows layer and small helpers with no dependencies |
| `ui` | Themes, widgets, page layout, icons, the logo and the fox mascot |
| `code` | Everything about source code: the editor, name suggestions, running programs, drafts |
| `account` | Local accounts, email verification, backup, and the sign-in and settings screens |
| `learning` | The levels and the learner's record: progress, skills, XP, answer history |
| `home` | The home screen: the journeys, the statistics and their backgrounds |
| `level` | Playing a level: the checks, hints, effects and the picture of memory |
| `speed` | Speed rounds and challenges |
| `sandbox` | The free code editor and its saved programs |
| `rankings` | The rankings screen |

[CLAUDE.md](CLAUDE.md) describes each module and the level file format.

## Accounts and email

Accounts are stored locally, with passwords kept only as salted PBKDF2-SHA256 hashes. They keep everyone's progress separate, but they aren't strong security: anyone who can open the program's folder can read or edit the progress files.

New accounts verify an email address with a 6-digit code, and a forgotten password can be reset by email. Each account also gets a one-time recovery code as a backup way back in.

Email is sent through [Resend](https://resend.com). To enable it, copy `email_config.example.txt` to `build\email_config.txt` and add your own **sending-only** API key:

```
api_key re_your_sending_only_key
from Increment <onboarding@resend.dev>
```

Never commit this file. Without it, the app works the same, just without the email step. For local development, `run-dev.bat` writes the emails to files in `build\email-test-outbox` instead of sending them, using separate test accounts.

## Controls

- Mouse to click, wheel or arrow keys / Page Up / Page Down to scroll
- `Esc` or `Backspace` goes back
- `F5` or `Ctrl+Enter` runs the program in a level or the code editor; the usual editing shortcuts work (undo, redo, copy, paste, `Tab` to indent)
- `Ctrl+S` in the code editor opens its saved programs
- `F11` switches full screen on and off. The app opens as a window filling the screen; choose maximised, full screen or a normal window under **Settings > Window**, and it's remembered next time

## Adding levels

Levels are plain text files in `assets/lessons/`. See the level format in [CLAUDE.md](CLAUDE.md), and run `python -I tools/check_lessons.py` after any change.
