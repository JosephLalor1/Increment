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

## Increment Fox

The original mascot lives at `assets/mascot/increment_fox.png`, with 12 expressions in `01_fox_happy.png` through `12_fox_celebrating.png`. The app loads them once at startup, preserves transparency and aspect ratios, and releases textures at shutdown. A missing expression falls back to the happy fox, the original image, or another available expression. With no images, learning and navigation still work. Asset discovery supports the executable in `build` or another working directory. Keep `assets` next to the executable or one level above it when distributing the app.

The fox welcomes students on the main menu, accompanies dashboard progress, highlights the first unfinished lesson in each roadmap, and encourages students on quiz results. Its messages respond to completion and quiz outcomes. It does not modify accounts, scores, or progress. If the PNG is missing or cannot load, the app continues with its existing illustrations.

The fox now reacts to confirmed quiz answers: excitement for correct answers, brief sadness followed by determination after an incorrect answer, then a return to thinking. The existing grading and explanations are unchanged. Correct/incorrect messages rotate, and the fox also accompanies lesson reading, review, and the editor. Hover over the editor fox for encouragement; click the menu fox for a general learning tip or a break-themed message. No inactivity is tracked. Expression fades, brief bounce effects, and celebration confetti are always enabled.

First-time lesson passes show a dismissible trophy celebration. Completing the final outstanding lesson in a track shows graduation instead, even when that lesson is not the final lesson file. C and C++ are checked independently. These screens are triggered only around the existing score write when saved completion changes from incomplete to complete; visiting a page, retrying an already-passed quiz, signing in, or restarting does not replay a milestone. No new progress records or account fields are added. **Continue** opens the regular results page, **Return to Roadmap** opens that track, and **View Progress** opens its statistics. Back dismisses the celebration to results. All celebration buttons remain visible while the content scrolls.

Automated mascot tests cover timed reactions, return to default expressions, score thresholds, out-of-order course completion, independent tracks, repeated navigation checks, account clearing, and preserved best scores. Journey rendering tests exercise missing images. Developer screenshots support `celebration` and `graduation`; these are synthetic previews and never save scores.

Manual interaction checks:

- In each track, answer one question correctly and one incorrectly. Check excitement, the sad-to-determined transition, and returning to thinking; explanations and answers must follow the existing quiz rules.
- Pass an unfinished lesson and verify the trophy, lesson title, confetti, and all three navigation buttons. Retry a previously passed lesson and confirm there is no new milestone popup.
- Complete the final outstanding lesson in C, then C++; verify the correct course name and graduation art. Reopen the journey and restart to confirm the celebration does not repeat.
- Check the fox beside the current journey station, on the menu, in lesson reading/review, and in the editor. Code typing and navigation must keep working.
- Try menu-fox tips and animated reactions, and check both themes at 900x640 with large text. Missing expression files should fall back safely. Real account files and mascot artwork should remain unchanged.

Manual mascot checks: open the menu, dashboard, C/C++ roadmaps, and passed/failed quiz results in both themes. Check 900x640 and large text; controls and statistics must stay visible. Scroll a roadmap after completing a lesson to see the fox beside the new recommendation. Fully completed tracks have no unfinished recommendation. Launch `build/increment.exe` from outside the project folder to check asset discovery.

## Courses and learning journeys

Choose the C or C++ course card on the main menu to browse the original sequential lesson list. Rows retain lesson numbers, titles, completion checks, practice icons, best scores, scrolling, and search. Courses never show the winding path or a roadmap/list toggle. Returning from a lesson restores the originating course list or search results.

Choose **Progress dashboard** for the separate gamified learning journey. Its winding path uses numbered circular stations for real lessons in existing order. Green checks show completed lessons; a glowing purple **START/CONTINUE** station marks the first unfinished lesson, accompanied by the fox. Muted upcoming stations remain open. Click a station or its title to study; preparation suggestions remain available on lesson pages. Both interfaces use the same lessons and saved progress.

Completion uses the existing saved best quiz score (70% passes). The recommendation is the first incomplete lesson in filename order within the selected track, even if later lessons are already complete. Totals and the progress bar update from the same scores. Optional preparation relationships are maintained in the lesson-ID table in `src/roadmap.c`; references to missing content are omitted.

Manual roadmap checks:

- Open both courses and confirm sequential lists; open Progress dashboard and confirm winding C/C++ journeys. Open completed and upcoming lessons from each interface.
- Pass or fail a quiz, return to the roadmap, and check completion and the first incomplete recommendation. A failed retry should preserve a previous pass.
- Scroll to the final lesson using the wheel and keyboard; check Back navigation and exercise/editor return navigation.
- Check light/dark themes and all text sizes at 900x640 and larger window sizes.
- With a fully completed track, check the congratulations message and that all lessons remain accessible.

