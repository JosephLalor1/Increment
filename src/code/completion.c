// Name completion for the code editor. See completion.h.
#include "code/completion.h"
#include "code/code_text.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_TYPED 2      // letters typed before names are offered
#define MAX_FOUND 256    // different names remembered from the code

// A word the language or its library has, with the shape typed after it.
typedef struct {
    const char *name;
    const char *after;
    int caretBack;
} KnownWord;

// Most common first: that is the order they are offered in.
static const KnownWord cWords[] = {
    { "printf", "(\"\")", 2 }, { "scanf", "(\"\")", 2 }, { "return", "", 0 }, { "int", "", 0 },
    { "double", "", 0 }, { "char", "", 0 }, { "float", "", 0 }, { "void", "", 0 }, { "while", " ()", 1 },
    { "for", " ()", 1 }, { "else", "", 0 }, { "switch", " ()", 1 }, { "case", "", 0 }, { "break", "", 0 },
    { "continue", "", 0 }, { "default", "", 0 }, { "main", "", 0 }, { "sizeof", "()", 1 }, { "struct", "", 0 },
    { "typedef", "", 0 }, { "const", "", 0 }, { "unsigned", "", 0 }, { "long", "", 0 }, { "short", "", 0 },
    { "malloc", "()", 1 }, { "calloc", "()", 1 }, { "realloc", "()", 1 }, { "free", "()", 1 }, { "NULL", "", 0 },
    { "strlen", "()", 1 }, { "strcpy", "()", 1 }, { "strcmp", "()", 1 }, { "strcat", "()", 1 },
    { "puts", "(\"\")", 2 }, { "putchar", "()", 1 }, { "getchar", "()", 1 },
};
static const KnownWord cppWords[] = {
    { "cout", "", 0 }, { "cin", "", 0 }, { "endl", "", 0 }, { "std", "", 0 }, { "string", "", 0 },
    { "class", "", 0 }, { "public", "", 0 }, { "private", "", 0 }, { "new", "", 0 }, { "delete", "", 0 },
    { "nullptr", "", 0 }, { "using", "", 0 }, { "namespace", "", 0 }, { "bool", "", 0 }, { "true", "", 0 },
    { "false", "", 0 }, { "this", "", 0 }, { "vector", "", 0 }, { "virtual", "", 0 },
};
// After a '#'.
static const KnownWord directives[] = { { "include", " <>", 1 }, { "define", "", 0 } };
// After `#include <`.
static const char *const headers[] = {
    "stdio.h", "stdlib.h", "string.h", "math.h", "stdbool.h", "ctype.h", "time.h", "iostream", "string", "vector",
};

#define COUNT(array) ((int)(sizeof(array) / sizeof(array)[0]))


static const KnownWord *FindKnown(const KnownWord *words, int count, const char *name, int nameLen)
{
    for (int i = 0; i < count; i++)
        if ((int)strlen(words[i].name) == nameLen && strncmp(words[i].name, name, (size_t)nameLen) == 0) return &words[i];
    return NULL;
}

// Whether a name is already offered.
static bool Offered(const CompletionList *list, const char *name, int nameLen)
{
    for (int i = 0; i < list->count; i++)
        if ((int)strlen(list->items[i].name) == nameLen && strncmp(list->items[i].name, name, (size_t)nameLen) == 0) return true;
    return false;
}

// Offers a name if it continues what is typed, isn't offered yet and there is room.
static void Offer(CompletionList *list, const char *typed, const char *name, int nameLen, const char *after,
                  int caretBack)
{
    if (list->count >= COMPLETION_MAX || nameLen <= list->typed || nameLen >= COMPLETION_NAME) return;
    if (strncmp(name, typed, (size_t)list->typed) != 0 || Offered(list, name, nameLen)) return;
    Completion *item = &list->items[list->count++];
    snprintf(item->name, sizeof item->name, "%.*s", nameLen, name);
    snprintf(item->after, sizeof item->after, "%s", after);
    item->caretBack = caretBack;
}

// The first character after pos on its line that isn't a space, or '\n'.
static char NextOnLine(const char *text, int len, int pos)
{
    while (pos < len && text[pos] == ' ') pos++;
    return pos < len ? text[pos] : '\n';
}

// After `#include <`: header names. Returns whether the caret is in such a place (so no
// other names should be offered), filling the list if anything matches.
static bool OfferHeaders(const char *text, int len, int cursor, CompletionList *out)
{
    int lineStart = cursor;
    while (lineStart > 0 && text[lineStart - 1] != '\n') lineStart--;
    int at = lineStart;
    while (at < cursor && text[at] == ' ') at++;
    if (at >= cursor || text[at] != '#') return false;
    at++;
    while (at < cursor && text[at] == ' ') at++;
    if (cursor - at < 7 || strncmp(text + at, "include", 7) != 0) return false;
    at += 7;
    while (at < cursor && text[at] == ' ') at++;
    if (at >= cursor || text[at] != '<') return false;
    int start = at + 1;
    for (int i = start; i < cursor; i++)
        if (!CodeIsWordChar(text[i]) && text[i] != '.') return true;  // past the name: nothing to offer
    out->typed = cursor - start;
    if (out->typed < 1 || (cursor < len && (CodeIsWordChar(text[cursor]) || text[cursor] == '.'))) return true;
    bool closed = cursor < len && text[cursor] == '>';
    for (int i = 0; i < COUNT(headers); i++)
        Offer(out, text + start, headers[i], (int)strlen(headers[i]), closed ? "" : ">", 0);
    return true;
}

