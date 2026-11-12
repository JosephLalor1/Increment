#ifndef CODE_LIBRARY_H
#define CODE_LIBRARY_H

#include "code/code_skills.h"

// A learner's saved programs from the free code editor: named source files kept in
// <folder>/saved, where folder is the signed-in user's code folder. Data only, unit-tested
// (tests/test_code_library.c); code_library_view.c is the page that saves and opens them.

#define CODE_NAME_MAX 48
#define CODE_SAVE_MAX (256 * 1024)  // the largest source saved or loaded, in bytes

typedef struct {
    char name[CODE_NAME_MAX + 1];
    bool cpp;           // saved as .cpp; .c otherwise
    CodeSkills skills;  // the concepts its source uses
} SavedProgram;

typedef enum { CODE_SAVE_OK, CODE_SAVE_EXISTS, CODE_SAVE_ERROR } CodeSaveResult;

// Names are portable file names (letters, digits, spaces, _ and -), never a Windows device
// name such as CON or COM1.
bool CodeLibraryNameValid(const char *name);

// Writes source to <folder>/saved/<name>.c or .cpp, all or nothing. An existing file is
// only overwritten with replace set; otherwise the result is CODE_SAVE_EXISTS.
CodeSaveResult CodeLibrarySave(const char *folder, const char *name, bool cpp, const char *source, bool replace);

// The saved source, for the caller to free(). NULL for a file that is missing, unreadable,
// binary or too large.
char *CodeLibraryLoad(const char *folder, const char *name, bool cpp);

// Every saved program, sorted by name; the caller frees *items. A folder with nothing
// saved yet is an empty list. False means some file could not be read (the rest are listed).
bool CodeLibraryList(const char *folder, SavedProgram **items, int *count);

#endif
