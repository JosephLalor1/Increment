// Tests for the picture of a program's memory (src/memory_probe.c): what the code alone
// says, and what an instrumented run reports. The run tests compile real programs.
//
// usage: test_probe <compiler bin folder> <empty folder>
#include "../src/memory_probe.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const ProbeVar *Var(const ProbeScan *scan, const char *name)
{
    for (int i = 0; i < scan->count; i++)
        if (strcmp(scan->vars[i].name, name) == 0) return &scan->vars[i];
    return NULL;
}

// Whether the pointer is guessed to point at a variable's element.
static bool Aims(const ProbeScan *scan, const char *pointer, const char *target, int element)
{
    const ProbeVar *var = Var(scan, pointer);
    return var && var->guess.kind == PROBE_TARGET_VAR && strcmp(scan->vars[var->guess.index].name, target) == 0
        && var->guess.element == element;
}

static void TestDeclarations(void)
{
    ProbeScan scan;
    ProbeScanCode("#include <stdio.h>\n"
                  "int glVar = 10;\n"
                  "int add(int a, int b) { int inner = a; return inner + b; }\n"
                  "void later(int x);\n"
                  "int main() {\n"
                  "    int i = 5, j = 10;\n"
                  "    int* ptr;\n"
                  "    int **pptr = &ptr;\n"
                  "    float a[4] = {0};\n"
                  "    char name[20] = \"int fake = 1;\";  // int alsoFake;\n"
                  "    double d; bool flag = true;\n"
                  "    int matrix[3][4];\n"
                  "    int (*fn)(int, int) = add;\n"
                  "    if (i > 2) { int hidden = 1; }\n"
                  "    for (int k = 0; k < 3; k++) { }\n"
                  "    i = add(i, j);\n"
                  "    return 0;\n"
                  "}\n",
                  &scan);
    assert(scan.count == 9 && scan.hasPointers);
    assert(Var(&scan, "glVar") && Var(&scan, "glVar")->global && Var(&scan, "glVar")->line == 1);
    assert(Var(&scan, "i")->kind == PROBE_INT && Var(&scan, "i")->pointerDepth == 0 && !Var(&scan, "i")->global);
    assert(Var(&scan, "j") && Var(&scan, "j")->line == 5);
    assert(Var(&scan, "ptr")->pointerDepth == 1 && Var(&scan, "pptr")->pointerDepth == 2);
    assert(Var(&scan, "a")->isArray && Var(&scan, "a")->kind == PROBE_FLOAT);
    assert(Var(&scan, "name")->isArray && Var(&scan, "name")->kind == PROBE_CHAR);
    assert(Var(&scan, "d")->kind == PROBE_FLOAT && Var(&scan, "flag")->kind == PROBE_BOOL);
    // Not variables of main: parameters, other functions' locals, nested blocks, text in
    // strings and comments, function pointers, arrays of arrays.
    assert(!Var(&scan, "inner") && !Var(&scan, "x") && !Var(&scan, "hidden") && !Var(&scan, "k"));
    assert(!Var(&scan, "fake") && !Var(&scan, "alsoFake") && !Var(&scan, "fn") && !Var(&scan, "matrix"));

    // C++: objects and pointers to them; references and calls aren't variables.
    ProbeScanCode("#include <iostream>\nusing namespace std;\n"
                  "struct Node { int data; Node* nextPtr; };\n"
                  "class Point { public: double x, y; Point(double a, double b) : x(a), y(b) {} };\n"
                  "float Maths::PI = 0;\n"
                  "int main() {\n"
                  "    Node* headPtr = nullptr;\n"
                  "    Point q(2.0, 3.0);\n"
                  "    Point* p = new Point(1, 2);\n"
                  "    int a = 5;\n"
                  "    int& ref = a;\n"
                  "    cout << a << endl;\n"
                  "    q.x = a * 2;\n"
                  "    delete p;\n"
                  "    return 0;\n"
                  "}\n",
                  &scan);
    assert(scan.count == 4);
    assert(Var(&scan, "headPtr")->kind == PROBE_OPAQUE && Var(&scan, "headPtr")->pointerDepth == 1);
    assert(Var(&scan, "headPtr")->guess.kind == PROBE_TARGET_NULL);
    assert(Var(&scan, "q")->kind == PROBE_OPAQUE && Var(&scan, "q")->pointerDepth == 0);
    assert(Var(&scan, "p")->guess.kind == PROBE_TARGET_FREED);  // new, then delete
    assert(!Var(&scan, "ref") && !Var(&scan, "PI") && !Var(&scan, "Maths"));

    // No pointers, or no main.
    ProbeScanCode("int main() { int x = 1; return x; }", &scan);
    assert(scan.count == 1 && !scan.hasPointers && scan.injectAt > 0);
    ProbeScanCode("int helper() { return 1; }", &scan);
    assert(scan.injectAt < 0);
    ProbeScanCode(NULL, &scan);
    assert(scan.count == 0);
}

