// Traditional sequential course browser, independent of the winding journey's layout.
#ifndef COURSES_SCREEN_H
#define COURSES_SCREEN_H
#include "learning/lesson.h"
typedef struct { int lesson,exam;Track track;bool back; } CoursesAction;
void CoursesScreenOpen(void);
CoursesAction CoursesScreen(const LessonSet *set);
#endif
