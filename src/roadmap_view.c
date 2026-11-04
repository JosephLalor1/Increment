// Drawing for the learning roadmap. See roadmap_view.h.
#include "roadmap_view.h"
#include "layout.h"
#include "progress.h"
#include "roadmap.h"
#include "ui.h"

float RoadmapViewPreparation(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size,
                             bool draw)
{
    char text[512];
    if (!RoadmapPreparation(set, lesson, text, sizeof text)) return 0;
    return UiWrappedText(gFonts.text, text, (Vector2){ x, y }, w, size, COL_DIM, draw);
}

// The track's completion line, progress bar and a message suited to where the learner is.
// Sets *next to the recommended lesson (-1 if none) and *total to the track's lesson count.
static float DrawSummary(const LessonSet *set, Track track, int *next, int *total, float x, float y, float w,
                         float scale)
{
    float top = y;
    int done;
    *next = RoadmapNext(set, track, &done, total);
    y += UiWrappedText(gFonts.bold, "Learning Roadmap", (Vector2){ x, y }, w, 34 * scale, COL_TEXT, true) + 12;
    int percent = *total ? done * 100 / *total : 0;
    UiText(gFonts.text, TextFormat("%d of %d lessons complete  |  %d%%", done, *total, percent), x, y, 20 * scale,
           COL_TEXT);
    y += 32 * scale;
    ProgressBar((Rectangle){ x, y, w, 10 }, *total ? (float)done / *total : 0, COL_PANEL_HI, COL_GOOD);
    y += 26;
    const char *message = *total == 0 ? "No lessons in this track yet."
                        : *next < 0  ? "Congratulations! You've completed this track. Revisit any lesson below."
                                     : "Follow the highlighted lesson, revisit a topic, or skip ahead. Every lesson is open.";
    y += UiWrappedText(gFonts.text, message, (Vector2){ x, y }, w, 19 * scale, COL_DIM, true) + 24;
    return y - top;
}

int RoadmapViewDraw(const LessonSet *set, Track track, float x, float y, float w, float *height)
{
    float top = y, scale = UiTextScale();
    int next, total, clicked = -1;
    y += DrawSummary(set, track, &next, &total, x, y, w, scale);

    // A numbered node on a vertical line at the left, and a card to its right per lesson.
    float nodeX = x + 23, cardX = x + 62, cardW = w - 62, textW = cardW - 36;
    int number = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        number++;
        bool passed = ProgressLessonPassed(lesson->id);
        bool recommended = i == next;
        Color stateColor = passed ? COL_GOOD : recommended ? COL_ACCENT : COL_DIM;

        float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, textW, 24 * scale, COL_TEXT, false);
        float prepH = RoadmapViewPreparation(set, lesson, 0, 0, textW, 17 * scale, false);
        float cardH = 24 + 28 * scale + titleH + (prepH ? prepH + 12 : 0) + 18;
        Rectangle card = { cardX, y, cardW, cardH };
        bool over = UiMouseOver(card);
        if (over) UiWantHand();

        if (number < total) {  // the line down to the next node
            DrawLineEx((Vector2){ nodeX, y + 24 }, (Vector2){ nodeX, y + cardH + 42 }, 3, COL_PANEL_HI);
        }
        DrawCircleV((Vector2){ nodeX, y + 24 }, 22, passed ? COL_GOOD_BG : COL_PANEL);
        if (passed) IconDraw(ICON_CHECK, (Vector2){ nodeX, y + 24 }, 24, COL_GOOD);
        else UiTextCentered(gFonts.mono, TextFormat("%d", number), nodeX, y + 12, 22, stateColor);

        UiRoundRect(card, 12, over ? COL_PANEL_HI : COL_PANEL);
        if (recommended) DrawRectangleRoundedLinesEx(card, 0.15f, 8, 2, COL_ACCENT);
        const char *status = passed ? "Completed" : recommended ? "Recommended Next" : "Upcoming - open anytime";
        UiText(gFonts.text, TextFormat("%02d  %s%s", number, status, lesson->practice ? "  |  Practice" : ""),
               cardX + 18, y + 14, 17 * scale, stateColor);
        float textY = y + 18 + 28 * scale;
        UiWrappedText(gFonts.text, lesson->title, (Vector2){ cardX + 18, textY }, textW, 24 * scale, COL_TEXT, true);
        if (prepH) RoadmapViewPreparation(set, lesson, cardX + 18, textY + titleH + 12, textW, 17 * scale, true);

        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = i;
        y += cardH + 20;
    }
    *height = y - top + 32;
    return clicked;
}
