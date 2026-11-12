// The home screen: the learning journeys and statistics. See dashboard.h.
#include "dashboard.h"
#include "account.h"
#include "ambience.h"
#include "brand.h"
#include "mouse_trail.h"
#include "dashboard_stats.h"
#include "journey_view.h"
#include "layout.h"
#include "review_view.h"
#include "skills_view.h"
#include "mascot.h"
#include "progress.h"
#include "roadmap.h"
#include "ui.h"

#include <math.h>

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
    TextLine(TextFormat("%d / %d levels complete  |  %.0f%%", stats.completed, stats.lessons, p * 100),
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
    float bodyH = UiWrappedText(gFonts.text, TextFormat("%d of %d levels complete", stats.completed, stats.lessons),
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
    TextLine(TextFormat("%d of %d levels complete", stats.completed, stats.lessons), textX, y + 30 + titleH, textW, 19 * scale, COL_DIM);
    if (next >= 0) {
        float buttonY = y + titleH + bodyH + 46;
        if (UiButtonIcon((Rectangle){ textX, buttonY, textW, 46 * scale }, stats.next < 0 ? ICON_TROPHY : ICON_PLAY,
                         stats.next < 0 ? "Keep practising" : "Continue learning", true)) *selected = next;
        // The action always follows actual content order, without unlocking requirements.
        TextLine(nextTitle, textX, buttonY + 54 * scale, textW, 16 * scale, COL_DIM);
    }
    return h + 24;
}

// Four badges derived from progress: first step, first level, five levels, all levels.
static float Milestones(DashboardStats stats, float x, float y, float w, float scale)
{
    const char *names[] = { "First step", "First level", "Five levels", "All levels" };
    bool earned[] = { stats.stepsDone >= 1, stats.completed >= 1, stats.completed >= 5,
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

// A clickable level row: status icon, title, a label line and a bar of its passed steps. Sets *clicked.
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
    int done = RoadmapStepsDone(lesson);
    bool passed = RoadmapLevelComplete(lesson);
    Icon status = passed ? ICON_CHECK : done > 0 ? ICON_PLAY : ICON_CODE;  // done / under way / not started
    IconDraw(status, (Vector2){ x + 32, y + 28 }, 24, passed ? COL_GOOD : COL_ACCENT);
    TextLine(lesson->title, x + 60, y + 14, w - 96, size, COL_TEXT);
    TextLine(label, x + 20, y + titleH + 22, w - 40, 17 * scale, COL_DIM);
    float fraction = lesson->stepCount ? (float)done / lesson->stepCount : 0;
    ProgressBar((Rectangle){ x + 20, y + h - 20, w - 40, 6 }, fraction, COL_BG, passed ? COL_GOOD : COL_ACCENT);
    *clicked = over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return h + 12;
}

// ---------------------------------------------------------------------------
// Page sections, top to bottom. Each draws at y and returns the height it used.

// Title, the overall journey card, the step count and a status message.
// Sets *empty when there are no lessons at all (the rest of the page is skipped).
static float OverviewSection(const LessonSet *set, DashboardStats all, float x, float y, float w, float scale,
                             int *selected, bool *empty)
{
    float top = y;
    y += TextLine("Your learning adventure", x, y, w, 34 * scale, COL_TEXT) + 12;
    y += TextLine("Small steps. Real progress. Choose your next challenge.", x, y, w, 19 * scale, COL_DIM) + 20;
    y += JourneyCard(set, all, x, y, w, scale, selected);
    y += TextLine(TextFormat("Coding steps: %d / %d passed", all.stepsDone, all.steps), x, y, w, 20 * scale, COL_TEXT) + 24;

    *empty = all.lessons == 0;
    if (*empty) {
        y += TextLine("No levels available yet. Add lesson files to assets/lessons to get started.", x, y, w,
                      21 * scale, COL_DIM);
    } else if (!all.stepsDone) {
        y += TextLine("Your journey starts here. Pass your first coding step and your progress will appear.",
                      x, y, w, 19 * scale, COL_ACCENT) + 24;
    } else if (all.completed == all.lessons) {
        y += TextLine("Congratulations! Every level is complete. Replay any of them to keep your skills sharp.",
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

// The next level in each track, to carry on with.
static float RecommendationSection(const LessonSet *set, float x, float y, float w, float scale, int *selected)
{
    float top = y;
    y += TextLine("Recommended focus", x, y, w, 28 * scale, COL_TEXT) + 12;
    for (int t = 0; t < TRACK_COUNT; t++) {
        const char *track = TrackName((Track)t);
        DashboardStats stats = DashboardCalculate(set, (Track)t);
        if (stats.next < 0) {
            y += TextLine(TextFormat("%s: %s", track, stats.lessons ? "All levels complete!" : "No levels available."),
                          x, y, w, 19 * scale, COL_GOOD) + 12;
            continue;
        }
        const Lesson *lesson = &set->items[stats.next];
        int done = RoadmapStepsDone(lesson);
        const char *label = done == 0
            ? TextFormat("%s | Recommended next - not started", track)
            : TextFormat("%s | Recommended next - %d / %d steps passed", track, done, lesson->stepCount);
        bool clicked;
        y += LessonRow(lesson, label, x, y, w, scale, &clicked);
        if (clicked) *selected = stats.next;
    }
    return y - top + 16;
}

// Gold mastery so far, then a tile for every lesson with a speed round: a gold star once
// it's won, a lightning bolt until then. Clicking a tile sets *speedRound to start it.
static float SpeedRoundSection(const LessonSet *set, DashboardStats all, float x, float y, float w, float scale,
                               int *speedRound)
{
    if (all.speedRounds == 0) return 0;
    float top = y;
    y += TextLine("Speed rounds", x, y, w, 28 * scale, COL_TEXT) + 12;
    y += TextLine("Ten generated \"what is x?\" questions against the clock, 10 seconds each. Win a level's round for "
                  "gold mastery.", x, y, w, 18 * scale, COL_DIM) + 12;
    y += TextLine(TextFormat("Gold mastery: %d / %d levels", all.gold, all.speedRounds), x, y, w, 20 * scale,
                  all.gold ? COL_GOLD : COL_TEXT) + 10;
    ProgressBar((Rectangle){ x, y, w, 7 }, (float)all.gold / all.speedRounds, COL_PANEL_HI, COL_GOLD);
    y += 24;

    int columns = w / scale >= 720 ? 3 : 2;
    float gap = 12, tileW = (w - gap * (columns - 1)) / columns, size = 18 * scale;
    float rowTop = y, rowH = 0;
    int column = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (!lesson->speedSkills[0]) continue;
        // Tiles in a row share the tallest one's height, so the grid stays even.
        if (column == 0) {
            rowH = 0;
            for (int j = i, n = 0; j < set->count && n < columns; j++) {
                if (!set->items[j].speedSkills[0]) continue;
                float h = UiWrappedText(gFonts.text, set->items[j].title, (Vector2){ 0, 0 }, tileW - 60, size, COL_TEXT, false);
                rowH = fmaxf(rowH, h + 46);
                n++;
            }
        }
        bool gold = ProgressGold(lesson->id);
        Rectangle tile = { x + column * (tileW + gap), rowTop, tileW, rowH };
        bool over = UiMouseOver(tile);
        if (over) UiWantHand();
        UiRoundRect(tile, 10, over ? COL_PANEL_HI : COL_PANEL);
        if (gold) DrawRectangleRoundedLinesEx(tile, 16 / fminf(tile.width, tile.height), 8, 2, COL_GOLD);
        IconDraw(gold ? ICON_STAR : ICON_BOLT, (Vector2){ tile.x + 26, tile.y + 26 }, 24, gold ? COL_GOLD : COL_ACCENT);
        UiWrappedText(gFonts.text, lesson->title, (Vector2){ tile.x + 48, tile.y + 12 }, tileW - 60, size, COL_TEXT, true);
        UiText(gFonts.text, TextFormat("%s  |  %s", TrackName(lesson->track), gold ? "Gold" : "Not won yet"),
               tile.x + 48, tile.y + rowH - 28, 15 * scale, gold ? COL_GOLD : COL_DIM);
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) *speedRound = i;
        if (++column == columns) {
            column = 0;
            rowTop += rowH + gap;
        }
    }
    if (column != 0) rowTop += rowH + gap;
    return rowTop - top + 16;
}

// Which levels the topic list shows, besides the track filter.
typedef enum { TOPICS_ALL, TOPICS_UNDER_WAY, TOPICS_NOT_STARTED, TOPICS_COMPLETED, TOPIC_FILTER_COUNT } TopicFilter;

static bool TopicShown(const Lesson *lesson, Track track, TopicFilter filter)
{
    if (track != TRACK_COUNT && lesson->track != track) return false;
    if (!RoadmapLevelReady(lesson)) return false;
    int done = RoadmapStepsDone(lesson);
    switch (filter) {
    case TOPICS_UNDER_WAY:   return done > 0 && done < lesson->stepCount;
    case TOPICS_NOT_STARTED: return done == 0;
    case TOPICS_COMPLETED:   return done == lesson->stepCount;
    default:                 return true;
    }
}

// "C | 3 / 10 steps passed | Under way" and the like, for one topic row.
static const char *TopicLabel(const Lesson *lesson)
{
    int done = RoadmapStepsDone(lesson);
    const char *status = done == 0 ? "Not started" : done == lesson->stepCount ? "Complete" : "Under way";
    return TextFormat("%s | %d / %d steps passed | %s", TrackName(lesson->track), done, lesson->stepCount, status);
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
    y += TextLine("Select any level to play it, or to replay its steps.", x, y, w, 18 * scale, COL_DIM) + 16;

    static const char *trackLabels[] = { "All tracks", "C", "C++" };
    static const Track trackValues[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    float chipW = (w - 20) / 3;
    for (int i = 0; i < 3; i++) {
        Rectangle chip = { x + i * (chipW + 10), y, chipW, 42 };
        if (FilterChip(chip, trackLabels[i], topics.track == trackValues[i])) topics.track = trackValues[i];
    }
    y += 54;

    static const char *filterLabels[TOPIC_FILTER_COUNT] = { "All levels", "Under way", "Not started", "Completed" };
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
        y += TextLine("No levels match this view. Try another filter or track.", x, y, w, 20 * scale, COL_DIM) + 16;
    }
    return y - top;
}

// The statistics page, starting at y; *height is its height, for scrolling. Returns a lesson to open, or -1; sets *speedRound to a lesson whose
// speed round should start, and *challenge when a short challenge should.
static int DrawPage(const LessonSet *set, float x, float y, float w, float scale, float *height, int *speedRound,
                    ReviewView *questions, bool *challenge)
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
    y += SpeedRoundSection(set, all, x, y, w, scale, speedRound);
    bool practiseSkills;
    y += SkillsViewDraw(x, y, w, scale, &practiseSkills);
    bool reviewChallenge;
    y += ReviewViewDraw(questions, x, y, w, scale, &reviewChallenge);
    *challenge = reviewChallenge || practiseSkills;  // challenges favour the weakest skills
    y += RecommendationSection(set, x, y, w, scale, &selected);
    y += TopicSection(set, x, y, w, scale, &selected);
    *height = y - top + 32;
    return selected;
}

// ---------------------------------------------------------------------------
// The screen

static struct {
    float scroll;                      // statistics view
    float contentHeight;               // measured last frame, for clamping the scroll
    HomeView view;                     // what shows below the home bar
    Track journeyTrack;                // the journey tab that's open
    float journeyScroll[TRACK_COUNT];  // where each journey is drawn (eases towards the target)
    float journeyTarget[TRACK_COUNT];  // where each journey is scrolling to
    float journeyHeight[TRACK_COUNT];
    ReviewView questions;              // the statistics' "Your speed round questions" section
} page;

void DashboardOpen(void)
{
    page.scroll = 0;
    page.contentHeight = 0;
    page.view = HOME_VIEW_JOURNEY;
    page.journeyTrack = TRACK_C;
    for (int i = 0; i < TRACK_COUNT; i++) {
        page.journeyScroll[i] = page.journeyTarget[i] = page.journeyHeight[i] = 0;
    }
    topics.track = TRACK_COUNT;
    topics.filter = TOPICS_ALL;
}

void DashboardShow(HomeView view, Track track)
{
    if (track >= 0 && track < TRACK_COUNT) page.journeyTrack = track;
    if (view >= 0 && view < HOME_VIEW_COUNT) page.view = view;
}

Vector2 DashboardLevelPoint(int lessonIndex)
{
    Vector2 point = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    if (page.view == HOME_VIEW_JOURNEY) JourneyViewStationPoint(lessonIndex, &point);
    return point;
}

void DashboardDemo(float scroll)
{
    page.scroll = scroll;
    page.contentHeight = 100000;  // let the requested scroll survive the first frame's clamp
    page.journeyScroll[page.journeyTrack] = page.journeyTarget[page.journeyTrack] = scroll;
    page.journeyHeight[page.journeyTrack] = 100000;
}

// An icon button in the home bar's style.
static bool BarButton(Rectangle r, Icon icon, const char *label)
{
    return UiButtonIconEx(r, icon, label, 18, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true);
}

// The Journey / Statistics switch: one pill with the open view in the accent
// colour, the bar's one highlighted button. right is its
// right edge; returns its left edge.
#define VIEW_SWITCH_W 256.0f  // the segment widths below plus the inset on both sides
static float ViewSwitch(float right, float y, float h)
{
    static const Icon icons[HOME_VIEW_COUNT] = { ICON_BOOK, ICON_TROPHY };
    static const char *labels[HOME_VIEW_COUNT] = { "Journey", "Statistics" };
    static const float widths[HOME_VIEW_COUNT] = { 116, 132 };
    const float inset = 4;
    float left = right - VIEW_SWITCH_W;
    UiRoundRect((Rectangle){ left, y, VIEW_SWITCH_W, h }, 8, COL_BG);

    float x = left + inset;
    for (int i = 0; i < HOME_VIEW_COUNT; i++) {
        bool open = page.view == (HomeView)i;
        Rectangle segment = { x, y + inset, widths[i], h - inset * 2 };
        if (UiButtonIconEx(segment, icons[i], labels[i], 17, open ? COL_ACCENT : COL_BG,
                           open ? COL_ACCENT_HI : COL_SECONDARY_HOVER, open ? COL_ON_ACCENT : COL_DIM, true)) {
            page.view = (HomeView)i;
        }
        x += widths[i];
    }
    return left;
}

// The bar across the top: the logo, then (from the right) Quit, Log out, Settings, the
// code editor, the view switch (lit in the accent colour, so the journey is the focus),
// and who is signed in when there's room. In a narrow
// window Log out, Settings and the code editor shrink to their icons.
static HomeChoice HomeBar(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), (int)TOPBAR_H, COL_PANEL);
    DrawRectangle(0, (int)TOPBAR_H - 1, GetScreenWidth(), 1, COL_PANEL_HI);
    BrandDrawLogo((Vector2){ 20, 14 }, 36, COL_BRAND, WHITE, LOGO_STATIC, false);
    UiText(gFonts.bold, "Increment", 66, 17, 26, COL_TEXT);
    float wordmarkRight = 66 + UiTextWidth(gFonts.bold, "Increment", 26);

    HomeChoice choice = HOME_STAY;
    float x = GetScreenWidth() - 16.0f, y = 12, h = 40, gap = 10;
    const float iconOnly = 44;
    float fullWidth = iconOnly + 120 + 120 + VIEW_SWITCH_W + 170 + gap * 4;
    bool compact = x - fullWidth < wordmarkRight + 24;
    float logOutW = compact ? iconOnly : 120, settingsW = compact ? iconOnly : 120, codeW = compact ? iconOnly : 170;

    x -= iconOnly;
    if (BarButton((Rectangle){ x, y, iconOnly, h }, ICON_POWER, NULL)) choice = HOME_QUIT;
    x -= gap + logOutW;
    if (BarButton((Rectangle){ x, y, logOutW, h }, ICON_LOGOUT, compact ? NULL : "Log out")) choice = HOME_LOG_OUT;
    x -= gap + settingsW;
    if (BarButton((Rectangle){ x, y, settingsW, h }, ICON_GEAR, compact ? NULL : "Settings")) choice = HOME_SETTINGS;
    x -= gap + codeW;
    if (BarButton((Rectangle){ x, y, codeW, h }, ICON_CODE, compact ? NULL : "Code editor")) choice = HOME_CODE_EDITOR;
    x = ViewSwitch(x - gap, y, h);

    const char *who = AccountCurrent();
    if (who) {
        const char *label = TextFormat("Signed in as %s", who);
        float labelW = 18 * 1.4f + UiTextWidth(gFonts.text, label, 18);  // icon + gap + text
        float labelX = x - 18 - labelW;
        if (labelX > wordmarkRight + 24) UiIconText(ICON_USER, gFonts.text, label, labelX, 22, 18, COL_DIM);
    }
    return choice;
}

