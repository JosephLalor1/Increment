#ifndef LEVEL_SCREEN_H
#define LEVEL_SCREEN_H

#include "lesson.h"

#include <stdbool.h>

// Playing a level: its coding steps, one at a time, with nothing to read. Each step shows a
// program in the editor, the target output and what the program prints now, line by line,
// ticked where they match. The learner changes the code and runs it until the two match.
// Hints are code too (step_hints.h): the line to work on, its shape, then the line itself.
// A run that prints the target must also be an honest solution (step_check.h): it needs the
// constructs the step is about, and a second, automatic run with different values must match
// too; otherwise the output panel shows why, in code and icons, and the step isn't passed.
// A pass records the step (progress.h), earns XP in the skills the learner's code used
// (step_skills.h, answers.h) with the reward animation, and unlocks the next step; passing
// the last step celebrates the level. Returns what the learner chose; main.c navigates.

typedef enum {
    LEVEL_STAY,        // nothing to do
    LEVEL_BACK,        // back to the journey
    LEVEL_OPEN_LEVEL,  // play another level: LevelAction.lessonIndex (the next one, after a celebration)
} LevelChoice;

typedef struct {
    LevelChoice kind;
    int lessonIndex;
} LevelAction;

// Opens a level at the first step not yet passed. Drafts are kept in draftFolder (normally
// the signed-in user's code folder; NULL or "" keeps none).
void LevelScreenOpen(const LessonSet *set, int lessonIndex, const char *draftFolder);

// Draws the screen and handles input.
LevelAction LevelScreen(void);

void LevelScreenClose(void);     // saves the draft and stops any running program
void LevelScreenShutdown(void);  // frees everything, at exit

// Dev aids for --shot: open a step even if it's locked, reveal hints, and (solved) put the
// solution in the editor; show the celebration; and wait for a run.
void LevelScreenDemo(int stepIndex, int hints, bool solved);
void LevelScreenDemoRun(void);  // runs the code as the learner would (checks and all); records nothing
void LevelScreenDemoCelebration(void);
bool LevelScreenBusy(void);

#endif
