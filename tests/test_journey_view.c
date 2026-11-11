// Integration check: rendering either journey must be read-only, including with missing
// mascot art, scrolling, completed tracks, and the minimum supported window size.
#include "../src/journey_view.h"
#include "../src/mascot.h"
#include "../src/progress.h"
#include "../src/roadmap.h"
#include "../src/ui.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(900, 640, "test_journey_view");
    UiLoadFonts("assets");
    MascotLoad("assets");
    LessonSet set;
    assert(LessonsLoad(&set, "assets/lessons"));
    char path[1024];
    snprintf(path, sizeof path, "%s/journey_progress.txt", argv[1]);
    ProgressLoad(path);
    // The first level complete and the second under way.
    for (int s = 0; s < set.items[0].stepCount; s++) ProgressRecordStep(set.items[0].id, s);
    ProgressRecordStep(set.items[1].id, 0);
    assert(RoadmapLevelComplete(&set.items[0]) && RoadmapLevelReady(&set.items[1]));
    assert(JourneyViewFocus(&set, TRACK_C) > 0);
    assert(JourneyViewFocus(&set, TRACK_CPP) == 0);
    char *before = LoadFileText(path);
    assert(before);
    for (int theme = 0; theme < 2; theme++) {
        UiSetTheme(theme != 0, 0);
        UiSetTextScale(theme ? 1.18f : 0.87f);
        for (int t = 0; t < TRACK_COUNT; t++) {
            float heights[2];
            for (int scroll = 0; scroll < 2; scroll++) {
                UiBeginFrame();
                BeginDrawing();
                ClearBackground(COL_BG);
                UiSetClip((Rectangle){ 0, 184, 900, 456 });
                int clicked = JourneyViewDraw(&set, (Track)t, 40, 184 - scroll * 700.0f, 820, &heights[scroll], NULL);
                UiClearClip();
                EndDrawing();
                UiEndFrame();
                assert(clicked == -1);
                assert(heights[scroll] > 640);
            }
            assert(fabsf(heights[0] - heights[1]) < 0.01f); // allow subpixel floating-point rounding
        }
    }
    char *after = LoadFileText(path);
    assert(after && strcmp(before, after) == 0);
    UnloadFileText(before);
    UnloadFileText(after);
    assert(RoadmapLevelComplete(&set.items[0]) && RoadmapStepsDone(&set.items[1]) == 1);

    MascotUnload();
    MascotLoad("build/nonexistent_journey_assets");
    assert(!MascotAvailable());
    for (int i = 0; i < set.count; i++)
        for (int s = 0; s < set.items[i].stepCount; s++) ProgressRecordStep(set.items[i].id, s);
    int done, total;
    assert(RoadmapNext(&set, TRACK_CPP, &done, &total) == -1 && done == total);
    assert(JourneyViewFocus(&set, TRACK_CPP) == 0);
    UiBeginFrame();
    BeginDrawing();
    float height;
    assert(JourneyViewDraw(&set, TRACK_CPP, 40, 184, 820, &height, NULL) == -1);
    LessonSet empty = { 0 };
    assert(JourneyViewDraw(&empty, TRACK_C, 40, 184, 820, &height, NULL) == -1 && height > 0);
    EndDrawing();
    UiEndFrame();
    ProgressLoad("");
    remove(path);
    MascotUnload();
    UiUnloadFonts();
    LessonsUnload(&set);
    CloseWindow();
    puts("All journey rendering tests passed");
    return 0;
}
