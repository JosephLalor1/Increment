// Lesson search: matching, ordering and excerpts. See search.h.
#include "search.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_WORDS 8

typedef struct {
    char text[MAX_WORDS][SEARCH_QUERY_MAX + 1];
    int count;
} Words;

static void SplitWords(const char *query, Words *w)
{
    w->count = 0;
    const char *p = query;
    while (*p && w->count < MAX_WORDS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != ' ' && *p != '\t') {
            if (n < SEARCH_QUERY_MAX) w->text[w->count][n++] = *p;
            p++;
        }
        w->text[w->count][n] = '\0';
        w->count++;
    }
}

// Case-insensitive strstr (ASCII), or NULL.
static const char *FindNoCase(const char *hay, const char *needle)
{
    if (!hay || !needle[0]) return NULL;
    size_t n = strlen(needle);
    for (const char *h = hay; *h; h++) {
        size_t i = 0;
        while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == n) return h;
    }
    return NULL;
}

static bool InText(const Lesson *L, const char *word)
{
    for (int i = 0; i < L->blockCount; i++)
        if (FindNoCase(L->blocks[i].text, word)) return true;
    for (int i = 0; i < L->exerciseCount; i++)
        if (FindNoCase(L->exercises[i].title, word) || FindNoCase(L->exercises[i].description, word)) return true;
    return false;
}

static bool TrackAllowed(const Lesson *L, SearchFilter filter)
{
    if (filter == SEARCH_C) return L->track == TRACK_C;
    if (filter == SEARCH_CPP) return L->track == TRACK_CPP;
    return true;
}

bool SearchIsBlank(const char *query)
{
    for (const char *p = query; *p; p++)
        if (*p != ' ' && *p != '\t') return false;
    return true;
}

static bool Matches(const Lesson *L, const Words *w, bool *allInTitle)
{
    *allInTitle = true;
    for (int i = 0; i < w->count; i++) {
        if (FindNoCase(L->title, w->text[i])) continue;
        *allInTitle = false;
        if (!InText(L, w->text[i])) return false;
    }
    return w->count > 0;
}

bool SearchMatches(const Lesson *L, const char *query, SearchFilter filter)
{
    Words w;
    SplitWords(query, &w);
    bool title;
    return TrackAllowed(L, filter) && Matches(L, &w, &title);
}

int SearchLessons(const LessonSet *set, const char *query, SearchFilter filter, int *out, int max)
{
    Words w;
    SplitWords(query, &w);
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {  // title matches, then text matches
        for (int i = 0; i < set->count && n < max; i++) {
            const Lesson *L = &set->items[i];
            bool title;
            if (!TrackAllowed(L, filter) || !Matches(L, &w, &title)) continue;
            if (title == (pass == 0)) out[n++] = i;
        }
    }
    return n;
}

bool SearchSnippet(const Lesson *L, const char *query, char *out, int size, int *matchAt, int *matchLen)
{
    out[0] = '\0';
    *matchAt = *matchLen = 0;
    if (size < 16) return false;
    Words w;
    SplitWords(query, &w);

    // The first word that appears in the text; prose blocks before code, since prose reads better.
    const char *text = NULL, *hit = NULL;
    int wordLen = 0;
    for (int i = 0; i < w.count && !hit; i++) {
        for (int pass = 0; pass < 2 && !hit; pass++) {
            for (int b = 0; b < L->blockCount && !hit; b++) {
                if ((L->blocks[b].type == BLOCK_CODE) != (pass == 1)) continue;
                const char *h = FindNoCase(L->blocks[b].text, w.text[i]);
                if (h) { text = L->blocks[b].text; hit = h; }
            }
        }
        for (int e = 0; e < L->exerciseCount && !hit; e++) {
            const char *fields[2] = { L->exercises[e].title, L->exercises[e].description };
            for (int f = 0; f < 2 && !hit; f++) {
                const char *h = FindNoCase(fields[f], w.text[i]);
                if (h) { text = fields[f]; hit = h; }
            }
        }
        if (hit) wordLen = (int)strlen(w.text[i]);
    }
    if (!hit) return false;

    // Start up to 40 characters before the match, at a word boundary when there is one.
    const char *start = hit - 40 < text ? text : hit - 40;
    if (start > text) {
        const char *s = start;
        while (s < hit && *s != ' ' && *s != '\n') s++;
        if (s < hit) start = s + 1;
    }
    int o = 0;
    if (start > text) { memcpy(out, "...", 3); o = 3; }
    bool lastSpace = false, reached = false;
    const char *p = start;
    for (; *p && o < size - 4; p++) {
        if (p == hit) { *matchAt = o; reached = true; }
        char c = *p;
        if (c == '\n' || c == '\t' || c == '\r') c = ' ';
        if (c == ' ' && lastSpace && (p < hit || p >= hit + wordLen)) continue;  // collapse runs
        lastSpace = c == ' ';
        out[o++] = c;
        if (p >= hit + wordLen && o > 150) { p++; break; }
    }
    if (*p) { memcpy(out + o, "...", 3); o += 3; }
    out[o] = '\0';
    *matchLen = reached ? wordLen : 0;
    if (*matchAt + *matchLen > o) *matchLen = o - *matchAt;
    return true;
}

const char *SearchFilterName(SearchFilter filter)
{
    switch (filter) {
    case SEARCH_C:   return "C";
    case SEARCH_CPP: return "C++";
    default:         return "C and C++";
    }
}
