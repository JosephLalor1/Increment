// The skill catalogue, XP rules and levels. See skills.h.
#include "learning/skills.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name, *title;
    SkillGroup group;
} SkillInfo;

static const SkillInfo skillInfo[SKILL_COUNT] = {
    [SKILL_PROGRAM_STRUCTURE]   = { "program_structure", "Program structure", SKILL_GROUP_C },
    [SKILL_OUTPUT]              = { "output", "Printing output", SKILL_GROUP_C },
    [SKILL_INPUT]               = { "input", "Reading input", SKILL_GROUP_C },
    [SKILL_TYPES]               = { "types", "Variables and types", SKILL_GROUP_C },
    [SKILL_OPERATORS]           = { "operators", "Operators and arithmetic", SKILL_GROUP_C },
    [SKILL_CONDITIONS]          = { "conditions", "Conditions and logic", SKILL_GROUP_C },
    [SKILL_LOOPS]               = { "loops", "Loops", SKILL_GROUP_C },
    [SKILL_ARRAYS]              = { "arrays", "Arrays", SKILL_GROUP_C },
    [SKILL_STRINGS]             = { "strings", "Strings", SKILL_GROUP_C },
    [SKILL_FUNCTIONS]           = { "functions", "Functions", SKILL_GROUP_C },
    [SKILL_SCOPE]               = { "scope", "Scope and lifetime", SKILL_GROUP_C },
    [SKILL_POINTERS]            = { "pointers", "Pointers", SKILL_GROUP_MEMORY },
    [SKILL_INDIRECTION]         = { "indirection", "Pointers to pointers", SKILL_GROUP_MEMORY },
    [SKILL_PARAMETER_PASSING]   = { "parameter_passing", "Passing by value, pointer and reference", SKILL_GROUP_MEMORY },
    [SKILL_POINTER_ARITHMETIC]  = { "pointer_arithmetic", "Pointer arithmetic", SKILL_GROUP_MEMORY },
    [SKILL_DYNAMIC_MEMORY]      = { "dynamic_memory", "Dynamic memory", SKILL_GROUP_MEMORY },
    [SKILL_MEMORY_SAFETY]       = { "memory_safety", "Memory safety", SKILL_GROUP_MEMORY },
    [SKILL_FUNCTION_POINTERS]   = { "function_pointers", "Function pointers", SKILL_GROUP_MEMORY },
    [SKILL_CPP_BASICS]          = { "cpp_basics", "C++ basics", SKILL_GROUP_CPP },
    [SKILL_REFERENCES]          = { "references", "References", SKILL_GROUP_CPP },
    [SKILL_CLASSES]             = { "classes", "Classes and objects", SKILL_GROUP_CPP },
    [SKILL_CONSTRUCTORS]        = { "constructors", "Constructors and destructors", SKILL_GROUP_CPP },
    [SKILL_ENCAPSULATION]       = { "encapsulation", "Access and static members", SKILL_GROUP_CPP },
    [SKILL_COPYING]             = { "copying", "Copying objects", SKILL_GROUP_CPP },
    [SKILL_DATA_STRUCTURES]     = { "data_structures", "Linked data structures", SKILL_GROUP_CPP },
    [SKILL_TRACING]             = { "tracing", "Code tracing", SKILL_GROUP_THINKING },
    [SKILL_DEBUGGING]           = { "debugging", "Debugging", SKILL_GROUP_THINKING },
    [SKILL_PROBLEM_SOLVING]     = { "problem_solving", "Problem solving", SKILL_GROUP_THINKING },
};

static const char *groupTitles[SKILL_GROUP_COUNT] = {
    "C fundamentals", "Pointers and memory", "C++ and objects", "Thinking skills",
};

const char *SkillName(Skill skill) { return skill < SKILL_COUNT ? skillInfo[skill].name : ""; }
const char *SkillTitle(Skill skill) { return skill < SKILL_COUNT ? skillInfo[skill].title : ""; }
SkillGroup SkillGroupOf(Skill skill) { return skill < SKILL_COUNT ? skillInfo[skill].group : SKILL_GROUP_C; }
const char *SkillGroupTitle(SkillGroup group) { return group < SKILL_GROUP_COUNT ? groupTitles[group] : ""; }

bool SkillFromName(const char *name, int length, Skill *out)
{
    if (length < 0) length = (int)strlen(name);
    for (int s = 0; s < SKILL_COUNT; s++) {
        if ((int)strlen(skillInfo[s].name) == length && strncmp(name, skillInfo[s].name, (size_t)length) == 0) {
            *out = (Skill)s;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// XP and levels

static int Clamp(int value, int lo, int hi) { return value < lo ? lo : value > hi ? hi : value; }

float SkillXpChange(bool correct, float weight, SkillAnswer answer)
{
    int difficulty = Clamp(answer.difficulty, 1, SKILL_MAX_DIFFICULTY);
    if (correct) {
        float overcame = 1 + 0.5f * Clamp(answer.wrongBefore, 0, 4);  // a question that used to beat you
        float familiar = 1 + 0.25f * Clamp(answer.rightBefore, 0, 6);  // easy points shrink with repetition
        float timed = answer.timeLeft < 0 ? 1 : 1.5f + 0.5f * fminf(answer.timeLeft, 1);  // speed rounds pay more
        return 10 * weight * difficulty * overcame / familiar * timed;
    }
    float easiness = (SKILL_MAX_DIFFICULTY + 1 - difficulty) / 2.0f;     // 1.5, 1, 0.5
    float forgot = 1 + 0.3f * Clamp(answer.rightBefore, 0, 3);          // you knew this once
    float firstMeeting = answer.rightBefore + answer.wrongBefore == 0 ? 0.5f : 1;
    float pressure = answer.timeLeft < 0 ? 1 : 0.75f;  // mistakes against the clock are forgiven a little
    return -6 * weight * easiness * forgot * firstMeeting * pressure;
}

float SkillXpForLevel(int level)
{
    return level <= 0 ? 0 : 50.0f * level * (level + 1) / 2;
}

int SkillLevel(float xp)
{
    int level = 0;
    while (level < 1000 && SkillXpForLevel(level + 1) <= xp) level++;
    return level;
}
