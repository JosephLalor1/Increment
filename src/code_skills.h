// Conservative local skill hints from C/C++ syntax; not a compiler or mastery assessment.
#ifndef CODE_SKILLS_H
#define CODE_SKILLS_H
#include "skills.h"
typedef struct { bool present[SKILL_COUNT]; bool complete; } CodeSkills;
// Skips comments, strings and preprocessor lines. No code execution or XP changes.
CodeSkills CodeSkillsDetect(const char *source, bool cpp);
#endif
