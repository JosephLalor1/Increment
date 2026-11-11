#include "../src/roadmap.h"
#include "../src/progress.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The roadmap's data logic: steps unlocking in order inside a level, where to resume, and
// progress through a track (levels without steps are skipped). Also loads the real lessons,
// so a level that fails to parse shows up here.

static void MakeLevel(Lesson *lesson, const char *id, Track track, int steps)
{
    memset(lesson, 0, sizeof *lesson);
    snprintf(lesson->id, sizeof lesson->id, "%s", id);
    lesson->track = track;
    lesson->stepCount = steps;
}

int main(void)
{
    LessonSet real;
    assert(LessonsLoad(&real, "assets/lessons") && real.count == 30);
    int ready = 0;
    for (int i = 0; i < real.count; i++) {
        ready += RoadmapLevelReady(&real.items[i]);
        for (int s = 0; s < real.items[i].stepCount; s++) {
            const Step *step = &real.items[i].steps[s];
            assert(step->starter[0] && step->expected[0] && step->solution[0]);
            assert(step->difficulty >= 1 && step->difficulty <= 3);
        }
    }
    assert(ready >= 1);  // the first levels are written
    LessonsUnload(&real);

    const char *path = "build/test_roadmap_progress.txt";
    remove(path);
    ProgressLoad(path);
    LessonSet set = { calloc(4, sizeof(Lesson)), 4 };
    assert(set.items);
    MakeLevel(&set.items[0], "c_one", TRACK_C, 3);
    MakeLevel(&set.items[1], "c_soon", TRACK_C, 0);  // not written yet
    MakeLevel(&set.items[2], "c_two", TRACK_C, 2);
    MakeLevel(&set.items[3], "cpp_one", TRACK_CPP, 2);
    const Lesson *one = &set.items[0];

    // Inside a level: only the first step is open, and it's where to start.
    assert(RoadmapStepsDone(one) == 0 && !RoadmapLevelComplete(one) && RoadmapResumeStep(one) == 0);
    assert(RoadmapStepOpen(one, 0) && !RoadmapStepOpen(one, 1) && !RoadmapStepOpen(one, 3) && !RoadmapStepOpen(one, -1));
    assert(ProgressRecordStep("c_one", 0));
    assert(RoadmapStepOpen(one, 1) && !RoadmapStepOpen(one, 2) && RoadmapResumeStep(one) == 1);
    assert(ProgressRecordStep("c_one", 2));  // passed out of order (possible only in older data)
    assert(RoadmapResumeStep(one) == 1 && !RoadmapStepOpen(one, 2) && RoadmapStepsDone(one) == 2);
    assert(ProgressRecordStep("c_one", 1));
    assert(RoadmapLevelComplete(one) && RoadmapResumeStep(one) == 0);  // a finished level replays from the start
    assert(!RoadmapLevelReady(&set.items[1]) && !RoadmapLevelComplete(&set.items[1]));

    // Along a track: unwritten levels are skipped, and progress is read back from the file.
    int done, total;
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == 2 && done == 1 && total == 2);
    assert(RoadmapNext(&set, TRACK_CPP, &done, &total) == 3 && done == 0 && total == 1);
    ProgressRecordStep("c_two", 0);
    ProgressRecordStep("c_two", 1);
    ProgressLoad(path);
    assert(RoadmapNext(&set, TRACK_C, &done, &total) == -1 && done == 2 && total == 2);
    assert(ProgressStepDone("c_two", 1) && !ProgressStepDone("c_two", 2));

    // Ids read back from a stored line.
    char lessonId[32];
    int step;
    assert(ProgressParseStepId("c_one/3", lessonId, sizeof lessonId, &step) && !strcmp(lessonId, "c_one") && step == 2);
    assert(!ProgressParseStepId("c_one!gold", lessonId, sizeof lessonId, &step));
    assert(!ProgressParseStepId("c_one/0", lessonId, sizeof lessonId, &step));
    assert(!ProgressParseStepId("c_one/x", lessonId, sizeof lessonId, &step));
    assert(!ProgressParseStepId("/2", lessonId, sizeof lessonId, &step));
    assert(!ProgressParseStepId("c_one#1", lessonId, sizeof lessonId, &step));  // an exercise, from older versions

    LessonSet empty = { 0 };
    assert(RoadmapNext(&empty, TRACK_CPP, &done, &total) == -1 && done == 0 && total == 0);
    ProgressLoad("");
    assert(!ProgressStepDone("c_one", 0));  // signed out: nothing
    remove(path);
    free(set.items);
    puts("Roadmap tests passed.");
    return 0;
}
