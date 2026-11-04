#include "roadmap.h"
#include "account_screens.h"
#include "layout.h"
#include "progress.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

// Suggestions reference stable content IDs, including useful C foundations for C++.
static const struct { const char *id, *before[3]; } prerequisites[] = {
    { "c_03_input", { "c_02_variables" } },
    { "c_04_arithmetic", { "c_02_variables" } },
    { "c_05_decisions", { "c_04_arithmetic" } },
    { "c_06_loops", { "c_05_decisions" } },
    { "c_07_arrays", { "c_02_variables", "c_06_loops" } },
    { "c_08_strings", { "c_07_arrays" } },
    { "c_09_functions", { "c_02_variables", "c_06_loops" } },
    { "c_10_exam1_practice", { "c_07_arrays", "c_09_functions" } },
    { "c_11_pointers", { "c_02_variables", "c_07_arrays" } },
    { "c_12_pass_by_pointer", { "c_09_functions", "c_11_pointers" } },
    { "c_13_pointer_to_pointer", { "c_11_pointers", "c_12_pass_by_pointer" } },
    { "c_14_pointer_arithmetic", { "c_07_arrays", "c_11_pointers" } },
    { "c_15_exam2_practice", { "c_12_pass_by_pointer", "c_14_pointer_arithmetic" } },
    { "c_16_malloc_free", { "c_11_pointers", "c_14_pointer_arithmetic" } },
    { "c_17_scope_globals_fnptrs", { "c_09_functions", "c_11_pointers" } },
    { "cpp_01_hello", { "c_09_functions" } },
    { "cpp_02_references", { "cpp_01_hello", "c_12_pass_by_pointer" } },
    { "cpp_03_new_delete", { "c_11_pointers", "cpp_01_hello" } },
    { "cpp_04_memory_mistakes", { "cpp_03_new_delete" } },
    { "cpp_05_dynamic_arrays", { "c_07_arrays", "cpp_03_new_delete" } },
    { "cpp_06_classes", { "cpp_02_references" } },
    { "cpp_07_objects_methods", { "cpp_06_classes" } },
    { "cpp_08_constructors", { "cpp_07_objects_methods", "cpp_03_new_delete" } },
    { "cpp_09_access_static", { "cpp_06_classes", "cpp_07_objects_methods" } },
    { "cpp_10_copy_constructors", { "cpp_08_constructors", "cpp_02_references" } },
    { "cpp_11_arrays_destructors", { "cpp_05_dynamic_arrays", "cpp_08_constructors" } },
    { "cpp_12_linked_lists", { "c_13_pointer_to_pointer", "cpp_03_new_delete" } },
    { "cpp_13_linked_list_class", { "cpp_12_linked_lists", "cpp_11_arrays_destructors" } },
};

int RoadmapNext(const LessonSet *set, Track track, int *done, int *total)
{
    int next = -1;
    *done = *total = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        (*total)++;
        if (ProgressBest(lesson->id) >= PASS_PERCENT) (*done)++;
        else if (next < 0) next = i;
    }
    return next;
}

float RoadmapPrerequisites(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size, bool draw)
{
    char text[512] = "Suggested preparation (optional): ";
    bool found = false;
    for (unsigned int p = 0; p < sizeof prerequisites / sizeof prerequisites[0]; p++) {
        if (strcmp(prerequisites[p].id, lesson->id) != 0) continue;
        for (int j = 0; j < 3 && prerequisites[p].before[j]; j++) {
            for (int i = 0; i < set->count; i++) {
                if (strcmp(set->items[i].id, prerequisites[p].before[j]) != 0) continue;
                size_t used = strlen(text);
                snprintf(text + used, sizeof text - used, "%s%s: %s", found ? "; " : "",
                         TrackName(set->items[i].track), set->items[i].title);
                found = true;
                break;
            }
        }
        break;
    }
    return found ? UiWrappedText(gFonts.text, text, (Vector2){ x, y }, w, size, COL_DIM, draw) : 0;
}

int RoadmapDraw(const LessonSet *set, Track track, float x, float y, float w, float *height)
{
    float top = y, ts = TextScale();
    int done, total, clicked = -1;
    int next = RoadmapNext(set, track, &done, &total);
    y += UiWrappedText(gFonts.bold, "Learning Roadmap", (Vector2){ x, y }, w, 34 * ts, COL_TEXT, true) + 12;
    UiText(gFonts.text, TextFormat("%d of %d lessons complete  |  %d%%", done, total, total ? done * 100 / total : 0), x, y, 20 * ts, COL_TEXT);
    y += 32 * ts;
    ProgressBar((Rectangle){ x, y, w, 10 }, total ? (float)done / total : 0, COL_PANEL_HI, COL_GOOD);
    y += 26;
    const char *message = total == 0 ? "No lessons in this track yet."
        : next < 0 ? "Congratulations! You've completed this track. Revisit any lesson below."
        : "Follow the highlighted lesson, revisit a topic, or skip ahead. Every lesson is open.";
    y += UiWrappedText(gFonts.text, message, (Vector2){ x, y }, w, 19 * ts, COL_DIM, true) + 24;
    int number = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        number++;
        bool passed = ProgressBest(lesson->id) >= PASS_PERCENT;
        Color state = passed ? COL_GOOD : i == next ? COL_ACCENT : COL_DIM;
        float cardX = x + 62, cardW = w - 62;
        float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, cardW - 36, 24 * ts, COL_TEXT, false);
        float prepH = RoadmapPrerequisites(set, lesson, 0, 0, cardW - 36, 17 * ts, false);
        float h = 24 + 28 * ts + titleH + (prepH ? prepH + 12 : 0) + 18;
        Rectangle card = { cardX, y, cardW, h };
        bool over = UiMouseOver(card);
        if (over) UiWantHand();
        if (number < total) DrawLineEx((Vector2){ x + 23, y + 24 }, (Vector2){ x + 23, y + h + 42 }, 3, COL_PANEL_HI);
        DrawCircleV((Vector2){ x + 23, y + 24 }, 22, passed ? COL_GOOD_BG : COL_PANEL);
        if (passed) IconDraw(ICON_CHECK, (Vector2){ x + 23, y + 24 }, 24, COL_GOOD);
        else UiTextCentered(gFonts.mono, TextFormat("%d", number), x + 23, y + 12, 22, state);
        UiRoundRect(card, 12, over ? COL_PANEL_HI : COL_PANEL);
        if (i == next) DrawRectangleRoundedLinesEx(card, 0.15f, 8, 2, COL_ACCENT);
        const char *status = passed ? "Completed" : i == next ? "Recommended Next" : "Upcoming - open anytime";
        UiText(gFonts.text, TextFormat("%02d  %s%s", number, status, lesson->practice ? "  |  Practice" : ""), cardX + 18, y + 14, 17 * ts, state);
        float textY = y + 18 + 28 * ts;
        UiWrappedText(gFonts.text, lesson->title, (Vector2){ cardX + 18, textY }, cardW - 36, 24 * ts, COL_TEXT, true);
        if (prepH) RoadmapPrerequisites(set, lesson, cardX + 18, textY + titleH + 12, cardW - 36, 17 * ts, true);
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = i;
        y += h + 20;
    }
    *height = y - top + 32;
    return clicked;
}
