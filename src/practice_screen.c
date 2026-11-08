// Custom revision UI. Navigation is returned to main.c; this file never saves progress.
#include "practice_screen.h"
#include "practice.h"
#include "quiz_view.h"
#include "layout.h"
#include "mascot.h"
#include "ui.h"

typedef enum { SETUP, QUESTIONS, RESULTS, REVIEW } PracticePage;
static struct {
    PracticeSession session;
    PracticePage page;
    float scroll, height;
    const char *error;
} practice;

static void Page(PracticePage page) { practice.page = page; practice.scroll = practice.height = 0; }
void PracticeScreenFree(void) { PracticeFree(&practice.session); practice.error = NULL; Page(SETUP); }
void PracticeScreenOpen(const LessonSet *lessons)
{
    if (practice.session.lessons != lessons) {
        PracticeScreenFree();
        if (!PracticeInit(&practice.session, lessons)) practice.error = "Not enough memory to create a practice session.";
    }
    Page(SETUP);
}
Track PracticeScreenTrack(void)
{
    return practice.session.track >= TRACK_C && practice.session.track < TRACK_COUNT ? practice.session.track : TRACK_C;
}

static float Line(const char *text, float x, float y, float width, float size, Color color)
{
    return UiWrappedText(gFonts.text, text, (Vector2){ x, y }, width, size, color, true);
}

static Rectangle Body(float top, float footer)
{
    Rectangle view = { 0, top, (float)GetScreenWidth(), GetScreenHeight() - top - footer };
    UpdateScroll(&practice.scroll, practice.height, view.height);
    BeginView(view);
    return view;
}

// Ends a page started with Body, adding its scroll bar.
static void EndBody(Rectangle view)
{
    EndView();
    PageScrollBar(view, &practice.scroll, practice.height);
}

static bool Start(void)
{
    if (!PracticeStart(&practice.session, (uint32_t)GetRandomValue(1, 0x7fffffff))) {
        practice.error = "Choose lessons with quiz questions before starting practice.";
        return false;
    }
    practice.error = NULL;
    MascotResetActor(FOX_PRACTICE, FOX_CODING);
    Page(QUESTIONS);
    return true;
}

static PracticeAction Setup(void)
{
    if (TopBar("Custom Practice Quiz")) return PRACTICE_HOME;
    PracticeSession *s = &practice.session;
    float x = ContentX(), w = ContentWidth();
    for (int t = 0; t < TRACK_COUNT; t++) {
        bool active = s->track == (Track)t;
        if (UiButtonEx((Rectangle){ x + t * (w + 12) / 2, 80, (w - 12) / 2, 42 },
                       t ? "C++ Programming" : "C Programming", 21, active ? COL_ACCENT : COL_PANEL,
                       active ? COL_ACCENT_HI : COL_SECONDARY_HOVER, active ? COL_ON_ACCENT : COL_TEXT, true)) {
            PracticeTrack(s, (Track)t);
            practice.scroll = practice.height = 0;
        }
    }
    const char *summary = s->available ? TextFormat("%d unique questions available from %d selected lessons.", s->available, s->selectedLessons)
                                      : "Select one or more lessons with quiz questions.";
    Line(practice.error ? practice.error : summary, x, 136, w, 19, practice.error ? COL_BAD : COL_DIM);
    if (UiButtonEx((Rectangle){ x, 174, 140, 36 }, "Select All", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) PracticeSelectAll(s, true);
    if (UiButtonEx((Rectangle){ x + 152, 174, 150, 36 }, "Clear Selection", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) PracticeSelectAll(s, false);
    UiText(gFonts.text, "Questions:", x + w - 374, 182, 18, COL_DIM);
    if (UiButtonEx((Rectangle){ x + w - 278, 174, 36, 36 }, "-", 22, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, s->available && s->requested > 1)) PracticeCount(s, s->requested - 1);
    UiTextCentered(gFonts.text, TextFormat("%d / %d", s->available ? s->requested : 0, s->available), x + w - 192, 182, 18, COL_TEXT);
    if (UiButtonEx((Rectangle){ x + w - 142, 174, 36, 36 }, "+", 22, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, s->requested < s->available)) PracticeCount(s, s->requested + 1);
    if (UiButtonEx((Rectangle){ x + w - 94, 174, 94, 36 }, "All", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, s->available > 0)) PracticeCount(s, s->available);

    Rectangle view = Body(226, 84);
    float top = view.y + 8 - practice.scroll, y = top;
    int shown = 0;
    for (int i = 0; i < s->lessons->count; i++) {
        const Lesson *lesson = &s->lessons->items[i];
        if (lesson->track != s->track) continue;
        shown++;
        float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, w - 90, 22 * UiTextScale(), COL_TEXT, false);
        Rectangle row = { x, y, w, titleH + 50 };
        int questions = PracticeLessonQuestions(lesson);
        bool enabled = questions > 0 && s->selected;
        bool over = enabled && UiMouseOver(row);
        bool selected = s->selected && s->selected[i];
        if (over) UiWantHand();
        UiRoundRect(row, 10, over ? COL_PANEL_HI : selected ? COL_NOTE_BG : COL_PANEL);
        UiRoundRect((Rectangle){ x + 18, y + 18, 28, 28 }, 6, selected ? COL_ACCENT : COL_BG);
        if (selected) IconDraw(ICON_CHECK, (Vector2){ x + 32, y + 32 }, 20, COL_ON_ACCENT);
        Line(lesson->title, x + 64, y + 12, w - 90, 22 * UiTextScale(), enabled ? COL_TEXT : COL_DIM);
        Line(questions ? TextFormat("%d quiz questions", questions) : "No quiz questions available for this lesson.",
             x + 64, y + titleH + 20, w - 90, 16, COL_DIM);
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) PracticeSelect(s, i, !selected);
        y += row.height + 12;
    }
    if (!shown) y += Line("No lessons are available in this track yet.", x, y, w, 22, COL_DIM) + 24;
    practice.height = y - top + 24;
    EndBody(view);
    float footer = GetScreenHeight() - 84.0f;
    Line("Revision only. Your lesson scores and course progress stay unchanged.", x, footer + 18, w - 320, 17, COL_DIM);
    if (UiButtonIcon((Rectangle){ x + w - 292, footer + 16, 292, 50 }, ICON_PLAY, "Start Practice", s->available > 0)) Start();
    return PRACTICE_STAY;
}

