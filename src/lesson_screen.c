// A lesson's reading, quiz and result pages. See lesson_screen.h.
#include "lesson_screen.h"
#include "bookmarks.h"
#include "layout.h"
#include "mascot.h"
#include "mascot_events.h"
#include "progress.h"
#include "quiz_attempt.h"
#include "roadmap_view.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

static struct {
    const LessonSet *set;
    int lessonIndex;
    const char *backLabel;    // where Back goes, for the result page's button
    LessonPage page;
    float scroll;             // of the reading or quiz page
    float contentHeight;      // measured last frame, for clamping the scroll

    int question;             // quiz: the current question
    QuizAttempt attempt;
    int lastPercent;          // result: the score just recorded
    FoxMilestone milestone;
    double celebrationStarted;
} reader;

static const Lesson *CurrentLesson(void) { return &reader.set->items[reader.lessonIndex]; }
const Lesson *LessonScreenLesson(void) { return CurrentLesson(); }

void LessonScreenClearAttempt(void)
{
    QuizAttemptBegin(&reader.attempt, NULL);
    reader.milestone = FOX_MILESTONE_NONE;
}

static void ShowPage(LessonPage page)
{
    reader.page = page;
    reader.scroll = 0;
    reader.contentHeight = 0;
}

void LessonScreenOpen(const LessonSet *set, int lessonIndex, const char *backLabel)
{
    reader.set = set;
    reader.lessonIndex = lessonIndex;
    reader.backLabel = backLabel;
    reader.milestone = FOX_MILESTONE_NONE;
    QuizAttemptBegin(&reader.attempt, &set->items[lessonIndex]);
    ShowPage(LESSON_PAGE_READING);
}

// This is the only new milestone trigger: compare existing saved state immediately
// around the normal score write. Merely opening a screen never calls it.
static void RecordCompletion(int percent)
{
    const Lesson *lesson = CurrentLesson();
    FoxProgressSnapshot before = FoxProgressCapture(reader.set, reader.lessonIndex);
    ProgressRecord(lesson->id, percent);
    reader.milestone = FoxProgressCompleted(reader.set, reader.lessonIndex, before);
    reader.celebrationStarted = GetTime();
    if (reader.milestone != FOX_MILESTONE_NONE) {
        FoxEvent event = reader.milestone == FOX_MILESTONE_COURSE ? FOX_COURSE_COMPLETE : FOX_LESSON_COMPLETE;
        MascotReact(FOX_CELEBRATION, event);
        MascotReact(FOX_JOURNEY, reader.milestone == FOX_MILESTONE_COURSE ? FOX_COURSE_COMPLETE : FOX_STATION_COMPLETE);
        ShowPage(LESSON_PAGE_CELEBRATION);
    } else ShowPage(LESSON_PAGE_RESULT);
}

// The next lesson in the same track, or -1 after the last one.
static int NextLessonInTrack(void)
{
    const LessonSet *set = reader.set;
    for (int i = reader.lessonIndex + 1; i < set->count; i++) {
        if (set->items[i].track == CurrentLesson()->track) return i;
    }
    return -1;
}

// Starts the quiz. A lesson without one is marked complete straight away.
static void StartQuiz(void)
{
    const Lesson *lesson = CurrentLesson();
    reader.question = 0;
    QuizAttemptBegin(&reader.attempt, lesson);
    reader.milestone = FOX_MILESTONE_NONE;
    MascotResetActor(FOX_QUIZ, FOX_THINKING);
    if (lesson->questionCount == 0) {
        reader.lastPercent = 100;
        RecordCompletion(100);
    } else {
        ShowPage(LESSON_PAGE_QUIZ);
    }
}

static void FinishQuiz(void)
{
    reader.lastPercent = QuizAttemptFinish(&reader.attempt);
    MascotReact(FOX_RESULT, reader.lastPercent >= PASS_PERCENT ? FOX_CORRECT : FOX_INCORRECT);
    RecordCompletion(reader.lastPercent);
}

