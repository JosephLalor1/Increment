// Progress statistics for the dashboard, from passed steps. See dashboard_stats.h.
#include "dashboard_stats.h"
#include "progress.h"
#include "roadmap.h"

DashboardStats DashboardCalculate(const LessonSet *set, Track track)
{
    DashboardStats stats = { 0 };
    stats.next = -1;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if ((track != TRACK_COUNT && lesson->track != track) || !RoadmapLevelReady(lesson)) continue;
        stats.lessons++;
        int done = RoadmapStepsDone(lesson);
        stats.steps += lesson->stepCount;
        stats.stepsDone += done;
        if (done > 0) stats.started++;
        if (done == lesson->stepCount) stats.completed++;
        else if (stats.next < 0) stats.next = i;
        if (lesson->speedSkills[0]) {
            stats.speedRounds++;
            if (ProgressGold(lesson->id)) stats.gold++;
        }
    }
    return stats;
}