// For accounts made before recovery codes existed: a reminder with a link to Settings.
// Returns the y below it (unchanged when not shown); sets *openSettings when clicked.
static float RecoveryReminder(float x, float y, bool *openSettings)
{
    if (!AccountCurrent() || AccountHasRecovery()) return y;
    const char *message = "You don't have a recovery code yet.";
    const char *link = "Make one in Settings";
    float messageW = 18 * 1.4f + UiTextWidth(gFonts.text, message, 18);
    UiIconText(ICON_KEY, gFonts.text, message, x, y, 18, COL_DIM);
    if (UiLink(x + messageW + 8, y, link, 18)) *openSettings = true;
    return y + 30;
}

// The journey's tabs and progress take this much room above its scrolling path.
#define JOURNEY_HEADER_H 108

// Behind the whole journey page under the home bar (the buttons along its top included):
// the learner's background code and the mouse trail. The trail is drawn here only in the
// strip above the path; over the path it is drawn after the stations (see JourneyView).
static void JourneyBackdrop(float pathTop)
{
    Rectangle page = ScrollView();
    AmbienceUpdate(page, GetFrameTime());
    MouseTrailUpdate(GetMousePosition(), GetFrameTime(), UiMouseOver(page));
    BeginView(page);
    AmbienceDraw();
    EndView();
    BeginView((Rectangle){ page.x, page.y, page.width, pathTop - page.y });
    MouseTrailDraw();
    EndView();
}

