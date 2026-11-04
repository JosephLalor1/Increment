#include "dashboard.h"
#include "dashboard_stats.h"
#include "layout.h"
#include "journey_view.h"
#include "mascot.h"
#include "progress.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Building blocks: text lines, cards and rows shared by the sections below.

// Wrapped text in the regular font; returns its height.
static float TextLine(const char *text, float x, float y, float w, float size, Color color)
{
    return UiWrappedText(gFonts.text, text, (Vector2){ x, y }, w, size, color, true);
}

// A track's card: completion, progress bar and a continue/revisit prompt. Sets *clicked.
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

// The big card at the top: a completion ring, encouragement and a "Continue learning"
// button for the first unfinished lesson (sets *selected when pressed).
static float JourneyCard(const LessonSet *set, DashboardStats stats, float x, float y, float w, float scale, int *selected)
{
    float ringSize = 116 * scale;
    float foxSize = MascotAvailable() ? 112 * scale : 0;
    float textX = x + ringSize + 52, textW = w - ringSize - 76 - (foxSize ? foxSize + 16 : 0);
    const char *title = stats.lessons && stats.next < 0 ? "Amazing work! Keep going!"
                      : stats.completed > 0 ? "You're making great progress!" : "Ready to learn something new?";
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
    if (foxSize) MascotDrawActor(FOX_DASHBOARD, (Rectangle){ x + w - foxSize - 16, y + (h - foxSize) / 2, foxSize, foxSize },
                                stats.completed > 0 ? FOX_LOVE_LEARNING : FOX_HAPPY);
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

// Four badges derived from progress: first lesson, five lessons, first exercise, all lessons.
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

// A toggle chip, filled with the accent colour when active.
static bool FilterChip(Rectangle r, const char *label, bool active)
{
    return UiButtonEx(r, label, 18, active ? COL_ACCENT : COL_PANEL,
                      active ? COL_ACCENT_HI : COL_SECONDARY_HOVER, active ? COL_ON_ACCENT : COL_TEXT, true);
}

// A clickable lesson row: status icon, title, a label line and a score bar. Sets *clicked.
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
    int score = ProgressBest(lesson->id);  // -1 when never attempted
    bool passed = score >= PASS_PERCENT;
    Icon status = passed ? ICON_CHECK : score >= 0 ? ICON_RETRY : ICON_BOOK;  // done / try again / not started
    IconDraw(status, (Vector2){ x + 32, y + 28 }, 24, passed ? COL_GOOD : COL_ACCENT);
    TextLine(lesson->title, x + 60, y + 14, w - 96, size, COL_TEXT);
    TextLine(label, x + 20, y + titleH + 22, w - 40, 17 * scale, COL_DIM);
    float fraction = score < 0 ? 0 : (float)score / 100;
    ProgressBar((Rectangle){ x + 20, y + h - 20, w - 40, 6 }, fraction, COL_BG, passed ? COL_GOOD : COL_ACCENT);
    *clicked = over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return h + 12;
}

// ---------------------------------------------------------------------------
// Page sections, top to bottom. Each draws at y and returns the height it used.

// Title, the overall journey card, the exercise count and a status message.
// Sets *empty when there are no lessons at all (the rest of the page is skipped).
static float OverviewSection(const LessonSet *set, DashboardStats all, float x, float y, float w, float scale,
                             int *selected, bool *empty)
{
    float top = y;
    y += TextLine("Your learning adventure", x, y, w, 34 * scale, COL_TEXT) + 12;
    y += TextLine("Small steps. Real progress. Choose your next challenge.", x, y, w, 19 * scale, COL_DIM) + 20;
    y += JourneyCard(set, all, x, y, w, scale, selected);
    y += TextLine(TextFormat("Coding exercises: %d / %d completed", all.exercisesCompleted, all.exercises),
                  x, y, w, 20 * scale, COL_TEXT) + 24;

    *empty = all.lessons == 0;
    if (*empty) {
        y += TextLine("No lessons available yet. Add lesson files to assets/lessons to get started.", x, y, w,
                      21 * scale, COL_DIM);
    } else if (!all.completed && !all.attempted && !all.exercisesCompleted) {
        y += TextLine("Your journey starts here. Choose a recommended lesson below; your progress will appear as you practise.",
                      x, y, w, 19 * scale, COL_ACCENT) + 24;
    } else if (all.completed == all.lessons) {
        y += TextLine("Congratulations! Every lesson is complete. Revisit any topic to keep your skills sharp.",
                      x, y, w, 21 * scale, COL_GOOD) + 24;
    }
    return y - top;
}

