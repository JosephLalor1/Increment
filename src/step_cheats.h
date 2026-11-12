#ifndef STEP_CHEATS_H
#define STEP_CHEATS_H

#include "lesson.h"

// Ways a program can print a step's target without doing the step, that reading the code
// shows (no run needed). Like step_check.h, nothing is declared per step: each is worked
// out from the step's starter, solution, input and target. Data only, unit-tested
// (tests/test_steps.c). A step's own solution never trips them.
//
// 1. The answer written in. Numbers and words in the target that the step's solution
//    doesn't contain (nor its starter or input) can only come from computing them:
//    "Sum: 15" from a loop, "ecilA" from reversing "Alice". If they are sitting in the
//    learner's code as literals, the answer was typed in. Words count at once; of the
//    numbers, most must be there (one or both of one or two, more than half of more), so
//    a loop bound that happens to match one doesn't count, and 0 and 1 never do.
// 2. A function skipped. Where the solution rewrites a function's body (a stub to fill, a
//    bug inside it) and the learner's is still the starter's, the work was done somewhere
//    else, or not at all. Likewise a function the step is about must be used: called
//    where the solution adds a call to it, not stripped of the calls the starter had, and
//    written if the starter already names it (a method main calls, a class's destructor).
// 3. Indexing where the step is about pointer arithmetic (it names that skill at full
//    weight): if the lines the solution adds never index with [ ], the lines the learner
//    adds mustn't either. Declaring an array is not indexing.

typedef enum {
    CHEAT_NONE,
    CHEAT_ANSWER_WRITTEN,      // note: the literals, as code: 15, "ecilA"
    CHEAT_FUNCTION_UNTOUCHED,  // note: the function left as it came, uncalled or unwritten: maxOf()
    CHEAT_INDEXED,             // note: the indexing: arr[i]
} StepCheatKind;

#define STEP_CHEAT_NOTE 96

typedef struct {
    StepCheatKind kind;
    char note[STEP_CHEAT_NOTE];  // what gave it away, as code, to show the learner
} StepCheat;

// The first of them that code commits for this step, or CHEAT_NONE.
StepCheat StepCheatFind(const Step *step, const char *code);

#endif
