#ifndef PIECE_DEMOS_H
#define PIECE_DEMOS_H

#include <stdbool.h>

// What each piece of code does, shown rather than said: for every piece a step can show
// for the first time (step_novelty.h), a tiny program and the frames of running it, one
// line at a time. Each frame says which line runs, what memory holds after it, what it
// prints, what the user types at it, and the expression on the line as worked out with
// the values. piece_demo_view.h plays them; tests/test_piece_demos.c compiles and runs
// every one to check it prints and reads exactly what its frames say, and that every piece
// the levels introduce has one.
//
// Memory is written "name=value;name=value", every variable after the line:
//   x=5  c='A'  s="Ann"  d=2.5  n=?           a value (? not set yet)
//   a=[4,8,15]  w=[h,i,\0,...]                 an array, a box per element
//   p=&x  p=&a[1]  p=NULL                      a pointer and what it points at
//   #1=[0,0]  #1=5  #1=~[4,5]  p=&#1  p=&#1[2] an unnamed block (the heap, or a string
//                                              literal); ~ is a freed block
//   !days=7                                    locked: const, or a private member
//   d.age=3                                    a member of an object

#define DEMO_FRAMES_MAX 16

typedef struct {
    int line;             // the line that runs (1-based, in code)
    const char *memory;   // every variable after it (see above)
    const char *printed;  // what it prints, added to the output
    const char *typed;    // what the user types at it (input, ending in "\n")
    const char *eval;     // an expression on the line with the values put in...
    const char *result;   // ...and what it comes to
    const char *look;     // a variable or element it reads ("x", "a[2]", "#1[1]"): it glows
} DemoFrame;

typedef struct {
    const char *pieces;   // the pieces it shows, separated by spaces (CodePiece keys)
    bool cpp;             // C++ (otherwise C)
    const char *code;     // what is shown: lines run inside main, or a whole program
    DemoFrame frames[DEMO_FRAMES_MAX];  // ends at the first with line 0
} PieceDemo;

// The example for a piece (a CodePiece key such as "for", "%d", "int*"), or NULL.
const PieceDemo *PieceDemoFind(const char *piece);

// Every example, for tests: n from 0 while it returns non-NULL.
const PieceDemo *PieceDemoAt(int n);

int PieceDemoFrameCount(const PieceDemo *demo);

// Whether the code is a whole program (it has its own main) rather than lines run inside
// one.
bool PieceDemoIsProgram(const PieceDemo *demo);

// ---------------------------------------------------------------------------
// One frame's memory, read from its text

#define DEMO_NAME   24
#define DEMO_VALUE  24
#define DEMO_CELLS  8
#define DEMO_VARS   10

typedef enum {
    DEMO_PLAIN,    // a value in a box
    DEMO_ARRAY,    // a row of boxes
    DEMO_POINTER,  // an arrow to target (element targetCell, or -1 for the whole)
    DEMO_NULL,     // a pointer to nothing
} DemoVarKind;

typedef struct {
    char name[DEMO_NAME];    // "#1" style names are unnamed blocks
    DemoVarKind kind;
    char cells[DEMO_CELLS][DEMO_VALUE];  // DEMO_PLAIN: cells[0]
    int cellCount;
    char target[DEMO_NAME];
    int targetCell;
    bool block;              // an unnamed block (name starts with '#')
    bool freed;              // a freed block
    bool locked;             // const or private ('!')
} DemoVar;

// Reads a frame's memory text into out (at most max variables). Returns how many.
int DemoMemoryParse(const char *memory, DemoVar *out, int max);

#endif