static void TestGuesses(void)
{
    ProbeScan scan;
    // The professor's walk through i, j, ptr and pptr.
    ProbeScanCode("int main() {\n"
                  "    int i = 2, j = 8;\n"
                  "    int* ptr;\n"
                  "    int** pptr;\n"
                  "    ptr = &i;\n"
                  "    pptr = &ptr;\n"
                  "    *ptr = 7;\n"
                  "    **pptr = 12;\n"
                  "    ptr = &j;\n"
                  "    return 0;\n"
                  "}\n",
                  &scan);
    assert(Aims(&scan, "ptr", "j", 0) && Aims(&scan, "pptr", "ptr", 0));
    // "*pptr = &i" aims ptr, not pptr.
    ProbeScanCode("int main() { int i, j; int* ptr = &j; int** pptr = &ptr; *pptr = &i; return 0; }", &scan);
    assert(Aims(&scan, "ptr", "i", 0) && Aims(&scan, "pptr", "ptr", 0));

    // Along an array: his float a[4] example.
    ProbeScanCode("int main() {\n"
                  "    float a[4];\n"
                  "    float* ptr;\n"
                  "    ptr = &(a[2]);\n"
                  "    ptr++;\n"
                  "    ptr = ptr - 3;\n"
                  "    ptr += 2;\n"
                  "    return 0;\n"
                  "}\n",
                  &scan);
    assert(Aims(&scan, "ptr", "a", 2));
    ProbeScanCode("int main() { int arr[5]; int* p = arr; int* q = arr + 3; int* r = &arr[1]; --q; return 0; }", &scan);
    assert(Aims(&scan, "p", "arr", 0) && Aims(&scan, "q", "arr", 2) && Aims(&scan, "r", "arr", 1));

    // Null, the heap, a copy of another pointer, and nothing known.
    ProbeScanCode("#include <stdlib.h>\n"
                  "int main() {\n"
                  "    int x;\n"
                  "    int* n = NULL;\n"
                  "    int* h = (int*)malloc(sizeof(int));\n"
                  "    int* c = &x;\n"
                  "    int* copy = c;\n"
                  "    int* blank;\n"
                  "    int* gone = (int*)malloc(4);\n"
                  "    free(gone);\n"
                  "    return 0;\n"
                  "}\n",
                  &scan);
    assert(Var(&scan, "n")->guess.kind == PROBE_TARGET_NULL && Var(&scan, "h")->guess.kind == PROBE_TARGET_HEAP);
    assert(Aims(&scan, "copy", "x", 0));
    assert(Var(&scan, "blank")->guess.kind == PROBE_TARGET_UNKNOWN && Var(&scan, "gone")->guess.kind == PROBE_TARGET_FREED);
}

