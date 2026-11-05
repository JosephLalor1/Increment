// Pointer diagram scripts: steps and memory snapshots, pointer arithmetic, structs, the
// heap, function frames, explanations, error messages, and that every ```memory block in
// the real lessons parses.
#include "../src/memory_model.h"
#include "../src/lesson.h"
#include "raylib.h"  // SetTraceLogLevel
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MemScript *script;

static void Parse(const char *text)
{
    char err[200];
    if (!MemScriptParse(text, script, err, sizeof err)) {
        fprintf(stderr, "unexpected parse error: %s\n", err);
        assert(0);
    }
}

static void ExpectError(const char *text, const char *fragment)
{
    char err[200];
    assert(!MemScriptParse(text, script, err, sizeof err));
    if (!strstr(err, fragment)) {
        fprintf(stderr, "error \"%s\" doesn't mention \"%s\"\n", err, fragment);
        assert(0);
    }
}

static int Cell(const char *name)
{
    for (int i = 0; i < script->cellCount; i++) {
        char n[48];
        MemCellName(script, i, n, sizeof n);
        if (strcmp(n, name) == 0) return i;
    }
    fprintf(stderr, "no cell %s\n", name);
    assert(0);
    return -1;
}

static const MemValue *Value(int step, const char *name) { return &script->steps[step].state.values[Cell(name)]; }

static void ExpectDescription(int step, const char *name, const char *fragment)
{
    char text[MEM_EXPLAIN_SIZE * 2];
    MemDescribeCell(script, step, Cell(name), text, sizeof text);
    if (!strstr(text, fragment)) {
        fprintf(stderr, "description of %s at step %d:\n  %s\ndoesn't mention \"%s\"\n", name, step, text, fragment);
        assert(0);
    }
}

// The professor's illustration: one pointer moving from i to j.
static void TestPointerMovesBetweenVariables(void)
{
    Parse("title One pointer, two variables\n"
          "> int i = 5, j = 10;\n var i int 5\n var j int 10\n say Two ints.\n"
          "> int *ptr;\n var ptr int* ?\n say ptr holds garbage.\n"
          "> ptr = &i;\n set ptr &i\n say ptr points to i.\n"
          "> *ptr = 3;\n set *ptr 3\n say i becomes 3.\n"
          "> ptr = &j;\n set ptr &j\n say now j.\n"
          "> *ptr = -2;\n set *ptr -2\n say j becomes -2.\n");
    assert(strcmp(script->title, "One pointer, two variables") == 0);
    assert(script->stepCount == 6);
    assert(strcmp(script->steps[3].code, "*ptr = 3;") == 0);
    assert(Value(1, "ptr")->kind == MEM_GARBAGE);
    assert(Value(2, "ptr")->kind == MEM_POINTER && Value(2, "ptr")->target == Cell("i"));
    assert(strcmp(Value(3, "i")->text, "3") == 0 && strcmp(Value(2, "i")->text, "5") == 0);  // each step is a snapshot
    assert(Value(4, "ptr")->target == Cell("j"));
    assert(strcmp(Value(5, "j")->text, "-2") == 0 && strcmp(Value(5, "i")->text, "3") == 0);
    // Addresses are realistic: ints 4 bytes apart, the pointer aligned to 8.
    assert(script->cells[Cell("j")].address == script->cells[Cell("i")].address + 4);
    assert(script->cells[Cell("ptr")].address % 8 == 0);
    char address[16];
    snprintf(address, sizeof address, "%d", script->cells[Cell("i")].address);
    assert(strcmp(Value(2, "ptr")->text, address) == 0);  // a pointer shows the address it holds

    ExpectDescription(1, "ptr", "random address");
    ExpectDescription(2, "ptr", "the address of i");
    ExpectDescription(3, "ptr", "So *ptr is i, which holds 3");
    ExpectDescription(3, "i", "ptr points here, so *ptr is another name for i");
}

static void TestPointerToPointerAndNull(void)
{
    Parse("> int x = 7; int *p = &x; int **pp = &p;\n var x int 7\n var p int* &x\n var pp int** &p\n say chain\n"
          "> **pp = 9;\n set **pp 9\n say through two arrows\n"
          "> p = NULL;\n set p NULL\n say nothing\n"
          "> int y = *(&x);\n var y int x\n say copy\n");
    assert(strcmp(Value(1, "x")->text, "9") == 0);
    assert(Value(2, "p")->kind == MEM_NULL);
    assert(strcmp(Value(3, "y")->text, "9") == 0);  // copying another place copies its value
    ExpectDescription(1, "pp", "So *pp is p, which holds the address");
    ExpectDescription(1, "pp", "So **pp is x, which holds 9");
    ExpectDescription(2, "p", "points at nothing");
    ExpectError("> x\n var p int* NULL\n set *p 3\n say no\n", "doesn't point at anything");
}

static void TestArraysAndArithmetic(void)
{
    Parse("> int a[3] = {10, 20, 30};\n array a int 10 20 30\n say an array\n"
          "> int *p = a;\n var p int* &a\n say p points at a[0]\n"
          "> p = p + 1;\n set p p+1\n say next element\n"
          "> p[1] = 99;\n set p[1] 99\n say index from p\n"
          "> *(p - 1) = 5;\n set p p-1\n set *p 5\n say back one\n");
    assert(Value(1, "p")->target == Cell("a[0]"));
    assert(Value(2, "p")->target == Cell("a[1]"));
    assert(strcmp(Value(3, "a[2]")->text, "99") == 0);
    assert(strcmp(Value(4, "a[0]")->text, "5") == 0);
    assert(script->cells[Cell("a[1]")].address == script->cells[Cell("a[0]")].address + 4);
    ExpectDescription(2, "p", "p+1 would point at the next element, a[2], 4 bytes further on");
    ExpectDescription(0, "a[1]", "element 1 of the array a");
    ExpectError("> x\n array a int 1 2\n var p int* &a[1]\n set p p+1\n say past\n", "past the end");
}

