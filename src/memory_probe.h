#ifndef MEMORY_PROBE_H
#define MEMORY_PROBE_H

#include <stdbool.h>

// What a program's variables look like in memory, for the level screen's picture of
// pointers. Two sources, both data only (unit-tested, tests/test_probe.c):
//
//  - The code alone (ProbeScanCode): the variables declared at the top level of main and
//    as globals, and for each pointer where the code says it points last (`ptr = &j;`,
//    `p = arr + 2;`, `*pptr = &i;`, `p = NULL;`, `ptr++`...). Cheap enough to do on every
//    keystroke; a guess, since nothing is run.
//
//  - A real run (ProbeInstrument, then ProbeRead): a copy of the code that, just before
//    main returns, prints every such variable's address and value, and for pointers the
//    heap block they point into. Heap blocks are tracked by standing in for malloc, calloc,
//    free, new and delete (nothing is really freed, so freed blocks can still be told
//    apart). ProbeResolve then says what an address is: a variable, a heap block, a freed
//    block, null, or nothing known.
//
// Variables in nested blocks or other functions, references, function pointers and
// arrays of more than one dimension are left out.

#define PROBE_MAX_VARS   20
#define PROBE_NAME       32
#define PROBE_MAX_CELLS  10  // elements of an array or heap block kept
#define PROBE_VALUE      16  // a value as text, with its terminator
#define PROBE_MAX_BLOCKS 12

typedef enum { PROBE_INT, PROBE_CHAR, PROBE_FLOAT, PROBE_BOOL, PROBE_OPAQUE } ProbeKind;  // OPAQUE: a struct or class

typedef enum {
    PROBE_TARGET_UNKNOWN,  // nothing says where it points
    PROBE_TARGET_NULL,
    PROBE_TARGET_VAR,      // a variable: index is its place in the list, element its array element
    PROBE_TARGET_HEAP,     // a heap block: index is the block (-1 in a guess: not known which)
    PROBE_TARGET_FREED,    // a heap block that has been freed
    PROBE_TARGET_WILD,     // an address that is none of those
} ProbeTargetKind;

typedef struct {
    ProbeTargetKind kind;
    int index, element;
} ProbeTarget;

// A variable as the code declares it.
typedef struct {
    char name[PROBE_NAME];
    ProbeKind kind;     // of the value at the end of the pointers, or of each element
    int pointerDepth;   // 0 for a plain value, 1 for `int*`, 2 for `int**`
    bool isArray;
    bool global;
    int line;           // zero-based line of its declaration
    ProbeTarget guess;  // pointers: where the code says it points (indexes into the scan's vars)
} ProbeVar;

typedef struct {
    ProbeVar vars[PROBE_MAX_VARS];
    int count;
    bool hasPointers;  // any of them is a pointer: the picture is worth showing
    int injectAt;      // offset in the code where a run reports from (-1: no main found)
} ProbeScan;

void ProbeScanCode(const char *code, ProbeScan *scan);

// The code with the report added (malloc'd; the caller frees it), or NULL when there is
// no main to report from. Its output holds the report among the program's own output.
char *ProbeInstrument(const char *code);

// A variable, or a heap block, as a run left it.
typedef struct {
    char name[PROBE_NAME];  // "" for a heap block
    unsigned long long address;
    int elementSize;
    int count;          // elements in all; `shown` of them are kept
    int shown;
    ProbeKind kind;
    int pointerDepth;   // of each element
    char values[PROBE_MAX_CELLS][PROBE_VALUE];       // each element as text ("" for opaque ones and pointers)
    unsigned long long pointers[PROBE_MAX_CELLS];    // pointer elements: the address each holds
    bool freed;         // heap blocks only
} ProbeCell;

typedef struct {
    bool valid;  // a whole report was read
    ProbeCell vars[PROBE_MAX_VARS];
    int varCount;
    ProbeCell blocks[PROBE_MAX_BLOCKS];
    int blockCount;
} ProbeSnapshot;

// Reads the report out of an instrumented program's output. False (and valid false) if it
// isn't all there, as when the program crashed or left main early.
bool ProbeRead(const char *output, ProbeSnapshot *snapshot);

// What the address is, among the snapshot's variables and heap blocks.
ProbeTarget ProbeResolve(const ProbeSnapshot *snapshot, unsigned long long address);

// The snapshot's variable with this name, or -1.
int ProbeFindVar(const ProbeSnapshot *snapshot, const char *name);

#endif
