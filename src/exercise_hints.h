#ifndef EXERCISE_HINTS_H
#define EXERCISE_HINTS_H

#include "lesson.h"

// Transient hint state for one open exercise. It owns no text and never saves progress.
typedef struct {
    const Exercise *exercise;
    int revealed;
} ExerciseHints;

// Opening an exercise (or NULL for playground/snippet) starts with no revealed hints.
void ExerciseHintsOpen(ExerciseHints *state, const Exercise *exercise);
// Whether another optional hint is available.
bool ExerciseHintsAvailable(const ExerciseHints *state);
// Reveal exactly one more hint; false when none remain. Does not change code or scores.
bool ExerciseHintsReveal(ExerciseHints *state);
// Read an already revealed hint by zero-based index; NULL for hidden/invalid hints.
const char *ExerciseHintsGet(const ExerciseHints *state, int index);

#endif