## Lesson search

The search box at the top of each course's lessons (or `Ctrl+F`) searches every lesson's title and text, including code and exercises, ignoring case. Each word you type must appear somewhere in the lesson. Choose **C and C++**, **C** or **C++** to filter the results; each result shows the lesson's title, track, best score and the text around the match. Click a result (or press `Enter` for the top one) to open the lesson; **Back** returns to the results. Clear the box with the **x** button to get the sequential course list back.

## Learning progress dashboard (IN-7)

Open **Progress dashboard** from the main menu to enter the **Learning Journey**. Switch between **C Journey** and **C++ Journey** using the fixed top tabs. A compact header shows course completion; the winding path is the main view, with layered circular stations, curved connections, hover effects, and an animated current lesson. The fox guides the first unfinished lesson. Each track retains its scroll position while switching tabs or visiting a lesson. Every station remains clickable, regardless of progress.

The **Statistics** button opens the detailed dashboard: overall completion, exercises, milestones, quiz scores, revision suggestions, and topic filters. Its **Journey** button returns to the path. Both views share existing navigation and scores, and completion updates immediately after returning from a quiz. The winding journey renderer is exclusive to the dashboard. Courses use the original sequential row renderer. Both read the same lesson order and progress system.

Use the wheel, arrow keys, Page Up/Down, or Home/End to scroll, or **Find my place** to jump to the first unfinished station. Journey scrolling eases smoothly toward its target. Developer screenshots support `dashboard` (journey) and `statistics`; choose a C++ lesson index to preview the C++ journey. Rendering tests cover both tracks, themes, scrolling alignment, missing art, empty/completed tracks, and unchanged saved scores.

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

For local development, run `run-dev.bat` (or `.\run-dev.bat` in PowerShell). It enables email verification automatically using a local outbox; no API key or internet connection is needed. It builds the app if the executable is missing. After changing code, run `build.bat` first to rebuild it.

The launcher uses separate test accounts in `build/email-test-data`. Create a test account, then open `build/email-test-outbox/<username>.txt` and enter its six-digit code. Password reset codes appear in `<username>_reset.txt` in the same folder. The verification screen includes **Copy outbox path** so you can paste the folder path into File Explorer. These messages are files, not real emails. The launcher overrides real sending for that run and does not change your terminal's environment or normal account data.

Registration identifies the active mode: local development outbox, real email enabled, or email verification unavailable. Normal launching without either an outbox or real configuration still permits local accounts without email; it now explains this explicitly.

For real email delivery, copy `email_config.example.txt` to `build/email_config.txt` and replace the placeholder with your own sending-only key. Launch the app normally with `INCREMENT_EMAIL_OUTBOX` unset; if you set it manually in PowerShell, remove it with `Remove-Item Env:INCREMENT_EMAIL_OUTBOX -ErrorAction SilentlyContinue`. The example file is safe to share; the real configuration stays ignored by Git.

Emails are sent through [Resend](https://resend.com). The app reads its settings from `email_config.txt`, placed next to `increment.exe` (for example `build\email_config.txt`):

```
api_key re_your_sending_only_key
from Increment <onboarding@resend.dev>
```

Use a **sending-only** API key, never a full-access one, and never commit the file (it's in `.gitignore`). While Resend is in test mode (no verified domain), the `onboarding@resend.dev` sender can only deliver to the email address of the Resend account; verify a domain in Resend to email anyone. Without `email_config.txt` the app still works, just without the email step.

## Quiz answer review

After completing a C or C++ quiz, choose **Review Answers** on the results page. Each question shows its original prompt and code, your selected answer, the correct answer, a Correct/Incorrect label, and the lesson's explanation when one exists. Unanswered questions display **Not answered**. Use the wheel, arrows, Page Up/Down, or Home/End to scroll. **Retry Quiz** and **Return to Lesson** remain visible at the bottom; Back returns to results.

Answers are kept in memory for the current lesson attempt, not written to accounts or progress files. Returning to the reading page keeps **Review Answers** available. Starting a new attempt, opening another lesson, or signing out clears the selections. Retry preserves the saved best score, even if the new result is lower. Viewing review never records or changes a score.

Manual checks: complete quizzes in both tracks with a mix of right/wrong answers; verify every selection, code snippet, and explanation against the quiz. Check Back to Results, Return to Lesson, and Retry Quiz; retry must clear selections while retaining best scores. Check scrolling and fixed navigation in both themes, all text sizes, and at 900x640. Restarting the app does not restore past attempt answers.

Developer preview: `build\increment.exe --shot build/review.png review 0` shows a synthetic attempt without saving progress. Replace `0` with a lesson index; an optional final argument sets the scroll position. The demo intentionally includes an unanswered final question; the normal quiz flow requires answering every question.

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
