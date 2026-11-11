// Code drafts, one file per draft. See drafts.h.
#include "drafts.h"

#include <stdio.h>
#include <stdlib.h>

void DraftPath(const char *folder, const char *key, bool cpp, char *out, size_t size)
{
    if (!folder || !folder[0]) {
        out[0] = '\0';
        return;
    }
    snprintf(out, size, "%s/%s.%s", folder, key, cpp ? "cpp" : "c");
}

char *DraftLoad(const char *folder, const char *key, bool cpp)
{
    char path[800];
    DraftPath(folder, key, cpp, path, sizeof path);
    if (!path[0]) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (text) text[fread(text, 1, (size_t)size, f)] = '\0';
    fclose(f);
    return text;
}

bool DraftSave(const char *folder, const char *key, bool cpp, const char *text, size_t length)
{
    char path[800];
    DraftPath(folder, key, cpp, path, sizeof path);
    if (!path[0]) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(text, 1, length, f) == length;
    return fclose(f) == 0 && ok;
}
