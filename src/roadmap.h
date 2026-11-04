#ifndef ROADMAP_H
#define ROADMAP_H

#include "lesson.h"

// Returns the first incomplete lesson's index, or -1. Never restricts access.
int RoadmapNext(const LessonSet *set, Track track, int *done, int *total);
// Draw inside the current scroll view. Returns a clicked lesson index or -1.
int RoadmapDraw(const LessonSet *set, Track track, float x, float y, float w, float *height);
float RoadmapPrerequisites(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size, bool draw);

#endif
