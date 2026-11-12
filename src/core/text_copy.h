#ifndef TEXT_COPY_H
#define TEXT_COPY_H

#include <stdlib.h>
#include <string.h>

// A copy of text for the caller to free (C99 has no strdup); NULL if out of memory.
// Header-only, so any module can use it without another file to link.
static inline char *CopyText(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy) memcpy(copy, text, length + 1);
    return copy;
}

#endif