static float MilestoneSection(DashboardStats all, float x, float y, float w, float scale)
{
    float top = y;
    y += TextLine("Your milestones", x, y, w, 26 * scale, COL_TEXT) + 12;
    y += Milestones(all, x, y, w, scale);
    return y - top;
}

// One card per track; clicking opens its first unfinished lesson (or its first lesson
// when the track is complete).
static float TrackSection(const LessonSet *set, float x, float y, float w, float scale, int *selected)
{
    float top = y;
    y += TextLine("Choose your adventure", x, y, w, 28 * scale, COL_TEXT) + 16;

    bool twoColumns = w / scale >= 720;  // stack the cards when large text would cramp them
    float cardW = twoColumns ? (w - 20) / 2 : w;
    float rowTop = y, tallest = 0;
    for (int t = 0; t < TRACK_COUNT; t++) {
        DashboardStats stats = DashboardCalculate(set, (Track)t);
        bool clicked;
        float cardX = x + (twoColumns ? t * (cardW + 20) : 0);
        float h = TrackSummary(stats, t == TRACK_C ? "C Programming" : "C++ Programming", cardX, y, cardW, scale,
                               &clicked);
        if (clicked) {
            if (stats.next >= 0) {
                *selected = stats.next;
            } else {
                for (int i = 0; i < set->count; i++) {
                    if (set->items[i].track == (Track)t) {
                        *selected = i;
                        break;
                    }
                }
            }
        }
        if (h > tallest) tallest = h;
        if (!twoColumns) y += h + 14;
    }
    if (twoColumns) y = rowTop + tallest + 24;
    return y - top;
}

static float QuizSection(DashboardStats all, float x, float y, float w, float scale)
{
    float top = y;
    y += TextLine("Quiz performance", x, y, w, 28 * scale, COL_TEXT) + 12;
    y += TextLine("Best saved scores only. A quiz passes at 70%; attempt history is not recorded.", x, y, w,
                  18 * scale, COL_DIM) + 12;
    y += TextLine(TextFormat("Passed: %d / %d  |  Attempted, below pass: %d  |  Not attempted: %d", all.passed,
                             all.quizzes, all.attempted - all.passed, all.quizzes - all.attempted),
                  x, y, w, 20 * scale, COL_TEXT) + 10;
    y += TextLine(TextFormat("Quiz lessons not yet passed: %d", all.quizzes - all.passed), x, y, w, 20 * scale,
                  COL_TEXT) + 10;
    if (all.attempted) {
        y += TextLine(TextFormat("Average best score: %.1f%%  |  Highest best score: %d%%", all.average, all.highest),
                      x, y, w, 20 * scale, COL_ACCENT) + 24;
    } else {
        y += TextLine("Average and highest score: no quizzes attempted yet.", x, y, w, 20 * scale, COL_DIM) + 24;
    }
    return y - top;
}

// The next lesson in each track, then every attempted quiz still below the pass mark.
static float RecommendationSection(const LessonSet *set, float x, float y, float w, float scale, int *selected)
{
    float top = y;
    y += TextLine("Recommended focus", x, y, w, 28 * scale, COL_TEXT) + 12;
    for (int t = 0; t < TRACK_COUNT; t++) {
        const char *track = TrackName((Track)t);
        DashboardStats stats = DashboardCalculate(set, (Track)t);
        if (stats.next < 0) {
            y += TextLine(TextFormat("%s: %s", track, stats.lessons ? "All lessons complete!" : "No lessons available."),
                          x, y, w, 19 * scale, COL_GOOD) + 12;
            continue;
        }
        const Lesson *lesson = &set->items[stats.next];
        int score = ProgressBest(lesson->id);
        const char *label = score < 0
            ? TextFormat("%s | Recommended next - not attempted", track)
            : TextFormat("%s | Recommended next - review best score: %d%%", track, score);
        bool clicked;
        y += LessonRow(lesson, label, x, y, w, scale, &clicked);
        if (clicked) *selected = stats.next;
    }

    int revisionCount = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        int score = ProgressBest(lesson->id);
        bool needsRevision = lesson->questionCount > 0 && score >= 0 && score < PASS_PERCENT;
        if (!needsRevision) continue;
        if (revisionCount++ == 0) {
            y += TextLine("Revision based on recorded quiz scores", x, y + 8, w, 21 * scale, COL_ACCENT) + 20;
        }
        bool clicked;
        y += LessonRow(lesson, TextFormat("%s | Best quiz: %d%% - below the pass threshold", TrackName(lesson->track), score),
                       x, y, w, scale, &clicked);
        if (clicked) *selected = i;
    }
    if (revisionCount == 0) {
        y += TextLine("No recorded quiz scores below the pass threshold. Unattempted lessons appear in the topic list.",
                      x, y, w, 18 * scale, COL_DIM) + 16;
    }
    return y - top + 16;
}

