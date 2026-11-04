#ifndef CODE_SCREEN_H
#define CODE_SCREEN_H

#include "lesson.h"

#include <stdbool.h>

// The code editor screen: write C or C++, compile and run it, and see the output.
// Three ways in: the free editor from the menu, "Try it" on a lesson's example, and a
// lesson exercise (where the output is checked against the expected output, and a pass is
// recorded in progress).

// Where drafts are saved, normally the signed-in user's code folder. NULL or "" turns
// saving off. Takes effect for the next Code* open call.
void CodeSetDraftFolder(const char *folder);

void CodeOpenPlayground(void);
void CodeOpenSnippet(const char *title, const char *code, bool cpp);
void CodeOpenExercise(const Lesson *lesson, int exerciseIndex);

// Draws the screen and handles input. Returns true when the user leaves it.
bool CodeScreen(void);

void CodeClose(void);     // saves the draft and stops any running program
void CodeShutdown(void);  // frees everything, at exit

// Dev aid for --shot: start running immediately; CodeBusy says when it has finished.
void CodeRunNow(void);
bool CodeBusy(void);

#endif
