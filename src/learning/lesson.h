#ifndef LESSON_H
#define LESSON_H

// Lessons as loaded from assets/lessons. A lesson is a level of the journey: a short
// sequence of coding steps, and nothing to read. Each step is a program to change until its
// output matches a target output; the steps of a level go from tweaking a working example
// to writing the program from scratch. The format is described in CLAUDE.md.

#include <stdbool.h>

#include "learning/skills.h"

#define MAX_STEPS 16  // steps in one level

typedef enum { TRACK_C, TRACK_CPP, TRACK_COUNT } Track;

// One coding step. It passes when the learner's program prints `expected` (given `input` on
// stdin); hints come from comparing their code with `solution` (step_hints.h).
typedef struct {
    char *starter;   // the code the editor starts with; always compiles, never already passes
    char *input;     // stdin, may be NULL
    char *expected;  // the target output
    char *solution;  // a reference solution; never NULL once loaded
    int difficulty;  // 1 (easy) to 3 (hard), from its "~" line
    bool fixedValues;  // "@step fixed": no second run with different values (step_check.h)
    SkillWeight skills[MAX_QUESTION_SKILLS];  // what the author says it practises (may be none:
    int skillCount;                           // skills are also detected from the learner's code)
} Step;

typedef struct {
    char id[32];
    char title[96];
    Track track;
    char speedSkills[96];  // "@speed pointers ...": the skills its speed round tests (speed_round.h); "" for none
    Step steps[MAX_STEPS];
    int stepCount;         // 0 for a level that isn't written yet
} Lesson;

typedef struct {
    Lesson *items;
    int count;
} LessonSet;

// Loads every *.txt in dir, sorted by filename. See CLAUDE.md for the format.
bool LessonsLoad(LessonSet *set, const char *dir);
void LessonsUnload(LessonSet *set);

const char *TrackName(Track t);

#endif
