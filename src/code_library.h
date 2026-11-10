// Named sandbox source files, scoped to the signed-in user's existing draft folder.
#ifndef CODE_LIBRARY_H
#define CODE_LIBRARY_H
#include "code_skills.h"
#define CODE_NAME_MAX 48
#define CODE_SAVE_MAX (256 * 1024)
typedef struct { char name[CODE_NAME_MAX+1]; bool cpp; CodeSkills skills; } SavedProgram;
typedef enum { CODE_SAVE_OK, CODE_SAVE_EXISTS, CODE_SAVE_ERROR } CodeSaveResult;
// Names are portable basenames (letters/digits/spaces/_/-), excluding Windows device names.
bool CodeLibraryNameValid(const char *name);
// Atomically writes plain source to <folder>/saved/<name>.c(pp). Existing files require replace.
CodeSaveResult CodeLibrarySave(const char *folder,const char *name,bool cpp,const char *source,bool replace);
// Caller frees returned code with free(). Rejects unreadable, binary or oversized files.
char *CodeLibraryLoad(const char *folder,const char *name,bool cpp);
// Caller frees the array. false means folder could not be read; absent library is empty.
bool CodeLibraryList(const char *folder,SavedProgram **items,int *count);
#endif
