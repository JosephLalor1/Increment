#ifndef COMPLETION_H
#define COMPLETION_H

#include <stdbool.h>

// Name completion for the code editor: what the word being typed could become. Candidates
// are the names already in the code (nearest the caret first; names in strings and comments
// don't count), then the language's common words and library functions, and header names
// after `#include <`. A name that is called comes with its shape: `printf("")`, `while ()`,
// `add()`, with the caret left inside. Pure functions: no editor, input or drawing.

#define COMPLETION_MAX  6   // the most offered at once
#define COMPLETION_NAME 48  // the longest name offered, with its terminator

typedef struct {
    char name[COMPLETION_NAME];  // what the word becomes
    char after[8];               // typed after the name: its shape, such as "()" (may be empty)
    int caretBack;               // how many characters before the end of `after` the caret is left
} Completion;

typedef struct {
    Completion items[COMPLETION_MAX];
    int count;
    int typed;  // how many characters of the name are already typed before the caret
} CompletionList;

// Finds completions for the word that ends at cursor in text (len characters). None are
// offered (count 0) for fewer than two letters typed (one after `#include <`), in the
// middle of a word, or inside a string or a comment; a name is never offered for itself.
void CompletionFind(const char *text, int len, int cursor, CompletionList *out);

#endif
