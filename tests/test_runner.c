// End-to-end tests for src/runner.c: compiles and runs real programs with the bundled gcc.
// Run via test.bat. Takes about 10 seconds (one test waits for the 5-second time limit).
#include "code/runner.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while (0)

static void RunToEnd(Runner *r, const char *code, bool cpp, const char *input)
{
    RunnerStart(r, code, cpp, input);
    double start = GetTime();
    while (RunnerBusy(r) && GetTime() - start < 60) {
        RunnerUpdate(r);
        WaitTime(0.01);
    }
    if (getenv("RUNNER_TEST_VERBOSE")) {
        printf("--- result %d, exit %lu: %s\n%s%s\n", r->result, r->exitCode, r->summary,
               r->compileOutput ? r->compileOutput : "", r->programOutput ? r->programOutput : "");
    }
}

static void TestPureHelpers(void)
{
    int bad;
    CHECK(RunnerOutputMatches("a  \r\nb\n\n\n", "a\nb", &bad) && bad == -1);
    CHECK(!RunnerOutputMatches("a\nc\n", "a\nb\n", &bad) && bad == 2);
    CHECK(!RunnerOutputMatches("a\n", "a\nb\n", &bad) && bad == 2);   // missing line
    CHECK(!RunnerOutputMatches("a\nb\nc", "a\nb", &bad) && bad == 3);  // extra line
    CHECK(RunnerOutputMatches("", "\n", &bad));

    char line[64];
    RunnerGetLine("one\ntwo\nthree", 2, line, sizeof line);
    CHECK(strcmp(line, "two") == 0);
    RunnerGetLine("one", 5, line, sizeof line);
    CHECK(line[0] == '\0');

    Diagnostic d[8];
    const char *out =
        "main.c: In function 'main':\n"
        "main.c:4:5: error: 'x' undeclared (first use in this function)\n"
        "main.c:7:12: warning: unused variable 'y' [-Wunused-variable]\n"
        "main.c:4:5: note: each undeclared identifier is reported only once\n"
        "other.c:1:1: error: not ours\n";
    int n = RunnerParseDiagnostics(out, "main.c", d, 8);
    CHECK(n == 2);
    CHECK(d[0].line == 4 && d[0].col == 5 && d[0].error && strstr(d[0].message, "undeclared"));
    CHECK(d[1].line == 7 && !d[1].error);

    CHECK(RunnerCrashReason(0xC0000005UL) != NULL);
    CHECK(RunnerCrashReason(0) == NULL && RunnerCrashReason(1) == NULL);
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "test_runner");  // GetTime needs the window system running

    TestPureHelpers();

    char compiler[600];
    CHECK(RunnerFindCompiler(false, compiler, sizeof compiler));
    CHECK(RunnerFindCompiler(true, compiler, sizeof compiler));

    Runner r = { 0 };

    // A normal program, with output checked line by line
    RunToEnd(&r, "#include <stdio.h>\nint main() {\n    printf(\"Hello\\n\");\n    return 0;\n}", false, NULL);
    CHECK(r.result == RESULT_OK && r.exitCode == 0);
    CHECK(RunnerOutputMatches(r.programOutput, "Hello", NULL));

    // A non-zero exit code is not a crash
    RunToEnd(&r, "int main() { return 7; }", false, NULL);
    CHECK(r.result == RESULT_OK && r.exitCode == 7);

    // Compile error, with the line number found
    RunToEnd(&r, "#include <stdio.h>\nint main() {\n    int x = 1\n    return x;\n}", false, NULL);
    CHECK(r.result == RESULT_COMPILE_ERROR);
    CHECK(r.errorCount >= 1);
    CHECK(r.diagCount >= 1 && (r.diags[0].line == 3 || r.diags[0].line == 4));

    // Input reaches scanf; at end of input scanf sees EOF
    RunToEnd(&r,
             "#include <stdio.h>\nint main() {\n    int a, b;\n    scanf(\"%d %d\", &a, &b);\n"
             "    printf(\"%d\\n\", a + b);\n    int c = scanf(\"%d\", &a);\n    printf(\"%d\\n\", c);\n}",
             false, "3 4\n");
    CHECK(r.result == RESULT_OK && RunnerOutputMatches(r.programOutput, "7\n-1", NULL));

    // C++ with iostream (linked statically, so no DLLs are needed)
    RunToEnd(&r, "#include <iostream>\nint main() { std::cout << \"C++ \" << 2 * 21 << std::endl; }", true, NULL);
    CHECK(r.result == RESULT_OK && RunnerOutputMatches(r.programOutput, "C++ 42", NULL));

    // A null pointer crash is reported, with output before it kept
    RunToEnd(&r,
             "#include <stdio.h>\nint main() {\n    printf(\"before\\n\");\n    fflush(stdout);\n"
             "    int *p = 0;\n    *p = 5;\n    return 0;\n}",
             false, NULL);
    CHECK(r.result == RESULT_CRASH && r.exitCode == 0xC0000005UL);
    CHECK(strstr(r.programOutput, "before") != NULL);

    // An endless loop is stopped by the time limit
    double t0 = GetTime();
    RunToEnd(&r, "int main() { volatile int x = 0; while (1) x++; }", false, NULL);
    CHECK(r.result == RESULT_TIMEOUT);
    CHECK(GetTime() - t0 < RUN_TIME_LIMIT + 10);

    // Trying to grab 2 GB fails against the memory limit instead of exhausting the machine
    RunToEnd(&r,
             "#include <stdio.h>\n#include <stdlib.h>\nint main() {\n"
             "    void *p = malloc((size_t)2000 * 1024 * 1024);\n"
             "    printf(\"%s\\n\", p ? \"got it\" : \"refused\");\n}",
             false, NULL);
    CHECK(r.result == RESULT_OK && RunnerOutputMatches(r.programOutput, "refused", NULL));

    RunnerFree(&r);
    CloseWindow();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All runner tests passed\n");
    return 0;
}
