#ifndef STEP_CHECK_H
#define STEP_CHECK_H

#include "lesson.h"
#include "skills.h"

#include <stdbool.h>

// Checks that a program which prints a step's target really solves the step, rather than
// just printing the answer. Nothing is declared per step: both checks are worked out from the
// step's own starter and solution. Data only, unit-tested (tests/test_steps.c); the level
// screen runs the programs.
//
// 1. A second run with different values. The step's "givens" are the literal values that its
//    starter and its solution both set ("int a = 3;" in both: the learner is meant to work
//    with a, not replace it). Changing every given, and every number in the step's input, the
//    same way in the solution and in the learner's code must still give matching output. So
//    "a = 8; b = 3;" passes a swap with 3 and 8 but not with 4 and 10, and printing the
//    answer as text fails as soon as the values change. If the learner's code no longer sets
//    a given to its value (they edited the given instead), that is caught without a run.
// 2. Missing constructs. If the solution needs a loop, a condition, an array, a function, a
//    pointer, input and so on (code_skills.h) and the learner's code has none, it didn't do
//    what the step is for (five printf lines instead of a loop).
//
// A step marked "@step fixed" skips the second run, for the rare program whose given values
// can't change safely (a size that must match an initialiser list, say).

#define STEP_CHECK_SUMMARY 160

// A step's second run: its solution and input with the given values changed.
typedef struct {
    char *solution;  // malloc'd
    char *input;     // malloc'd, or NULL when the step has no input
    int changes;     // values changed in the solution plus numbers changed in the input
    char summary[STEP_CHECK_SUMMARY];  // the changed givens as code: "a = 4, b = 10"
} StepVariation;

// Builds the step's second run. Returns false (and an empty variation) when the step has
// nothing to vary or is marked fixed.
bool StepVariationMake(const Step *step, StepVariation *out);
void StepVariationFree(StepVariation *variation);

// The learner's code with the step's givens changed as in StepVariationMake (malloc'd).
// *missing is how many givens the code no longer sets to their values, and missingSummary
// (room for STEP_CHECK_SUMMARY) lists those as code: "a = 3, b = 8".
char *StepVariationApply(const Step *step, const char *code, int *missing, char *missingSummary);

// The constructs the step's solution uses that code doesn't, in catalogue order; returns how
// many (at most max).
int StepMissingConstructs(const Step *step, const char *code, bool cpp, Skill *out, int max);

#endif
