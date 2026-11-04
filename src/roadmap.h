#ifndef ROADMAP_H
#define ROADMAP_H

#include "lesson.h"

#include <stdbool.h>
#include <stddef.h>

// The learning roadmap's data: where a learner is in a track and what each lesson builds on.
// Suggestions only; no lesson is ever locked. Drawing lives in the screens that show it.

// Counts the track's lessons (*total) and passed ones (*done). Returns the first lesson
// not yet passed, in lesson order, or -1 when the track is complete or empty.
int RoadmapNext(const LessonSet *set, Track track, int *done, int *total);

// Writes "Suggested preparation (optional): C: Arrays; C: Loops" for lessons that have
// suggestions. Returns false (and writes "") when there are none.
bool RoadmapPreparation(const LessonSet *set, const Lesson *lesson, char *out, size_t size);

#endif
