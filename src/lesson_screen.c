// A lesson's reading, quiz and result pages. See lesson_screen.h.
#include "lesson_screen.h"
#include "layout.h"
#include "progress.h"
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
    int picked;               // quiz: the chosen answer, or -1 until answered
    int correctCount;         // quiz: right answers so far
    int lastPercent;          // result: the score just recorded
} reader;

static const Lesson *CurrentLesson(void) { return &reader.set->items[reader.lessonIndex]; }
const Lesson *LessonScreenLesson(void) { return CurrentLesson(); }

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
    ShowPage(LESSON_PAGE_READING);
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
    reader.picked = -1;
    reader.correctCount = 0;
    if (lesson->questionCount == 0) {
        reader.lastPercent = 100;
        ProgressRecord(lesson->id, 100);
        ShowPage(LESSON_PAGE_RESULT);
    } else {
        ShowPage(LESSON_PAGE_QUIZ);
    }
}

static void FinishQuiz(void)
{
    const Lesson *lesson = CurrentLesson();
    reader.lastPercent = reader.correctCount * 100 / lesson->questionCount;
    ProgressRecord(lesson->id, reader.lastPercent);
    ShowPage(LESSON_PAGE_RESULT);
}

void LessonScreenDemo(LessonPage page, float scroll, bool answered)
{
    const Lesson *lesson = CurrentLesson();
    if (page == LESSON_PAGE_QUIZ && lesson->questionCount == 0) page = LESSON_PAGE_READING;
    reader.page = page;
    reader.question = 0;
    reader.picked = answered && page == LESSON_PAGE_QUIZ ? 0 : -1;  // the first choice, right or wrong
    reader.correctCount = 3;
    reader.lastPercent = 75;
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

static LessonAction ReadingPage(void)
{
    const Lesson *lesson = CurrentLesson();
    LessonAction action = { LESSON_STAY, NULL, 0 };
    if (TopBar(TextFormat("%s track", TrackName(lesson->track)))) {
        action.kind = LESSON_BACK;
        return action;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&reader.scroll, reader.contentHeight, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float scale = UiTextScale();
    float top = view.y + 32 - reader.scroll, y = top;
    const char *tryExample = NULL;  // acted on after the view is closed
    int openExercise = -1;

    y += UiWrappedText(gFonts.text, lesson->title, (Vector2){ x, y }, w, 44 * scale, COL_TEXT, true) + 16;
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
        else if (choice == reader.picked) fill = COL_BAD_BG;
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

    UiText(gFonts.text, TextFormat("Question %d of %d", reader.question + 1, lesson->questionCount), x, y, 20, COL_DIM);
    y += 32;
    ProgressBar((Rectangle){ x, y, w, 6 }, (float)reader.question / lesson->questionCount, COL_PANEL_HI, COL_ACCENT);
    y += 30;

    y += UiWrappedText(gFonts.text, q->prompt, (Vector2){ x, y }, w, 28 * scale, COL_TEXT, true) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * scale, true) + 20;

    bool answered = reader.picked >= 0;
    for (int i = 0; i < q->choiceCount; i++) {
        bool chosen;
        y += DrawChoice(q, i, answered, x, y, w, scale, &chosen) + 12;
        if (chosen) {
            reader.picked = i;
            if (i == q->correct) reader.correctCount++;
            answered = true;
        }
    }

    if (answered) {  // feedback, the explanation, then on to the next question or the result
        y += 8;
        bool right = reader.picked == q->correct;
        UiIconText(right ? ICON_CHECK : ICON_CROSS, gFonts.bold, right ? "Correct!" : "Not quite.", x, y, 26,
                   right ? COL_GOOD : COL_BAD);
        y += 40;
        if (q->explain) y += UiWrappedText(gFonts.text, q->explain, (Vector2){ x, y }, w, 21 * scale, COL_TEXT, true) + 16;

        bool lastQuestion = reader.question + 1 >= lesson->questionCount;
        bool next = UiButtonIcon((Rectangle){ x, y, 240, 50 }, lastQuestion ? ICON_TROPHY : ICON_NEXT,
                                 lastQuestion ? "See results" : "Next question", true)
                 || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
        if (next && lastQuestion) {
            FinishQuiz();
        } else if (next) {
            reader.question++;
            reader.picked = -1;
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
    DrawCircleV((Vector2){ cx, y + 50 }, 50, passed ? COL_GOOD_BG : COL_BAD_BG);
    IconDraw(passed ? ICON_TROPHY : ICON_BOOK, (Vector2){ cx, y + 50 }, 56, passed ? COL_GOOD : COL_BAD);
    y += 124;

    if (lesson->questionCount == 0) {
        UiTextCentered(gFonts.text, "Lesson complete", cx, y, 56, COL_GOOD);
        y += 90;
    } else {
        UiTextCentered(gFonts.text, TextFormat("%d%%", reader.lastPercent), cx, y, 96, passed ? COL_GOOD : COL_BAD);
        y += 120;
        UiTextCentered(gFonts.text, TextFormat("%d of %d correct", reader.correctCount, lesson->questionCount), cx, y,
                       26, COL_TEXT);
        y += 44;
        const char *verdict = passed ? "Nice work. Lesson complete."
                                     : TextFormat("You need %d%% to pass. Review the lesson and try again.", PASS_PERCENT);
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

LessonAction LessonScreen(void)
{
    switch (reader.page) {
    case LESSON_PAGE_QUIZ:   return QuizPage();
    case LESSON_PAGE_RESULT: return ResultPage();
    default:                 return ReadingPage();
    }
}
