#include "dashboard.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

static Track topicTrack = TRACK_COUNT;
static int topicFilter;

void DashboardReset(void)
{
    topicTrack = TRACK_COUNT;
    topicFilter = 0;
}

static float TextLine(const char *text, float x, float y, float w, float size, Color color)
{
    return UiWrappedText(gFonts.text, text, (Vector2){ x, y }, w, size, color, true);
}

static float TrackSummary(DashboardStats stats, const char *title, float x, float y, float w, float scale, bool *clicked)
{
    float titleH = UiWrappedText(gFonts.bold, title, (Vector2){ 0, 0 }, w - 40, 26 * scale, COL_TEXT, false);
    float h = titleH + 132 * scale;
    Rectangle card = { x, y, w, h };
    bool over = UiMouseOver(card);
    if (over) UiWantHand();
    UiRoundRect((Rectangle){ x, y + 5, w, h }, 14, COL_PANEL_HI);
    UiRoundRect(card, 14, over ? COL_PANEL_HI : COL_PANEL);
    TextLine(title, x + 20, y + 16, w - 40, 26 * scale, COL_TEXT);
    float p = stats.lessons ? (float)stats.completed / stats.lessons : 0;
    TextLine(TextFormat("%d / %d lessons complete  |  %.0f%%", stats.completed, stats.lessons, p * 100),
             x + 20, y + titleH + 26, w - 40, 18 * scale, COL_DIM);
    ProgressBar((Rectangle){ x + 20, y + h - 62 * scale, w - 40, 12 }, p, COL_BG, COL_GOOD);
    UiIconText(stats.next >= 0 ? ICON_PLAY : ICON_RETRY, gFonts.text,
               stats.next >= 0 ? "Continue this track" : "Revisit this track", x + 20, y + h - 36 * scale, 18 * scale, COL_ACCENT);
    *clicked = over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return h;
}

static float JourneyCard(const LessonSet *set, DashboardStats stats, float x, float y, float w, float scale, int *selected)
{
    float ringSize = 116 * scale;
    float textX = x + ringSize + 52, textW = w - ringSize - 76;
    const char *title = stats.lessons && stats.next < 0 ? "Look how far you've come!" : "One step closer. Every lesson counts.";
    float titleH = UiWrappedText(gFonts.bold, title, (Vector2){ 0, 0 }, textW, 27 * scale, COL_TEXT, false);
    float bodyH = UiWrappedText(gFonts.text, TextFormat("%d of %d lessons complete", stats.completed, stats.lessons),
                               (Vector2){ 0, 0 }, textW, 19 * scale, COL_DIM, false);
    int next = stats.next >= 0 ? stats.next : stats.lessons ? 0 : -1;
    const char *nextTitle = next >= 0 ? set->items[next].title : "";
    float nextH = UiWrappedText(gFonts.text, nextTitle, (Vector2){ 0, 0 }, textW, 16 * scale, COL_DIM, false);
    float h = titleH + bodyH + nextH + 112 * scale;
    if (h < ringSize + 48) h = ringSize + 48;
    UiRoundRect((Rectangle){ x, y + 5, w, h }, 18, COL_PANEL_HI);
    UiRoundRect((Rectangle){ x, y, w, h }, 18, COL_PANEL);
    Vector2 center = { x + 24 + ringSize / 2, y + h / 2 };
    float ratio = stats.lessons ? (float)stats.completed / stats.lessons : 0;
    DrawRing(center, ringSize / 2 - 12, ringSize / 2, 0, 360, 96, COL_BG);
    if (ratio > 0) DrawRing(center, ringSize / 2 - 12, ringSize / 2, -90, -90 + ratio * 360, 96, COL_GOOD);
    UiTextCentered(gFonts.bold, TextFormat("%.0f%%", ratio * 100), center.x, center.y - 22 * scale, 32 * scale, COL_GOOD);
    UiTextCentered(gFonts.text, "complete", center.x, center.y + 17 * scale, 14 * scale, COL_DIM);
    UiWrappedText(gFonts.bold, title, (Vector2){ textX, y + 22 }, textW, 27 * scale, COL_TEXT, true);
    TextLine(TextFormat("%d of %d lessons complete", stats.completed, stats.lessons), textX, y + 30 + titleH, textW, 19 * scale, COL_DIM);
    if (next >= 0) {
        float buttonY = y + titleH + bodyH + 46;
        if (UiButtonIcon((Rectangle){ textX, buttonY, textW, 46 * scale }, stats.next < 0 ? ICON_TROPHY : ICON_PLAY,
                         stats.next < 0 ? "Keep practising" : "Continue learning", true)) *selected = next;
        // The action always follows actual content order, without unlocking requirements.
        TextLine(nextTitle, textX, buttonY + 54 * scale, textW, 16 * scale, COL_DIM);
    }
    return h + 24;
}

