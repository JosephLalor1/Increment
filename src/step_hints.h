#ifndef STEP_HINTS_H
#define STEP_HINTS_H

#include <stdbool.h>

// Hints for a coding step, made of code rather than words. Comparing the learner's code with
// the step's solution (line_diff.h) finds the first place they differ; the screen then shows,
// one hint at a time: which line to work on, the shape of the line that belongs there (names
// and values blanked out), and finally the line itself. Data only, unit-tested
// (tests/test_steps.c).

typedef enum {
    STEP_HINT_NONE,    // nothing differs from the solution
    STEP_HINT_CHANGE,  // line `line` should become `target`
    STEP_HINT_ADD,     // `target` belongs before line `line` (line may be one past the end)
    STEP_HINT_REMOVE,  // line `line` doesn't belong
} StepHintKind;

#define STEP_HINT_TEXT 256

typedef struct {
    StepHintKind kind;
    int line;                      // zero-based line in the learner's code
    char target[STEP_HINT_TEXT];   // the solution's line, trimmed; "" for STEP_HINT_REMOVE
    char shape[STEP_HINT_TEXT];    // target with its names and values blanked (StepHintShape)
} StepHint;

// How many hints a step offers, in reveal order: the line, its shape, then the line itself.
#define STEP_HINT_LEVELS 3

// The first difference between code and solution.
StepHint StepHintFind(const char *code, const char *solution);

// A line's shape: keywords, well-known library names, operators and punctuation stay; other
// names and numbers become "_"; inside quotes, format specifiers such as %d, escapes such as \n
// and spaces stay and the rest becomes "_". `printf("Total: %d\n", sum + 1);` has the shape
// `printf("_ %d\n", _ + _);`. Preprocessor lines are kept whole.
void StepHintShape(const char *line, char *out, int size);

#endif
