#ifndef TRACK_SCREEN_H
#define TRACK_SCREEN_H

#include "lesson.h"
#include "search.h"

// Traditional sequential course lesson list with search. Search results replace the
// list while the box holds text; clearing it restores the list and its scroll position.

// Entering from the menu: show this track from the top, with the search cleared.
void TrackScreenOpen(Track track);

// Draws the screen and handles input. Returns a lesson index to open, PAGE_STAY or
// PAGE_BACK (see layout.h). Search text and scroll positions survive a visit to a lesson.
int TrackScreen(const LessonSet *set);

// Dev aid for --shot: type query into the search box (NULL leaves it alone; any string,
// even blank, also focuses the box), pick the track filter, and scroll the course list.
void TrackScreenDemo(const char *query, SearchFilter filter, float scroll);

#endif
