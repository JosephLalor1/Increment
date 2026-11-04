#include "mascot_events.h"
#include "progress.h"
#include "roadmap.h"

FoxProgressSnapshot FoxProgressCapture(const LessonSet *set, int index)
{
    FoxProgressSnapshot result = { false, false };
    if (index < 0 || index >= set->count) return result;
    const Lesson *lesson = &set->items[index];
    int done, total;
    RoadmapNext(set, lesson->track, &done, &total);
    result.lessonPassed = ProgressLessonPassed(lesson->id);
    result.courseComplete = total > 0 && done == total;
    return result;
}

FoxMilestone FoxProgressCompleted(const LessonSet *set, int index, FoxProgressSnapshot before)
{
    FoxProgressSnapshot after = FoxProgressCapture(set, index);
    return FoxCompletionTransition(before.lessonPassed, after.lessonPassed, before.courseComplete, after.courseComplete);
}
