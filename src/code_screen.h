#ifndef CODE_SCREEN_H
#define CODE_SCREEN_H

#include "lesson.h"

#include <stdbool.h>

// The code editor screen: write C or C++, compile and run it, and see the output.
// Four ways in: the free editor from the menu, "Try it" on a lesson's example, a lesson
// exercise (where the output is checked against the expected output, and a pass is
// recorded in progress) and a quiz's coding question (checked the same way, but the result
// goes back to the quiz rather than into progress, and there's no "Show solution").

// Where drafts are saved, normally the signed-in user's code folder. NULL or "" turns
// saving off. Takes effect for the next Code* open call.
void CodeSetDraftFolder(const char *folder);

void CodeOpenPlayground(void);
void CodeOpenSnippet(const char *title, const char *code, bool cpp);
void CodeOpenExercise(const Lesson *lesson, int exerciseIndex);
// Opens coding question questionIndex (zero-based) of the lesson's quiz. startFresh begins
// from the starter code (a new quiz attempt); otherwise the learner's draft is reloaded.
void CodeOpenQuizQuestion(const Lesson *lesson, int questionIndex, bool startFresh);
// Whether the coding question opened last has been solved: some run since it was opened
// printed the expected output. False for the other ways in.
bool CodeQuizSolved(void);

// Draws the screen and handles input. Returns true when the user leaves it.
bool CodeScreen(void);

void CodeClose(void);     // saves the draft and stops any running program
void CodeShutdown(void);  // frees everything, at exit

// Dev aid for --shot: start running immediately; CodeBusy says when it has finished.
void CodeRunNow(void);
bool CodeBusy(void);
// Dev aid: reveal up to count hints for the open exercise, without editing code or progress.
void CodeHintsDemo(int count);

#endif
