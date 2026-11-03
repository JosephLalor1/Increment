# Increment

A desktop app for learning C and C++, written in C99 with raylib 6.0. This is an AI-only project: Claude writes all code and lesson content; the user directs.

## Build and run

- `build.bat` builds a debug `build\increment.exe`; `build.bat release` builds an optimised one with no console window.
- The toolchain is the raylib Windows bundle at `C:\raylib` (w64devkit gcc 15.2 + `raylib\src\libraylib.a`). Override with `RAYLIB_DIR`. Do not use the MSYS2 gcc on PATH; it has no raylib.
- Build must stay warning-free under `-Wall -Wextra`.
- `test.bat` builds and runs `tests\test_account.c` against a throwaway temp dir. Run it after touching `account.c` or `platform.c`.

## Verifying UI changes

`increment.exe --shot out.png <screen> [lessonIndex] [scroll]` renders one screen to a PNG and exits. Screens: `splash gallery login register reset recovery settings delete menu list lesson quiz answered result` (`register`, `reset`, `recovery` and `delete` pre-fill demo state via `AccountScreensDemo`). Check both themes: the theme comes from the signed-in user's (or last user's) settings file. Write shots to the scratchpad and view them after any visual change. `lessonIndex` indexes all lessons in filename order.

Env vars for testing:
- `INCREMENT_DATA_DIR`: where accounts and progress live (default: the exe's folder). Point it at the scratchpad so you never touch real accounts.
- `INCREMENT_SHOT_USER` / `INCREMENT_SHOT_PASS`: sign in before a shot.
- `INCREMENT_SHOT_HEIGHT`: window height for a shot (min 640), to check small layouts.

Python's `hashlib.pbkdf2_hmac('sha256', pw, salt, iterations)` produces hashes the app accepts, which is handy for making test accounts.

## Layout

- `src/main.c`: screen state machine and navigation, lesson/quiz/result screens, the `--shot` dev aid.
- `src/account_screens.c`: login (sign in / create / reset modes), recovery-code and settings screens. They return an `AccountAction`; main.c decides where to go next.
- `src/layout.c`: page pieces shared by screens (top bar, content column, scroll views).
- `src/ui.c`: themes, immediate-mode widgets (buttons, links, text boxes), word wrap, syntax-highlighted code blocks, fonts. Widgets draw and handle input in the same call. Text boxes only take input if they had focus at the start of the frame, so moving focus can't let one key press trigger two fields. Esc/Backspace in the top bar don't navigate while a text box has focus.
- Colours: always use the `COL_*` macros (they read the live `gTheme`), never literal colours, so both themes work. Text on an accent fill uses `COL_ON_ACCENT`; neutral buttons hover with `COL_SECONDARY_HOVER`.
- `src/brand.c`: the logo (rounded tile with two plus signs, the second larger and higher: "++", one step up), drawn from shapes. In-app it uses `COL_BRAND` (the deep shade of the user's accent, so the white pluses keep their contrast); the icon files use `BRAND_BLUE`.
- `src/icons.c`: the line-icon set (`IconDraw`), drawn from shapes so icons follow the theme. Use `UiButtonIcon`/`UiButtonIconEx` and `UiIconText` rather than drawing icons by hand. Check new or changed icons with `--shot out.png gallery`.
- `src/splash.c`: launch animation (2.4 s, any key or click skips). `--shot out.png splash 0 <ms>` renders one moment of it.
- `tools/make_icons.c` + `build.bat icons`: regenerates `assets/icon/` (PNGs at 16-256 px, `logo_512.png`, `increment.ico`) from `brand.c`. Re-run after changing the logo; the generated files are committed. `src/increment.rc` embeds the `.ico` and version info in the exe via windres. Window and taskbar icons are loaded from the PNGs at startup.
- `src/lesson.c`: parser for lesson files.
- `src/account.c`: local accounts in `<data>/accounts.txt`: `@last name` plus `name iter salt hash [riter rsalt rhash]` lines (the recovery triple is optional; older 4-field lines still load). Passwords and recovery codes use PBKDF2-HMAC-SHA256 with a 16-byte random salt and 210k iterations; iteration counts are stored per secret so they can be raised later. Recovery codes are 16 symbols from a 32-char alphabet with no look-alikes (80 bits), shown once, used once (a reset issues a new one), and matched without regard to case or dashes. Per-user settings (theme, accent, text size) live in `users/<name>.settings.txt`. Usernames are 3-20 of `[A-Za-z0-9_-]`, case-insensitive, and that character set is what makes them safe as filenames.
- `src/platform.c`: the only file that includes `<windows.h>` (random bytes, PBKDF2, atomic file replace). Never include raylib.h there; the headers clash.
- `src/progress.c`: best score per lesson id, saved to `<data>/users/<lowercase name>.txt`. 70% passes. The first account created takes over a pre-accounts `progress.txt`.
- `assets/lessons/*.txt`: lesson content, loaded in filename order (`c_NN_*`, `cpp_NN_*`).

## Lessons

Lessons must follow the professor's notes in `Class Material/` (git-ignored: it's copyrighted course material). Keep his topic order, examples and terms; fix bugs in his code and say so. The C track is `c_NN_*` (Intro to C, Lab1, Pointers.ppt, pointer lab, C memory management); the C++ track is `cpp_NN_*` (references, memory management deck, Classes deck, linked-list lecture and lab). `Pointers.ppt` is legacy format: convert it with PowerPoint COM (`Presentations.Open(..., ReadOnly)` then `SaveAs(..., 24)`) into the scratchpad to read it.

After any lesson change, run `python -I tools/check_lessons.py` and get it to 0 problems. It compiles and runs every code block containing `main()` (gcc for C, g++ for C++), compares stdout with a following `Output:` paragraph plus code block, runs every "What does this print?" quiz snippet against the marked answer, and checks structure, ASCII-only text and code width (66 columns, the most that fits at Large text size). Markers inside code: `// does not compile`, `// undefined behaviour` (compile only, warnings allowed), `// stdin: 3 7 4`. Write expected output with tabs expanded to 8-column stops.

## Lesson format

```
@id c_04_functions        (defaults to the filename; never change it once shipped, progress keys on it)
@track C                  (C or C++)
@title Functions
@kind practice            (optional: exam/lab practice; shows a target icon in the list)

Plain lines form paragraphs; a blank line ends one.
# Heading
> Note callout
- Bullet item             (one block per bullet; an indented next line continues it)
(three backticks on their own line open/close a code block)

@quiz                     (everything after this is quiz)
? Question prompt
(a code block right after a prompt attaches to that question)
- wrong choice
* correct choice          (exactly one per question)
! Optional explanation shown after answering
```

- Keep lesson text ASCII where possible. Fonts include Latin-1 and common typographic punctuation, nothing else.
- Limits: 256 blocks, 16 questions, 6 choices per question, 1023 chars per line.
- Code samples must be correct and compile. Quiz answers must be unambiguous.

## Gotchas

- raylib batches draws; flush with `rlDrawRenderBatchActive()` before reading the framebuffer.
- Fonts come from `assets/fonts/text.ttf` and `mono.ttf` if present, otherwise Segoe UI and Consolas from `C:\Windows\Fonts`.
