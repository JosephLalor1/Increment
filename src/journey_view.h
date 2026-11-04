#ifndef JOURNEY_VIEW_H
#define JOURNEY_VIEW_H

#include "lesson.h"

// Winding path for the progress dashboard's C and C++ journeys. Draw inside a clipped
// scroll view. Returns the clicked lesson's actual index, or -1; never writes progress.
int JourneyViewDraw(const LessonSet *set, Track track, float x, float y, float width, float *height);
float JourneyViewFocus(const LessonSet *set, Track track); // scroll offset of the first unfinished station

#endif
