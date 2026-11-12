// Compiles and runs learners' programs in the background and interprets the results.
// See runner.h.
#include "code/runner.h"
#include "core/text_copy.h"
#include "core/platform.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Removes '\r' so Windows line endings don't show up as stray characters.
static char *StripCR(const char *s)
{
    char *out = malloc(strlen(s) + 1);
    size_t o = 0;
    for (; *s; s++) if (*s != '\r') out[o++] = *s;
    out[o] = '\0';
    return out;
}

static bool TryDir(const char *dir, const char *exe, char *out, size_t size)
{
    if (!dir || !dir[0]) return false;
    char path[1024];  // room for a 600-byte dir plus the exe name
    snprintf(path, sizeof path, "%s/%s", dir, exe);
    if (!FileExists(path)) return false;
    snprintf(out, size, "%s", path);
    return true;
}

bool RunnerFindCompiler(bool cpp, char *out, size_t size)
{
    const char *exe = cpp ? "g++.exe" : "gcc.exe";
    char dir[600];
    if (TryDir(getenv("INCREMENT_COMPILER_DIR"), exe, out, size)) return true;
    snprintf(dir, sizeof dir, "%sw64devkit/bin", GetApplicationDirectory());  // bundled with the app
    if (TryDir(dir, exe, out, size)) return true;
    const char *raylib = getenv("RAYLIB_DIR");
    if (raylib) {
        snprintf(dir, sizeof dir, "%s/w64devkit/bin", raylib);
        if (TryDir(dir, exe, out, size)) return true;
    }
    if (TryDir("C:/raylib/w64devkit/bin", exe, out, size)) return true;
    return PlatformFindOnPath(exe, out, size);
}

const char *RunnerCrashReason(unsigned long code)
{
    switch (code) {
    case 0xC0000005UL: return "invalid memory access: a bad or null pointer, or reading past the end of an array";
    case 0xC00000FDUL: return "stack overflow: often a function that calls itself forever";
    case 0xC0000094UL: return "integer division by zero";
    case 0xC000008EUL: return "floating-point division by zero";
    case 0xC0000409UL: return "the program detected memory corruption (a buffer overrun) and stopped itself";
    case 0xC0000374UL: return "heap corruption: often deleting or freeing memory twice, or writing past an allocation";
    case 0xC0000017UL: return "out of memory";
    case 0xC000001DUL: return "illegal instruction";
    case 3:            return "the program aborted (for example an uncaught exception, a failed new, or abort())";
    default:           return NULL;
    }
}

int RunnerParseDiagnostics(const char *output, const char *fileName, Diagnostic *out, int max)
{
    int count = 0;
    size_t prefix = strlen(fileName);
    const char *p = output;
    while (*p && count < max) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        char line[512];
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';

        // gcc format: main.c:LINE:COL: error: message
        if (strncmp(line, fileName, prefix) == 0 && line[prefix] == ':') {
            int ln, col;
            char kind[32] = "", msg[200] = "";
            if (sscanf(line + prefix + 1, "%d:%d: %31[^:]: %199[^\n]", &ln, &col, kind, msg) == 4) {
                bool error = strcmp(kind, "error") == 0 || strcmp(kind, "fatal error") == 0;
                if (error || strcmp(kind, "warning") == 0) {
                    out[count].line = ln;
                    out[count].col = col;
                    out[count].error = error;
                    snprintf(out[count].message, sizeof out[count].message, "%s", msg);
                    count++;
                }
            }
        }
        p = end ? end + 1 : p + n;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Output comparison

// Splits text into lines with trailing spaces and '\r' removed, dropping trailing blank lines.
static int SplitLines(const char *text, char ***outLines)
{
    int cap = 16, count = 0;
    char **lines = malloc(sizeof(char *) * (size_t)cap);
    const char *p = text;
    while (true) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\r' || p[n - 1] == '\t')) n--;
        if (count == cap) {
            cap *= 2;
            lines = realloc(lines, sizeof(char *) * (size_t)cap);
        }
        lines[count] = malloc(n + 1);
        memcpy(lines[count], p, n);
        lines[count][n] = '\0';
        count++;
        if (!end) break;
        p = end + 1;
    }
    while (count > 0 && lines[count - 1][0] == '\0') free(lines[--count]);
    *outLines = lines;
    return count;
}