static float Milestones(DashboardStats stats, float x, float y, float w, float scale)
{
    const char *names[] = { "First lesson", "Five lessons", "First exercise", "All lessons" };
    bool earned[] = { stats.completed >= 1, stats.completed >= 5, stats.exercisesCompleted >= 1,
                      stats.lessons > 0 && stats.completed == stats.lessons };
    float gap = 12, cardW = (w - gap) / 2;
    float h = 76 * scale;
    for (int i = 0; i < 4; i++) {
        float bx = x + (i % 2) * (cardW + gap), by = y + (i / 2) * (h + gap);
        UiRoundRect((Rectangle){ bx, by, cardW, h }, 12, earned[i] ? COL_GOOD_BG : COL_PANEL);
        IconDraw(earned[i] ? ICON_TROPHY : ICON_TARGET, (Vector2){ bx + 28, by + h / 2 }, 26 * scale, earned[i] ? COL_GOOD : COL_DIM);
        TextLine(names[i], bx + 54, by + 14 * scale, cardW - 66, 18 * scale, earned[i] ? COL_GOOD : COL_TEXT);
        TextLine(earned[i] ? "Achieved!" : "Keep going", bx + 54, by + 42 * scale, cardW - 66, 14 * scale, COL_DIM);
    }
    return h * 2 + gap + 24;
}

static bool FilterChip(Rectangle r, const char *label, bool active)
{
    return UiButtonEx(r, label, 18, active ? COL_ACCENT : COL_PANEL,
                      active ? COL_ACCENT_HI : COL_SECONDARY_HOVER, active ? COL_ON_ACCENT : COL_TEXT, true);
}

static float LessonRow(const Lesson *lesson, const char *label, float x, float y, float w, float scale, bool *clicked)
{
    float size = 21 * scale;
    float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, w - 96, size, COL_TEXT, false);
    float labelH = UiWrappedText(gFonts.text, label, (Vector2){ 0, 0 }, w - 40, 17 * scale, COL_DIM, false);
    float h = titleH + labelH + 58;
    Rectangle row = { x, y, w, h };
    bool over = UiMouseOver(row);
    if (over) UiWantHand();
    UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);
    int best = ProgressBest(lesson->id);
    IconDraw(best >= PASS_PERCENT ? ICON_CHECK : best >= 0 ? ICON_RETRY : ICON_BOOK,
             (Vector2){ x + 32, y + 28 }, 24, best >= PASS_PERCENT ? COL_GOOD : COL_ACCENT);
    TextLine(lesson->title, x + 60, y + 14, w - 96, size, COL_TEXT);
    TextLine(label, x + 20, y + titleH + 22, w - 40, 17 * scale, COL_DIM);
    int score = ProgressBest(lesson->id);
    Color fill = score >= PASS_PERCENT ? COL_GOOD : COL_ACCENT;
    ProgressBar((Rectangle){ x + 20, y + h - 20, w - 40, 6 }, score < 0 ? 0 : (float)score / 100, COL_BG, fill);
    *clicked = over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return h + 12;
}

