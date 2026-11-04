// Traditional sequential course lists and lesson search. See track_screen.h.
#include "track_screen.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

static struct {
    Track track;
    float listScroll;
    float listHeight;         // measured last frame, for clamping the scroll

    char query[SEARCH_QUERY_MAX + 1];      // the search box (also its focus identity)
    char lastQuery[SEARCH_QUERY_MAX + 1];  // last frame's query, to reset scrolling when it changes
    SearchFilter filter;
    float resultsScroll;
    float resultsHeight;
} page;  // this screen's state

void TrackScreenOpen(Track t)
{
    page.track = t;
    page.listScroll = 0;
    page.query[0] = '\0';
}

void TrackScreenDemo(const char *query, SearchFilter filter, float scroll)
{
    if (query) {
        snprintf(page.query, sizeof page.query, "%s", query);
        UiFocus(page.query);
    }
    page.filter = filter;
    page.listScroll = scroll;
    page.listHeight = 100000;  // let the requested scroll survive the first frame's clamp
}

// "New" or "Best: 80%", the score note at the right of a lesson row.
static const char *ScoreText(const Lesson *lesson)
{
    int best = ProgressBest(lesson->id);
    return best < 0 ? "New" : TextFormat("Best: %d%%", best);
}

// ---------------------------------------------------------------------------
// Plain lesson list

// One row per lesson in the track: number (or a tick / practice target), title, score.
static int DrawLessonList(const LessonSet *set, float x, float y, float w)
{
    float top = y;
    int number = 0, clicked = -1;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != page.track) continue;
        number++;

        Rectangle row = { x, y, w, 72 };
        bool over = UiMouseOver(row);
        if (over) UiWantHand();
        UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);

        bool passed = ProgressLessonPassed(lesson->id);
        Vector2 badge = { x + 40, y + 36 };
        DrawCircleV(badge, 20, passed ? COL_GOOD_BG : COL_BG);
        if (passed) IconDraw(ICON_CHECK, badge, 22, COL_GOOD);
        else if (lesson->practice) IconDraw(ICON_TARGET, badge, 22, COL_ACCENT);
        else UiTextCentered(gFonts.mono, TextFormat("%d", number), x + 40, y + 25, 22, COL_DIM);

        UiText(gFonts.text, lesson->title, x + 80, y + 22, 24, COL_TEXT);
        const char *score = ScoreText(lesson);
        float scoreW = MeasureTextEx(gFonts.text, score, 18, 0).x;
        UiText(gFonts.text, score, x + w - scoreW - 24, y + 26, 18, passed ? COL_GOOD : COL_DIM);

        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = i;
        y += 84;
    }
    if (number == 0) UiText(gFonts.text, "No lessons in this track yet.", x, y, 22, COL_DIM);
    page.listHeight = (y - top) + 48;
    return clicked;
}

