// Optional coding exam milestones, derived from real level order; never progression gates.
#ifndef CHECKPOINTS_H
#define CHECKPOINTS_H
#include "learning/lesson.h"
#define EXAM_MAX_TASKS 5
#ifndef EXAM_PASS_PERCENT
#define EXAM_PASS_PERCENT 60
#endif
#ifndef EXAM_DURATION_SECONDS
#define EXAM_DURATION_SECONDS 1800
#endif
typedef struct {
    char id[64], title[256];
    Track track;
    int section, count, lessons[EXAM_MAX_TASKS], steps[EXAM_MAX_TASKS];
    int duration, passPercent;
} Checkpoint;
// Ready levels only; groups of 4-5 where possible, with a final short group if necessary.
int CheckpointCount(const LessonSet *set, Track track);
bool CheckpointGet(const LessonSet *set, Track track, int section, Checkpoint *out);
bool CheckpointPassed(const Checkpoint *exam);
// Every well-formed exam is available, independent of saved progress or scores.
bool CheckpointAvailable(const LessonSet *set, const Checkpoint *exam);
// Every existing playable lesson is open; only invalid/empty content is rejected.
bool CheckpointLessonOpen(const LessonSet *set, int lesson);
// Exam after this actual lesson index, or -1. No checkpoint for an empty section.
int CheckpointAfter(const LessonSet *set, int lesson);
// First not-yet-passed checkpoint, for suggestions only; never an access requirement.
int CheckpointPending(const LessonSet *set, Track track);
#endif
