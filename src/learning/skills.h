#ifndef SKILLS_H
#define SKILLS_H

#include <stdbool.h>

// Skills and the rules for their XP. Every coding step and speed round question exercises one
// or more skills, each with a weight (a question that's mostly code tracing and only a little
// about pointers might be tracing 1, pointers 0.3) and a difficulty from 1 to 3. Getting it
// right earns XP in its skills; getting it wrong loses some. This is the catalogue and the arithmetic only: the
// learner's own XP is kept by skill_progress.h. Pure functions, unit-tested
// (tests/test_skills.c).

typedef enum {
    // C fundamentals
    SKILL_PROGRAM_STRUCTURE, SKILL_OUTPUT, SKILL_INPUT, SKILL_TYPES, SKILL_OPERATORS, SKILL_CONDITIONS,
    SKILL_LOOPS, SKILL_ARRAYS, SKILL_STRINGS, SKILL_FUNCTIONS, SKILL_SCOPE,
    // Pointers and memory
    SKILL_POINTERS, SKILL_INDIRECTION, SKILL_PARAMETER_PASSING, SKILL_POINTER_ARITHMETIC, SKILL_DYNAMIC_MEMORY,
    SKILL_MEMORY_SAFETY, SKILL_FUNCTION_POINTERS,
    // C++ and objects
    SKILL_CPP_BASICS, SKILL_REFERENCES, SKILL_CLASSES, SKILL_CONSTRUCTORS, SKILL_ENCAPSULATION, SKILL_COPYING,
    SKILL_DATA_STRUCTURES,
    // Thinking skills
    SKILL_TRACING, SKILL_DEBUGGING, SKILL_PROBLEM_SOLVING,
    SKILL_COUNT
} Skill;

typedef enum { SKILL_GROUP_C, SKILL_GROUP_MEMORY, SKILL_GROUP_CPP, SKILL_GROUP_THINKING, SKILL_GROUP_COUNT } SkillGroup;

typedef struct {
    Skill skill;
    float weight;  // how much the question exercises it, 0 to 1
} SkillWeight;

#define MAX_QUESTION_SKILLS 4

// What one answer did to the learner's skills: each skill's XP before and after. The
// achievement animation (skill_reward.h) shows it.
typedef struct {
    Skill skill;
    float xpBefore, xpAfter;
} SkillGain;

typedef struct {
    SkillGain items[MAX_QUESTION_SKILLS];
    int count;     // 0 when nothing was recorded (signed out)
    bool correct;  // the answer was right
} SkillGains;
#define SKILL_MAX_DIFFICULTY 3

const char *SkillName(Skill skill);    // as written in lesson files: "tracing"
const char *SkillTitle(Skill skill);   // for people: "Code tracing"
SkillGroup SkillGroupOf(Skill skill);
const char *SkillGroupTitle(SkillGroup group);
bool SkillFromName(const char *name, int length, Skill *out);  // length < 0: up to the '\0'


// What's known about the question being answered, beyond its skills, for weighing the XP.
typedef struct {
    int difficulty;   // 1 (easy) to 3 (hard)
    int rightBefore;  // times the learner had got this question right before
    int wrongBefore;  // and wrong
    float timeLeft;   // answered against the clock (speed rounds): the share of the time left,
                      // 0 to 1; negative for untimed answers (coding steps)
} SkillAnswer;

// The XP one skill gains (positive) or loses (negative) for an answer. Right: 10 x weight x
// difficulty, worth more for a question the learner has missed before (up to x3) and less for
// one they keep getting right. Against the clock it's worth more again: x1.5, up to x2 for an
// answer given straight away. Wrong: losing more on easy questions than hard ones, more for a
// question they'd got right before (forgetting), half as much the first time they meet a
// question, and a quarter less under time pressure.
float SkillXpChange(bool correct, float weight, SkillAnswer answer);

// Levels: reaching level n takes 50 x (1 + 2 + ... + n) XP, so each level takes a little
// longer: 50, 150, 300, 500, ...
int SkillLevel(float xp);
float SkillXpForLevel(int level);

#endif