// Which lessons the topic list shows, besides the track filter.
typedef enum { TOPICS_ALL, TOPICS_TO_REVIEW, TOPICS_UNATTEMPTED, TOPICS_COMPLETED, TOPIC_FILTER_COUNT } TopicFilter;

static bool TopicShown(const Lesson *lesson, Track track, TopicFilter filter)
{
    int score = ProgressBest(lesson->id);
    if (track != TRACK_COUNT && lesson->track != track) return false;
    switch (filter) {
    case TOPICS_TO_REVIEW:   return lesson->questionCount > 0 && score >= 0 && score < PASS_PERCENT;
    case TOPICS_UNATTEMPTED: return score < 0;
    case TOPICS_COMPLETED:   return score >= PASS_PERCENT;
    default:                 return true;
    }
}

// "C | Best quiz: 80% | Passed" and the like, for one topic row.
static const char *TopicLabel(const Lesson *lesson)
{
    const char *track = TrackName(lesson->track);
    int score = ProgressBest(lesson->id);
    if (!lesson->questionCount) {
        return TextFormat("%s | No quiz | %s", track, score >= PASS_PERCENT ? "Marked complete" : "Not completed");
    }
    const char *status = score < 0 ? "Not attempted" : score >= PASS_PERCENT ? "Passed" : "Needs revision";
    if (score < 0) return TextFormat("%s | %s", track, status);
    return TextFormat("%s | Best quiz: %d%% | %s", track, score, status);
}

static struct {
    Track track;         // TRACK_COUNT for all tracks
    TopicFilter filter;
} topics = { TRACK_COUNT, TOPICS_ALL };

// Track and status filter chips, then every lesson that passes them.
static float TopicSection(const LessonSet *set, float x, float y, float w, float scale, int *selected)
{
    float top = y;
    y += TextLine("Topic performance", x, y, w, 28 * scale, COL_TEXT) + 12;
    y += TextLine("Select any lesson to study, retry its quiz, or practise its exercises.", x, y, w, 18 * scale,
                  COL_DIM) + 16;

    static const char *trackLabels[] = { "All tracks", "C", "C++" };
    static const Track trackValues[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    float chipW = (w - 20) / 3;
    for (int i = 0; i < 3; i++) {
        Rectangle chip = { x + i * (chipW + 10), y, chipW, 42 };
        if (FilterChip(chip, trackLabels[i], topics.track == trackValues[i])) topics.track = trackValues[i];
    }
    y += 54;

    static const char *filterLabels[TOPIC_FILTER_COUNT] = { "All lessons", "To review", "Unattempted", "Completed" };
    chipW = (w - 30) / 4;
    for (int i = 0; i < TOPIC_FILTER_COUNT; i++) {
        Rectangle chip = { x + i * (chipW + 10), y, chipW, 42 };
        if (FilterChip(chip, filterLabels[i], topics.filter == (TopicFilter)i)) topics.filter = (TopicFilter)i;
    }
    y += 60;

    int shown = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (!TopicShown(lesson, topics.track, topics.filter)) continue;
        shown++;
        bool clicked;
        y += LessonRow(lesson, TopicLabel(lesson), x, y, w, scale, &clicked);
        if (clicked) *selected = i;
    }
    if (shown == 0) {
        y += TextLine("No lessons match this view. Try another filter or track.", x, y, w, 20 * scale, COL_DIM) + 16;
    }
    return y - top;
}

// The whole page, starting at y. Returns the clicked lesson index or -1; *height is the
// page's height, for scrolling.
static int DrawPage(const LessonSet *set, float x, float y, float w, float scale, float *height)
{
    float top = y;
    int selected = -1;
    DashboardStats all = DashboardCalculate(set, TRACK_COUNT);

    bool empty;
    y += OverviewSection(set, all, x, y, w, scale, &selected, &empty);
    if (empty) {
        *height = y - top + 40;
        return -1;
    }
    y += MilestoneSection(all, x, y, w, scale);
    y += TrackSection(set, x, y, w, scale, &selected);
    y += QuizSection(all, x, y, w, scale);
    y += RecommendationSection(set, x, y, w, scale, &selected);
    y += TopicSection(set, x, y, w, scale, &selected);
    *height = y - top + 32;
    return selected;
}

