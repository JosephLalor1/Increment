#include "../src/dashboard_stats.h"
#include "../src/progress.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void CheckEmpty(DashboardStats stats)
{
    assert(stats.lessons == 0 && stats.completed == 0);
    assert(stats.exercises == 0 && stats.exercisesCompleted == 0);
    assert(stats.quizzes == 0 && stats.attempted == 0 && stats.passed == 0);
    assert(stats.average == 0 && stats.highest == -1 && stats.next == -1);
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

    // Heap allocation: Lesson contains large content arrays.
    LessonSet set = { calloc(4, sizeof(Lesson)), 4 };
    assert(set.items);
    const char *ids[] = { "c_first", "cpp_first", "c_second", "cpp_no_quiz" };
    for (int i = 0; i < set.count; i++) {
        snprintf(set.items[i].id, sizeof set.items[i].id, "%s", ids[i]);
        set.items[i].track = i % 2 ? TRACK_CPP : TRACK_C;
        set.items[i].questionCount = i == 3 ? 0 : 2;
        set.items[i].exerciseCount = 2;
    }
    snprintf(set.items[0].speedSkills, sizeof set.items[0].speedSkills, "arithmetic");
    snprintf(set.items[1].speedSkills, sizeof set.items[1].speedSkills, "references");
    DashboardStats all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.lessons == 4 && all.completed == 0 && all.quizzes == 3);
    assert(all.attempted == 0 && all.highest == -1 && all.next == 0);
    assert(all.exercises == 8 && all.exercisesCompleted == 0);
    assert(all.speedRounds == 2 && all.gold == 0);
    ProgressRecordGold("cpp_first");
    ProgressRecordGold("c_second");  // no speed round in this lesson: not counted
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.speedRounds == 2 && all.gold == 1);
    assert(DashboardCalculate(&set, TRACK_C).gold == 0 && DashboardCalculate(&set, TRACK_CPP).gold == 1);

    ProgressRecord("c_first", 0); // zero is attempted, not an absent score
    ProgressRecord("c_second", PASS_PERCENT); // skip first incomplete lesson
    ProgressRecord("cpp_no_quiz", 100); // manual completion is not a quiz
    ProgressRecord("c_first#1", 100);
    ProgressRecord("c_first#2", 99); // exercise completion requires 100
    ProgressRecord("stale_lesson", 100); // deleted content must not inflate counts
    ProgressRecord("c_first#8", 100); // nor an exercise absent from current content
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 2 && all.attempted == 2 && all.passed == 1);
    assert(fabsf(all.average - 35.0f) < 0.01f && all.highest == 70);
    assert(all.exercisesCompleted == 1 && all.next == 0);
    DashboardStats c = DashboardCalculate(&set, TRACK_C);
    DashboardStats cpp = DashboardCalculate(&set, TRACK_CPP);
    assert(c.lessons == 2 && c.completed == 1 && c.attempted == 2 && c.next == 0);
    assert(cpp.lessons == 2 && cpp.completed == 1 && cpp.attempted == 0 && cpp.next == 1);
    assert(cpp.highest == -1 && cpp.average == 0);
    assert(c.exercisesCompleted == 1 && cpp.exercisesCompleted == 0);

    ProgressRecord("c_first", PASS_PERCENT - 1);
    c = DashboardCalculate(&set, TRACK_C);
    assert(c.completed == 1 && c.passed == 1 && c.next == 0);
    ProgressRecord("c_first#2", 100);
    assert(DashboardCalculate(&set, TRACK_C).exercisesCompleted == 2);

    ProgressRecord("c_first", 100);
    ProgressRecord("c_first", 20); // retry cannot lower best score
    ProgressRecord("cpp_first", 80);
    ProgressLoad(path); // calculate from persisted data, not only in-memory records
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 4 && all.passed == 3 && all.attempted == 3);
    assert(all.highest == 100 && fabsf(all.average - 250.0f / 3) < 0.01f);
    assert(all.next == -1 && all.exercisesCompleted == 2);
    assert(DashboardCalculate(&set, TRACK_C).next == -1);
    assert(DashboardCalculate(&set, TRACK_CPP).next == -1);

    ProgressLoad(""); // logout/account change must clear all derived statistics
    all = DashboardCalculate(&set, TRACK_COUNT);
    assert(all.completed == 0 && all.attempted == 0 && all.exercisesCompleted == 0);
    assert(all.next == 0);
    set.count = 1; // missing C++ track
    CheckEmpty(DashboardCalculate(&set, TRACK_CPP));
    free(set.items);
    remove(path);
    puts("All dashboard tests passed");
    return 0;
}
