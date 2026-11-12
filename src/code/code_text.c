// Reading C and C++ source as text. See code_text.h.
#include "code/code_text.h"
#include "core/text_copy.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool CodeIsWordChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

char *CodeBlanked(const char *code, CodeBlanking blanking)
{
    size_t len = strlen(code);
    char *out = CopyText(code);
    if (!out) return NULL;
    bool lineStart = true;  // nothing but spaces so far on this line: a '#' starts a directive
    for (size_t i = 0; i < len;) {
        char c = out[i];
        size_t end = i;     // one past the part starting at i, if a part starts here
        bool blank = true;
        if (c == '/' && out[i + 1] == '/') {
            while (end < len && out[end] != '\n') end++;
        } else if (c == '/' && out[i + 1] == '*') {
            end = i + 2;
            while (end < len && !(out[end] == '*' && out[end + 1] == '/')) end++;
            end = end < len ? end + 2 : len;
        } else if (c == '"' || c == '\'') {
            // To the closing quote, or the end of the line if it is never closed.
            end = i + 1;
            while (end < len && out[end] != c && out[end] != '\n') end += out[end] == '\\' && end + 1 < len ? 2 : 1;
            if (end < len && out[end] == c) end++;
            blank = blanking != CODE_KEEP_STRINGS;
        } else if (c == '#' && lineStart && blanking == CODE_BLANK_DIRECTIVES) {
            while (end < len && out[end] != '\n') end++;
        }
        if (end > i) {
            for (size_t k = i; blank && k < end; k++)
                if (out[k] != '\n') out[k] = ' ';
            i = end;
            continue;
        }
        if (c == '\n') lineStart = true;
        else if (c != ' ' && c != '\t') lineStart = false;
        i++;
    }
    return out;
}

int CodeMatchBracket(const char *code, int open, int end)
{
    char opener = code[open], closer = opener == '(' ? ')' : opener == '[' ? ']' : '}';
    int depth = 0;
    for (int i = open; i < end; i++) {
        if (code[i] == opener) depth++;
        else if (code[i] == closer && --depth == 0) return i;
    }
    return end;
}
