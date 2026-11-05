#ifndef MEMORY_MODEL_H
#define MEMORY_MODEL_H

#include <stdbool.h>
#include <stddef.h>

// The model behind the interactive pointer diagrams in lessons: a ```memory block holds a
// short script, and this module turns it into a list of steps, each one a line of code plus
// a complete snapshot of memory after it runs. It also writes plain-English explanations of
// any box. Nothing here draws (memory_view.c does), so it can be unit-tested.
//
// Script lines (one command per line; blank lines and lines starting with # are ignored):
//   title <text>                     optional heading for the diagram
//   struct <Type> <field>:<type> ... declares a struct's fields, e.g. struct Node data:int next:Node*
//   > <code>                         starts a new step showing this line of code
//   var <name> <type> [values]       a variable on the stack (a struct takes one value per field)
//   array <name> <type> v1 v2 ...    a stack array with one element per value
//   heap <handle> <type> [values]    a block on the heap; the handle is only for the script
//   heaparray <handle> <type> v...   an array on the heap
//   set <place> <value>              stores a value: set *p 3, set a[1] 7, set p->next &n2
//   free <pointer>                   frees the heap block the pointer points at (free(p));
//                                    "delete <pointer>" does the same, for C++ lessons
//   frame <name>                     a function call starts: later vars go in its stack frame
//   return                           the innermost function returns: its variables disappear
//   say <text>                       explanation for the step (several lines join up)
//
// Places: name, name[i], name.field, *place, place->field, and pointer[i].
// Values: ? (never set: garbage), NULL or nullptr, &place, ptr+N / ptr-N (pointer arithmetic),
// another place (copies its value, e.g. q = p or *pp), or a literal such as 5, 2.5 or 'a'.

#define MEM_MAX_CELLS  64
#define MEM_MAX_GROUPS 24
#define MEM_MAX_FRAMES 6
#define MEM_MAX_STEPS  24
#define MEM_NAME_SIZE  24
#define MEM_TYPE_SIZE  20
#define MEM_TEXT_SIZE  24
#define MEM_CODE_SIZE  100
#define MEM_EXPLAIN_SIZE 600

typedef enum { MEM_STACK, MEM_HEAP } MemRegion;
typedef enum { MEM_VAR, MEM_ARRAY, MEM_STRUCT } MemShape;

typedef enum {
    MEM_GARBAGE,  // never given a value
    MEM_PLAIN,    // an ordinary value, shown as written
    MEM_NULL,     // a null pointer
    MEM_POINTER,  // the address of another cell
} MemValueKind;

typedef struct {
    MemValueKind kind;
    char text[MEM_TEXT_SIZE];  // as shown in the box: "5", "NULL", "?", or the target's address
    int target;                // MEM_POINTER: the cell it points at
} MemValue;

// One box: a variable, an array element or a struct field. Fixed for the whole script.
typedef struct {
    char label[MEM_NAME_SIZE];  // "i", "[2]", "next"
    char type[MEM_TYPE_SIZE];   // "int", "int*"
    int group;
    int address;
} MemCell;

// A variable, array or struct: one or more cells side by side.
typedef struct {
    char name[MEM_NAME_SIZE];   // variable name, or the script's handle for a heap block
    char type[MEM_TYPE_SIZE];   // element or struct type
    MemRegion region;
    MemShape shape;
    int frame;                  // stack: index into MemScript.frames
    int firstCell, cellCount;
} MemGroup;

// Memory after one step.
typedef struct {
    MemValue values[MEM_MAX_CELLS];
    bool live[MEM_MAX_GROUPS];   // exists now (created, and its function hasn't returned)
    bool freed[MEM_MAX_GROUPS];  // a heap block that has been freed (still drawn, greyed)
} MemState;

typedef struct {
    char code[MEM_CODE_SIZE];
    char explain[MEM_EXPLAIN_SIZE];
    MemState state;
} MemStep;

typedef struct {
    char title[MEM_CODE_SIZE];
    MemCell cells[MEM_MAX_CELLS];
    int cellCount;
    MemGroup groups[MEM_MAX_GROUPS];
    int groupCount;
    char frames[MEM_MAX_FRAMES][MEM_NAME_SIZE];  // function names; frame 0 is "main"
    int frameCount;
    MemStep steps[MEM_MAX_STEPS];
    int stepCount;
} MemScript;

// Parses a script. On failure returns false and writes "line N: what's wrong" to err.
// MemScript is large (about 200 KB), so allocate it on the heap.
bool MemScriptParse(const char *text, MemScript *out, char *err, size_t errSize);

// Whether a cell holds a pointer (its type contains '*').
bool MemIsPointerType(const char *type);

// How the cell is named in code: "i", "a[2]", "n.next", or for a heap block "(heap int)".
void MemCellName(const MemScript *s, int cell, char *out, size_t size);

// A clear explanation of one box at a step: what it is, what it holds, and for pointers
// what they point at and what dereferencing gives (following chains like **pp); also who
// points at it, dangling pointers, freed blocks and leaks.
void MemDescribeCell(const MemScript *s, int step, int cell, char *out, size_t size);

#endif
