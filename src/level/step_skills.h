#ifndef STEP_SKILLS_H
#define STEP_SKILLS_H

#include "learning/lesson.h"
#include "learning/skills.h"

#include <stdbool.h>

// The skills a passed coding step practised, detected from what the learner wrote. Data only,
// unit-tested (tests/test_steps.c).

// Skills for a pass of step with the learner's code, strongest first (the more advanced skill
// first on equal weights), at most MAX_QUESTION_SKILLS; returns how many. Three sources, the
// strongest weight winning:
//  - the step's own "~" skills, at the author's weights;
//  - what the lines the learner wrote or changed use (code_skills.h), at STEP_WRITTEN_WEIGHT,
//    so a step counts for the code that was actually written, not the code it came with;
//  - problem solving, when they wrote STEP_SOLVING_LINES lines or more.
int StepSkills(const Step *step, const char *code, bool cpp, SkillWeight *out);

// Whether passing a step the learner has passed before earns XP again: only for work done
// since `before`, the code they started from this time. Starting from the starter (the step
// was reset, or never edited) the pass itself is the work. Starting from code of their own
// (usually the draft that already solved it), they must have rewritten at least as many lines
// as the step asks for (the lines its solution adds to its starter, at least one); spacing,
// indentation and blank lines don't count, so running the old answer again earns nothing.
bool StepReplayEarns(const Step *step, const char *before, const char *code);

#define STEP_WRITTEN_WEIGHT 0.8f
#define STEP_SOLVING_LINES 5
#define STEP_SOLVING_WEIGHT 0.6f

#endif
