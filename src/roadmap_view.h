#ifndef ROADMAP_VIEW_H
#define ROADMAP_VIEW_H

#include "lesson.h"

#include <stdbool.h>

// Drawing for the learning roadmap (the data is in roadmap.h). Text sizes follow the
// user's lesson text size (UiTextScale).

// Draws a track's roadmap from (x, y), width w, inside the current scroll view: progress
// summary, then a connected card per lesson. Sets *height to the height used. Returns the
// clicked lesson's index, or -1.
int RoadmapViewDraw(const LessonSet *set, Track track, float x, float y, float w, float *height);

// The "Suggested preparation (optional): ..." line for a lesson, wrapped to w at the given
// font size. Returns its height, or 0 if the lesson has no suggestions. draw=false measures.
float RoadmapViewPreparation(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size,
                             bool draw);

#endif