int DashboardDraw(const LessonSet *set, float x, float y, float w, float scale, float *height)
{
    float top = y;
    int selected = -1;
    DashboardStats all = DashboardCalculate(set, TRACK_COUNT);
    y += TextLine("Your learning adventure", x, y, w, 34 * scale, COL_TEXT) + 12;
    y += TextLine("Small steps. Real progress. Choose your next challenge.",
                  x, y, w, 19 * scale, COL_DIM) + 20;
    y += JourneyCard(set, all, x, y, w, scale, &selected);
    y += TextLine(TextFormat("Coding exercises: %d / %d completed", all.exercisesCompleted, all.exercises),
                  x, y, w, 20 * scale, COL_TEXT) + 24;
    if (!all.lessons) {
        y += TextLine("No lessons available yet. Add lesson files to assets/lessons to get started.", x, y, w, 21 * scale, COL_DIM);
        *height = y - top + 40;
        return -1;
    }
    if (!all.completed && !all.attempted && !all.exercisesCompleted)
        y += TextLine("Your journey starts here. Choose a recommended lesson below; your progress will appear as you practise.",
                      x, y, w, 19 * scale, COL_ACCENT) + 24;
    else if (all.completed == all.lessons)
        y += TextLine("Congratulations! Every lesson is complete. Revisit any topic to keep your skills sharp.",
                      x, y, w, 21 * scale, COL_GOOD) + 24;

    y += TextLine("Your milestones", x, y, w, 26 * scale, COL_TEXT) + 12;
    y += Milestones(all, x, y, w, scale);
    y += TextLine("Choose your adventure", x, y, w, 28 * scale, COL_TEXT) + 16;

    // Stack cards when large text would make two columns cramped.
    bool columns = w / scale >= 720;
    float cardW = columns ? (w - 20) / 2 : w;
    float trackY = y, maxH = 0;
    for (int t = 0; t < TRACK_COUNT; t++) {
        DashboardStats stats = DashboardCalculate(set, (Track)t);
        bool clicked;
        float h = TrackSummary(stats, t == TRACK_C ? "C Programming" : "C++ Programming",
                               x + (columns ? t * (cardW + 20) : 0), y, cardW, scale, &clicked);
        if (clicked) {
            if (stats.next >= 0) selected = stats.next;
            else for (int i = 0; i < set->count; i++)
                if (set->items[i].track == (Track)t) { selected = i; break; }
        }
        if (h > maxH) maxH = h;
        if (!columns) y += h + 14;
    }
    if (columns) y = trackY + maxH + 24;
    y += TextLine("Quiz performance", x, y, w, 28 * scale, COL_TEXT) + 12;
    y += TextLine("Best saved scores only. A quiz passes at 70%; attempt history is not recorded.", x, y, w, 18 * scale, COL_DIM) + 12;
    y += TextLine(TextFormat("Passed: %d / %d  |  Attempted, below pass: %d  |  Not attempted: %d",
                  all.passed, all.quizzes, all.attempted - all.passed, all.quizzes - all.attempted),
                  x, y, w, 20 * scale, COL_TEXT) + 10;
    y += TextLine(TextFormat("Quiz lessons not yet passed: %d", all.quizzes - all.passed), x, y, w, 20 * scale, COL_TEXT) + 10;
    if (all.attempted)
        y += TextLine(TextFormat("Average best score: %.1f%%  |  Highest best score: %d%%", all.average, all.highest),
                      x, y, w, 20 * scale, COL_ACCENT) + 24;
    else y += TextLine("Average and highest score: no quizzes attempted yet.", x, y, w, 20 * scale, COL_DIM) + 24;

    y += TextLine("Recommended focus", x, y, w, 28 * scale, COL_TEXT) + 12;
    for (int t = 0; t < TRACK_COUNT; t++) {
        DashboardStats stats = DashboardCalculate(set, (Track)t);
        if (stats.next < 0) {
            y += TextLine(TextFormat("%s: %s", TrackName((Track)t), stats.lessons ? "All lessons complete!" : "No lessons available."),
                          x, y, w, 19 * scale, COL_GOOD) + 12;
            continue;
        }
        const Lesson *lesson = &set->items[stats.next];
        int score = ProgressBest(lesson->id);
        const char *label = score < 0 ? TextFormat("%s | Recommended next - not attempted", TrackName((Track)t))
            : TextFormat("%s | Recommended next - review best score: %d%%", TrackName((Track)t), score);
        bool clicked;
        y += LessonRow(lesson, label, x, y, w, scale, &clicked);
        if (clicked) selected = stats.next;
    }
    int revisionCount = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        int score = ProgressBest(lesson->id);
        if (lesson->questionCount == 0 || score < 0 || score >= PASS_PERCENT) continue;
        if (!revisionCount++)
            y += TextLine("Revision based on recorded quiz scores", x, y + 8, w, 21 * scale, COL_ACCENT) + 20;
        bool clicked;
        y += LessonRow(lesson, TextFormat("%s | Best quiz: %d%% - below the pass threshold", TrackName(lesson->track), score),
                       x, y, w, scale, &clicked);
        if (clicked) selected = i;
    }
    if (!revisionCount)
        y += TextLine("No recorded quiz scores below the pass threshold. Unattempted lessons appear in the topic list.",
                      x, y, w, 18 * scale, COL_DIM) + 16;
    y += 16;
    y += TextLine("Topic performance", x, y, w, 28 * scale, COL_TEXT) + 12;
    y += TextLine("Select any lesson to study, retry its quiz, or practise its exercises.", x, y, w, 18 * scale, COL_DIM) + 16;
    const char *tracks[] = { "All tracks", "C", "C++" };
    Track values[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    float chipW = (w - 20) / 3;
    for (int i = 0; i < 3; i++)
        if (FilterChip((Rectangle){ x + i * (chipW + 10), y, chipW, 42 }, tracks[i], topicTrack == values[i])) topicTrack = values[i];
    y += 54;
    const char *filters[] = { "All lessons", "To review", "Unattempted", "Completed" };
    chipW = (w - 30) / 4;
    for (int i = 0; i < 4; i++)
        if (FilterChip((Rectangle){ x + i * (chipW + 10), y, chipW, 42 }, filters[i], topicFilter == i)) topicFilter = i;
    y += 60;
    int shown = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        int score = ProgressBest(lesson->id);
        if (topicTrack != TRACK_COUNT && lesson->track != topicTrack) continue;
        if (topicFilter == 1 && (!lesson->questionCount || score < 0 || score >= PASS_PERCENT)) continue;
        if (topicFilter == 2 && score >= 0) continue;
        if (topicFilter == 3 && score < PASS_PERCENT) continue;
        shown++;
        const char *status = score < 0 ? "Not attempted" : score >= PASS_PERCENT ? "Passed" : "Needs revision";
        const char *label;
        if (!lesson->questionCount)
            label = TextFormat("%s | No quiz | %s", TrackName(lesson->track), score >= PASS_PERCENT ? "Marked complete" : "Not completed");
        else if (score < 0) label = TextFormat("%s | %s", TrackName(lesson->track), status);
        else label = TextFormat("%s | Best quiz: %d%% | %s", TrackName(lesson->track), score, status);
        bool clicked;
        y += LessonRow(lesson, label, x, y, w, scale, &clicked);
        if (clicked) selected = i;
    }
    if (!shown) y += TextLine("No lessons match this view. Try another filter or track.", x, y, w, 20 * scale, COL_DIM) + 16;
    *height = y - top + 32;
    return selected;
}
