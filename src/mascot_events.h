#ifndef MASCOT_EVENTS_H
#define MASCOT_EVENTS_H
#include "lesson.h"
#include "mascot_state.h"

typedef struct { bool lessonPassed, courseComplete; } FoxProgressSnapshot;
FoxProgressSnapshot FoxProgressCapture(const LessonSet *set, int lessonIndex);
FoxMilestone FoxProgressCompleted(const LessonSet *set, int lessonIndex, FoxProgressSnapshot before);
#endif
