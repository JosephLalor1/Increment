#ifndef TRACK_SCREEN_H
#define TRACK_SCREEN_H

#include "lesson.h"
#include "search.h"

// A track's lessons: the roadmap (or a plain list), with a lesson search box above it.
// While the box holds text, search results replace the roadmap; clearing it brings the
// roadmap back where it was.

// Entering from the menu: show this track from the top, with the search cleared.
// Whether the roadmap or the plain list is shown is remembered between visits.
void TrackScreenOpen(Track track);

// Draws the screen and handles input. Returns a lesson index to open, PAGE_STAY or
// PAGE_BACK (see layout.h). Search text and scroll positions survive a visit to a lesson.
int TrackScreen(const LessonSet *set);

// Dev aid for --shot: type query into the search box (NULL leaves it alone; any string,
// even blank, also focuses the box), pick the track filter, and scroll the roadmap.
void TrackScreenDemo(const char *query, SearchFilter filter, float scroll);

#endif
