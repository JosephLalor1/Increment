# Increment

Learn C and C++ by writing code. Increment is a desktop app where every level is a sequence of small coding steps: change the program until its output matches the target. It is built in C with [raylib](https://www.raylib.com/).

## Features

- **Two journeys.** A C track (a first program through pointers and memory management) and a C++ track (references, `new` and `delete`, classes, linked lists): 30 levels, 236 steps. Each track is a winding path with optional coding exam milestones; **Find my place** suggests the next level or not-yet-passed exam. Every lesson, step and exam is open from the start, in any order.
- **Coding exams.** Groups of 4-5 levels end in an optional timed, closed-book coding assessment, with saved submissions and revision review. **Courses** remains a separate, sequential list with unrestricted access.
- **Levels are coding steps.** Early steps tweak a working program, later ones fill a gap, fix a bug, and finally write the program from an empty `main`. The editor sits beside the target output and your program's output, compared line by line. Programs are compiled with gcc/g++; compiler errors are marked on their lines.
- **Two journeys.** A C track (a first program through pointers and memory management) and a C++ track (references, `new` and `delete`, classes, linked lists): 30 levels, 236 steps. Each track is a winding path of levels; **Find my place** jumps to the next one. Any level can be opened at any time.
- **Levels are coding steps.** Early steps tweak a working program, later ones fill a gap, fix a bug, and finally write the program from an empty `main`. The editor sits beside the target output and your program's output, compared line by line. Steps that read input have several test cases, and a step passes only when every one of them prints its target. Programs are compiled with gcc/g++; compiler errors are marked on their lines.
- **Hints are code.** A hint highlights the line to work on, then shows its shape, then the line itself. Each one costs some of the step's XP.
- **Honest solutions only.** Printing the answer doesn't pass: the code must use what the step teaches, and a second run with different values must match too.
- **A picture of memory.** Code with a pointer in it gets a diagram under the editor: a box per variable and an arrow per pointer, redrawn from real addresses after each run. Freed blocks, null and wild pointers show up in red.
- **Skills and XP.** The skills in the code you write are detected (28 of them, from loops and strings to pointers, classes and debugging) and earn XP. Statistics shows each skill's level and which ones need work.
- **Speed rounds.** Most levels offer an optional round of ten generated "what is x?" snippets against the clock; ten in a row earns gold on that level. Answers are scheduled like flashcards (SM-2), and a **Challenge** practises the skills that most need it.
- **A free code editor.** Write and run your own C or C++, with name suggestions, saved programs and test cases: give each an input and, if you like, the output it should give, and Run checks them all.
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

## Journey coding exam checkpoints

**Home > Journey** contains prominent exam stations connected to the same winding trail as the levels. **Courses** opens a traditional ordered list instead. All real lessons and exams can be selected immediately in both views, including Statistics and Next level actions. `CheckpointLessonOpen` and `CheckpointAvailable` validate that content exists; they never check completion, previous exams or scores. Individual coding steps are also freely selectable, and Next does not require passing the current step.

Sections are derived from ready levels in actual per-track file order, without a duplicate curriculum table. C currently groups 17 levels as **4 / 4 / 4 / 5**, and C++ groups 13 as **4 / 4 / 5**. Empty/unwritten levels are not assessed; a final short section is supported when a curriculum cannot be divided into groups of four or five. Checkpoint names identify the real first/last level, and IDs hash the ordered level IDs and include the track. Adding/reordering curriculum can therefore create a new checkpoint identity rather than silently granting an old pass to different content.

Exams are optional practice milestones, not progression gates. A fresh account can start Exam 2 or a final lesson in either track without completing anything else. Not Started, In Progress/Attempted, and Completed/Passed are visual statuses only; failed exams show their recorded best score and remain immediately retryable. Opening a lesson records a backward-compatible `!started` status marker without granting a passed step. Existing completion and exam records are retained; later failed retakes preserve earlier passes. C and C++ results remain independent.

The default duration is **30 minutes**, and the passing mark is **60%** for recording pass/fail only; it never affects content access. `EXAM_DURATION_SECONDS` and `EXAM_PASS_PERCENT` in `learning/checkpoints.h` are build-time defaults; each `Checkpoint` carries its own duration/threshold for future definitions. Exams select the final coding step of each level in the immediately preceding section, preserving its starter, input, target output, reference, fixed-value flag and weighted skill metadata. No questions are drawn from later sections. These are curriculum tasks, rather than a new unseen assessment bank.

During an exam, write code, compile/run it, switch tasks, or Finish Exam. Each task retains its own buffer. The absolute monotonic deadline keeps running during task switches and the submission confirmation; expiry freezes all buffers and starts grading exactly once. Each task is equally weighted, with up to two tests: normal execution/output plus the existing changed-input/givens assessment where supported. The existing required-construct and hardcoded-answer checks apply. Fixed tasks or tasks without changeable givens use one test. A compiler/infrastructure failure is labelled separately from an incorrect program. Source is limited to 512 KiB per task; edits exceeding this limit are rejected before they can prevent deadline submission. No notes, hints, references, or guided answers are available while coding.

Results show the total marks/percentage, threshold, time, passed task count, per-task test results, submitted source, compiler output, runtime/output failures, changed-value feedback and reference solution. Concept labels and assessment reports support comparison and revision. **Retry in Learning Mode** reuses the existing learning editor/checks/hints with a detached reference snapshot: it changes neither the exam submission nor ordinary level progress/XP. Return to Dashboard and Retake are available after the attempt is saved. A failed write shows Retry Save so an unsaved submission is not silently discarded.

Every submitted attempt is a separate, atomically written `.exam` snapshot in the account's `users/<name>_code/exams/` folder. Snapshots include source, task references, outcomes and skill metadata, so review survives curriculum edits. Normal application exit during an exam retains an interrupted zero-score submission; a forced process kill/crash is not resumable. Files are included in account backup/restore and removed with account deletion. These local files are not encrypted or tamper-proof. No exam result writes a lesson score or awards skill XP.

Developer screenshots: `--shot build/exam.png exam <lessonIndex> <section>` shows the checkpoint intro (section is zero-based, lesson picks the track); `exam-active`, `exam-results`, `exam-review`, and `courses` preview the corresponding views. Exam demos never save anything. Set `INCREMENT_DATA_DIR` and `INCREMENT_EMAIL_OUTBOX` to isolated test folders for experiments.

Manual checks: with a new account, open final C/C++ lessons and later exams before attempting earlier content; select later coding steps directly. Verify Dashboard, Courses, Statistics and Next work without prerequisite messages or lock icons. Attempt and fail an exam, then open another exam/lesson and retry immediately. Check task buffers, grading, history, revision and Return to Dashboard, then reopen to verify saved statuses. Check both themes at 900x640: exam positions and connections must remain unchanged. Automated tests cover unrestricted access for all real lessons/steps/exams, unchanged grouping, real grading and deadlines, started/completed statuses, retained history and legacy progress, and read-only journey rendering.

## Source folders

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
