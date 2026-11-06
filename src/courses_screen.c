// A simple course list for revision's Return to Lessons action and direct home access.
#include "courses_screen.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

static struct { Track track; float scroll, height; } courses;
void CoursesScreenOpen(Track track)
{
    courses.track = track >= 0 && track < TRACK_COUNT ? track : TRACK_C;
    courses.scroll = courses.height = 0;
}

int CoursesScreen(const LessonSet *lessons)
{
    if (TopBar("Courses - Lesson Browser")) return COURSES_BACK;
    float x = ContentX(), w = ContentWidth();
    for (int t = 0; t < TRACK_COUNT; t++) {
        bool active = courses.track == (Track)t;
        if (UiButtonEx((Rectangle){ x + t * (w + 12) / 2, 80, (w - 12) / 2, 42 }, t ? "C++ Course" : "C Course", 21,
                       active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                       active ? COL_ON_ACCENT : COL_TEXT, true)) CoursesScreenOpen((Track)t);
    }
    Rectangle view = { 0, 138, (float)GetScreenWidth(), GetScreenHeight() - 138.0f };
    UpdateScroll(&courses.scroll, courses.height, view.height);
    BeginView(view);
    float top = view.y + 8 - courses.scroll, y = top;
    int number = 0, selected = -1;
    for (int i = 0; i < lessons->count; i++) {
        const Lesson *lesson = &lessons->items[i];
        if (lesson->track != courses.track) continue;
        number++;
        float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, w - 210, 23 * UiTextScale(), COL_TEXT, false);
        float h = titleH + 32;
        if (h < 72) h = 72;
        Rectangle row = { x, y, w, h };
        bool over = UiMouseOver(row), passed = ProgressLessonPassed(lesson->id);
        if (over) UiWantHand();
        UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);
        DrawCircleV((Vector2){ x + 36, y + h / 2 }, 20, passed ? COL_GOOD_BG : COL_BG);
        if (passed) IconDraw(ICON_CHECK, (Vector2){ x + 36, y + h / 2 }, 22, COL_GOOD);
        else UiTextCentered(gFonts.mono, TextFormat("%d", number), x + 36, y + h / 2 - 12, 21, COL_DIM);
        UiWrappedText(gFonts.text, lesson->title, (Vector2){ x + 76, y + 16 }, w - 210, 23 * UiTextScale(), COL_TEXT, true);
        const char *score = ProgressScoreNote(lesson->id);
        UiText(gFonts.text, score, x + w - UiTextWidth(gFonts.text, score, 17) - 20, y + h / 2 - 10, 17, passed ? COL_GOOD : COL_DIM);
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) selected = i;
        y += h + 12;
    }
    if (!number) y += UiWrappedText(gFonts.text, "No lessons in this course yet.", (Vector2){ x, y }, w, 23, COL_DIM, true) + 24;
    courses.height = y - top + 24;
    EndView();
    return selected >= 0 ? selected : COURSES_STAY;
}