// The C / C++ tabs, the track's progress and its winding path. Returns a clicked lesson or -1,
// and sets *clickedAt to the middle of its station.
static int JourneyView(const LessonSet *set, float top, Vector2 *clickedAt)
{
    float x = ContentX(), w = ContentWidth();
    for (int t = 0; t < TRACK_COUNT; t++) {
        bool active = page.journeyTrack == (Track)t;
        Rectangle tab = { x + t * (w / 2 + 6), top, w / 2 - 6, 42 };
        if (UiButtonEx(tab, t == TRACK_C ? "C Journey" : "C++ Journey", 20, active ? COL_ACCENT : COL_PANEL,
                       active ? COL_ACCENT_HI : COL_SECONDARY_HOVER, active ? COL_ON_ACCENT : COL_TEXT, true)) {
            page.journeyTrack = (Track)t;
        }
    }
    Track track = page.journeyTrack;
    DashboardStats stats = DashboardCalculate(set, track);
    int percent = stats.lessons ? stats.completed * 100 / stats.lessons : 0;
    UiText(gFonts.text, TextFormat("%s  |  %d / %d levels complete  |  %d%%", TrackName(track), stats.completed,
                                   stats.lessons, percent),
           x, top + 56, 18, COL_TEXT);
    ProgressBar((Rectangle){ x, top + 86, w, 7 }, stats.lessons ? (float)stats.completed / stats.lessons : 0,
                COL_PANEL_HI, COL_ACCENT);
    if (UiButtonEx((Rectangle){ x + w - 174, top + 49, 174, 30 }, "Find my place", 16, COL_PANEL, COL_SECONDARY_HOVER,
                   COL_ACCENT, true)) {
        page.journeyTarget[track] = JourneyViewFocus(set, track);
    }

    Rectangle view = ScrollView();
    view.y = top + JOURNEY_HEADER_H;
    view.height = GetScreenHeight() - view.y;
    UpdateScroll(&page.journeyTarget[track], page.journeyHeight[track], view.height);
    float blend = GetFrameTime() * 16;  // ease towards the target, so "Find my place" glides
    if (blend > 1) blend = 1;
    page.journeyScroll[track] += (page.journeyTarget[track] - page.journeyScroll[track]) * blend;
    // The background code was drawn behind the whole page (JourneyBackdrop); the mouse trail
    // goes over the stations.
    BeginView(view);
    int clicked = JourneyViewDraw(set, track, x, view.y + 8 - page.journeyScroll[track], w, &page.journeyHeight[track],
                                  clickedAt);
    page.journeyHeight[track] += 8;
    MouseTrailDraw();
    EndView();
    // Dragging the bar moves the journey directly, without the easing.
    if (PageScrollBar(view, &page.journeyTarget[track], page.journeyHeight[track]) && UiScrollBarDragging())
        page.journeyScroll[track] = page.journeyTarget[track];
    return clicked;
}

