#ifndef SPEED_SCREEN_H
#define SPEED_SCREEN_H

#include "lesson.h"

// The speed round screen: an optional, timed run of generated "what is x?" questions that
// earns gold mastery of a level, or a short challenge on the skills the learner most needs to
// practise (see speed_round.h for the rules, speed_plan.h for how questions are chosen). An
// intro page, then the run itself, played under falling code that, with a slow zoom, camera
// shake and a red edge, grows fiercer as each question's 10 seconds run out and calms the
// moment it's answered. A wrong answer or timeout shows the code with every variable's value
// line by line; ten right answers record gold with ProgressRecordGold. A short challenge ends
// with every question explained. Each answer is recorded in the learner's
// review history (review.h). Leaving is returned to main.c.

typedef enum { SPEED_SCREEN_STAY, SPEED_SCREEN_BACK } SpeedScreenAction;

// Opens the intro for a lesson with an "@speed" line. lesson must outlive the screen.
void SpeedScreenOpen(const Lesson *lesson);
// Opens the intro for a short challenge drawn from all of set (which must outlive the screen).
void SpeedScreenOpenChallenge(const LessonSet *set);
SpeedScreenAction SpeedScreen(void);

// Dev aid for --shot, after an Open: 0 the intro, 1 the count-in, 2 a question elapsedMs into its
// time (question 7, effects at that tension), 3 the failed page, 4 the gold page (or a perfect
// challenge's results), 5 a challenge's results with some misses. Demos never record anything.
void SpeedScreenDemo(int stage, float elapsedMs);

#endif
