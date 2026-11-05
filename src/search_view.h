#ifndef SEARCH_VIEW_H
#define SEARCH_VIEW_H

#include "lesson.h"
#include "raylib.h"
#include "search.h"

#include <stdbool.h>

// The lesson search box and its results, drawn by whichever screen hosts them (the home
// dashboard). The host keeps a SearchView, draws the box, and while SearchViewActive is
// true shows SearchViewResults in place of its normal content. Matching is in search.h.

typedef struct {
    char query[SEARCH_QUERY_MAX + 1];      // the box's text (also its focus identity)
    char lastQuery[SEARCH_QUERY_MAX + 1];  // last frame's query, to reset scrolling when it changes
    SearchFilter filter;                   // C, C++ or both
    float resultsScroll;
    float resultsHeight;                   // measured last frame, for clamping the scroll
} SearchView;

void SearchViewClear(SearchView *s);       // empty the box (the host's normal content returns)
bool SearchViewActive(const SearchView *s);  // the box holds more than spaces

// Draws the box in `box` with its clear button, and, while it's focused and empty, a hint
// under it. Ctrl+F focuses it. Sets *submitted when Enter is pressed. Returns the y below
// everything drawn, where the host's content can start.
float SearchViewBox(SearchView *s, Rectangle box, bool *submitted);

// Draws the track filter, the result count and one row per matching lesson (or a "no
// matches" panel) scrolling inside view. Enter (submitted) opens the top result. Returns
// the lesson index to open, or -1.
int SearchViewResults(SearchView *s, const LessonSet *set, Rectangle view, bool submitted);

// Dev aid for --shot: type query (NULL leaves the box alone; any string, even blank, also
// focuses it) and pick the filter.
void SearchViewDemo(SearchView *s, const char *query, SearchFilter filter);

#endif
