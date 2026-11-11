#ifndef DRAFTS_H
#define DRAFTS_H

#include <stdbool.h>
#include <stddef.h>

// Code drafts: the learner's unfinished code, kept as one file per draft in a folder (normally
// the signed-in user's code folder, users/<name>_code/), so it's still there next time. A
// draft is named by a key ("playground", "c_01_hello_step3") and saved as <key>.c or
// <key>.cpp. Plain file handling, no drawing.

// Writes <folder>/<key>.c(pp) into out; "" when folder is NULL or "" (drafts are off).
void DraftPath(const char *folder, const char *key, bool cpp, char *out, size_t size);
// The draft's text (malloc'd, caller frees), or NULL when there's none.
char *DraftLoad(const char *folder, const char *key, bool cpp);
// Saves length bytes of text as the draft. Returns false if drafts are off or it failed.
bool DraftSave(const char *folder, const char *key, bool cpp, const char *text, size_t length);

#endif
