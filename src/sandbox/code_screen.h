#ifndef CODE_SCREEN_H
#define CODE_SCREEN_H

#include <stdbool.h>

// The free code editor, from the home bar: write C or C++, compile and run it on one or more
// test cases (each an input and, optionally, the output it should give), see the output,
// and save programs (code_library.h). Lessons' coding steps have their own screen
// (level_screen.h).

// Where drafts are saved, normally the signed-in user's code folder. NULL or "" turns
// saving off. Takes effect for the next Code* open call.
void CodeSetDraftFolder(const char *folder);

// Opens the editor with the draft left last time (or a hello-world template), in the
// language used last.
void CodeOpenPlayground(void);

// Draws the screen and handles input. Returns true when the user leaves it.
bool CodeScreen(void);

void CodeClose(void);     // saves the draft and stops any running program
void CodeShutdown(void);  // frees everything, at exit

// Dev aid for --shot: start running immediately; CodeBusy says when it has finished.
void CodeRunNow(void);
bool CodeBusy(void);
// Dev aid for --shot: a program that reads a number, with three test cases (one expecting
// the wrong output).
void CodeDemoCases(void);

#endif
