#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"

typedef struct {
    int lessons, completed;
    int exercises, exercisesCompleted;
    int quizzes, attempted, passed, highest;
    float average;
    int next; // index in LessonSet, -1 when complete or empty
} DashboardStats;

// TRACK_COUNT selects all tracks. Reads existing progress without writing it.
DashboardStats DashboardCalculate(const LessonSet *set, Track track);
void DashboardReset(void); // reset topic filters when entering from the menu
// Draw inside a scroll view; returns the clicked lesson index, or -1.
int DashboardDraw(const LessonSet *set, float x, float y, float w, float scale, float *height);

#endif
