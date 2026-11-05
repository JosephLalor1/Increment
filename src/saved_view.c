// The saved (bookmarked) lessons list on the home screen. See saved_view.h.
#include "saved_view.h"
#include "bookmarks.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

#include <stdio.h>

#define ROW_H 76.0f

// One saved lesson: track badge, title, score, and a filled bookmark that removes it.
// Returns true when the row (not its remove button) is clicked; sets *removed when the
// button is.
static bool DrawSavedRow(const Lesson *lesson, float x, float y, float w, bool *removed)
{
    Rectangle row = { x, y, w, ROW_H };
    Rectangle remove = { x + w - 60, y + (ROW_H - 44) / 2, 44, 44 };
    bool overRemove = UiMouseOver(remove);
    bool over = UiMouseOver(row) && !overRemove;
    if (over) UiWantHand();
    UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);

    float badgeW = TrackBadge(TrackName(lesson->track), x + 20, y + (ROW_H - 28) / 2);

    bool passed = ProgressLessonPassed(lesson->id);
    const char *score = ProgressScoreNote(lesson->id);
    float scoreW = UiTextWidth(gFonts.text, score, 18);
    float scoreX = remove.x - 16 - scoreW;
    UiText(gFonts.text, score, scoreX, y + (ROW_H - 18) / 2 - 2, 18, passed ? COL_GOOD : COL_DIM);
    if (passed) IconDraw(ICON_CHECK, (Vector2){ scoreX - 20, y + ROW_H / 2 }, 18, COL_GOOD);

    char title[128];
    snprintf(title, sizeof title, "%s", lesson->title);
    float titleX = x + 20 + badgeW + 14;
    UiFitText(title, sizeof title, gFonts.text, 24, scoreX - (passed ? 40 : 20) - titleX);
    UiText(gFonts.text, title, titleX, y + (ROW_H - 24) / 2 - 2, 24, COL_TEXT);

    // The filled bookmark shows the lesson is saved; clicking it un-saves.
    *removed = UiButtonIconEx(remove, ICON_BOOKMARK_FILLED, NULL, 22, over ? COL_PANEL_HI : COL_PANEL,
                              COL_SECONDARY_HOVER, COL_ACCENT, true);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// With nothing saved: what bookmarks are for and how to add one. Returns its height.
static float DrawEmptyState(float x, float y, float w, bool *browse)
{
    const char *advice = "Open any lesson and press Save lesson at the top right of the page. Lessons you save "
                         "from the C and C++ tracks are listed here, so you can come back to topics you found "
                         "useful or difficult.";
    float adviceH = UiWrappedText(gFonts.text, advice, (Vector2){ 0, 0 }, w - 96, 19, COL_DIM, false);
    Rectangle panel = { x, y, w, 68 + adviceH + 64 };
    UiRoundRect(panel, 12, COL_PANEL);
    IconDraw(ICON_BOOKMARK, (Vector2){ x + 40, y + 42 }, 28, COL_ACCENT);
    UiText(gFonts.bold, "No saved lessons yet", x + 72, y + 26, 24, COL_TEXT);
    UiWrappedText(gFonts.text, advice, (Vector2){ x + 72, y + 68 }, w - 96, 19, COL_DIM, true);
    if (UiLink(x + 72, y + 68 + adviceH + 18, "Browse the journey", 19)) *browse = true;
    return panel.height;
}

SavedChoice SavedViewDraw(SavedView *s, const LessonSet *set, Rectangle view)
{
    SavedChoice choice = { -1, false };
    UpdateScroll(&s->scroll, s->height, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth();
    float top = view.y + 8 - s->scroll, y = top;

    UiText(gFonts.bold, "Saved lessons", x, y, 28, COL_TEXT);
    int shown = 0;
    for (int i = 0; i < set->count; i++) shown += BookmarkIsSet(set->items[i].id);  // ids of removed lessons don't count
    if (shown > 0) {
        const char *count = shown == 1 ? "1 lesson" : TextFormat("%d lessons", shown);
        UiText(gFonts.text, count, x + w - UiTextWidth(gFonts.text, count, 19), y + 8, 19, COL_DIM);
    }
    y += 48;

    if (shown == 0) y += DrawEmptyState(x, y, w, &choice.browse) + 16;
    // Course order (C first, then C++), so a track's saved lessons stay together.
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (!BookmarkIsSet(lesson->id)) continue;
        bool removed = false;
        if (DrawSavedRow(lesson, x, y, w, &removed)) choice.openLesson = i;
        if (removed) BookmarkSet(lesson->id, false);  // the row disappears next frame
        y += ROW_H + 12;
    }
    s->height = (y - top) + 24;
    EndView();
    return choice;
}
