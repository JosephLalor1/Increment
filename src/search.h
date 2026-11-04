#ifndef SEARCH_H
#define SEARCH_H

#include "lesson.h"
#include <stdbool.h>

// Lesson search: plain functions with no input or drawing (tested in tests/test_search.c).
//
// A query is split into words at spaces. A lesson matches when every word appears somewhere
// in its title or text (headings, paragraphs, notes, bullets, code, and exercise titles and
// descriptions), ignoring case.

#define SEARCH_QUERY_MAX 64

typedef enum { SEARCH_BOTH, SEARCH_C, SEARCH_CPP } SearchFilter;

bool SearchIsBlank(const char *query);  // empty or only spaces
bool SearchMatches(const Lesson *L, const char *query, SearchFilter filter);

// Fills out[] with matching lesson indexes (at most max) and returns how many there are.
// Lessons whose title holds every word come first; otherwise lessons keep their usual order.
int SearchLessons(const LessonSet *set, const char *query, SearchFilter filter, int *out, int max);

// An excerpt of the lesson text around the first word of the query, on one line, with "..."
// where it was cut. *matchAt and *matchLen locate the word in out. Returns false (and an empty
// out) if no word of the query appears in the text, for example when only the title matched.
bool SearchSnippet(const Lesson *L, const char *query, char *out, int size, int *matchAt, int *matchLen);

const char *SearchFilterName(SearchFilter filter);  // "C and C++", "C", "C++"

#endif
