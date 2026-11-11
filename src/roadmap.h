#ifndef ROADMAP_H
#define ROADMAP_H

#include "lesson.h"

#include <stdbool.h>

// Where a learner is: in a level (which steps are passed, which is next) and along a track
// (which level comes next). Worked out from progress.h; data only, nothing draws.
//
// Inside a level the steps unlock in order: a step opens once every step before it has been
// passed. Levels themselves are never locked; the journey only suggests the next one. A level
// with no steps isn't written yet and is skipped.

bool RoadmapLevelReady(const Lesson *lesson);     // it has steps to play
int RoadmapStepsDone(const Lesson *lesson);       // how many of its steps are passed
bool RoadmapLevelComplete(const Lesson *lesson);  // ready, and every step passed
// The step to open the level at: the first one not yet passed, or 0 when all are.
int RoadmapResumeStep(const Lesson *lesson);
// Whether a step can be opened: every step before it has been passed.
bool RoadmapStepOpen(const Lesson *lesson, int stepIndex);

// Counts the track's ready levels (*total) and complete ones (*done). Returns the first ready
// level not yet complete, in order, or -1 when the track is complete or has none.
int RoadmapNext(const LessonSet *set, Track track, int *done, int *total);

#endif
