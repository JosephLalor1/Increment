#ifndef STEP_SKILLS_H
#define STEP_SKILLS_H

#include "lesson.h"
#include "skills.h"

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

#define STEP_WRITTEN_WEIGHT 0.8f
#define STEP_SOLVING_LINES 5
#define STEP_SOLVING_WEIGHT 0.6f

#endif