// The statistics page, scrolling below top. Returns a clicked lesson or -1, and sets
// *speedRound to a lesson whose speed round was clicked and *challenge for a short challenge.
static int StatisticsView(const LessonSet *set, float top, int *speedRound, bool *challenge)
{
    Rectangle view = ScrollView();
    view.height -= top - view.y;
    view.y = top;
    UpdateScroll(&page.scroll, page.contentHeight, view.height);
    BeginView(view);
    int clicked = DrawPage(set, ContentX(), view.y + 12 - page.scroll, ContentWidth(), UiTextScale(),
                           &page.contentHeight, speedRound, &page.questions, challenge);
    page.contentHeight += 24;
    EndView();
    PageScrollBar(view, &page.scroll, page.contentHeight);
    return clicked;
}

HomeAction DashboardScreen(const LessonSet *set)
{
    HomeAction action = { HOME_STAY, -1, GetMousePosition() };  // a level's row is clicked at the mouse
    action.kind = HomeBar();
    if (IsKeyPressed(KEY_ESCAPE)) page.view = HOME_VIEW_JOURNEY;

    // The row under the home bar: the recovery code reminder if there is one, then a short
    // challenge and the rankings on the right.
    float top = TOPBAR_H + 14;
    float right = ContentX() + ContentWidth();
    // The journey's background code and mouse trail fill the page, round its buttons too.
    if (page.view == HOME_VIEW_JOURNEY) JourneyBackdrop(top + 42 + 14 + JOURNEY_HEADER_H);
    if (BarButton((Rectangle){ right - 276, top, 132, 42 }, ICON_BOLT, "Challenge")) action.kind = HOME_CHALLENGE;
    if (BarButton((Rectangle){ right - 132, top, 132, 42 }, ICON_TROPHY, "Rankings")) action.kind = HOME_RANKINGS;
    bool openSettings = false;
    RecoveryReminder(ContentX(), top + 10, &openSettings);
    if (openSettings) action.kind = HOME_SETTINGS;
    top += 42 + 14;

    int clicked;
    if (page.view == HOME_VIEW_STATISTICS) {
        int speedRound = -1;
        bool challenge = false;
        clicked = StatisticsView(set, top, &speedRound, &challenge);
        if (challenge && action.kind == HOME_STAY) action.kind = HOME_CHALLENGE;
        if (speedRound >= 0 && action.kind == HOME_STAY) {
            action.kind = HOME_SPEED_ROUND;
            action.lessonIndex = speedRound;
        }
    } else {
        clicked = JourneyView(set, top, &action.clickedAt);
    }

    if (clicked >= 0 && action.kind == HOME_STAY) {
        action.kind = HOME_OPEN_LESSON;
        action.lessonIndex = clicked;
    }
    return action;
}
