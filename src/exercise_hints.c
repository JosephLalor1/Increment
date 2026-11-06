// Sequential exercise hints, independent of rendering, editors, accounts and progress.
#include "exercise_hints.h"
#include <stddef.h>

void ExerciseHintsOpen(ExerciseHints *state, const Exercise *exercise)
{
    state->exercise = exercise;
    state->revealed = 0;
}

bool ExerciseHintsAvailable(const ExerciseHints *state)
{
    return state->exercise && state->revealed >= 0 && state->revealed < state->exercise->hintCount
        && state->revealed < MAX_EXERCISE_HINTS;
}

bool ExerciseHintsReveal(ExerciseHints *state)
{
    if (!ExerciseHintsAvailable(state)) return false;
    state->revealed++;
    return true;
}

const char *ExerciseHintsGet(const ExerciseHints *state, int index)
{
    if (!state->exercise || index < 0 || index >= state->revealed
        || index >= state->exercise->hintCount || index >= MAX_EXERCISE_HINTS) return NULL;
    return state->exercise->hints[index];
}
