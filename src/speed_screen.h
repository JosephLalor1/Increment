#ifndef SPEED_SCREEN_H
#define SPEED_SCREEN_H

#include "lesson.h"

// The speed round screen: an optional, timed run of "what is x?" questions that earns gold
// mastery of a lesson (see speed_round.h for the rules). An intro page, then the run itself,
// played under falling code that, with a slow zoom, camera shake and a red edge, grows
// fiercer as each question's 5 seconds run out and calms the moment it's answered. A wrong
// answer or timeout shows the code with every variable's value line by line; ten right
// answers record gold with ProgressRecordGold. Leaving is returned to main.c.

typedef enum { SPEED_SCREEN_STAY, SPEED_SCREEN_BACK } SpeedScreenAction;

// Opens the intro for a lesson with an "@speed" line. lesson must outlive the screen.
void SpeedScreenOpen(const Lesson *lesson);
SpeedScreenAction SpeedScreen(void);

// Dev aid for --shot: 0 the intro, 1 the count-in, 2 a question elapsedMs into its
// 5 seconds (question 7, effects at that tension), 3 the failed page, 4 the gold page.
void SpeedScreenDemo(int stage, float elapsedMs);

#endif
