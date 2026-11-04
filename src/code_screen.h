#ifndef CODE_SCREEN_H
#define CODE_SCREEN_H

#include "lesson.h"

#include <stdbool.h>

// The code editor screen: write C or C++, compile and run it, and see the output.
// Three ways in: the free editor from the menu, "Try it" on a lesson's example, and a
// lesson exercise (where the output is checked against the expected output).
// Requires a signed-in user (drafts are saved per user).

void CodeOpenPlayground(void);
void CodeOpenSnippet(const char *title, const char *code, bool cpp);
void CodeOpenExercise(const Lesson *lesson, int index);

// Draws the screen and handles input. Returns true when the user leaves it.
bool CodeScreen(void);

void CodeClose(void);     // saves the draft and stops any running program
void CodeShutdown(void);  // frees everything, at exit

bool CodeExerciseDone(const Lesson *lesson, int index);  // passed at least once

// Dev aid for --shot: start running immediately; CodeBusy says when it has finished.
void CodeRunNow(void);
bool CodeBusy(void);

#endif
