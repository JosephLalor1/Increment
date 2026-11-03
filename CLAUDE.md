# Increment

A desktop app for learning C and C++, written in C99 with raylib 6.0. This is an AI-only project: Claude writes all code and lesson content; the user directs.

## Build and run

- `build.bat` builds a debug `build\increment.exe`; `build.bat release` builds an optimised one with no console window.
- The toolchain is the raylib Windows bundle at `C:\raylib` (w64devkit gcc 15.2 + `raylib\src\libraylib.a`). Override with `RAYLIB_DIR`. Do not use the MSYS2 gcc on PATH; it has no raylib.
- Build must stay warning-free under `-Wall -Wextra`.
- `test.bat` builds and runs `tests\test_account.c` against a throwaway temp dir. Run it after touching `account.c` or `platform.c`.

## Verifying UI changes

`increment.exe --shot out.png [login|register|menu|list|lesson|quiz|answered|result] [lessonIndex] [scroll]` renders one screen to a PNG and exits. Write shots to the scratchpad and view them after any visual change. `lessonIndex` indexes all lessons in filename order.

Env vars for testing:
- `INCREMENT_DATA_DIR`: where accounts and progress live (default: the exe's folder). Point it at the scratchpad so you never touch real accounts.
- `INCREMENT_SHOT_USER` / `INCREMENT_SHOT_PASS`: sign in before a shot.
- `INCREMENT_SHOT_HEIGHT`: window height for a shot (min 640), to check small layouts.

Python's `hashlib.pbkdf2_hmac('sha256', pw, salt, iterations)` produces hashes the app accepts, which is handy for making test accounts.

## Layout

- `src/main.c`: screen state machine (login, menu, list, lesson, quiz, result) and the `--shot` dev aid.
- `src/ui.c`: immediate-mode widgets (buttons, text boxes), word wrap, syntax-highlighted code blocks, fonts. Widgets draw and handle input in the same call. Text boxes only take input if they had focus at the start of the frame, so moving focus can't let one key press trigger two fields.
- `src/lesson.c`: parser for lesson files.
- `src/account.c`: local accounts in `<data>/accounts.txt` (`name iterations salt-hex hash-hex` lines plus `@last name`). Passwords use PBKDF2-HMAC-SHA256 with a 16-byte random salt and 210k iterations; the iteration count is stored per account so it can be raised later. Usernames are 3-20 of `[A-Za-z0-9_-]`, case-insensitive, and that character set is what makes them safe as filenames.
- `src/platform.c`: the only file that includes `<windows.h>` (random bytes, PBKDF2, atomic file replace). Never include raylib.h there; the headers clash.
- `src/progress.c`: best score per lesson id, saved to `<data>/users/<lowercase name>.txt`. 70% passes. The first account created takes over a pre-accounts `progress.txt`.
- `assets/lessons/*.txt`: lesson content, loaded in filename order (`c_NN_*`, `cpp_NN_*`).

## Lesson format

```
@id c_04_functions        (defaults to the filename; never change it once shipped, progress keys on it)
@track C                  (C or C++)
@title Functions

Plain lines form paragraphs; a blank line ends one.
# Heading
> Note callout
(three backticks on their own line open/close a code block)

@quiz                     (everything after this is quiz)
? Question prompt
(a code block right after a prompt attaches to that question)
- wrong choice
* correct choice          (exactly one per question)
! Optional explanation shown after answering
```

- Keep lesson text ASCII where possible. Fonts include Latin-1 and common typographic punctuation, nothing else.
- Limits: 128 blocks, 16 questions, 6 choices per question, 1023 chars per line.
- Code samples must be correct and compile. Quiz answers must be unambiguous.

## Gotchas

- raylib batches draws; flush with `rlDrawRenderBatchActive()` before reading the framebuffer.
- Fonts come from `assets/fonts/text.ttf` and `mono.ttf` if present, otherwise Segoe UI and Consolas from `C:\Windows\Fonts`.
