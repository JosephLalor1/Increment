#ifndef DASHBOARD_STATS_H
#define DASHBOARD_STATS_H

#include "lesson.h"

// Progress statistics for the dashboard, worked out from saved scores. No drawing, so
// it's unit-tested on its own (tests/test_dashboard.c).

typedef struct {
    int lessons, completed;               // lessons in scope, and those passed
    int exercises, exercisesCompleted;
    int quizzes, attempted, passed;       // lessons with quizzes; tried; scored PASS_PERCENT+
    int highest;                          // best quiz score, -1 if none attempted
    float average;                        // mean best score over attempted quizzes
    int next;                             // first unpassed lesson's index, -1 when complete or empty
    int speedRounds, gold;                // lessons with a speed round; those won (gold mastery)
} DashboardStats;

// track TRACK_COUNT means every track. Reads progress; never writes it.
DashboardStats DashboardCalculate(const LessonSet *set, Track track);

#endif
