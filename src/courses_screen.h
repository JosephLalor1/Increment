// Traditional lesson browser used independently of the gamified home journey.
#ifndef COURSES_SCREEN_H
#define COURSES_SCREEN_H
#include "lesson.h"
enum { COURSES_STAY = -1, COURSES_BACK = -2 };
// Traditional sequential lesson browser, separate from the gamified dashboard.
void CoursesScreenOpen(Track track);
// Returns lesson index, COURSES_BACK, or COURSES_STAY; main.c navigates.
int CoursesScreen(const LessonSet *lessons);
#endif
