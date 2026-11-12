#ifndef SKILL_PROGRESS_H
#define SKILL_PROGRESS_H

#include "learning/skills.h"

#include <stdbool.h>

// The signed-in learner's XP and accuracy in each skill (skills.h), kept in
// users/<name>.skills.txt as lines of "<skill> <xp> <right> <wrong> <accuracy>". Plain data,
// nothing draws, unit-tested (tests/test_skills.c).

// The learner's standing in a skill.
typedef struct {
    float xp;        // never below 0
    int right, wrong;
    float accuracy;  // recent accuracy, 0 to 1: an average that favours the latest answers
} SkillProgress;

void SkillsLoad(const char *path);  // "" clears (signed out); a missing file is all zero
bool SkillsTracking(void);          // a learner's skills are loaded, so answers count
const SkillProgress *SkillProgressOf(Skill skill);
// Applies an answer to each of its skills and saves. Does nothing while signed out.
bool SkillsRecord(const SkillWeight *skills, int count, bool correct, SkillAnswer answer);

// How weak the learner is in a skill, 0 (strong) to 1 (weak), from recent accuracy and level.
// A skill not yet tried is 0.5. Challenges favour questions in weak skills.
float SkillWeakness(Skill skill);

#endif
