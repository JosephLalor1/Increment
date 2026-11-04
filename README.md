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

## Learning progress dashboard (IN-7)

Open **Progress dashboard** from the main menu. The scrollable dashboard includes overall lesson completion, completed coding exercises, separate C and C++ progress cards, quiz performance, revision suggestions, and clickable topic performance bars. Use the wheel, arrow keys, Page Up/Down, or Home/End to scroll. Opening a lesson and going Back returns to the dashboard at the previous scroll position.

The dashboard presents a circular completion display and **Continue learning** action. Click either track card to open its first unfinished lesson (or revisit a completed track). Milestone badges celebrate the first completed lesson, five completed lessons, the first completed exercise, and completion of all lessons. These badges are derived from existing progress. Topic filters select all tracks, C, or C++, and all lessons, revision topics, unattempted lessons, or completed lessons. Filters remain selected when returning from a lesson and reset when entering from the main menu.

All statistics come from the signed-in user's existing progress file; the dashboard does not save duplicate records. Lesson completion uses the existing 70% threshold. Exercises count as complete only when their existing `<lesson id>#<exercise number>` score is at least 100. Only lessons and exercises currently available in the application contribute to totals.

Quiz statistics include only lessons containing quiz questions. The average is the arithmetic mean of saved best scores across attempted quiz lessons; unattempted lessons are excluded, while a recorded 0% is included. Highest score, passed quizzes, attempted quizzes below 70%, and unattempted quizzes are shown separately. Quiz-free lessons marked complete contribute to lesson progress, but not quiz performance. There is no attempt count or historical trend because those are not stored.

The first unfinished lesson in each track is recommended in existing lesson order. Attempted quizzes below 70% appear separately as revision suggestions. Every recommendation and topic row opens its lesson without restrictions. Both themes and the text-size setting are supported; track cards stack when large text needs more room.

Manual dashboard checks:

- Open the dashboard from the menu; scroll through every section and open a lesson from a recommendation and from topic performance.
- Return from the lesson, quiz results, and an exercise/editor session; check that dashboard navigation and scroll position are preserved.
- Pass a quiz, return, and check completion, averages, and the next unfinished lesson. Retry with a lower score and check that the best score is preserved.
- Complete a coding exercise and check the completed exercise count. An incomplete exercise must not count.
- Check a new account, an account with partial progress, and a fully completed track. Sign out and switch accounts to confirm statistics stay separate.
- Check light/dark themes and all text sizes, including 900x640. The menu dashboard button must remain visible.

`test.bat` includes dashboard statistics tests using synthetic lessons and a temporary progress file. For a screenshot using the existing developer tool, run `build\increment.exe --shot build/dashboard.png dashboard`; set `INCREMENT_DATA_DIR` to a scratch directory for isolated previews.

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
