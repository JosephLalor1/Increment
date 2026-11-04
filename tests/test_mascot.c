#include "../src/mascot_state.h"
#include "../src/mascot_events.h"
#include "../src/progress.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Reactions(void)
{
    FoxState state;
    FoxStateInit(&state, FOX_THINKING, 0);
    assert(FoxStateUpdate(&state, FOX_THINKING, 0) == FOX_THINKING);
    FoxStateReact(&state, FOX_CORRECT, 1);
    assert(FoxStateUpdate(&state, FOX_THINKING, 1.1) == FOX_EXCITED);
    assert(state.previous == FOX_THINKING && state.transition == 1);
    const char *firstMessage = FoxStateMessage(&state, "fallback");
    FoxStateReact(&state, FOX_CORRECT, 2);
    assert(strcmp(firstMessage, FoxStateMessage(&state, "fallback")) != 0);
    assert(FoxStateUpdate(&state, FOX_THINKING, 6) == FOX_THINKING && !state.reacting);
    assert(strcmp(FoxStateMessage(&state, "fallback"), "fallback") == 0);
    FoxStateReact(&state, FOX_INCORRECT, 10);
    assert(FoxStateUpdate(&state, FOX_THINKING, 10.5) == FOX_SAD);
    assert(FoxStateUpdate(&state, FOX_THINKING, 11) == FOX_DETERMINED);
    assert(strstr(FoxStateMessage(&state, "fallback"), "Keep trying"));
    assert(FoxStateUpdate(&state, FOX_THINKING, 14) == FOX_THINKING);
    FoxStateReact(&state, FOX_LESSON_COMPLETE, 20);
    assert(FoxStateUpdate(&state, FOX_CODING, 21) == FOX_TROPHY);
    assert(FoxStateUpdate(&state, FOX_CODING, 27) == FOX_CODING);
    FoxStateReact(&state, FOX_STATION_COMPLETE, 28);
    assert(FoxStateUpdate(&state, FOX_CODING, 29) == FOX_CELEBRATING);
    FoxStateReact(&state, FOX_COURSE_COMPLETE, 30);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 31) == FOX_GRADUATION);
    FoxStateReact(&state, FOX_TIP, 40);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 41) == FOX_IDEA);
    FoxStateReact(&state, FOX_BREAK, 50);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 51) == FOX_SLEEPY);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 54) == FOX_HAPPY);
    FoxStateInit(&state, FOX_HAPPY, 60); // account boundary drops reactions
    assert(!state.reacting && state.current == FOX_HAPPY);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Reactions();
    char path[1024];
    snprintf(path, sizeof path, "%s/mascot_progress.txt", argv[1]);
    ProgressLoad(path);
    LessonSet set = { calloc(4, sizeof(Lesson)), 4 };
    assert(set.items);
    const char *ids[] = { "c_first", "cpp_first", "c_last", "cpp_last" };
    for (int i = 0; i < 4; i++) {
        snprintf(set.items[i].id, sizeof set.items[i].id, "%s", ids[i]);
        set.items[i].track = i % 2 ? TRACK_CPP : TRACK_C;
    }
    FoxProgressSnapshot before = FoxProgressCapture(&set, 2);
    ProgressRecord("c_last", 69);
    assert(FoxProgressCompleted(&set, 2, before) == FOX_MILESTONE_NONE);
    ProgressRecord("c_last", 70);
    assert(FoxProgressCompleted(&set, 2, before) == FOX_MILESTONE_LESSON);
    before = FoxProgressCapture(&set, 0);
    ProgressRecord("c_first", 100); // final outstanding lesson, not the last file
    assert(FoxProgressCompleted(&set, 0, before) == FOX_MILESTONE_COURSE);
    assert(!FoxProgressCapture(&set, 1).courseComplete); // C cannot graduate C++
    before = FoxProgressCapture(&set, 0);
    for (int i = 0; i < 5; i++) // reopening pages has no new transition
        assert(FoxProgressCompleted(&set, 0, before) == FOX_MILESTONE_NONE);
    char *saved = LoadFileText(path);
    assert(saved);
    for (int i = 0; i < 4; i++) FoxProgressCapture(&set, i);
    char *after = LoadFileText(path);
    assert(after && strcmp(saved, after) == 0); // read-only lookups do not save
    UnloadFileText(saved);
    UnloadFileText(after);
    ProgressRecord("c_first", 0); // retry preserves best and does not re-celebrate
    assert(ProgressBest("c_first") == 100);
    assert(FoxProgressCompleted(&set, 0, before) == FOX_MILESTONE_NONE);
    before = FoxProgressCapture(&set, 1);
    ProgressRecord("cpp_first", 100);
    assert(FoxProgressCompleted(&set, 1, before) == FOX_MILESTONE_LESSON);
    before = FoxProgressCapture(&set, 3);
    ProgressRecord("cpp_last", 100);
    assert(FoxProgressCompleted(&set, 3, before) == FOX_MILESTONE_COURSE);
    before = FoxProgressCapture(&set, 3);
    ProgressLoad(path); // restarts do not replay a graduation
    assert(FoxProgressCompleted(&set, 3, before) == FOX_MILESTONE_NONE);
    ProgressLoad("");
    assert(!FoxProgressCapture(&set, 0).lessonPassed);
    LessonSet empty = { 0 };
    assert(!FoxProgressCapture(&empty, 0).courseComplete);
    free(set.items);
    remove(path);
    puts("All mascot event tests passed");
    return 0;
}