static PracticeAction Questions(void)
{
    if (TopBar("Practice Quiz - Exit Practice")) { Page(SETUP); return PRACTICE_HOME; }
    PracticeSession *s = &practice.session;
    const Question *q = PracticeQuestionAt(s, s->current);
    const Lesson *source = PracticeSource(s, s->current);
    if (!q || !source) { Page(SETUP); return PRACTICE_STAY; }
    Rectangle view = Body(TOPBAR_H, 84);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 24 - practice.scroll, y = top;
    y += Line(TextFormat("Question %d of %d", s->current + 1, s->count), x, y, w, 20, COL_DIM) + 12;
    y += Line(TextFormat("Source: %s Programming > %s", TrackName(source->track), source->title), x, y, w, 19 * scale, COL_ACCENT) + 14;
    ProgressBar((Rectangle){ x, y, w, 6 }, (float)s->current / s->count, COL_PANEL_HI, COL_ACCENT);
    y += 24;
    y += Line(q->prompt, x, y, w, 28 * scale, COL_TEXT) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * scale, true) + 16;
    for (int c = 0; c < q->choiceCount; c++) {
        bool chosen;
        y += QuizViewChoice(q, c, s->questions[s->current].answer, x, y, w, scale, &chosen) + 12;
        if (chosen && PracticePick(s, c)) MascotReact(FOX_PRACTICE, QuizAnswerCorrect(q, c) ? FOX_CORRECT : FOX_INCORRECT);
    }
    int answer = s->questions[s->current].answer;
    bool answered = answer >= 0;
    if (answered) {
        bool right = QuizAnswerCorrect(q, answer);
        y += Line(right ? "Correct!" : "Not quite.", x, y, w, 24 * scale, right ? COL_GOOD : COL_BAD) + 12;
        if (q->explain) y += Line(q->explain, x, y, w, 21 * scale, COL_TEXT) + 16;
    }
    y += MascotCompanionActor(FOX_PRACTICE, x, y, w, 72, 18 * scale, answered ? FOX_DETERMINED : FOX_CODING,
                              answered ? "Keep going! Every question helps you learn!" : "Ready to test your knowledge?") + 16;
    practice.height = y - top + 24;
    EndBody(view);
    bool last = s->current + 1 == s->count;
    if (UiButtonIcon((Rectangle){ x + w - 292, GetScreenHeight() - 68.0f, 292, 50 }, ICON_NEXT,
                     last ? "Submit Practice" : "Next Question", answered) || (answered && IsKeyPressed(KEY_ENTER))) {
        if (PracticeAdvance(s)) {
            if (s->completed) {
                MascotResetActor(FOX_PRACTICE, PracticePercent(s) >= 80 ? FOX_CELEBRATING : FOX_LOVE_LEARNING);
                Page(RESULTS);
            } else { MascotResetActor(FOX_PRACTICE, FOX_CODING); practice.scroll = practice.height = 0; }
        }
    }
    return PRACTICE_STAY;
}