void LessonScreenDemo(LessonPage page, float scroll, bool answered)
{
    const Lesson *lesson = CurrentLesson();
    if ((page == LESSON_PAGE_QUIZ || page == LESSON_PAGE_REVIEW) && lesson->questionCount == 0) page = LESSON_PAGE_READING;
    reader.page = page;
    reader.question = 0;
    QuizAttemptBegin(&reader.attempt, lesson);
    if (page == LESSON_PAGE_RESULT || page == LESSON_PAGE_REVIEW) {
        // Synthetic, unsaved demo: alternate right/wrong answers, with the last unanswered.
        for (int i = 0; i < lesson->questionCount - 1; i++) {
            const Question *q = &lesson->questions[i];
            QuizAttemptPick(&reader.attempt, i, i % 2 && q->choiceCount > 1 ? (q->correct + 1) % q->choiceCount : q->correct);
        }
        reader.lastPercent = QuizAttemptFinish(&reader.attempt);
    } else if (answered && page == LESSON_PAGE_QUIZ) {
        QuizAttemptPick(&reader.attempt, 0, 0);
        MascotReact(FOX_QUIZ, lesson->questions[0].correct == 0 ? FOX_CORRECT : FOX_INCORRECT);
    }
    if (page == LESSON_PAGE_CELEBRATION) {
        reader.milestone = answered ? FOX_MILESTONE_COURSE : FOX_MILESTONE_LESSON;
        reader.celebrationStarted = GetTime();
        MascotReact(FOX_CELEBRATION, answered ? FOX_COURSE_COMPLETE : FOX_LESSON_COMPLETE);
    }
    reader.scroll = scroll;
    reader.contentHeight = 100000;  // let the requested scroll survive the first frame's clamp
}

// ---------------------------------------------------------------------------
// Reading page

// True for paragraphs like "Output:" or "Example output:" that introduce program output.
static bool IntroducesOutput(const char *text)
{
    const char *ending = "output:";
    size_t length = strlen(text), endingLength = strlen(ending);
    if (length < endingLength) return false;
    for (size_t i = 0; i < endingLength; i++) {
        char c = text[length - endingLength + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != ending[i]) return false;
    }
    return true;
}

