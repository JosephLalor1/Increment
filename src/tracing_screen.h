// Prediction task UI, opened from a lesson and returning to that lesson.
#ifndef TRACING_SCREEN_H
#define TRACING_SCREEN_H
#include "lesson.h"
// Resets answer/feedback on every open. The lesson must outlive this screen.
void TracingScreenOpen(const Lesson *lesson, int index);
// Clears all session state at the account boundary.
void TracingScreenClear(void);
// Draws and handles input; true asks main to return to the lesson.
bool TracingScreen(void);
#endif