static void TestReadingAReport(void)
{
    ProbeSnapshot snap;
    const char *report = "the program's own output\n"
                         "@@MEM BEGIN\n"
                         "@@MEM V i 1000 4 1 i 0 -25\n"
                         "@@MEM V c 1004 1 1 c 0 65\n"
                         "@@MEM V arr 1010 4 3 i 0 10 20 30\n"
                         "@@MEM V ptr 1020 8 1 i 1 1018\n"
                         "@@MEM V heap 1028 8 1 i 1 5000\n"
                         "@@MEM H 5000 4 2 i 0 0 7 9\n"
                         "@@MEM H 5000 4 2 i 0 0 7 9\n"
                         "@@MEM V gone 1030 8 1 f 1 6000\n"
                         "@@MEM H 6000 8 1 f 0 1\n"
                         "@@MEM V nothing 1038 8 1 i 1 0\n"
                         "@@MEM END\n";
    assert(ProbeRead(report, &snap) && snap.valid);
    assert(snap.varCount == 7 && snap.blockCount == 2);
    assert(strcmp(snap.vars[0].values[0], "-25") == 0 && strcmp(snap.vars[1].values[0], "'A'") == 0);
    assert(snap.vars[2].count == 3 && snap.vars[2].shown == 3 && strcmp(snap.vars[2].values[2], "30") == 0);
    assert(ProbeFindVar(&snap, "ptr") == 3 && ProbeFindVar(&snap, "nope") == -1);

    ProbeTarget target = ProbeResolve(&snap, snap.vars[3].pointers[0]);  // into arr, its third element
    assert(target.kind == PROBE_TARGET_VAR && target.index == 2 && target.element == 2);
    target = ProbeResolve(&snap, snap.vars[4].pointers[0]);
    assert(target.kind == PROBE_TARGET_HEAP && target.index == 0 && strcmp(snap.blocks[0].values[1], "9") == 0);
    assert(ProbeResolve(&snap, 0x6000).kind == PROBE_TARGET_FREED && snap.blocks[1].shown == 1);
    assert(ProbeResolve(&snap, 0).kind == PROBE_TARGET_NULL);
    assert(ProbeResolve(&snap, 0x9999).kind == PROBE_TARGET_WILD);

    // A report that was cut short (the program crashed) is no report.
    assert(!ProbeRead("@@MEM BEGIN\n@@MEM V i 1000 4 1 i 0 5\n", &snap) && !snap.valid && snap.varCount == 0);
    assert(!ProbeRead("plain output\n", &snap) && !ProbeRead(NULL, &snap));
}

// ---------------------------------------------------------------------------
// Real runs

static char compilerBin[600], workDir[600];

// Instruments, compiles and runs the code; reads what it reports.
static ProbeSnapshot Run(const char *code, bool cpp, const char *expectOutput)
{
    static int runs;
    char source[800], exe[800], out[800], command[6000];
    snprintf(source, sizeof source, "%s\\probe%d.%s", workDir, ++runs, cpp ? "cpp" : "c");
    snprintf(exe, sizeof exe, "%s\\probe%d.exe", workDir, runs);
    snprintf(out, sizeof out, "%s\\probe%d.txt", workDir, runs);
    char *instrumented = ProbeInstrument(code);
    assert(instrumented);
    FILE *file = fopen(source, "wb");
    assert(file);
    fputs(instrumented, file);
    fclose(file);
    free(instrumented);
    snprintf(command, sizeof command, "set \"PATH=%s;%%PATH%%\" && \"%s\\%s\" -static -w \"%s\" -o \"%s\" && \"%s\" > \"%s\"",
             compilerBin, compilerBin, cpp ? "g++.exe" : "gcc.exe", source, exe, exe, out);
    int status = system(command);
    if (status != 0) printf("FAIL: probe run %d didn't build or run (%d)\n", runs, status);
    assert(status == 0);

    static char output[20000];
    file = fopen(out, "rb");
    assert(file);
    size_t n = fread(output, 1, sizeof output - 1, file);
    fclose(file);
    output[n] = '\0';
    if (expectOutput) assert(strstr(output, expectOutput) != NULL);  // the program still does its own job
    ProbeSnapshot snap;
    assert(ProbeRead(output, &snap));
    return snap;
}

// What the named pointer points at.
static ProbeTarget TargetOf(const ProbeSnapshot *snap, const char *pointer)
{
    int var = ProbeFindVar(snap, pointer);
    assert(var >= 0 && snap->vars[var].pointerDepth > 0);
    return ProbeResolve(snap, snap->vars[var].pointers[0]);
}

