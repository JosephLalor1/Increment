#ifndef CODE_LIBRARY_VIEW_H
#define CODE_LIBRARY_VIEW_H

#include "sandbox/code_library.h"

// The page inside the free code editor for saving its code under a name and opening a
// saved program. It only reports the choice; code_screen.c acts on it.

typedef struct {
    bool closed;   // the page is done: go back to the editor
    bool cpp;      // the opened program's language
    char *source;  // an opened program's code, for the caller to free; NULL if none was opened
    char name[CODE_NAME_MAX + 1];  // the name of the program opened or last saved
} CodeLibraryAction;

// Opens the page for the editor's current code. `source` is borrowed until the page
// closes, so nothing may edit or run meanwhile.
void CodeLibraryViewOpen(const char *folder, const char *source, bool cpp, const char *name);

// Forgets the borrowed code and frees the listing: at close, sign-out and shutdown.
void CodeLibraryViewClear(void);

// Draws the page and handles its input.
CodeLibraryAction CodeLibraryView(void);

#endif
