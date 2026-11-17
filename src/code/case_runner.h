#ifndef CASE_RUNNER_H
#define CASE_RUNNER_H

#include "code/runner.h"

// Runs one program on several test cases: compiles it once, then runs it with each case's
// input in turn and keeps what each run printed. Used by a level's steps (one case per
// "@input"/"@expect" pair) and by the code editor's test cases. Call CaseRunnerStart, then
// CaseRunnerUpdate once per frame until CaseRunnerBusy is false.
//
// A build that fails gives every case the same result (a compile error, say), and the
// compiler's diagnostics stay in `runner`. A case that crashes doesn't stop the others; one
// that times out does (the rest would most likely wait as long), and the cases after it are
// left unrun (RESULT_NONE).

#define CASE_RUNNER_MAX 8

// What one case's run did.
typedef struct {
    RunResult result;       // RESULT_NONE until it has run
    char *output;           // what it printed (malloc'd; "" before it runs)
    bool truncated;         // there was too much output to keep
    unsigned long exitCode;
    char summary[256];      // one line, as Runner.summary
} CaseOutcome;

typedef struct {
    Runner runner;          // builds the program, then runs it per case
    char *inputs[CASE_RUNNER_MAX];
    int count;              // cases in this run
    int running;            // the case being run; count once all are done (or stopped)
    CaseOutcome outcomes[CASE_RUNNER_MAX];
    bool buildFailed;       // it never got as far as running: every case has the build's result
} CaseRunner;

// inputs[i] is case i's stdin (NULL for none); count is clamped to 1..CASE_RUNNER_MAX.
void CaseRunnerStart(CaseRunner *c, const char *code, bool cpp, const char *const *inputs, int count);
void CaseRunnerUpdate(CaseRunner *c);
bool CaseRunnerBusy(const CaseRunner *c);
void CaseRunnerCancel(CaseRunner *c);
void CaseRunnerFree(CaseRunner *c);

#endif