// ---------------------------------------------------------------------------
// The screen

static struct {
    float scroll;
    float contentHeight;  // measured last frame, for clamping the scroll
    bool statistics;
    Track journeyTrack;
    float journeyScroll[TRACK_COUNT];
    float journeyTarget[TRACK_COUNT];
    float journeyHeight[TRACK_COUNT];
} page;

void DashboardOpen(void)
{
    page.scroll = 0;
    page.contentHeight = 0;
    page.statistics = false;
    page.journeyTrack = TRACK_C;
    for (int i = 0; i < TRACK_COUNT; i++) {
        page.journeyScroll[i] = page.journeyTarget[i] = page.journeyHeight[i] = 0;
    }
    topics.track = TRACK_COUNT;
    topics.filter = TOPICS_ALL;
}

void DashboardDemo(float scroll)
{
    page.scroll = scroll;
    page.contentHeight = 100000;  // let the requested scroll survive the first frame's clamp
    page.journeyScroll[page.journeyTrack] = page.journeyTarget[page.journeyTrack] = scroll;
    page.journeyHeight[page.journeyTrack] = 100000;
}

void DashboardDemoTrack(Track track, bool statistics)
{
    if (track >= 0 && track < TRACK_COUNT) page.journeyTrack = track;
    page.statistics = statistics;
}

int DashboardScreen(const LessonSet *set)
{
    if (TopBar(page.statistics ? "Learning Statistics" : "Your Learning Journey")) return PAGE_BACK;
    if (UiButtonIconEx((Rectangle){ GetScreenWidth() - 194.0f, 12, 178, 40 },
                       page.statistics ? ICON_BOOK : ICON_TROPHY, page.statistics ? "Journey" : "Statistics", 18,
                       COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) page.statistics = !page.statistics;

    if (!page.statistics) {
        float x = ContentX(), w = ContentWidth();
        for (int t = 0; t < TRACK_COUNT; t++) {
            bool active = page.journeyTrack == (Track)t;
            Rectangle tab = { x + t * (w / 2 + 6), TOPBAR_H + 12, w / 2 - 6, 42 };
            if (UiButtonEx(tab, t == TRACK_C ? "C Journey" : "C++ Journey", 20,
                           active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                           active ? COL_ON_ACCENT : COL_TEXT, true)) page.journeyTrack = (Track)t;
        }
        DashboardStats stats = DashboardCalculate(set, page.journeyTrack);
        UiText(gFonts.text, TextFormat("%s  |  %d / %d lessons complete  |  %d%%", TrackName(page.journeyTrack),
               stats.completed, stats.lessons, stats.lessons ? stats.completed * 100 / stats.lessons : 0),
               x, TOPBAR_H + 68, 18, COL_TEXT);
        ProgressBar((Rectangle){ x, TOPBAR_H + 98, w, 7 }, stats.lessons ? (float)stats.completed / stats.lessons : 0,
                     COL_PANEL_HI, COL_GOOD);
        Rectangle journeyView = ScrollView();
        journeyView.y = TOPBAR_H + 120;
        journeyView.height = GetScreenHeight() - journeyView.y;
        Track track = page.journeyTrack;
        if (UiButtonEx((Rectangle){ x + w - 174, TOPBAR_H + 61, 174, 30 }, "Find my place", 16,
                       COL_PANEL, COL_SECONDARY_HOVER, COL_ACCENT, true))
            page.journeyTarget[track] = JourneyViewFocus(set, track);
        UpdateScroll(&page.journeyTarget[track], page.journeyHeight[track], journeyView.height);
        float blend = GetFrameTime() * 16;
        if (blend > 1) blend = 1;
        page.journeyScroll[track] += (page.journeyTarget[track] - page.journeyScroll[track]) * blend;
        BeginView(journeyView);
        int clicked = JourneyViewDraw(set, track, x, journeyView.y + 8 - page.journeyScroll[track], w,
                                      &page.journeyHeight[track]);
        page.journeyHeight[track] += 8;
        EndView();
        return clicked >= 0 ? clicked : PAGE_STAY;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&page.scroll, page.contentHeight, view.height);
    BeginView(view);
    int clicked = DrawPage(set, ContentX(), view.y + 24 - page.scroll, ContentWidth(), UiTextScale(),
                           &page.contentHeight);
    page.contentHeight += 24;
    EndView();
    return clicked >= 0 ? clicked : PAGE_STAY;
}
