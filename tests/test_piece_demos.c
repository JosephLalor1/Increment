// Tests for the animated examples of new pieces of code (level/piece_demos.c): every
// example is well formed, shows the pieces it claims (so they light up in it), really
// prints and reads what its frames say (each is compiled and run), and every piece the
// levels show for the first time has one.
//
// usage: test_piece_demos <compiler bin folder> <empty folder>   (run from the repo folder)
#include "level/piece_demos.h"
#include "level/step_novelty.h"
#include "learning/lesson.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition, ...)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            printf("FAIL: " __VA_ARGS__);                                                                              \
            printf("\n");                                                                                              \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static int LineCount(const char *code)
{
    int lines = 1;
    for (const char *p = code; *p; p++) lines += *p == '\n';
    return lines;
}

// The variable a name or element ("a", "a[2]", "#1[1]") refers to in a frame's memory, or NULL.
static const DemoVar *Refers(const DemoVar *vars, int count, const char *what)
{
    char name[DEMO_NAME];
    snprintf(name, sizeof name, "%s", what);
    char *bracket = strchr(name, '[');
    int cell = bracket ? atoi(bracket + 1) : -1;
    if (bracket) *bracket = '\0';
    for (int i = 0; i < count; i++) {
        size_t length = strlen(name);
        bool same = strcmp(vars[i].name, name) == 0;
        bool member = strncmp(vars[i].name, name, length) == 0 && vars[i].name[length] == '.';  // an object
        if ((same || member) && cell < vars[i].cellCount) return &vars[i];
    }
    return NULL;
}

static void TestWellFormed(void)
{
    int count = 0;
    for (const PieceDemo *demo; (demo = PieceDemoAt(count)); count++) {
        int frames = PieceDemoFrameCount(demo), lines = LineCount(demo->code);
        CHECK(frames > 0, "demo '%s' has no frames", demo->pieces);
        for (int f = 0; f < frames; f++) {
            const DemoFrame *frame = &demo->frames[f];
            CHECK(frame->line <= lines, "demo '%s' frame %d runs line %d of %d", demo->pieces, f + 1, frame->line, lines);
            CHECK(!frame->eval == !frame->result, "demo '%s' frame %d: eval without result", demo->pieces, f + 1);
            DemoVar vars[DEMO_VARS];
            int count = DemoMemoryParse(frame->memory, vars, DEMO_VARS);
            int items = frame->memory && frame->memory[0] ? 1 : 0;
            for (const char *p = frame->memory; p && *p; p++) items += *p == ';';
            CHECK(count == items, "demo '%s' frame %d: memory '%s' read as %d", demo->pieces, f + 1, frame->memory, count);
            for (int v = 0; v < count; v++)
                if (vars[v].kind == DEMO_POINTER)
                    CHECK(Refers(vars, count, vars[v].target), "demo '%s' frame %d: %s points at nothing shown",
                          demo->pieces, f + 1, vars[v].name);
            if (frame->look)
                CHECK(Refers(vars, count, frame->look), "demo '%s' frame %d looks at '%s', not in memory", demo->pieces,
                      f + 1, frame->look);
        }
        // Each piece it claims is in its code, found the way the level finds new pieces, and
        // no other example claims it.
        char pieces[200];
        snprintf(pieces, sizeof pieces, "%s", demo->pieces);
        static CodePiece found[512];
        int foundCount = CodePiecesFind(demo->code, found, 512);
        for (char *piece = strtok(pieces, " "); piece; piece = strtok(NULL, " ")) {
            bool shown = false;
            for (int i = 0; i < foundCount && !shown; i++) shown = strcmp(found[i].key, piece) == 0;
            CHECK(shown, "demo '%s' never shows '%s' in its code", demo->pieces, piece);
            CHECK(PieceDemoFind(piece) == demo, "'%s' finds another demo first", piece);
        }
    }
    CHECK(count > 40, "only %d demos", count);
    CHECK(PieceDemoFind("for") && !PieceDemoFind("goto") && !PieceDemoFind(""), "lookup");
}

static void TestMemoryText(void)
{
    DemoVar vars[DEMO_VARS];
    int count = DemoMemoryParse("x=5;a=[4,8,\\0,...];p=&a[2];q=NULL;!k=7;#1=~[1,2];h=&#1;d.age=?", vars, DEMO_VARS);
    assert(count == 8);
    assert(vars[0].kind == DEMO_PLAIN && strcmp(vars[0].cells[0], "5") == 0);
    assert(vars[1].kind == DEMO_ARRAY && vars[1].cellCount == 4 && strcmp(vars[1].cells[2], "\\0") == 0);
    assert(vars[2].kind == DEMO_POINTER && strcmp(vars[2].target, "a") == 0 && vars[2].targetCell == 2);
    assert(vars[3].kind == DEMO_NULL);
    assert(vars[4].locked && strcmp(vars[4].name, "k") == 0);
    assert(vars[5].block && vars[5].freed && vars[5].cellCount == 2);
    assert(vars[6].kind == DEMO_POINTER && strcmp(vars[6].target, "#1") == 0 && vars[6].targetCell == -1);
    assert(strcmp(vars[7].name, "d.age") == 0 && strcmp(vars[7].cells[0], "?") == 0);
    assert(DemoMemoryParse("", vars, DEMO_VARS) == 0 && DemoMemoryParse(NULL, vars, DEMO_VARS) == 0);
}

