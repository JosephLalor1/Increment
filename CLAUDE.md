# Increment

A desktop app for learning C and C++, written in C99 with raylib 6.0. This is an AI-only project: Claude writes all code and lesson content; the user directs.

## Build and run

- `build.bat` builds a debug `build\increment.exe`; `build.bat release` builds an optimised one with no console window.
- The toolchain is the raylib Windows bundle at `C:\raylib` (w64devkit gcc 15.2 + `raylib\src\libraylib.a`). Override with `RAYLIB_DIR`. Do not use the MSYS2 gcc on PATH; it has no raylib.
- Build must stay warning-free under `-Wall -Wextra`.

## Verifying UI changes

`increment.exe --shot out.png [menu|list|lesson|quiz|answered|result] [lessonIndex] [scroll]` renders one screen to a PNG and exits. Write shots to the scratchpad and view them after any visual change. `lessonIndex` indexes all lessons in filename order.

## Layout

- `src/main.c`: screen state machine (menu, list, lesson, quiz, result) and the `--shot` dev aid.
- `src/ui.c`: immediate-mode widgets, word wrap, syntax-highlighted code blocks, fonts. Widgets draw and handle input in the same call.
- `src/lesson.c`: parser for lesson files.
- `src/progress.c`: best score per lesson id, saved to `progress.txt` next to the exe. 70% passes.
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
