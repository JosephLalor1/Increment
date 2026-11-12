#ifndef CODE_SKILLS_H
#define CODE_SKILLS_H

#include "learning/skills.h"

// Which concepts a piece of C or C++ uses (loops, arrays, pointers, classes...), read from
// its syntax. A cautious guess from local patterns, not a compiler: it is used to detect
// the skills in the lines a learner wrote, to check a step's solution has the constructs
// it should, and to tag saved programs. Data only, unit-tested (tests/test_code_skills.c).

typedef struct {
    bool present[SKILL_COUNT];  // by Skill: the code uses it
    bool complete;              // false if the code couldn't be read (no source, out of memory)
} CodeSkills;

// Comments, strings and preprocessor lines are skipped. Nothing is run and no XP changes.
CodeSkills CodeSkillsDetect(const char *source, bool cpp);

#endif