// Every piece a level shows for the first time has an example.
static void TestEveryNewPieceHasOne(void)
{
    static LessonSet set;
    LessonsLoad(&set, "assets/lessons");
    CHECK(set.count > 0, "no lessons loaded");
    for (int l = 0; l < set.count; l++)
        for (int s = 0; s < set.items[l].stepCount; s++) {
            StepNovelty novelty;
            StepNoveltyFind(&set, l, s, &novelty);
            for (int i = 0; i < novelty.count; i++)
                CHECK(PieceDemoFind(novelty.pieces[i].key), "%s step %d shows '%s' for the first time with no demo",
                      set.items[l].id, s + 1, novelty.pieces[i].key);
        }
    LessonsUnload(&set);
}

// ---------------------------------------------------------------------------
// Running them

static char compilerBin[600], workDir[600];

static int TestRuns(void)
{
    int ran = 0;
    for (int n = 0; PieceDemoAt(n); n++) {
        const PieceDemo *demo = PieceDemoAt(n);
        char source[800], exe[800], input[800], out[800], command[6000];
        const char *extension = demo->cpp ? "cpp" : "c";
        snprintf(source, sizeof source, "%s\\demo%d.%s", workDir, n, extension);
        snprintf(exe, sizeof exe, "%s\\demo%d.exe", workDir, n);
        snprintf(input, sizeof input, "%s\\demo%d.in", workDir, n);
        snprintf(out, sizeof out, "%s\\demo%d.txt", workDir, n);

        // Lines run inside a main with the usual headers; a whole program gets the headers
        // if it names none.
        const char *headers = demo->cpp ? "#include <iostream>\n#include <cstring>\n#include <cmath>\nusing namespace std;\n"
                                        : "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n";
        FILE *file = fopen(source, "wb");
        assert(file);
        if (PieceDemoIsProgram(demo))
            fprintf(file, "%s%s\n", strstr(demo->code, "#include") ? "" : headers, demo->code);
        else
            fprintf(file, "%s%s {\n%s\nreturn 0;\n}\n", headers, demo->cpp ? "int main()" : "int main(void)", demo->code);
        fclose(file);

        // What the frames say is typed is the input; what they say is printed, the output.
        static char expected[4000];
        expected[0] = '\0';
        file = fopen(input, "wb");
        assert(file);
        for (int f = 0; f < PieceDemoFrameCount(demo); f++) {
            if (demo->frames[f].typed) fputs(demo->frames[f].typed, file);
            if (demo->frames[f].printed) strcat(expected, demo->frames[f].printed);
        }
        fclose(file);

        snprintf(command, sizeof command,
                 "set \"PATH=%s;%%PATH%%\" && \"%s\\%s\" -static -Wall -Wextra -Werror \"%s\" -o \"%s\" && \"%s\" < \"%s\" > \"%s\"",
                 compilerBin, compilerBin, demo->cpp ? "g++.exe" : "gcc.exe", source, exe, exe, input, out);
        int status = system(command);
        CHECK(status == 0, "demo '%s' didn't build or run (%d)", demo->pieces, status);
        if (status != 0) continue;
        ran++;

        static char output[4000];
        file = fopen(out, "rb");
        assert(file);
        size_t length = fread(output, 1, sizeof output - 1, file);
        fclose(file);
        output[length] = '\0';
        char *to = output;  // Windows line ends
        for (char *from = output; *from; from++)
            if (*from != '\r') *to++ = *from;
        *to = '\0';
        CHECK(strcmp(output, expected) == 0, "demo '%s' prints\n%s\nbut its frames say\n%s", demo->pieces, output, expected);
    }
    return ran;
}

int main(int argc, char **argv)
{
    TestMemoryText();
    TestWellFormed();
    TestEveryNewPieceHasOne();
    if (argc < 3) {
        printf("usage: test_piece_demos <compiler bin folder> <empty folder>\n");
        return 2;
    }
    snprintf(compilerBin, sizeof compilerBin, "%s", argv[1]);
    snprintf(workDir, sizeof workDir, "%s", argv[2]);
    int ran = TestRuns();
    if (failures) {
        printf("%d piece demo checks failed\n", failures);
        return 1;
    }
    printf("All piece demo tests passed (%d examples built and run)\n", ran);
    return 0;
}
