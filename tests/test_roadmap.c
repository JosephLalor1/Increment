#include "../src/roadmap.h"
#include "../src/progress.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Roadmap drawing is linked but these tests exercise the data logic without a window.
float TextScale(void) { return 1.0f; }

int main(void)
{
    LessonSet set;
    assert(LessonsLoad(&set, "assets/lessons"));
    const char *path = "build/test_roadmap_progress.txt";
    remove(path);
    ProgressLoad(path);
    int done, total;
    int first = RoadmapNext(&set, TRACK_C, &done, &total);
    assert(first >= 0 && done == 0 && total == 17);
    int cpp = RoadmapNext(&set, TRACK_CPP, &done, &total);
    assert(cpp >= 0 && done == 0 && total == 13);
    ProgressRecord(set.items[first + 2].id, 100); // skipping ahead leaves the first gap
    ProgressRecord(set.items[first].id, PASS_PERCENT - 1);
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == first && done == 1);
    ProgressRecord(set.items[first].id, PASS_PERCENT);
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == first + 1 && done == 2);
    ProgressRecord(set.items[first].id, 0); // a failed retry preserves the best score
    ProgressLoad(path);
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == first + 1 && done == 2);
    assert(RoadmapNext(&set, TRACK_CPP, &done, &total) == cpp && done == 0);
    for (int i = 0; i < set.count; i++)
        if (set.items[i].track == TRACK_C) ProgressRecord(set.items[i].id, 100);
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == -1 && done == total);
    LessonSet empty = { 0 };
    assert(RoadmapNext(&empty, TRACK_CPP, &done, &total) == -1 && done == 0 && total == 0);
    ProgressLoad("");
    remove(path);
    LessonsUnload(&set);
    puts("Roadmap tests passed.");
    return 0;
}
