# Increment

A desktop app for learning C and C++, written in C99 with raylib 6.0. This is an AI-only project: Claude writes all code and lesson content; the user directs.

## Build and run

- `build.bat` builds a debug `build\increment.exe`; `build.bat release` builds an optimised one with no console window.
- The toolchain is the raylib Windows bundle at `C:\raylib` (w64devkit gcc 15.2 + `raylib\src\libraylib.a`). Override with `RAYLIB_DIR`. Do not use the MSYS2 gcc on PATH; it has no raylib.
- Build must stay warning-free under `-Wall -Wextra`.
- `test.bat` builds and runs the tests (about 15 s): `tests\test_account.c` (throwaway data dir, including email verification), `tests\test_email.c` (offline; `RESEND_LIVE_TEST=1` also sends one real email to Resend's simulated `delivered@resend.dev`), `tests\test_editor.c` (editing operations), `tests\test_search.c` (lesson search), `tests\test_memory_model.c` (pointer diagram scripts, including every ` ```memory ` block in the lessons), `tests\test_bookmarks.c` (saved lessons), `tests\test_notes.c` (lesson notes), `tests\test_window_mode.c` (window setting), `tests\test_roadmap.c` and `tests\test_dashboard.c` (roadmap and dashboard data) and `tests\test_runner.c` (compiles and runs real programs, including a crash, an endless loop and a 2 GB allocation). Run it after touching `account.c`, `email.c`, `email_address.c`, `platform.c`, `editor.c`, `runner.c`, `search.c`, `memory_model.c`, `bookmarks.c`, `notes.c`, `window_mode.c`, `roadmap.c`, `progress.c` or `dashboard_stats.c`.
- Never commit: the user commits to their GitHub repo themselves and hand-tests before each commit.

## Verifying UI changes

`increment.exe --shot out.png <screen> [lessonIndex] [scroll]` renders one screen to a PNG and exits. Screens: `splash gallery login register reset resetemail resetcode recovery verify addemail settings delete home statistics saved search lesson notes quiz answered result review celebration graduation editor exercise tryit` (`home <lessonIndex>` shows that lesson's track's journey) (`register`, `reset`, `recovery` and `delete` pre-fill demo state via the screens' `...Demo` functions; `exercise <lessonIndex> <exerciseNumber>` opens an exercise; `tryit <lessonIndex>` opens its first runnable example; set `INCREMENT_SHOT_RUN=1` to compile and run first, and the shot waits for it). Code editor shots need a signed-in user; put draft files in `<data>/users/<name>_code/` to show particular code. Check both themes: the theme comes from the signed-in user's (or last user's) settings file. Write shots to the scratchpad and view them after any visual change. `lessonIndex` indexes all lessons in filename order.

Env vars for testing:
- `INCREMENT_DATA_DIR`: where accounts and progress live (default: the exe's folder). Point it at the scratchpad so you never touch real accounts.
- `INCREMENT_SHOT_USER` / `INCREMENT_SHOT_PASS`: sign in before a shot.
- `INCREMENT_SHOT_HEIGHT`: window height for a shot (min 640), to check small layouts.
- `INCREMENT_SHOT_SEARCH` / `INCREMENT_SHOT_FILTER` (`c` or `cpp`): with the `search` screen, type a query into the home search box (`" "` shows the empty-search hint).
- `INCREMENT_SHOT_DIAGRAM="step[,box]"`: pointer diagrams in a lesson shot start at that step (1-based) with that box (a cell index) selected, already settled. Use a `lesson <index> <scroll>` shot; `INCREMENT_SHOT_HEIGHT` up to the screen height shows more at once.
- `INCREMENT_EMAIL_OUTBOX`: write verification emails to this folder instead of sending them. Always set it for shots and manual experiments, so nothing reaches Resend. The `verify` and `addemail` shot screens show the verification screen's two modes; `resetemail` and `resetcode` show the two steps of an email password reset (the account needs a verified address in its `users/<name>.email.txt`).

Python's `hashlib.pbkdf2_hmac('sha256', pw, salt, iterations)` produces hashes the app accepts, which is handy for making test accounts.

## Layout

## Code structure rules

These apply to every change (the user asked for them as a standing rule):

- **One job per file, low coupling.** Screens draw and handle input, then return what the user asked for (an `AccountAction`, `HomeAction` or `LessonAction`). Only `main.c` switches screens. Data and logic modules (`account`, `progress`, `bookmarks`, `notes`, `memory_model`, `roadmap`, `search`, `dashboard_stats`, `lesson`, `runner`, editing operations in `editor`) never draw, so they can be unit-tested.
- **Shared things live in one place.** Lesson-sized text uses `UiTextScale()`; the pass mark and exercise progress keys are only built in `progress.c` (`ProgressLessonPassed`, `ProgressExerciseDone`, `ProgressRecordExercise`); secrets are wiped with `WIPE` from `wipe.h`. Don't copy a helper into a second file: move it to `ui`/`layout` or the module that owns the concern.
- **Readable code.** Names say what a thing means (`confirmNewPassword`, not `confirm2`); every file starts with a comment saying what it is for, every public function is described in its header, and comments explain why rather than restating the code.
- **Nothing breaks.** After a refactor, the `--shot` screenshots must match the previous build pixel-for-pixel (build both with the same flags), and `test.bat` must pass.

## Layout

- `src/main.c`: startup (window; assets; data folder) and navigation: it runs the current screen and acts on what it returns. `src/app_screen.h` lists the screens.
- `src/dashboard.c`: the home screen, shown after sign-in. A home bar (the Journey/Statistics/Saved view switch, its open view the bar's only accent-filled button; code editor, Log out, Quit; labels drop to icons in a narrow window), the lesson search box, then the C/C++ journeys (`src/journey_view.c`), the statistics (from `src/dashboard_stats.c`) or the saved lessons (`src/saved_view.c`); while the search box holds text, results replace them. It returns a `HomeAction`; `DashboardShow(view, track)` picks the view. `src/search_view.c`: the search box and results, usable by any screen (state in a `SearchView`). `src/lesson_screen.c`: a lesson's reading, quiz, result, review and celebration pages; "Try it", exercises and "Return to Roadmap" come back to main.c as a `LessonAction`. There is no separate menu or lesson-list screen: lessons are opened from the journeys, statistics or search.
- `src/roadmap.c`: roadmap data (next lesson in a track, the optional preparation table keyed by lesson id). `src/roadmap_view.c`: drawing the "Suggested preparation" line.
- `src/dev_shot.c`: the `--shot` dev aid and the icon gallery. Each screen module has a `...Demo` function that `dev_shot.c` uses to set up demo state.
- Account screens, one file each: `src/login_screen.c` (sign in / create / reset modes), `src/verify_screen.c`, `src/recovery_screen.c`, `src/settings_screen.c` (also `ApplyUserAppearance`, which applies theme, accent and text size). Public API in `account_screens.h`; what they share with each other (resend cooldown, sent-code notice, handing a new recovery code to the recovery screen) is in `account_screens_shared.h`. They return an `AccountAction`; main.c decides where to go next.
- `src/layout.c`: page pieces shared by screens (top bar, content column, scroll views).
- `src/ui.c`: themes, immediate-mode widgets (buttons, links, text boxes), word wrap, syntax-highlighted code blocks, fonts. Widgets draw and handle input in the same call. Text boxes only take input if they had focus at the start of the frame, so moving focus can't let one key press trigger two fields. Esc/Backspace in the top bar don't navigate while a text box has focus.
- Colours: always use the `COL_*` macros (they read the live `gTheme`), never literal colours, so both themes work. Text on an accent fill uses `COL_ON_ACCENT`; neutral buttons hover with `COL_SECONDARY_HOVER`.
- `src/brand.c`: the logo (rounded tile with a minimalist white fox head, after the mascot: triangle ears and face, eyes and nose cut out in the tile colour), drawn from shapes. In-app it uses `COL_BRAND` (the deep shade of the user's accent, so the white fox keeps its contrast); the icon files use `BRAND_BLUE`.
- `src/icons.c`: the line-icon set (`IconDraw`), drawn from shapes so icons follow the theme. Use `UiButtonIcon`/`UiButtonIconEx` and `UiIconText` rather than drawing icons by hand. Check new or changed icons with `--shot out.png gallery`.
- `src/splash.c`: launch animation (2.4 s, any key or click skips). `--shot out.png splash 0 <ms>` renders one moment of it.
- `tools/make_icons.c` + `build.bat icons`: regenerates `assets/icon/` (PNGs at 16-256 px, `logo_512.png`, `increment.ico`) from `brand.c`. Re-run after changing the logo; the generated files are committed. `src/increment.rc` embeds the `.ico` and version info in the exe via windres. Window and taskbar icons are loaded from the PNGs at startup; the resource is named `GLFW_ICON` so the window falls back to the embedded icon if they're missing.
- `src/editor.c`: multi-line code editor widget (selection, clipboard, undo/redo, auto-indent, syntax colours via `UiCodeTokens`, error-line marks). Editing operations are plain functions with no input or drawing, so they're unit-tested; keep new editing behaviour in that layer and add tests for it.
- `src/runner.c`: compiles (with `-static`, so C++ programs need no DLLs) and runs learner code in the background, finds the compiler (`RunnerFindCompiler`), parses gcc diagnostics, explains crash codes, and compares output for exercises. `platform.c` runs the processes: no console window, a job object with a 512 MB memory cap, 5 s run limit, crash dialogs disabled with `SetErrorMode` (without it a crash hangs until the time limit when the app is started from Explorer or PowerShell), and the compiler's bin folder first on PATH (otherwise gcc can pick up another toolchain's `ld`, such as MSYS2's). Each build uses a new exe name, so a locked old exe can't block linking.
- `src/code_screen.c`: the code editor screen. Three ways in: the free editor (home bar), "Try it" on any lesson code block containing `main(` (Input is filled from a `// stdin:` marker), and lesson exercises. Drafts are saved per user in `users/<name>_code/` (deleted with the account); exercise passes are recorded with `ProgressRecordExercise`. main.c sets the draft folder with `CodeSetDraftFolder(AccountCodeDir())`, so the code screen doesn't depend on accounts.
- `src/lesson.c`: parser for lesson files.
- Pointer diagrams: a ` ```memory ` block in a lesson is a script (format in `memory_model.h`) that `src/memory_model.c` turns into steps, each a code line plus a full snapshot of memory (stack frames, heap, values, pointer targets, freed blocks), and that writes the plain-English explanation of any box (what it holds, what `*p`/`**pp` reach, who points at it, dangling pointers, leaks). `src/memory_view.c` draws it in the lesson: the code list (click a line to run up to it), boxes with declaration labels and addresses, animated arrows (grow out, glide when retargeted, a dot follows the selected pointer), and the explanation. Variables that are gone but still pointed at stay as greyed "ghosts" so dangling arrows point somewhere. Colours are `COL_*` only, so they follow the theme and accent. Search skips these blocks.
- `src/search.c`: lesson search (every word of the query must appear in the title or text, ignoring case; title matches first; excerpts around the match). Plain functions, tested; the box and results are drawn by `search_view.c` on the home screen.
- `src/account.c`: local accounts in `<data>/accounts.txt`: `@last name` plus `name iter salt hash [riter rsalt rhash]` lines (the recovery triple is optional; older 4-field lines still load). Passwords and recovery codes use PBKDF2-HMAC-SHA256 with a 16-byte random salt and 210k iterations; iteration counts are stored per secret so they can be raised later. Recovery codes are 16 symbols from a 32-char alphabet with no look-alikes (80 bits), shown once, used once (a reset issues a new one), and matched without regard to case or dashes. Per-user settings (theme, accent, text size) live in `users/<name>.settings.txt`. Usernames are 3-20 of `[A-Za-z0-9_-]`, case-insensitive, and that character set is what makes them safe as filenames.
- Email verification: `src/email.c` sends 6-digit codes through the Resend REST API (`POST https://api.resend.com/emails`) with an idempotency key per send, retrying 429/5xx/network errors with backoff. Settings come from `email_config.txt` (`api_key`, `from`), looked for next to the exe and in the data dir; it's git-ignored and must never be committed. The key is a **sending-only** Resend key ("Increment app (sending only)"). Resend is in test mode (no verified domain): the sender is `Increment <onboarding@resend.dev>`, which only delivers to the Resend account owner's own address, and Resend's 403 message is shown to the user as-is. Verification is required only when `EmailConfigured()`; without a config file the app works with no email step. `account.c` keeps each user's verified address, pending address and two independent hashed one-time codes in `users/<name>.email.txt`: `code` (verification) and `reset` (password reset), each 15 min and 5 tries; changing address keeps the old verified one until the new code is entered. Screen: `VerifyScreen` in `verify_screen.c`; sign-up sends the first code, and sign-in sends unverified accounts there.
- Password reset: "Forgot your password?" emails a reset code to the account's verified address (shown masked, `a***@example.com`) via `AccountStartPasswordReset` / `AccountResetPasswordWithEmail`, which work while signed out and sign the user in. Recovery codes still work ("Use a recovery code instead"), and are the only option for accounts without a verified email or when email isn't configured. An email reset doesn't issue a new recovery code; a recovery-code reset does.
- `src/platform.c`: the only file that includes `<windows.h>` (random bytes, PBKDF2, atomic file replace, child processes, PATH, temp dir, HTTPS via WinHTTP on a worker thread). Never include raylib.h there; the headers clash.
- `src/progress.c`: best score per lesson id, saved to `<data>/users/<lowercase name>.txt`. 70% passes. Exercise passes are stored as `<lesson id>#<n>`; only progress.c builds those ids. The first account created takes over a pre-accounts `progress.txt`.
- `src/bookmarks.c`: the signed-in user's saved lessons, as lesson ids (one per line, in saved order) in `<data>/users/<name>.bookmarks.txt` (`AccountBookmarksPath()`; deleted with the account). main.c loads it at sign-in and clears it at sign-out. The lesson page's top bar has the Save toggle; `saved_view.c` lists them in course order.
- `src/window_mode.c`: the window mode (maximised by default, full screen = borderless, or a centred 1280x800 window), the only code that moves or resizes the window. It's per computer, not per account, so it works on the login screen: saved as `window <n>` in `<data>/window.txt`. Settings > Window sets it; F11 toggles full screen and back to the previous windowed mode, and saves. Nothing applies until `WindowModeEnable()`, which main.c skips for `--shot` runs so screenshots stay 1280x800. Tested by `tests\test_window_mode.c` without opening a window.
- `src/notes.c`: lesson notes, one file per lesson in `<data>/users/<name>_notes/<lesson id>.txt` (`AccountNotesDir()`; deleted with the account). Saves replace the file atomically; blank notes delete it; files over 64 KB are refused. `src/notes_panel.c`: the "My notes" panel on the reading page (Notes button in the top bar; the lesson column narrows to make room), built on the plain-text `Editor`. It autosaves 1 s after typing stops, on Save/Ctrl+S, when leaving the reading page, when switching account and at exit, and shows Saved / Unsaved / the error. main.c sets its folder with `NotesPanelSetFolder(AccountNotesDir())` when opening a lesson and clears it at sign-out. Shot: `notes <lessonIndex>` opens the panel (`INCREMENT_SHOT_NOTES` puts text in it without saving). While the notes box has focus, Enter, Esc, Backspace and the scroll keys don't navigate (they check `UiAnyFocus()`).
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

(```memory instead of ``` opens an interactive pointer diagram; see memory_model.h)

@exercise Title           (optional, before @quiz; up to 8)
Plain lines are the task description.
@hint A gentle clue       (optional; one nonempty line per hint, up to 8, in reveal order)
@hint A more specific clue
(first unlabelled code block: starter code, including a test main())
@input                    (optional: the next code block is stdin)
@expect                   (the next code block is the expected output)
@solution                 (the next code block is a reference solution)

@quiz                     (everything after this is quiz)
? Question prompt
(a code block right after a prompt attaches to that question)
- wrong choice
* correct choice          (exactly one per question)
! Optional explanation shown after answering
```

- Keep lesson text ASCII where possible. Fonts include Latin-1 and common typographic punctuation, nothing else.
- Limits: 256 blocks, 16 questions, 6 choices per question, 8 exercises, 8 hints per exercise, 1023 chars per line.
- Exercises: the checker requires the starter to compile but not already pass, and the solution to produce the expected output exactly (trailing spaces and blank lines ignored, as in the app).
- A code block after a paragraph ending "output:" is drawn as plain text, not syntax-highlighted.
- Code samples must be correct and compile. Quiz answers must be unambiguous.
- Pointer diagrams follow the professor's example right above them, one step per line of code, with a clear `say` for every step. `test.bat` (test_memory_model) checks every diagram parses; `check_lessons.py` checks their code lines fit 66 columns.

## Gotchas

- raylib batches draws; flush with `rlDrawRenderBatchActive()` before reading the framebuffer.
- Don't edit C files with inline Python or sed when the new text contains backslash escapes (`\n`, `\"`, `\t`): the escapes get turned into real characters. Use the Edit tool, or build backslashes with `chr(92)`.
- Fonts come from `assets/fonts/text.ttf` and `mono.ttf` if present, otherwise Segoe UI and Consolas from `C:\Windows\Fonts`.
