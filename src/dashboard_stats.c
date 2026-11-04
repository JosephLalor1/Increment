#include "dashboard.h"
#include "progress.h"

DashboardStats DashboardCalculate(const LessonSet *set, Track track)
{
    DashboardStats stats = { 0 };
    stats.next = -1;
    stats.highest = -1;
    int scoreSum = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (track != TRACK_COUNT && lesson->track != track) continue;
        stats.lessons++;
        int best = ProgressBest(lesson->id);
        if (best >= PASS_PERCENT) stats.completed++;
        else if (stats.next < 0) stats.next = i;
        stats.exercises += lesson->exerciseCount;
        for (int e = 0; e < lesson->exerciseCount; e++)
            if (ProgressExerciseDone(lesson->id, e)) stats.exercisesCompleted++;
        if (lesson->questionCount == 0) continue;
        stats.quizzes++;
        if (best < 0) continue;
        stats.attempted++;
        scoreSum += best;
        if (best > stats.highest) stats.highest = best;
        if (best >= PASS_PERCENT) stats.passed++;
    }
    if (stats.attempted) stats.average = (float)scoreSum / stats.attempted;
    return stats;
}