static void TestRuns(void)
{
    // C: stack variables, a pointer to a pointer, an array, null and a wild pointer.
    ProbeSnapshot snap = Run("#include <stdio.h>\n"
                             "int total = 3;\n"
                             "int main() {\n"
                             "    int i = 2, j = 8;\n"
                             "    int* ptr;\n"
                             "    int** pptr;\n"
                             "    float a[4] = {0};\n"
                             "    float* walker = &(a[2]);\n"
                             "    char grade = 'B';\n"
                             "    int* nothing = NULL;\n"
                             "    ptr = &i;\n"
                             "    pptr = &ptr;\n"
                             "    **pptr = 12;\n"
                             "    *pptr = &j;\n"
                             "    *walker = 3.5f;\n"
                             "    walker++;\n"
                             "    printf(\"i = %d\\n\", i);\n"
                             "    return 0;\n"
                             "}\n",
                             false, "i = 12");
    int i = ProbeFindVar(&snap, "i"), j = ProbeFindVar(&snap, "j"), a = ProbeFindVar(&snap, "a");
    assert(i >= 0 && strcmp(snap.vars[i].values[0], "12") == 0);
    assert(strcmp(snap.vars[ProbeFindVar(&snap, "total")].values[0], "3") == 0);
    assert(strcmp(snap.vars[ProbeFindVar(&snap, "grade")].values[0], "'B'") == 0);
    assert(TargetOf(&snap, "ptr").kind == PROBE_TARGET_VAR && TargetOf(&snap, "ptr").index == j);
    assert(TargetOf(&snap, "pptr").index == ProbeFindVar(&snap, "ptr"));
    assert(snap.vars[a].count == 4 && strcmp(snap.vars[a].values[2], "3.5") == 0);
    assert(TargetOf(&snap, "walker").index == a && TargetOf(&snap, "walker").element == 3);
    assert(TargetOf(&snap, "nothing").kind == PROBE_TARGET_NULL);

    // C: malloc and free. A freed block is still known, as freed.
    snap = Run("#include <stdio.h>\n#include <stdlib.h>\n"
               "int main() {\n"
               "    int size = 3;\n"
               "    int* arr = (int*)malloc(size * sizeof(int));\n"
               "    int* gone = (int*)malloc(sizeof(int));\n"
               "    for (int k = 0; k < size; k++) arr[k] = k * 10;\n"
               "    int* second = arr + 1;\n"
               "    free(gone);\n"
               "    printf(\"done\\n\");\n"
               "    return 0;\n"
               "}\n",
               false, "done");
    ProbeTarget target = TargetOf(&snap, "arr");
    assert(target.kind == PROBE_TARGET_HEAP && target.element == 0);
    assert(snap.blocks[target.index].count == 3 && strcmp(snap.blocks[target.index].values[2], "20") == 0);
    assert(TargetOf(&snap, "second").index == target.index && TargetOf(&snap, "second").element == 1);
    assert(TargetOf(&snap, "gone").kind == PROBE_TARGET_FREED);

    // C++: new and delete, objects, a linked list's nodes, and rows of a 2D array.
    snap = Run("#include <iostream>\nusing namespace std;\n"
               "struct Node { int data; Node* nextPtr; };\n"
               "int main() {\n"
               "    int* five = new int(5);\n"
               "    int* old = new int(1);\n"
               "    delete old;\n"
               "    Node* headPtr = new Node{10, nullptr};\n"
               "    Node local = {20, nullptr};\n"
               "    Node* toLocal = &local;\n"
               "    int** grid = new int*[2];\n"
               "    grid[0] = new int[3];\n"
               "    grid[1] = new int[3];\n"
               "    grid[1][2] = 42;\n"
               "    cout << *five << endl;\n"
               "    return 0;\n"
               "}\n",
               true, "5");
    target = TargetOf(&snap, "five");
    assert(target.kind == PROBE_TARGET_HEAP && strcmp(snap.blocks[target.index].values[0], "5") == 0);
    assert(TargetOf(&snap, "old").kind == PROBE_TARGET_FREED);
    assert(TargetOf(&snap, "headPtr").kind == PROBE_TARGET_HEAP);
    assert(TargetOf(&snap, "toLocal").kind == PROBE_TARGET_VAR && TargetOf(&snap, "toLocal").index == ProbeFindVar(&snap, "local"));
    target = TargetOf(&snap, "grid");
    assert(target.kind == PROBE_TARGET_HEAP && snap.blocks[target.index].pointerDepth == 1 && snap.blocks[target.index].count == 2);
    ProbeTarget row = ProbeResolve(&snap, snap.blocks[target.index].pointers[1]);  // the rows are blocks too
    assert(row.kind == PROBE_TARGET_HEAP && strcmp(snap.blocks[row.index].values[2], "42") == 0);
}

int main(int argc, char **argv)
{
    TestDeclarations();
    TestGuesses();
    TestReadingAReport();
    if (argc < 3) {
        printf("usage: test_probe <compiler bin folder> <empty folder>\n");
        return 2;
    }
    snprintf(compilerBin, sizeof compilerBin, "%s", argv[1]);
    snprintf(workDir, sizeof workDir, "%s", argv[2]);
    TestRuns();
    printf("All memory probe tests passed\n");
    return 0;
}