static PracticeAction Results(void)
{
    if (TopBar("Practice Results")) return PRACTICE_HOME;
    PracticeSession *s = &practice.session;
    Rectangle view = Body(TOPBAR_H, 84);
    float x = ContentX(), w = ContentWidth(), top = view.y + 28 - practice.scroll, y = top;
    int percent = PracticePercent(s), correct = PracticeCorrect(s);
    y += Line("Practice Complete!", x, y, w, 36 * UiTextScale(), COL_TEXT) + 16;
    if (MascotAvailable()) { MascotDrawActor(FOX_PRACTICE, (Rectangle){ x + (w - 156) / 2, y, 156, 136 }, percent >= 80 ? FOX_CELEBRATING : FOX_LOVE_LEARNING); y += 148; }
    y += Line(TextFormat("%d%%", percent), x, y, w, 72, COL_ACCENT) + 12;
    y += Line(TextFormat("%d questions  |  %d correct  |  %d incorrect", s->count, correct, s->count - correct), x, y, w, 23, COL_TEXT) + 18;
    y += Line(percent >= 80 ? "Fantastic work! Keep practising to build your confidence." : "Practice makes progress! Review your answers and try again.", x, y, w, 21, COL_DIM) + 18;
    y += Line("This revision result does not change lesson scores or completion.", x, y, w, 18, COL_DIM) + 24;
    practice.height = y - top + 24;
    EndBody(view);
    float bw = (w - 24) / 3, footer = GetScreenHeight() - 68.0f;
    if (UiButtonIconEx((Rectangle){ x, footer, bw, 50 }, ICON_BOOK, "Review Answers", 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) Page(REVIEW);
    if (UiButtonIconEx((Rectangle){ x + bw + 12, footer, bw, 50 }, ICON_RETRY, "Practice Again", 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) Page(SETUP);
    if (UiButtonIcon((Rectangle){ x + 2 * (bw + 12), footer, bw, 50 }, ICON_BACK, "Return to Lessons", true)) return PRACTICE_LESSONS;
    return PRACTICE_STAY;
}

static PracticeAction Review(void)
{
    if (TopBar("Practice Review - Back to Results")) { Page(RESULTS); return PRACTICE_STAY; }
    Rectangle view = Body(TOPBAR_H, 84);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale(), top = view.y + 24 - practice.scroll, y = top;
    for (int i = 0; i < practice.session.count; i++) {
        QuizReviewItem item;
        if (!PracticeReview(&practice.session, i, &item)) continue;
        const Lesson *source = PracticeSource(&practice.session, i);
        const char *label = TextFormat("Source: %s Programming > %s", TrackName(source->track), source->title);
        float sourceH = UiWrappedText(gFonts.text, label, (Vector2){ 0, 0 }, w - 40, 18 * scale, COL_ACCENT, false);
        float h = QuizViewReview(&item, i + 1, 0, 0, w - 40, scale, false) + sourceH + 54;
        UiRoundRect((Rectangle){ x, y, w, h }, 12, COL_PANEL);
        Line(label, x + 20, y + 16, w - 40, 18 * scale, COL_ACCENT);
        QuizViewReview(&item, i + 1, x + 20, y + sourceH + 30, w - 40, scale, true);
        y += h + 20;
    }
    practice.height = y - top + 24;
    EndBody(view);
    if (UiButtonIcon((Rectangle){ x + (w - 300) / 2, GetScreenHeight() - 68.0f, 300, 50 }, ICON_BACK, "Back to Results", true)) Page(RESULTS);
    return PRACTICE_STAY;
}

PracticeAction PracticeScreen(void)
{
    if (!practice.session.lessons) return PRACTICE_HOME;
    switch (practice.page) {
    case QUESTIONS: return Questions();
    case RESULTS: return Results();
    case REVIEW: return Review();
    default: return Setup();
    }
}

void PracticeScreenDemo(Track track, int page)
{
    PracticeTrack(&practice.session, track);
    PracticeSelectAll(&practice.session, true);
    PracticeCount(&practice.session, 5);
    if (page == 0 || !PracticeStart(&practice.session, 1234)) return;
    if (page > 1) {
        for (int i = 0; i < practice.session.count; i++) {
            const Question *q = PracticeQuestionAt(&practice.session, i);
            PracticePick(&practice.session, i % 2 ? (q->correct + 1) % q->choiceCount : q->correct);
            PracticeAdvance(&practice.session);
        }
    }
    Page(page == 1 ? QUESTIONS : page == 2 ? RESULTS : REVIEW);
}
