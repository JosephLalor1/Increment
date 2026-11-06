// Custom practice setup, questions, results and review UI.
#ifndef PRACTICE_SCREEN_H
#define PRACTICE_SCREEN_H
#include "lesson.h"

typedef enum { PRACTICE_STAY, PRACTICE_HOME, PRACTICE_LESSONS } PracticeAction;
// Enter setup, preserving this user's previous choices. Lessons must remain loaded.
void PracticeScreenOpen(const LessonSet *lessons);
// Clear all session references and choices on logout and shutdown.
void PracticeScreenFree(void);
PracticeAction PracticeScreen(void);
Track PracticeScreenTrack(void); // selected track for Return to Lessons
// Unsaved developer previews: 0 setup, 1 question, 2 results, 3 review.
void PracticeScreenDemo(Track track, int page);
#endif
