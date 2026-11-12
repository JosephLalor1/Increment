// Where a learner is in a level and along a track. See roadmap.h.
#include "learning/roadmap.h"
#include "learning/progress.h"

bool RoadmapLevelReady(const Lesson *lesson) { return lesson->stepCount > 0; }

int RoadmapStepsDone(const Lesson *lesson)
{
    int done = 0;
    for (int i = 0; i < lesson->stepCount; i++) done += ProgressStepDone(lesson->id, i);
    return done;
}

bool RoadmapLevelComplete(const Lesson *lesson)
{
    return RoadmapLevelReady(lesson) && RoadmapStepsDone(lesson) == lesson->stepCount;
}

int RoadmapResumeStep(const Lesson *lesson)
{
    for (int i = 0; i < lesson->stepCount; i++)
        if (!ProgressStepDone(lesson->id, i)) return i;
    return 0;
}

bool RoadmapStepOpen(const Lesson *lesson, int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= lesson->stepCount) return false;
    for (int i = 0; i < stepIndex; i++)
        if (!ProgressStepDone(lesson->id, i)) return false;
    return true;
}

int RoadmapNext(const LessonSet *set, Track track, int *done, int *total)
{
    int next = -1;
    *done = *total = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track || !RoadmapLevelReady(lesson)) continue;
        (*total)++;
        if (RoadmapLevelComplete(lesson)) (*done)++;
        else if (next < 0) next = i;
    }
    return next;
}