bool RunnerOutputMatches(const char *got, const char *want, int *badLine)
{
    char **g, **w;
    int gn = SplitLines(got ? got : "", &g), wn = SplitLines(want ? want : "", &w);
    int bad = -1;
    for (int i = 0; i < gn || i < wn; i++) {
        if (i >= gn || i >= wn || strcmp(g[i], w[i]) != 0) {
            bad = i + 1;
            break;
        }
    }
    for (int i = 0; i < gn; i++) free(g[i]);
    for (int i = 0; i < wn; i++) free(w[i]);
    free(g);
    free(w);
    if (badLine) *badLine = bad;
    return bad < 0;
}

void RunnerGetLine(const char *text, int n, char *out, size_t size)
{
    out[0] = '\0';
    const char *p = text ? text : "";
    for (int line = 1; line < n; line++) {
        p = strchr(p, '\n');
        if (!p) return;
        p++;
    }
    size_t len = strcspn(p, "\r\n");
    if (len >= size) len = size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}

// ---------------------------------------------------------------------------
// Running

static void SetText(char **dst, const char *s)
{
    free(*dst);
    *dst = StripCR(s);
}

static void Finish(Runner *r, RunResult result, const char *summary)
{
    r->phase = RUN_DONE;
    r->result = result;
    snprintf(r->summary, sizeof r->summary, "%s", summary);
    PlatformProcessFree(r->proc);
    r->proc = NULL;
}

static bool EnsureWorkDir(Runner *r)
{
    if (r->workDir[0]) return true;
    char temp[400];
    unsigned char rnd[4];
    if (!PlatformTempDir(temp, sizeof temp) || !PlatformRandom(rnd, sizeof rnd)) return false;
    // A per-run-of-the-app folder, so two copies of the app never clash.
    snprintf(r->workDir, sizeof r->workDir, "%sincrement_run_%02x%02x%02x%02x", temp, rnd[0], rnd[1], rnd[2], rnd[3]);
    if (!DirectoryExists(r->workDir) && MakeDirectory(r->workDir) != 0) {
        r->workDir[0] = '\0';
        return false;
    }
    return true;
}

void RunnerStart(Runner *r, const char *code, bool cpp, const char *input)
{
    RunnerCancel(r);
    r->cpp = cpp;
    r->result = RESULT_NONE;
    r->diagCount = r->errorCount = r->warningCount = 0;
    r->exitCode = 0;
    r->runSeconds = 0;
    r->outputTruncated = false;
    SetText(&r->compileOutput, "");
    SetText(&r->programOutput, "");
    free(r->input);
    r->input = CopyText(input ? input : "");

    if (!RunnerFindCompiler(cpp, r->compiler, sizeof r->compiler)) {
        Finish(r, RESULT_NO_COMPILER, cpp ? "No C++ compiler (g++) found." : "No C compiler (gcc) found.");
        return;
    }
    const char *src = cpp ? "main.cpp" : "main.c";
    if (!EnsureWorkDir(r)) {
        Finish(r, RESULT_FAILED, "Could not create a temporary folder.");
        return;
    }
    char path[700];
    snprintf(path, sizeof path, "%s/%s", r->workDir, src);
    FILE *f = fopen(path, "wb");
    if (!f) {
        Finish(r, RESULT_FAILED, "Could not save the code to a temporary file.");
        return;
    }
    fputs(code, f);
    fputc('\n', f);
    fclose(f);
    // Each build gets a fresh exe name, so a failed build can never run the previous program,
    // and an old exe still locked (by a dying process or a virus scanner) can't block linking.
    if (r->exeName[0]) {
        snprintf(path, sizeof path, "%s/%s", r->workDir, r->exeName);
        remove(path);
    }
    snprintf(r->exeName, sizeof r->exeName, "prog%d.exe", ++r->buildCount);

    // The compiler must find its own assembler and linker first, not another toolchain's on PATH.
    char binDir[600];
    snprintf(binDir, sizeof binDir, "%s", r->compiler);
    for (char *c = binDir; *c; c++) if (*c == '/') *c = '\\';
    char *slash = strrchr(binDir, '\\');
    if (slash) {
        *slash = '\0';
        PlatformPrependPath(binDir);
    }

    char cmd[1600];
    snprintf(cmd, sizeof cmd, "\"%s\" %s -Wall -fdiagnostics-color=never -fmax-errors=30 -static -o %s %s",
             r->compiler, cpp ? "-std=c++17" : "-std=c11", r->exeName, src);
    char err[256];
    r->proc = PlatformProcessStart(cmd, r->workDir, NULL, err, sizeof err);
    if (!r->proc) {
        Finish(r, RESULT_FAILED, err);
        return;
    }
    r->phase = RUN_COMPILING;
    r->phaseStart = GetTime();
    snprintf(r->summary, sizeof r->summary, "Compiling...");
}

