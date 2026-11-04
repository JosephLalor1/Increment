#ifndef RUNNER_H
#define RUNNER_H

#include <stdbool.h>
#include <stddef.h>

// Compiles and runs a learner's program in the background: call RunnerStart, then
// RunnerUpdate once per frame until RunnerBusy is false.
//
// The compiler is the gcc/g++ from the w64devkit bundled with raylib (see
// RunnerFindCompiler). Programs run with a time limit and the memory limit from
// platform.h, and get `input` on stdin.

#define RUN_TIME_LIMIT     5.0   // seconds, for the program
#define COMPILE_TIME_LIMIT 30.0  // seconds, for the compiler
#define RUNNER_MAX_DIAGS   64

typedef enum { RUN_IDLE, RUN_COMPILING, RUN_RUNNING, RUN_DONE } RunPhase;

typedef enum {
    RESULT_NONE,
    RESULT_OK,             // compiled and ran to completion (any exit code)
    RESULT_COMPILE_ERROR,
    RESULT_CRASH,          // e.g. access violation, stack overflow
    RESULT_TIMEOUT,
    RESULT_NO_COMPILER,
    RESULT_FAILED,         // couldn't write files or start a process
} RunResult;

typedef struct {
    int line, col;
    bool error;            // false for warnings
    char message[200];
} Diagnostic;

typedef struct PlatformProcess PlatformProcess;

typedef struct {
    RunPhase phase;
    RunResult result;
    bool cpp;
    char *compileOutput;   // compiler messages (warnings appear even when it succeeds)
    char *programOutput;
    bool outputTruncated;
    unsigned long exitCode;
    double runSeconds;
    Diagnostic diags[RUNNER_MAX_DIAGS];
    int diagCount, errorCount, warningCount;
    char summary[256];     // one line for the status bar

    // internal
    PlatformProcess *proc;
    double phaseStart;
    char *input;
    char workDir[600];
    char compiler[600];
    char exeName[32];      // a new name per build, so a still-locked old .exe can't block it
    int buildCount;
} Runner;

void RunnerStart(Runner *r, const char *code, bool cpp, const char *input);
void RunnerUpdate(Runner *r);
bool RunnerBusy(const Runner *r);
void RunnerCancel(Runner *r);
void RunnerFree(Runner *r);

// Finds gcc (or g++). Looks in INCREMENT_COMPILER_DIR, a w64devkit next to the app,
// RAYLIB_DIR and C:\raylib, then PATH.
bool RunnerFindCompiler(bool cpp, char *out, size_t size);

// Plain-English explanation of a Windows crash exit code, or NULL if it isn't one.
const char *RunnerCrashReason(unsigned long exitCode);

// Compares program output with the expected output, ignoring trailing spaces, '\r' and
// trailing blank lines. On a mismatch, sets *badLine to the first differing line (1-based).
bool RunnerOutputMatches(const char *got, const char *want, int *badLine);

// Copies line n (1-based) of text into out ("" if there is no such line).
void RunnerGetLine(const char *text, int n, char *out, size_t size);

// Pulls file:line:col diagnostics out of gcc output. Exposed for tests.
int RunnerParseDiagnostics(const char *output, const char *fileName, Diagnostic *out, int max);

#endif
