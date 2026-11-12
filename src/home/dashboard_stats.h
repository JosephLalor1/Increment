#ifndef DASHBOARD_STATS_H
#define DASHBOARD_STATS_H

#include "learning/lesson.h"

// Progress statistics for the dashboard, worked out from passed steps. No drawing, so it's
// unit-tested on its own (tests/test_dashboard.c). Levels that aren't written yet (no steps)
// don't count.

typedef struct {
    int lessons, completed;    // levels in scope, and those with every step passed
    int started;               // levels with at least one step passed, complete or not
    int steps, stepsDone;      // their steps, and those passed
    int next;                  // first level not complete, -1 when all are or there are none
    int speedRounds, gold;     // levels with a speed round; those won (gold mastery)
} DashboardStats;

// track TRACK_COUNT means every track. Reads progress; never writes it.
DashboardStats DashboardCalculate(const LessonSet *set, Track track);

#endif
