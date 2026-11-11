#ifndef JOURNEY_VIEW_H
#define JOURNEY_VIEW_H

#include "lesson.h"

#include "raylib.h"

#include <stdbool.h>

// Winding path for the home screen's C and C++ journeys: a station per level, showing its
// passed steps; levels not written yet show as "coming soon" and can't be clicked. Draw
// inside a clipped scroll view. Returns the clicked level's actual index, or -1, and sets
// *clickedAt (if not NULL) to the middle of that level's station on screen; never writes
// progress.
int JourneyViewDraw(const LessonSet *set, Track track, float x, float y, float width, float *height,
                    Vector2 *clickedAt);
float JourneyViewFocus(const LessonSet *set, Track track); // scroll offset of the first unfinished level

// The middle of a level's station on screen, as last drawn (even if scrolled out of view).
// False if the last draw didn't include that level.
bool JourneyViewStationPoint(int lessonIndex, Vector2 *point);

// Dev aid for --shot: the next draw reports this level as clicked.
void JourneyViewDemoClick(int lessonIndex);

#endif