// A name found in the code.
typedef struct {
    int start, len;
    int distance;   // from its nearest use to the caret, in characters
    bool callable;  // followed by "(" somewhere
} FoundName;

static int ByDistance(const void *a, const void *b)
{
    const FoundName *x = a, *y = b;
    return x->distance != y->distance ? x->distance - y->distance : x->start - y->start;
}

void CompletionFind(const char *text, int len, int cursor, CompletionList *out)
{
    memset(out, 0, sizeof *out);
    if (!text || cursor < 0 || cursor > len) return;
    if (OfferHeaders(text, len, cursor, out)) return;

    int wordStart = cursor;
    while (wordStart > 0 && CodeIsWordChar(text[wordStart - 1])) wordStart--;
    out->typed = cursor - wordStart;
    if (out->typed < MIN_TYPED || isdigit((unsigned char)text[wordStart])) return;
    if (cursor < len && CodeIsWordChar(text[cursor])) return;  // in the middle of a word
    const char *typed = text + wordStart;

    // After a '#', only directives make sense.
    if (wordStart > 0 && text[wordStart - 1] == '#') {
        for (int i = 0; i < COUNT(directives); i++)
            Offer(out, typed, directives[i].name, (int)strlen(directives[i].name), directives[i].after,
                  directives[i].caretBack);
        return;
    }

    // Read the code for its names, skipping strings, comments and #include lines; if the
    // caret turns out to be inside a string or comment, nothing is offered.
    FoundName found[MAX_FOUND];
    int foundCount = 0;
    bool cpp = false;
    for (int i = 0; i < len;) {
        char c = text[i];
        int regionEnd = -1;  // the last character of a string or comment starting here
        if (c == '/' && i + 1 < len && text[i + 1] == '/') {
            regionEnd = i;
            while (regionEnd < len && text[regionEnd] != '\n') regionEnd++;
        } else if (c == '/' && i + 1 < len && text[i + 1] == '*') {
            regionEnd = i + 2;
            while (regionEnd < len && !(text[regionEnd] == '/' && text[regionEnd - 1] == '*' && regionEnd > i + 2)) regionEnd++;
        } else if (c == '"' || c == '\'') {
            regionEnd = i + 1;
            while (regionEnd < len && text[regionEnd] != c && text[regionEnd] != '\n') {
                if (text[regionEnd] == '\\' && regionEnd + 1 < len) regionEnd++;
                regionEnd++;
            }
        }
        if (regionEnd >= 0) {
            if (cursor > i && cursor <= regionEnd) {
                out->count = 0;
                return;
            }
            i = regionEnd < len ? regionEnd + 1 : len;
            continue;
        }
        if (c == '#') {  // an #include's header isn't made of names
            int at = i + 1;
            while (at < len && text[at] == ' ') at++;
            if (len - at >= 7 && strncmp(text + at, "include", 7) == 0) {
                while (i < len && text[i] != '\n') {
                    if (len - i >= 8 && strncmp(text + i, "iostream", 8) == 0) cpp = true;
                    i++;
                }
                continue;
            }
            i++;
            continue;
        }
        if (isdigit((unsigned char)c)) {  // a number, with any suffix
            while (i < len && (CodeIsWordChar(text[i]) || text[i] == '.')) i++;
            continue;
        }
        if (!CodeIsWordChar(c)) {
            if (c == ':' && i + 1 < len && text[i + 1] == ':') cpp = true;
            i++;
            continue;
        }
        int start = i;
        while (i < len && CodeIsWordChar(text[i])) i++;
        int nameLen = i - start;
        if (nameLen == 5 && strncmp(text + start, "class", 5) == 0) cpp = true;
        if (start == wordStart || nameLen <= out->typed || strncmp(text + start, typed, (size_t)out->typed) != 0) continue;
        int distance = abs(start - wordStart);
        bool callable = NextOnLine(text, len, i) == '(';
        int known = -1;
        for (int k = 0; k < foundCount && known < 0; k++)
            if (found[k].len == nameLen && strncmp(text + found[k].start, text + start, (size_t)nameLen) == 0) known = k;
        if (known >= 0) {
            if (distance < found[known].distance) found[known].distance = distance;
            found[known].callable |= callable;
        } else if (foundCount < MAX_FOUND) {
            found[foundCount++] = (FoundName){ start, nameLen, distance, callable };
        }
    }

    // A name that is about to be called anyway doesn't need its brackets again.
    bool bracketFollows = NextOnLine(text, len, cursor) == '(';

    // The code's own names first, nearest the caret first; then the language's.
    qsort(found, (size_t)foundCount, sizeof found[0], ByDistance);
    for (int i = 0; i < foundCount; i++) {
        const char *name = text + found[i].start;
        const KnownWord *known = FindKnown(cWords, COUNT(cWords), name, found[i].len);
        if (!known) known = FindKnown(cppWords, COUNT(cppWords), name, found[i].len);
        const char *after = known ? known->after : found[i].callable ? "()" : "";
        int caretBack = known ? known->caretBack : found[i].callable ? 1 : 0;
        Offer(out, typed, name, found[i].len, bracketFollows ? "" : after, bracketFollows ? 0 : caretBack);
    }
    for (int i = 0; i < COUNT(cWords); i++)
        Offer(out, typed, cWords[i].name, (int)strlen(cWords[i].name), bracketFollows ? "" : cWords[i].after,
              bracketFollows ? 0 : cWords[i].caretBack);
    for (int i = 0; cpp && i < COUNT(cppWords); i++)
        Offer(out, typed, cppWords[i].name, (int)strlen(cppWords[i].name), cppWords[i].after, cppWords[i].caretBack);
}