static void StartProgram(Runner *r)
{
    char cmd[700], err[256];
    snprintf(cmd, sizeof cmd, "\"%s/%s\"", r->workDir, r->exeName);
    r->proc = PlatformProcessStart(cmd, r->workDir, r->input, err, sizeof err);
    if (!r->proc) {
        Finish(r, RESULT_FAILED, err);
        return;
    }
    r->phase = RUN_RUNNING;
    r->phaseStart = GetTime();
    snprintf(r->summary, sizeof r->summary, "Running...");
}

void RunnerUpdate(Runner *r)
{
    if (r->phase != RUN_COMPILING && r->phase != RUN_RUNNING) return;
    bool running = PlatformProcessUpdate(r->proc);
    double elapsed = GetTime() - r->phaseStart;

    if (r->phase == RUN_COMPILING) {
        if (running && elapsed > COMPILE_TIME_LIMIT) {
            PlatformProcessKill(r->proc);
            Finish(r, RESULT_FAILED, "The compiler took too long and was stopped.");
            return;
        }
        if (running) return;

        SetText(&r->compileOutput, PlatformProcessOutput(r->proc));
        r->diagCount = RunnerParseDiagnostics(r->compileOutput, r->cpp ? "main.cpp" : "main.c", r->diags, RUNNER_MAX_DIAGS);
        for (int i = 0; i < r->diagCount; i++) {
            if (r->diags[i].error) r->errorCount++;
            else r->warningCount++;
        }
        unsigned long code = PlatformProcessExitCode(r->proc);
        PlatformProcessFree(r->proc);
        r->proc = NULL;

        char exe[700];
        snprintf(exe, sizeof exe, "%s/%s", r->workDir, r->exeName);
        if (code != 0 || !FileExists(exe)) {
            char msg[128];
            if (r->errorCount) snprintf(msg, sizeof msg, "Didn't compile: %d error%s.", r->errorCount, r->errorCount == 1 ? "" : "s");
            else snprintf(msg, sizeof msg, "Didn't compile. See the messages below.");
            Finish(r, RESULT_COMPILE_ERROR, msg);
            return;
        }
        StartProgram(r);
        return;
    }

    // RUN_RUNNING
    if (running && elapsed > RUN_TIME_LIMIT) {
        PlatformProcessKill(r->proc);
        SetText(&r->programOutput, PlatformProcessOutput(r->proc));
        r->outputTruncated = PlatformProcessTruncated(r->proc);
        r->runSeconds = elapsed;
        Finish(r, RESULT_TIMEOUT, "Stopped after 5 seconds. Is there a loop that never ends, or is it waiting for input?");
        return;
    }
    if (running) return;

    SetText(&r->programOutput, PlatformProcessOutput(r->proc));
    r->outputTruncated = PlatformProcessTruncated(r->proc);
    r->exitCode = PlatformProcessExitCode(r->proc);
    r->runSeconds = elapsed;
    const char *crash = RunnerCrashReason(r->exitCode);
    char msg[256];
    if (crash) {
        snprintf(msg, sizeof msg, "Crashed: %s.", crash);
        Finish(r, RESULT_CRASH, msg);
    } else {
        snprintf(msg, sizeof msg, "Finished in %.2f s with exit code %lu.", r->runSeconds, r->exitCode);
        Finish(r, RESULT_OK, msg);
    }
}

bool RunnerBusy(const Runner *r) { return r->phase == RUN_COMPILING || r->phase == RUN_RUNNING; }

void RunnerCancel(Runner *r)
{
    if (r->proc) {
        PlatformProcessFree(r->proc);
        r->proc = NULL;
    }
    if (RunnerBusy(r)) {
        r->phase = RUN_DONE;
        r->result = RESULT_NONE;
        snprintf(r->summary, sizeof r->summary, "Stopped.");
    }
}

void RunnerFree(Runner *r)
{
    RunnerCancel(r);
    free(r->compileOutput);
    free(r->programOutput);
    free(r->input);
    if (r->workDir[0]) PlatformRemoveDir(r->workDir);
    memset(r, 0, sizeof *r);
}
