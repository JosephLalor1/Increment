#ifndef LINE_DIFF_H
#define LINE_DIFF_H

#include <stdbool.h>

// Lines that differ between two pieces of code. Lines are compared without their leading and
// trailing whitespace, and blank lines are skipped, so re-indenting or spacing code out
// changes nothing. Data only, unit-tested (tests/test_steps.c).

typedef enum {
    DIFF_SAME,     // in both: oldLine and newLine are set
    DIFF_REMOVED,  // only in the old text: oldLine is set, newLine is -1
    DIFF_ADDED,    // only in the new text: newLine is set, oldLine is -1
} DiffKind;

typedef struct {
    DiffKind kind;
    int oldLine, newLine;  // zero-based line numbers in each text, -1 where it has none
} DiffOp;

#define LINE_DIFF_MAX_LINES 400  // longer texts are compared up to here only

// Fills ops (room for maxOps) with the smallest set of changes that turns oldText into
// newText, in order, and returns how many there are. Removals come before additions where
// a line was changed.
int LineDiff(const char *oldText, const char *newText, DiffOp *ops, int maxOps);

// Copies line `line` (zero-based) of text into out without its surrounding whitespace.
// Returns false (and writes "") when there's no such line.
bool LineDiffGetLine(const char *text, int line, char *out, int size);

#endif
