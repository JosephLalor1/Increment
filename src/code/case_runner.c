// Runs one program on several test cases, building it once. See case_runner.h.
#include "code/case_runner.h"
#include "core/text_copy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ClearOutcome(CaseOutcome *o)
{
    free(o->output);
    memset(o, 0, sizeof *o);
}

// Copies what the runner's latest run did into case index's outcome.
static void Keep(CaseRunner *c, int index)
{
    CaseOutcome *o = &c->outcomes[index];
    const Runner *r = &c->runner;
    free(o->output);
    o->output = CopyText(r->programOutput ? r->programOutput : "");
    o->result = r->result;
    o->truncated = r->outputTruncated;
    o->exitCode = r->exitCode;
    snprintf(o->summary, sizeof o->summary, "%s", r->summary);
}

// The build failed: every case shares its result, and nothing more runs.
static void BuildFailed(CaseRunner *c)
{
    for (int i = 0; i < c->count; i++) Keep(c, i);
    c->running = c->count;
    c->buildFailed = true;
}

void CaseRunnerStart(CaseRunner *c, const char *code, bool cpp, const char *const *inputs, int count)
{
    CaseRunnerCancel(c);
    if (count < 1) count = 1;
    if (count > CASE_RUNNER_MAX) count = CASE_RUNNER_MAX;
    for (int i = 0; i < CASE_RUNNER_MAX; i++) {
        ClearOutcome(&c->outcomes[i]);
        free(c->inputs[i]);
        c->inputs[i] = i < count && inputs && inputs[i] ? CopyText(inputs[i]) : NULL;
        c->outcomes[i].output = CopyText("");
    }
    c->count = count;
    c->running = 0;
    c->buildFailed = false;
    // Runner compiles, then runs the program on case 0's input.
    RunnerStart(&c->runner, code, cpp, c->inputs[0]);
    if (!RunnerBusy(&c->runner)) BuildFailed(c);  // no compiler, or nowhere to build
}

void CaseRunnerUpdate(CaseRunner *c)
{
    if (c->running >= c->count) return;
    bool compiling = c->runner.phase == RUN_COMPILING;
    RunnerUpdate(&c->runner);
    if (RunnerBusy(&c->runner)) return;
    if (compiling) {  // it never got as far as running the program
        BuildFailed(c);
        return;
    }
    Keep(c, c->running);
    bool timedOut = c->runner.result == RESULT_TIMEOUT;
    c->running++;
    if (timedOut) c->running = c->count;  // the others would most likely wait as long
    while (c->running < c->count && !RunnerRunAgain(&c->runner, c->inputs[c->running])) {
        Keep(c, c->running);  // couldn't start: say why, and try the next
        c->running++;
    }
}

bool CaseRunnerBusy(const CaseRunner *c) { return c->running < c->count; }

void CaseRunnerCancel(CaseRunner *c)
{
    RunnerCancel(&c->runner);
    c->running = c->count;
}

void CaseRunnerFree(CaseRunner *c)
{
    RunnerFree(&c->runner);
    for (int i = 0; i < CASE_RUNNER_MAX; i++) {
        ClearOutcome(&c->outcomes[i]);
        free(c->inputs[i]);
        c->inputs[i] = NULL;
    }
    c->count = c->running = 0;
}