static void TestHeapStructsAndLeaks(void)
{
    Parse("struct Node data:int next:Node*\n"
          "> Node *head = new Node;\n heap n1 Node 1 NULL\n var head Node* &n1\n say first node\n"
          "> head->next = new Node;\n heap n2 Node 2 NULL\n set head->next &n2\n say second node\n"
          "> head->next->data = 5;\n set head->next->data 5\n say through two arrows\n"
          "> delete head->next;\n free head->next\n say freed\n"
          "> head = NULL;\n set head NULL\n say leak\n");
    assert(script->groups[script->cells[Cell("the heap Node's data")].group].region == MEM_HEAP);
    // Groups in creation order: n1, head, n2.
    int n2data = script->groups[2].firstCell;  // n2's data field
    assert(strcmp(script->steps[2].state.values[n2data].text, "5") == 0);
    assert(script->steps[3].state.freed[2] && !script->steps[2].state.freed[2]);
    char text[MEM_EXPLAIN_SIZE * 2];
    MemDescribeCell(script, 3, n2data, text, sizeof text);
    assert(strstr(text, "has been freed"));
    MemDescribeCell(script, 3, script->groups[0].firstCell + 1, text, sizeof text);  // n1.next
    assert(strstr(text, "dangling pointer"));
    // A Node* points at the whole Node, not just its first field.
    ExpectDescription(0, "head", "which is the address of the heap Node:");
    ExpectDescription(0, "head", "So *head is the heap Node (data 1, next NULL)");
    ExpectDescription(0, "the heap Node's data", "head points at this whole Node, so head->data is this field");
    MemDescribeCell(script, 4, script->groups[0].firstCell, text, sizeof text);  // n1, with head now NULL
    assert(strstr(text, "memory leak"));
    MemDescribeCell(script, 3, script->groups[0].firstCell, text, sizeof text);  // still reachable
    assert(!strstr(text, "memory leak"));
    ExpectError("struct N v:int\n> x\n heap h N 1\n var p N* &h\n free p\n free p\n say twice\n", "dangling");
    ExpectError("> x\n var i int 1\n var p int* &i\n free p\n say stack\n", "heap");
}

static void TestFunctionFrames(void)
{
    Parse("> int a = 1, b = 2;\n var a int 1\n var b int 2\n say main\n"
          "> swap(&a, &b);\n frame swap\n var x int* &a\n var y int* &b\n var temp int *x\n say call\n"
          "> *x = *y; *y = temp;\n set *x *y\n set *y temp\n say swap\n"
          "> }\n return\n say back in main\n");
    int x = Cell("x");
    assert(script->groups[script->cells[x].group].frame == 1 && strcmp(script->frames[1], "swap") == 0);
    assert(script->steps[1].state.live[script->cells[x].group]);
    assert(!script->steps[3].state.live[script->cells[x].group]);  // gone once swap returns
    assert(strcmp(Value(3, "a")->text, "2") == 0 && strcmp(Value(3, "b")->text, "1") == 0);
    ExpectDescription(1, "x", "belongs to swap()");
    ExpectError("> x\n return\n say no\n", "without a matching frame");

    // Returning the address of a local: the pointer dangles, and the gone variable says why.
    Parse("> int *p = f();\n var p int* ?\n frame f\n say call\n"
          "> int x = 5;\n var x int 5\n say local\n"
          "> return &x;\n set p &x\n return\n say dangling\n");
    ExpectDescription(2, "p", "x no longer exists (its function has returned), so p is a dangling pointer");
    ExpectDescription(2, "x", "But f has returned, so x no longer exists");
}

static void TestErrors(void)
{
    ExpectError("", "no steps");
    ExpectError("var i int 1\n", "must come after a > line");
    ExpectError("> x\n var i int 1\n", "has no 'say'");
    ExpectError("> x\n set q 1\n say no\n", "unknown name 'q'");
    ExpectError("> x\n var i int 1\n var i int 2\n say no\n", "already declared");
    ExpectError("> x\n dance\n say no\n", "line 2: unknown command");
    ExpectError("> x\n var i int 1\n set *i 3\n say no\n", "not a pointer");
}

// Every ```memory block in the real lessons must parse.
static void TestLessonDiagrams(void)
{
    LessonSet set;
    assert(LessonsLoad(&set, "assets/lessons"));
    int diagrams = 0;
    for (int l = 0; l < set.count; l++) {
        for (int b = 0; b < set.items[l].blockCount; b++) {
            const Block *block = &set.items[l].blocks[b];
            if (block->type != BLOCK_MEMORY) continue;
            char err[200];
            if (!MemScriptParse(block->text, script, err, sizeof err)) {
                fprintf(stderr, "%s: diagram %d: %s\n", set.items[l].id, diagrams + 1, err);
                assert(0);
            }
            for (int s = 0; s < script->stepCount; s++) {
                assert(strlen(script->steps[s].code) <= 66);  // fits the code box at Large text size
            }
            diagrams++;
        }
    }
    printf("%d lesson diagrams parse\n", diagrams);
    LessonsUnload(&set);
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    script = malloc(sizeof *script);
    TestPointerMovesBetweenVariables();
    TestPointerToPointerAndNull();
    TestArraysAndArithmetic();
    TestHeapStructsAndLeaks();
    TestFunctionFrames();
    TestErrors();
    TestLessonDiagrams();
    free(script);
    printf("memory model: all tests passed\n");
    return 0;
}
