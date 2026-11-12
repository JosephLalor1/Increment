#include "home/dashboard_stats.h"
#include "learning/progress.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The dashboard's statistics, from passed steps.

static void CheckEmpty(DashboardStats stats)
{
    assert(stats.lessons == 0 && stats.completed == 0 && stats.started == 0);
    assert(stats.steps == 0 && stats.stepsDone == 0 && stats.next == -1);
    assert(stats.speedRounds == 0 && stats.gold == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2); // caller supplies an isolated, throwaway directory
    char path[1024];
    snprintf(path, sizeof path, "%s/dashboard_progress.txt", argv[1]);
    ProgressLoad(path);
    LessonSet empty = { 0 };
    CheckEmpty(DashboardCalculate(&empty, TRACK_COUNT));
    CheckEmpty(DashboardCalculate(&empty, TRACK_C));

    // Heap allocation: Lesson holds its steps inline.
    LessonSet set = { calloc(4, sizeof(Lesson)), 4 };
    assert(set.items);
    const char *ids[] = { "c_first", "cpp_first", "c_second", "cpp_soon" };
    for (int i = 0; i < set.count; i++) {
        snprintf(set.items[i].id, sizeof set.items[i].id, "%s", ids[i]);
        set.items[i].track = i % 2 ? TRACK_CPP : TRACK_C;
        set.items[i].stepCount = i == 3 ? 0 : 2;  // cpp_soon isn't written yet
    }
    snprintf(set.items[0].speedSkills, sizeof set.items[0].speedSkills, "arithmetic");
    snprintf(set.items[1].speedSkills, sizeof set.items[1].speedSkills, "references");
    snprintf(set.items[3].speedSkills, sizeof set.items[3].speedSkills, "objects");  // unwritten: not counted
    DashboardStats all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.lessons == 3 && all.completed == 0 && all.started == 0 && all.next == 0);
    assert(all.steps == 6 && all.stepsDone == 0);
    assert(all.speedRounds == 2 && all.gold == 0);
    ProgressRecordGold("cpp_first");
    ProgressRecordGold("c_second");  // no speed round in this level: not counted
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.speedRounds == 2 && all.gold == 1);
    assert(DashboardCalculate(&set, TRACK_C).gold == 0 && DashboardCalculate(&set, TRACK_CPP).gold == 1);

    ProgressRecordStep("c_first", 0);
    ProgressRecordStep("c_second", 0);
    ProgressRecordStep("c_second", 1);   // a later level finished first: the earlier one is still next
    ProgressRecordStep("stale_level", 0); // deleted content must not inflate counts
    ProgressRecordStep("c_first", 7);     // nor a step absent from current content
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 1 && all.started == 2 && all.stepsDone == 3 && all.next == 0);
    DashboardStats c = DashboardCalculate(&set, TRACK_C);
    DashboardStats cpp = DashboardCalculate(&set, TRACK_CPP);
    assert(c.lessons == 2 && c.completed == 1 && c.stepsDone == 3 && c.next == 0);
    assert(cpp.lessons == 1 && cpp.completed == 0 && cpp.stepsDone == 0 && cpp.next == 1);

    ProgressRecordStep("c_first", 1);
    ProgressRecordStep("cpp_first", 0);
    ProgressRecordStep("cpp_first", 1);
    ProgressLoad(path); // calculate from persisted data, not only in-memory records
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 3 && all.stepsDone == 6 && all.next == -1);

    ProgressLoad(""); // logout/account change must clear all derived statistics
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 0 && all.stepsDone == 0 && all.next == 0 && all.gold == 0);
    set.count = 1; // missing C++ track
    CheckEmpty(DashboardCalculate(&set, TRACK_CPP));
    free(set.items);
    remove(path);
    puts("All dashboard tests passed");
    return 0;
}
