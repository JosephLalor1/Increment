// The lesson search box and its results, for any screen to host. See search_view.h.
#include "search_view.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

void SearchViewClear(SearchView *s)
{
    s->query[0] = '\0';
    s->resultsScroll = 0;
    s->resultsHeight = 0;
}

bool SearchViewActive(const SearchView *s) { return !SearchIsBlank(s->query); }

void SearchViewDemo(SearchView *s, const char *query, SearchFilter filter)
{
    if (query) {
        snprintf(s->query, sizeof s->query, "%s", query);
        UiFocus(s->query);
    }
    s->filter = filter;
}

// "New" or "Best: 80%", the score note at the right of a lesson row.
static const char *ScoreText(const Lesson *lesson)
{
    int best = ProgressBest(lesson->id);
    return best < 0 ? "New" : TextFormat("Best: %d%%", best);
}

// ---------------------------------------------------------------------------
// Results

// The excerpt around the match on one line, with the matched word in the accent colour.
static void DrawExcerpt(SearchView *s, const Lesson *lesson, float x, float y, float maxWidth)
{
    const float size = 18;
    char excerpt[256];
    int matchAt, matchLen;
    if (!SearchSnippet(lesson, s->query, excerpt, sizeof excerpt, &matchAt, &matchLen)) {
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
static bool DrawResultRow(SearchView *s, const Lesson *lesson, float x, float y, float w)
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

    DrawExcerpt(s, lesson, x + 20, y + 58, w - 40);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// "C and C++" / "C" / "C++" toggle buttons.
static void DrawFilterButtons(SearchView *s, float x, float y)
{
    static const float widths[] = { 124, 56, 72 };  // indexed by SearchFilter
    for (int i = 0; i < 3; i++) {
        bool on = (int)s->filter == i;
        Rectangle button = { x, y, widths[i], 38 };
        if (UiButtonEx(button, SearchFilterName((SearchFilter)i), 18, on ? COL_ACCENT : COL_PANEL,
                       on ? COL_ACCENT_HI : COL_SECONDARY_HOVER, on ? COL_ON_ACCENT : COL_TEXT, true)) {
            s->filter = (SearchFilter)i;
            s->resultsScroll = 0;
        }
        x += widths[i] + 8;
    }
}

// The panel shown when nothing matches: what was searched, tips, and ways out.
// Returns its height.
static float DrawNoMatches(SearchView *s, float x, float y, float w)
{
    const char *advice = "Check the spelling, or try fewer or shorter words: \"point\" finds pointer and pointers.";
    float adviceH = UiWrappedText(gFonts.text, advice, (Vector2){ 0, 0 }, w - 96, 19, COL_DIM, false);
    Rectangle panel = { x, y, w, 68 + adviceH + 64 };
    UiRoundRect(panel, 12, COL_PANEL);
    IconDraw(ICON_INFO, (Vector2){ x + 40, y + 42 }, 28, COL_ACCENT);

    char heading[160];
    if (s->filter == SEARCH_BOTH) snprintf(heading, sizeof heading, "No lessons match \"%s\".", s->query);
    else snprintf(heading, sizeof heading, "No %s lessons match \"%s\".", SearchFilterName(s->filter), s->query);
    UiFitText(heading, sizeof heading, gFonts.bold, 24, w - 96);
    UiText(gFonts.bold, heading, x + 72, y + 26, 24, COL_TEXT);
    UiWrappedText(gFonts.text, advice, (Vector2){ x + 72, y + 68 }, w - 96, 19, COL_DIM, true);

    float linkX = x + 72, linkY = y + 68 + adviceH + 18;
    if (s->filter != SEARCH_BOTH) {
        const char *widen = "Search C and C++";
        if (UiLink(linkX, linkY, widen, 19)) {
            s->filter = SEARCH_BOTH;
            s->resultsScroll = 0;
        }
        linkX += UiTextWidth(gFonts.text, widen, 19) + 32;
    }
    if (UiLink(linkX, linkY, "Clear search", 19)) {
        s->query[0] = '\0';
        UiFocus(s->query);
    }
    return panel.height;
}

int SearchViewResults(SearchView *s, const LessonSet *set, Rectangle view, bool submitted)
{
    static int found[1024];
    int foundCount = SearchLessons(set, s->query, s->filter, found, 1024);
    if (submitted && foundCount > 0) return found[0];

    UpdateScroll(&s->resultsScroll, s->resultsHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth();
    float top = view.y + 8 - s->resultsScroll, y = top;
    int clicked = -1;

    DrawFilterButtons(s, x, y);
    if (foundCount > 0) {
        const char *count = foundCount == 1 ? "1 lesson found" : TextFormat("%d lessons found", foundCount);
        UiText(gFonts.text, count, x + w - UiTextWidth(gFonts.text, count, 19), y + 8, 19, COL_DIM);
    }
    y += 56;

    if (foundCount == 0) y += DrawNoMatches(s, x, y, w) + 16;
    for (int i = 0; i < foundCount; i++) {
        if (DrawResultRow(s, &set->items[found[i]], x, y, w)) clicked = found[i];
        y += 104;
    }
    s->resultsHeight = (y - top) + 24;
    EndView();
    return clicked;
}

// ---------------------------------------------------------------------------
// The box

float SearchViewBox(SearchView *s, Rectangle box, bool *submitted)
{
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_F)) UiFocus(s->query);
    *submitted = UiTextBox(box, s->query, sizeof s->query, "Search all lessons  (Ctrl+F)", false);

    Rectangle clear = { box.x + box.width - 44, box.y + 6, 36, 36 };
    if (s->query[0] && UiButtonIconEx(clear, ICON_CROSS, NULL, 18, COL_CODE_BG, COL_SECONDARY_HOVER, COL_DIM, true)) {
        s->query[0] = '\0';  // the caller shows its normal content again
        UiFocus(s->query);
    }
    if (strcmp(s->query, s->lastQuery) != 0) {  // new query: results start at the top
        snprintf(s->lastQuery, sizeof s->lastQuery, "%s", s->query);
        s->resultsScroll = 0;
        s->resultsHeight = 0;
    }

    float bottom = box.y + box.height + 12;
    if (SearchIsBlank(s->query) && UiHasFocus(s->query)) {
        // An empty search: say what the box does instead of searching for nothing.
        const char *hint = "Type a word to search every lesson's title and text, like \"pointer\" or \"malloc\".";
        UiIconText(ICON_INFO, gFonts.text, hint, box.x + 2, bottom - 2, 18, COL_DIM);
        bottom += 30;
    }
    return bottom;
}

