// Planning speed rounds and short challenges. See speed_plan.h.
#include "speed_plan.h"
#include "roadmap.h"
#include "review.h"
#include "skill_progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t NextRandom(uint32_t *seed)
{
    if (*seed == 0) *seed = 0x9e3779b9u;
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static int RandomBelow(uint32_t *seed, int n) { return n <= 1 ? 0 : (int)(NextRandom(seed) % (uint32_t)n); }

void SpeedSkillReviewId(SpeedSkill skill, char *out, int size)
{
    snprintf(out, (size_t)size, "skill:%s", SpeedSkillName(skill));
}

bool SpeedSkillFromReviewId(const char *id, SpeedSkill *skill)
{
    if (strncmp(id, "skill:", 6) != 0) return false;
    return SpeedSkillsParse(id + 6, skill, 1) == 1;
}

int SpeedSkillWeights(SpeedSkill skill, SkillWeight *out)
{
    // Generated questions are all "what is x after these lines?", so tracing comes first.
    static const struct { Skill topic; float topicWeight; Skill extra; float extraWeight; } map[SPEED_SKILL_COUNT] = {
        [SPEED_VARIABLES]          = { SKILL_TYPES, 0.8f, SKILL_COUNT, 0 },
        [SPEED_ARITHMETIC]         = { SKILL_OPERATORS, 0.8f, SKILL_COUNT, 0 },
        [SPEED_CONDITIONS]         = { SKILL_CONDITIONS, 0.8f, SKILL_OPERATORS, 0.3f },
        [SPEED_LOOPS]              = { SKILL_LOOPS, 0.8f, SKILL_COUNT, 0 },
        [SPEED_ARRAYS]             = { SKILL_ARRAYS, 0.8f, SKILL_COUNT, 0 },
        [SPEED_STRINGS]            = { SKILL_STRINGS, 1.0f, SKILL_ARRAYS, 0.3f },
        [SPEED_FUNCTIONS]          = { SKILL_FUNCTIONS, 0.8f, SKILL_PARAMETER_PASSING, 0.3f },
        [SPEED_POINTERS]           = { SKILL_POINTERS, 0.8f, SKILL_COUNT, 0 },
        [SPEED_POINTER_TO_POINTER] = { SKILL_INDIRECTION, 0.8f, SKILL_POINTERS, 0.4f },
        [SPEED_PASS_BY_POINTER]    = { SKILL_PARAMETER_PASSING, 0.8f, SKILL_POINTERS, 0.4f },
        [SPEED_POINTER_ARITHMETIC] = { SKILL_POINTER_ARITHMETIC, 0.8f, SKILL_ARRAYS, 0.3f },
        [SPEED_MALLOC]             = { SKILL_DYNAMIC_MEMORY, 0.8f, SKILL_POINTERS, 0.3f },
        [SPEED_FUNCTION_POINTERS]  = { SKILL_FUNCTION_POINTERS, 0.8f, SKILL_FUNCTIONS, 0.3f },
        [SPEED_REFERENCES]         = { SKILL_REFERENCES, 0.8f, SKILL_PARAMETER_PASSING, 0.4f },
        [SPEED_NEW_DELETE]         = { SKILL_DYNAMIC_MEMORY, 0.8f, SKILL_POINTERS, 0.3f },
        [SPEED_OBJECTS]            = { SKILL_CLASSES, 0.8f, SKILL_COPYING, 0.4f },
        [SPEED_LINKED_LIST]        = { SKILL_DATA_STRUCTURES, 0.8f, SKILL_POINTERS, 0.4f },
    };
    int count = 0;
    out[count++] = (SkillWeight){ SKILL_TRACING, 1.0f };
    if (skill >= SPEED_SKILL_COUNT) return count;
    out[count++] = (SkillWeight){ map[skill].topic, map[skill].topicWeight };
    if (map[skill].extra != SKILL_COUNT) out[count++] = (SkillWeight){ map[skill].extra, map[skill].extraWeight };
    return count;
}

// How much a question's skills need work: their weakness, averaged by weight (0 to 1).
static float SkillsNeed(const SkillWeight *skills, int count)
{
    float total = 0, weights = 0;
    for (int i = 0; i < count; i++) {
        total += SkillWeakness(skills[i].skill) * skills[i].weight;
        weights += skills[i].weight;
    }
    return weights > 0 ? total / weights : 0.5f;
}

#define WEAK_SKILL_BOOST 40  // need added for a skill whose topics are all at their weakest
#define CHALLENGE_SKILLS 4   // a challenge spreads its questions over this many of the neediest skills

static SpeedPlanItem SkillItem(SpeedSkill skill)
{
    SpeedPlanItem item = { skill, "" };
    SpeedSkillReviewId(skill, item.reviewId, sizeof item.reviewId);
    return item;
}

static void Shuffle(SpeedPlanItem *items, int count, uint32_t *seed)
{
    for (int i = count - 1; i > 0; i--) {
        int j = RandomBelow(seed, i + 1);
        SpeedPlanItem t = items[i]; items[i] = items[j]; items[j] = t;
    }
}

int SpeedPlanLesson(const Lesson *lesson, uint32_t *seed, SpeedPlanItem *out)
{
    SpeedSkill skills[SPEED_SKILL_COUNT];
    int skillCount = SpeedSkillsParse(lesson->speedSkills, skills, SPEED_SKILL_COUNT);
    if (skillCount == 0) return 0;
    for (int slot = 0; slot < SPEED_QUESTIONS; slot++) out[slot] = SkillItem(skills[RandomBelow(seed, skillCount)]);
    return SPEED_QUESTIONS;
}

typedef struct {
    float need;
    SpeedSkill skill;
} Candidate;

static int ByNeed(const void *a, const void *b)
{
    float na = ((const Candidate *)a)->need, nb = ((const Candidate *)b)->need;
    return na < nb ? 1 : na > nb ? -1 : 0;
}

// Whether a level's speed round tests a skill.
static bool LevelTests(const Lesson *lesson, SpeedSkill skill)
{
    SpeedSkill skills[SPEED_SKILL_COUNT];
    int count = SpeedSkillsParse(lesson->speedSkills, skills, SPEED_SKILL_COUNT);
    for (int i = 0; i < count; i++)
        if (skills[i] == skill) return true;
    return false;
}

int SpeedPlanChallenge(const LessonSet *set, long today, uint32_t *seed, SpeedPlanItem *out)
{
    Candidate candidates[SPEED_SKILL_COUNT];
    int count = 0;
    // Skills the learner has met: answered in a speed round, or tested by a level they've
    // started. With neither, the first level that has a speed round starts them off.
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
        SpeedPlanItem item = SkillItem((SpeedSkill)s);
        const ReviewItem *history = ReviewFind(item.reviewId);
        bool met = history != NULL;
        for (int l = 0; l < set->count && !met; l++)
            met = RoadmapStepsDone(&set->items[l]) > 0 && LevelTests(&set->items[l], (SpeedSkill)s);
        if (!met) continue;
        SkillWeight skills[MAX_QUESTION_SKILLS];
        int skillCount = SpeedSkillWeights((SpeedSkill)s, skills);
        float need = ReviewNeed(history, today) + WEAK_SKILL_BOOST * SkillsNeed(skills, skillCount);
        candidates[count++] = (Candidate){ need + RandomBelow(seed, 400) / 100.0f, (SpeedSkill)s };
    }
    for (int l = 0; l < set->count && count == 0; l++) {
        SpeedSkill skills[SPEED_SKILL_COUNT];
        int skillCount = SpeedSkillsParse(set->items[l].speedSkills, skills, SPEED_SKILL_COUNT);
        for (int i = 0; i < skillCount; i++) candidates[count++] = (Candidate){ 0, skills[i] };
    }
    if (count == 0) return 0;

    // The neediest few skills share the questions, each question a new generated one.
    qsort(candidates, (size_t)count, sizeof *candidates, ByNeed);
    int spread = count < CHALLENGE_SKILLS ? count : CHALLENGE_SKILLS;
    for (int i = 0; i < SPEED_QUESTIONS; i++) out[i] = SkillItem(candidates[i % spread].skill);
    Shuffle(out, SPEED_QUESTIONS, seed);  // so the neediest aren't always first
    return SPEED_QUESTIONS;
}