// Draws block `index` of the lesson at y and returns the height it used. Sets
// *tryExample when its "Try it" button is pressed.
static float DrawBlock(const Lesson *lesson, int index, float x, float y, float w, float scale,
                       const char **tryExample)
{
    const Block *block = &lesson->blocks[index];
    switch (block->type) {
    case BLOCK_HEADING:
        return 14 + UiWrappedText(gFonts.text, block->text, (Vector2){ x, y + 14 }, w, 30 * scale, COL_ACCENT, true) + 8;

    case BLOCK_TEXT:
        return UiWrappedText(gFonts.text, block->text, (Vector2){ x, y }, w, 22 * scale, COL_TEXT, true) + 14;

    case BLOCK_NOTE: {  // a tinted callout with an info icon
        float iconSize = 22 * scale, textX = 30 + iconSize, size = 21 * scale;
        float textH = UiWrappedText(gFonts.text, block->text, (Vector2){ 0, 0 }, w - textX - 24, size, COL_TEXT, false);
        UiRoundRect((Rectangle){ x, y, w, textH + 28 }, 8, COL_NOTE_BG);
        IconDraw(ICON_INFO, (Vector2){ x + 18 + iconSize / 2, y + 14 + size * 0.6f }, iconSize, COL_ACCENT);
        UiWrappedText(gFonts.text, block->text, (Vector2){ x + textX, y + 14 }, w - textX - 24, size, COL_TEXT, true);
        return textH + 28 + 16;
    }

    case BLOCK_CODE: {
        // A block after "Output:" (or "Example output:") is program output: plain, not highlighted.
        bool isOutput = index > 0 && lesson->blocks[index - 1].type == BLOCK_TEXT
                     && IntroducesOutput(lesson->blocks[index - 1].text);
        float h = isOutput ? UiOutputBlock(block->text, (Vector2){ x, y }, w, 20 * scale, true)
                           : UiCodeBlock(block->text, (Vector2){ x, y }, w, 20 * scale, true);
        if (strstr(block->text, "main(")) {  // a complete program: offer to run it
            Rectangle tryIt = { x + w - 112, y + 10, 100, 34 };
            if (UiButtonIconEx(tryIt, ICON_PLAY, "Try it", 17, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
                *tryExample = block->text;
            }
        }
        return h + 16;
    }

    case BLOCK_BULLET: {
        float size = 22 * scale;
        DrawCircleV((Vector2){ x + 9, y + size * 0.62f }, 3.5f * scale, COL_ACCENT);
        float h = UiWrappedText(gFonts.text, block->text, (Vector2){ x + 28, y }, w - 28, size, COL_TEXT, true) + 4;
        bool lastInList = index + 1 >= lesson->blockCount || lesson->blocks[index + 1].type != BLOCK_BULLET;
        return lastInList ? h + 10 : h;
    }
    }
    return 0;
}

// The "Exercises" heading and one row per exercise. Sets *opened to the exercise whose
// Start/Open button was pressed. Returns the height used.
static float DrawExercises(const Lesson *lesson, float x, float y, float w, float scale, int *opened)
{
    float top = y;
    y += 14;
    y += UiWrappedText(gFonts.text, "Exercises", (Vector2){ x, y }, w, 30 * scale, COL_ACCENT, true) + 10;
    for (int i = 0; i < lesson->exerciseCount; i++) {
        bool done = ProgressExerciseDone(lesson->id, i);
        UiRoundRect((Rectangle){ x, y, w, 64 }, 10, COL_PANEL);
        Vector2 badge = { x + 34, y + 32 };
        DrawCircleV(badge, 18, done ? COL_GOOD_BG : COL_BG);
        IconDraw(done ? ICON_CHECK : ICON_CODE, badge, 20, done ? COL_GOOD : COL_ACCENT);
        UiText(gFonts.text, lesson->exercises[i].title, x + 66, y + 20, 21, COL_TEXT);
        if (UiButtonIcon((Rectangle){ x + w - 150, y + 12, 136, 40 }, ICON_NEXT, done ? "Open" : "Start", true)) {
            *opened = i;
        }
        y += 76;
    }
    return y - top;
}

// The bookmark toggle at the right of the reading page's top bar. "Saved", with a filled
// bookmark in the accent colour, while the lesson is bookmarked; "Save lesson" otherwise.
static void SaveButton(const Lesson *lesson)
{
    bool saved = BookmarkIsSet(lesson->id);
    Rectangle button = { GetScreenWidth() - 16.0f - 160, 12, 160, 40 };
    if (UiButtonIconEx(button, saved ? ICON_BOOKMARK_FILLED : ICON_BOOKMARK, saved ? "Saved" : "Save lesson", 18,
                       COL_PANEL_HI, COL_SECONDARY_HOVER, saved ? COL_ACCENT : COL_TEXT, true)) {
        BookmarkToggle(lesson->id);
    }
}

static LessonAction ReadingPage(void)
{
    const Lesson *lesson = CurrentLesson();
    LessonAction action = { LESSON_STAY, NULL, 0 };
    if (TopBar(TextFormat("%s track", TrackName(lesson->track)))) {
        action.kind = LESSON_BACK;
        return action;
    }
    SaveButton(lesson);

    Rectangle view = ScrollView();
    UpdateScroll(&reader.scroll, reader.contentHeight, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float scale = UiTextScale();
    float top = view.y + 32 - reader.scroll, y = top;
    const char *tryExample = NULL;  // acted on after the view is closed
    int openExercise = -1;

    y += UiWrappedText(gFonts.text, lesson->title, (Vector2){ x, y }, w, 44 * scale, COL_TEXT, true) + 16;
    y += MascotCompanionActor(FOX_READING, x, y, w, 70, 18 * scale, FOX_IDEA,
                              "Try tracing examples one line at a time before running them.") + 14;
    float preparationH = RoadmapViewPreparation(reader.set, lesson, x, y, w, 19 * scale, true);
    if (preparationH) y += preparationH + 24;

    for (int i = 0; i < lesson->blockCount; i++) y += DrawBlock(lesson, i, x, y, w, scale, &tryExample);
    if (lesson->exerciseCount > 0) y += DrawExercises(lesson, x, y, w, scale, &openExercise);

    y += 16;
    const char *label = lesson->questionCount
        ? TextFormat("Start quiz (%d question%s)", lesson->questionCount, lesson->questionCount == 1 ? "" : "s")
        : "Mark as complete";
    Icon icon = lesson->questionCount ? ICON_PLAY : ICON_CHECK;
    if (UiButtonIcon((Rectangle){ x, y, 340, 52 }, icon, label, true) || IsKeyPressed(KEY_ENTER)) StartQuiz();
    y += 52 + 48;
    if (reader.attempt.completed) {
        if (UiButtonIconEx((Rectangle){ x, y, 340, 48 }, ICON_BOOK, "Review Answers", 22,
                           COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) ShowPage(LESSON_PAGE_REVIEW);
        y += 64;
    }

    reader.contentHeight = y - top + 32;
    EndView();

    if (tryExample) {
        action.kind = LESSON_TRY_EXAMPLE;
        action.exampleCode = tryExample;
    } else if (openExercise >= 0) {
        action.kind = LESSON_OPEN_EXERCISE;
        action.exerciseIndex = openExercise;
    }
    return action;
}

// ---------------------------------------------------------------------------
// Quiz page

// One answer button. Before an answer it's clickable (or press its number key); after,
// the right answer turns green and a wrong pick red. Returns its height.
static float DrawChoice(const Question *q, int choice, bool answered, float x, float y, float w, float scale,
                        bool *chosen)
{
    const char *label = TextFormat("%d.  %s", choice + 1, q->choices[choice]);
    float h = UiWrappedText(gFonts.text, label, (Vector2){ 0, 0 }, w - 48, 22 * scale, COL_TEXT, false) + 28;
    Rectangle r = { x, y, w, h };
    bool over = !answered && UiMouseOver(r);
    if (over) UiWantHand();

    Color fill = over ? COL_PANEL_HI : COL_PANEL;
    Color textColor = COL_TEXT;
    if (answered) {
        if (choice == q->correct) fill = COL_GOOD_BG;
        else if (choice == reader.attempt.answers[reader.question]) fill = COL_BAD_BG;
        else textColor = COL_DIM;
    }
    UiRoundRect(r, 10, fill);
    UiWrappedText(gFonts.text, label, (Vector2){ x + 24, y + 14 }, w - 48, 22 * scale, textColor, true);

    *chosen = (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || (!answered && IsKeyPressed(KEY_ONE + choice));
    return h;
}

static LessonAction QuizPage(void)
{
    const Lesson *lesson = CurrentLesson();
    const Question *q = &lesson->questions[reader.question];
    LessonAction action = { LESSON_STAY, NULL, 0 };
    if (TopBar(TextFormat("Quiz: %s", lesson->title))) {
        ShowPage(LESSON_PAGE_READING);
        return action;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&reader.scroll, reader.contentHeight, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float scale = UiTextScale();
    float top = view.y + 32 - reader.scroll, y = top;

    y += MascotCompanionActor(FOX_QUIZ, x, y, w, 72, 18 * scale, FOX_THINKING,
                              "Take your time. Think through what the code does.") + 14;

    UiText(gFonts.text, TextFormat("Question %d of %d", reader.question + 1, lesson->questionCount), x, y, 20, COL_DIM);
    y += 32;
    ProgressBar((Rectangle){ x, y, w, 6 }, (float)reader.question / lesson->questionCount, COL_PANEL_HI, COL_ACCENT);
    y += 30;

    y += UiWrappedText(gFonts.text, q->prompt, (Vector2){ x, y }, w, 28 * scale, COL_TEXT, true) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * scale, true) + 20;

    bool answered = reader.attempt.answers[reader.question] >= 0;
    for (int i = 0; i < q->choiceCount; i++) {
        bool chosen;
        y += DrawChoice(q, i, answered, x, y, w, scale, &chosen) + 12;
        if (chosen) {
            if (QuizAttemptPick(&reader.attempt, reader.question, i))
                MascotReact(FOX_QUIZ, i == q->correct ? FOX_CORRECT : FOX_INCORRECT);
            answered = true;
        }
    }

    if (answered) {  // feedback, the explanation, then on to the next question or the result
        y += 8;
        bool right = reader.attempt.answers[reader.question] == q->correct;
        UiIconText(right ? ICON_CHECK : ICON_CROSS, gFonts.bold, right ? "Correct!" : "Not quite.", x, y, 26,
                   right ? COL_GOOD : COL_BAD);
        y += 40;
        y += MascotCompanionActor(FOX_QUIZ, x, y, w, 72, 18 * scale, FOX_THINKING,
                                  right ? "That's correct! Great job!" : "Every mistake helps you learn!") + 14;
        if (q->explain) y += UiWrappedText(gFonts.text, q->explain, (Vector2){ x, y }, w, 21 * scale, COL_TEXT, true) + 16;

        bool lastQuestion = reader.question + 1 >= lesson->questionCount;
        bool next = UiButtonIcon((Rectangle){ x, y, 240, 50 }, lastQuestion ? ICON_TROPHY : ICON_NEXT,
                                 lastQuestion ? "See results" : "Next question", true)
                 || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
        if (next && lastQuestion) {
            FinishQuiz();
        } else if (next) {
            reader.question++;
            MascotResetActor(FOX_QUIZ, FOX_THINKING);
            reader.scroll = 0;
        }
        y += 50;
    }

    reader.contentHeight = y - top + 64;
    EndView();
    return action;
}

// ---------------------------------------------------------------------------
// Result page

static LessonAction ResultPage(void)
{
    const Lesson *lesson = CurrentLesson();
    LessonAction action = { LESSON_STAY, NULL, 0 };
    if (TopBar(lesson->title)) {
        action.kind = LESSON_BACK;
        return action;
    }

    float cx = GetScreenWidth() / 2.0f;
    float y = 100;
    bool passed = reader.lastPercent >= PASS_PERCENT;

    // A trophy for a pass, an open book (go back and review) otherwise.
    if (MascotAvailable()) {
        MascotDrawActor(FOX_RESULT, (Rectangle){ cx - 70, y - 12, 140, 128 },
                        passed ? (reader.lastPercent == 100 ? FOX_CELEBRATING : FOX_TROPHY) : FOX_DETERMINED);
        IconDraw(passed ? ICON_TROPHY : ICON_BOOK, (Vector2){ cx + 86, y + 70 }, 30, passed ? COL_GOOD : COL_ACCENT);
    } else {
        DrawCircleV((Vector2){ cx, y + 50 }, 50, passed ? COL_GOOD_BG : COL_BAD_BG);
        IconDraw(passed ? ICON_TROPHY : ICON_BOOK, (Vector2){ cx, y + 50 }, 56, passed ? COL_GOOD : COL_BAD);
    }
    y += 124;

    if (lesson->questionCount == 0) {
        UiTextCentered(gFonts.text, "Lesson complete", cx, y, 56, COL_GOOD);
        y += 90;
    } else {
        UiTextCentered(gFonts.text, TextFormat("%d%%", reader.lastPercent), cx, y, 96, passed ? COL_GOOD : COL_BAD);
        y += 120;
        UiTextCentered(gFonts.text, TextFormat("%d of %d correct", QuizAttemptCorrect(&reader.attempt), lesson->questionCount), cx, y,
                       26, COL_TEXT);
        y += 44;
        const char *verdict = passed ? "Amazing work! Keep going!"
                                     : TextFormat("Every mistake helps you improve! You need %d%% to pass.", PASS_PERCENT);
        UiTextCentered(gFonts.text, verdict, cx, y, 22, COL_DIM);
        y += 70;
    }

    // Review / Retry (or back) / Next lesson.
    int next = NextLessonInTrack();
    float buttonW = 220, buttonH = 52, gap = 20;
    float x0 = cx - (buttonW * 3 + gap * 2) / 2.0f;
    if (UiButtonIconEx((Rectangle){ x0, y, buttonW, buttonH }, ICON_BOOK, "Review lesson", 22, COL_PANEL,
                       COL_SECONDARY_HOVER, COL_TEXT, true)) {
        ShowPage(LESSON_PAGE_READING);
    }
    bool hasQuiz = lesson->questionCount > 0;
    if (hasQuiz && reader.attempt.completed && UiButtonIconEx((Rectangle){ cx - 160, y + buttonH + 16, 320, 48 },
            ICON_BOOK, "Review Answers", 22, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        ShowPage(LESSON_PAGE_REVIEW);
    }
    if (UiButtonIconEx((Rectangle){ x0 + buttonW + gap, y, buttonW, buttonH }, hasQuiz ? ICON_RETRY : ICON_BACK,
                       hasQuiz ? "Retry quiz" : reader.backLabel, 22, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        if (hasQuiz) StartQuiz();
        else action.kind = LESSON_BACK;
    }
    bool hasNext = next >= 0;
    if (UiButtonIcon((Rectangle){ x0 + (buttonW + gap) * 2, y, buttonW, buttonH }, hasNext ? ICON_NEXT : ICON_TROPHY,
                     hasNext ? "Next lesson" : "End of lessons", hasNext)
        || (hasNext && IsKeyPressed(KEY_ENTER))) {
        LessonScreenOpen(reader.set, next, reader.backLabel);
    }
    return action;
}

// ---------------------------------------------------------------------------

// One question's contents; the same wrapping pass measures the surrounding card.
static float ReviewContents(const QuizReviewItem *item, int number, float x, float y, float w, float scale, bool draw)
{
    const Question *q = item->question;
    float top = y;
    Color state = item->correct ? COL_GOOD : COL_BAD;
    const char *status = !item->answered ? "Not answered" : item->correct ? "Correct" : "Incorrect";
    if (draw) UiIconText(item->correct ? ICON_CHECK : ICON_CROSS, gFonts.bold,
                        TextFormat("Question %d - %s", number, status), x, y, 22 * scale, state);
    y += 38 * scale;
    y += UiWrappedText(gFonts.text, q->prompt, (Vector2){ x, y }, w, 24 * scale, COL_TEXT, draw) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * scale, draw) + 16;
    y += UiWrappedText(gFonts.text, "Your answer:", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
    y += UiWrappedText(gFonts.text, item->selectedAnswer ? item->selectedAnswer : "Not answered",
                       (Vector2){ x, y }, w, 22 * scale, state, draw) + 16;
    y += UiWrappedText(gFonts.text, "Correct answer:", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
    y += UiWrappedText(gFonts.text, item->correctAnswer ? item->correctAnswer : "Not available",
                       (Vector2){ x, y }, w, 22 * scale, COL_GOOD, draw) + 16;
    if (item->explanation) {
        y += UiWrappedText(gFonts.text, "Explanation", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
        y += UiWrappedText(gFonts.text, item->explanation, (Vector2){ x, y }, w, 21 * scale, COL_TEXT, draw) + 12;
    }
    return y - top;
}

static LessonAction ReviewPage(void)
{
    LessonAction action = { LESSON_STAY, NULL, 0 };
    if (TopBar("Quiz Review - Back to Results")) {
        ShowPage(LESSON_PAGE_RESULT);
        return action;
    }
    const Lesson *lesson = CurrentLesson();
    Rectangle view = ScrollView();
    view.height -= 84; // navigation stays visible while questions scroll
    UpdateScroll(&reader.scroll, reader.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 24 - reader.scroll, y = top;
    y += UiWrappedText(gFonts.text, lesson->title, (Vector2){ x, y }, w, 30 * scale, COL_TEXT, true) + 12;
    y += MascotCompanionActor(FOX_REVIEW, x, y, w, 72, 18 * scale, FOX_THINKING, "Let's understand what happened!") + 14;
    y += UiWrappedText(gFonts.text, "This attempt's answers. Reviewing does not change your saved best score.",
                       (Vector2){ x, y }, w, 18 * scale, COL_DIM, true) + 24;
    for (int i = 0; i < lesson->questionCount; i++) {
        QuizReviewItem item;
        if (!QuizAttemptReview(&reader.attempt, i, &item)) continue;
        float h = ReviewContents(&item, i + 1, 0, 0, w - 40, scale, false) + 40;
        UiRoundRect((Rectangle){ x, y, w, h }, 12, COL_PANEL);
        ReviewContents(&item, i + 1, x + 20, y + 20, w - 40, scale, true);
        y += h + 20;
    }
    reader.contentHeight = y - top + 24;
    EndView();
    float footerY = GetScreenHeight() - 84.0f;
    DrawRectangle(0, (int)footerY, GetScreenWidth(), 84, COL_BG);
    float bw = (w - 20) / 2;
    if (UiButtonIconEx((Rectangle){ x, footerY + 16, bw, 50 }, ICON_BOOK, "Return to Lesson", 21,
                       COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) ShowPage(LESSON_PAGE_READING);
    if (UiButtonIcon((Rectangle){ x + bw + 20, footerY + 16, bw, 50 }, ICON_RETRY, "Retry Quiz", true)) StartQuiz();
    return action;
}

static LessonAction CelebrationPage(void)
{
    LessonAction action = { LESSON_STAY, NULL, 0 };
    bool graduation = reader.milestone == FOX_MILESTONE_COURSE;
    if (TopBar(graduation ? "Course Graduation" : "Lesson Complete")) {
        reader.milestone = FOX_MILESTONE_NONE;
        ShowPage(LESSON_PAGE_RESULT);
        return action;
    }
    const Lesson *lesson = CurrentLesson();
    Rectangle view = ScrollView();
    view.height -= 88;
    UpdateScroll(&reader.scroll, reader.contentHeight, view.height);
    BeginView(view);
    MascotConfetti(view, GetTime() - reader.celebrationStarted, graduation);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 24 - reader.scroll, y = top;
    y += UiWrappedText(gFonts.bold, graduation ? "CONGRATULATIONS!" : "LESSON COMPLETE!",
                       (Vector2){ x, y }, w, 36 * scale, COL_GOOD, true) + 12;
    if (graduation)
        y += UiWrappedText(gFonts.text, TextFormat("You completed the %s programming course!", TrackName(lesson->track)),
                           (Vector2){ x, y }, w, 27 * scale, COL_TEXT, true) + 10;
    if (MascotAvailable()) {
        MascotDrawActor(FOX_CELEBRATION, (Rectangle){ x + (w - 200) / 2, y, 200, 176 },
                        graduation ? FOX_GRADUATION : FOX_TROPHY);
    } else IconDraw(ICON_TROPHY, (Vector2){ x + w / 2, y + 88 }, 90, COL_GOOD);
    y += 190;
    y += UiWrappedText(gFonts.text, lesson->title, (Vector2){ x, y }, w, 26 * scale, COL_TEXT, true) + 12;
    y += UiWrappedText(gFonts.text, graduation ? "You did it! Your coding journey has reached an amazing milestone!"
                                               : "Fantastic work! Another step closer to becoming a programmer!",
                       (Vector2){ x, y }, w, 20 * scale, COL_DIM, true) + 24;
    reader.contentHeight = y - top + 24;
    EndView();
    float footer = GetScreenHeight() - 88.0f;
    float bw = (w - 24) / 3;
    if (UiButtonIconEx((Rectangle){ x, footer + 18, bw, 50 }, ICON_BACK, "Return to Roadmap", 18,
                       COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        action.kind = LESSON_ROADMAP;
        MascotReact(FOX_JOURNEY, graduation ? FOX_COURSE_COMPLETE : FOX_STATION_COMPLETE);
    }
    if (UiButtonIconEx((Rectangle){ x + bw + 12, footer + 18, bw, 50 }, ICON_TROPHY, "View Progress", 18,
                       COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) action.kind = LESSON_PROGRESS;
    if (UiButtonIcon((Rectangle){ x + 2 * (bw + 12), footer + 18, bw, 50 }, ICON_NEXT, "Continue", true)) {
        reader.milestone = FOX_MILESTONE_NONE;
        ShowPage(LESSON_PAGE_RESULT);
    }
    if (action.kind != LESSON_STAY) {
        reader.milestone = FOX_MILESTONE_NONE;
        ShowPage(LESSON_PAGE_RESULT);
    }
    return action;
}

LessonAction LessonScreen(void)
{
    switch (reader.page) {
    case LESSON_PAGE_QUIZ:   return QuizPage();
    case LESSON_PAGE_RESULT: return ResultPage();
    case LESSON_PAGE_REVIEW: return ReviewPage();
    case LESSON_PAGE_CELEBRATION: return CelebrationPage();
    default:                 return ReadingPage();
    }
}
