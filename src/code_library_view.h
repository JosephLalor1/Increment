// Save/open UI inside the sandbox; returns choices to the code screen.
#ifndef CODE_LIBRARY_VIEW_H
#define CODE_LIBRARY_VIEW_H
#include "code_library.h"
typedef struct { bool closed,cpp; char *source; char name[CODE_NAME_MAX+1]; } CodeLibraryAction;
// The source is borrowed until the panel closes. No editing/running while it is open.
void CodeLibraryViewOpen(const char *folder,const char *source,bool cpp,const char *name);
// Clears borrowed state and frees the listing at close/logout/shutdown.
void CodeLibraryViewClear(void);
// Caller frees action.source. closed with no source means return without loading.
CodeLibraryAction CodeLibraryView(void);
#endif