// The original sequential lesson list, scrolling in view. Returns a clicked lesson or -1.
static int DrawNavigation(const LessonSet *set, Rectangle view)
{
    UpdateScroll(&page.listScroll, page.listHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth();
    float y = view.y + 24 - page.listScroll;
    int clicked = DrawLessonList(set, x, y, w);
    EndView();
    return clicked;
}

// ---------------------------------------------------------------------------
// Search results

// The excerpt around the match on one line, with the matched word in the accent colour.
static void DrawExcerpt(const Lesson *lesson, float x, float y, float maxWidth)
{
    const float size = 18;
    char excerpt[256];
    int matchAt, matchLen;
    if (!SearchSnippet(lesson, page.query, excerpt, sizeof excerpt, &matchAt, &matchLen)) {
        UiText(gFonts.text, "Matches the lesson title", x, y, size, COL_DIM);
        return;
    }
    UiFitText(excerpt, sizeof excerpt, gFonts.text, size, maxWidth);
    int length = (int)strlen(excerpt);  // the match may have been cut off by UiFitText
    if (matchAt > length) matchAt = length;
    if (matchAt + matchLen > length) matchLen = length - matchAt;

    char part[256];
    snprintf(part, sizeof part, "%.*s", matchAt, excerpt);  // before the match
    UiText(gFonts.text, part, x, y, size, COL_DIM);
    x += UiTextWidth(gFonts.text, part, size);
    snprintf(part, sizeof part, "%.*s", matchLen, excerpt + matchAt);  // the match
    UiText(gFonts.text, part, x, y, size, COL_ACCENT);
    x += UiTextWidth(gFonts.text, part, size);
    UiText(gFonts.text, excerpt + matchAt + matchLen, x, y, size, COL_DIM);  // after it
}

// One result: track badge, title, score, and the excerpt. Returns true when clicked.
static bool DrawResultRow(const Lesson *lesson, float x, float y, float w)
{
    Rectangle row = { x, y, w, 92 };
    bool over = UiMouseOver(row);
    if (over) UiWantHand();
    UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);

    const char *trackName = TrackName(lesson->track);
    float badgeW = UiTextWidth(gFonts.bold, trackName, 17) + 22;
    UiRoundRect((Rectangle){ x + 20, y + 18, badgeW, 28 }, 14, COL_BG);
    UiTextCentered(gFonts.bold, trackName, x + 20 + badgeW / 2, y + 22, 17, COL_ACCENT);

    bool passed = ProgressLessonPassed(lesson->id);
    const char *score = ScoreText(lesson);
    float scoreW = UiTextWidth(gFonts.text, score, 18);
    UiText(gFonts.text, score, x + w - scoreW - 24, y + 22, 18, passed ? COL_GOOD : COL_DIM);
    if (passed) IconDraw(ICON_CHECK, (Vector2){ x + w - scoreW - 44, y + 32 }, 18, COL_GOOD);

    char title[128];
    snprintf(title, sizeof title, "%s", lesson->title);
    float titleX = x + 20 + badgeW + 14;
    UiFitText(title, sizeof title, gFonts.text, 24, x + w - scoreW - 64 - titleX);
    UiText(gFonts.text, title, titleX, y + 16, 24, COL_TEXT);

    DrawExcerpt(lesson, x + 20, y + 58, w - 40);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// "C and C++" / "C" / "C++" toggle buttons.
static void DrawFilterButtons(float x, float y)
{
    static const float widths[] = { 124, 56, 72 };  // indexed by SearchFilter
    for (int i = 0; i < 3; i++) {
        bool on = (int)page.filter == i;
        Rectangle button = { x, y, widths[i], 38 };
        if (UiButtonEx(button, SearchFilterName((SearchFilter)i), 18, on ? COL_ACCENT : COL_PANEL,
                       on ? COL_ACCENT_HI : COL_SECONDARY_HOVER, on ? COL_ON_ACCENT : COL_TEXT, true)) {
            page.filter = (SearchFilter)i;
            page.resultsScroll = 0;
        }
        x += widths[i] + 8;
    }
}

// The panel shown when nothing matches: what was searched, tips, and ways out.
// Returns its height.
static float DrawNoMatches(float x, float y, float w)
{
    const char *advice = "Check the spelling, or try fewer or shorter words: \"point\" finds pointer and pointers.";
    float adviceH = UiWrappedText(gFonts.text, advice, (Vector2){ 0, 0 }, w - 96, 19, COL_DIM, false);
    Rectangle panel = { x, y, w, 68 + adviceH + 64 };
    UiRoundRect(panel, 12, COL_PANEL);
    IconDraw(ICON_INFO, (Vector2){ x + 40, y + 42 }, 28, COL_ACCENT);

    char heading[160];
    if (page.filter == SEARCH_BOTH) snprintf(heading, sizeof heading, "No lessons match \"%s\".", page.query);
    else snprintf(heading, sizeof heading, "No %s lessons match \"%s\".", SearchFilterName(page.filter), page.query);
    UiFitText(heading, sizeof heading, gFonts.bold, 24, w - 96);
    UiText(gFonts.bold, heading, x + 72, y + 26, 24, COL_TEXT);
    UiWrappedText(gFonts.text, advice, (Vector2){ x + 72, y + 68 }, w - 96, 19, COL_DIM, true);

    float linkX = x + 72, linkY = y + 68 + adviceH + 18;
    if (page.filter != SEARCH_BOTH) {
        const char *widen = "Search C and C++";
        if (UiLink(linkX, linkY, widen, 19)) {
            page.filter = SEARCH_BOTH;
            page.resultsScroll = 0;
        }
        linkX += UiTextWidth(gFonts.text, widen, 19) + 32;
    }
    if (UiLink(linkX, linkY, "Clear search", 19)) {
        page.query[0] = '\0';
        UiFocus(page.query);
    }
    return panel.height;
}

// Search results in place of the course list. Enter (submitted) opens the top result.
// Returns the lesson to open, or -1.
static int DrawResults(const LessonSet *set, Rectangle view, bool submitted)
{
    static int found[1024];
    int foundCount = SearchLessons(set, page.query, page.filter, found, 1024);
    if (submitted && foundCount > 0) return found[0];

    UpdateScroll(&page.resultsScroll, page.resultsHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth();
    float top = view.y + 8 - page.resultsScroll, y = top;
    int clicked = -1;

    DrawFilterButtons(x, y);
    if (foundCount > 0) {
        const char *count = foundCount == 1 ? "1 lesson found" : TextFormat("%d lessons found", foundCount);
        UiText(gFonts.text, count, x + w - UiTextWidth(gFonts.text, count, 19), y + 8, 19, COL_DIM);
    }
    y += 56;

    if (foundCount == 0) y += DrawNoMatches(x, y, w) + 16;
    for (int i = 0; i < foundCount; i++) {
        if (DrawResultRow(&set->items[found[i]], x, y, w)) clicked = found[i];
        y += 104;
    }
    page.resultsHeight = (y - top) + 24;
    EndView();
    return clicked;
}

// ---------------------------------------------------------------------------
// The screen

// The search box, fixed under the top bar, with its clear button and (while it's focused
// and empty) a hint. Returns the y below it; sets *submitted when Enter was pressed.
static float DrawSearchBar(bool *submitted)
{
    Rectangle box = { ContentX(), TOPBAR_H + 16, ContentWidth(), 48 };
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_F)) UiFocus(page.query);
    *submitted = UiTextBox(box, page.query, sizeof page.query, "Search all lessons  (Ctrl+F)", false);

    Rectangle clear = { box.x + box.width - 44, box.y + 6, 36, 36 };
    if (page.query[0] && UiButtonIconEx(clear, ICON_CROSS, NULL, 18, COL_CODE_BG, COL_SECONDARY_HOVER, COL_DIM, true)) {
        page.query[0] = '\0';  // back to the course list, where it was
        UiFocus(page.query);
    }
    if (strcmp(page.query, page.lastQuery) != 0) {  // new query: results start at the top
        snprintf(page.lastQuery, sizeof page.lastQuery, "%s", page.query);
        page.resultsScroll = 0;
        page.resultsHeight = 0;
    }

    float bottom = box.y + box.height + 12;
    if (SearchIsBlank(page.query) && UiHasFocus(page.query)) {
        // An empty search: say what the box does instead of searching for nothing.
        const char *hint = "Type a word to search every lesson's title and text, like \"pointer\" or \"malloc\".";
        UiIconText(ICON_INFO, gFonts.text, hint, box.x + 2, bottom - 2, 18, COL_DIM);
        bottom += 30;
    }
    return bottom;
}

int TrackScreen(const LessonSet *set)
{
    if (TopBar(TextFormat("%s Course", TrackName(page.track)))) return PAGE_BACK;

    bool submitted;
    float contentTop = DrawSearchBar(&submitted);
    Rectangle view = ScrollView();
    view.height -= contentTop - view.y;
    view.y = contentTop;

    int clicked;
    if (!SearchIsBlank(page.query)) {
        clicked = DrawResults(set, view, submitted);
        if (clicked >= 0) UiFocus(NULL);  // so Backspace in the lesson goes back, not into the box
    } else {
        clicked = DrawNavigation(set, view);
    }
    return clicked >= 0 ? clicked : PAGE_STAY;
}
