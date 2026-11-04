#ifndef ROADMAP_VIEW_H
#define ROADMAP_VIEW_H

#include "lesson.h"

#include <stdbool.h>

// Preparation-note drawing for lessons. The dashboard path lives in journey_view.h.

// The "Suggested preparation (optional): ..." line for a lesson, wrapped to w at the given
// font size. Returns its height, or 0 if the lesson has no suggestions. draw=false measures.
float RoadmapViewPreparation(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size,
                             bool draw);

#endif
