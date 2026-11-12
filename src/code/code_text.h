#ifndef CODE_TEXT_H
#define CODE_TEXT_H

#include <stdbool.h>

// Reading C and C++ source as text: the small pieces every module that looks at a
// learner's code needs (the memory picture, the cheat checks, the second run, name
// suggestions, the editor). No drawing, no raylib.

// Whether c can be part of a name or number: a letter, digit or underscore.
bool CodeIsWordChar(char c);

// What CodeBlanked replaces with spaces, besides comments.
typedef enum {
    CODE_KEEP_STRINGS,       // comments only: what the program says, strings included
    CODE_BLANK_STRINGS,      // also string and character literals: brackets and names can
                             // be taken at face value
    CODE_BLANK_DIRECTIVES,   // also preprocessor lines: only the code itself is left
} CodeBlanking;

// A copy of code, the same length and with its newlines kept (so offsets and line numbers
// still match), with those parts replaced by spaces. The caller frees it; NULL if out of
// memory.
char *CodeBlanked(const char *code, CodeBlanking blanking);

// The index of the bracket that closes the one at `open` (one of ( [ {), looking no
// further than `end`; `end` if it is never closed. Give it blanked code.
int CodeMatchBracket(const char *code, int open, int end);

#endif
