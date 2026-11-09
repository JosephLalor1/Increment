// Planning speed rounds and short challenges. See speed_plan.h.
#include "speed_plan.h"
#include "progress.h"
#include "review.h"
#include "quiz_attempt.h"
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

#define WEAK_SKILL_BOOST 40  // need added for a question whose skills are all at their weakest

static SpeedPlanItem BankItem(const Lesson *lesson, const Question *q)
{
    SpeedPlanItem item = { q, SPEED_ARITHMETIC, "" };
    LessonQuestionId(lesson, q, item.reviewId, sizeof item.reviewId);
    return item;
}

static SpeedPlanItem SkillItem(SpeedSkill skill)
{
    SpeedPlanItem item = { NULL, skill, "" };
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
    int bank[MAX_QUESTIONS], bankCount = 0;
    for (int i = 0; i < lesson->questionCount && i < MAX_QUESTIONS; i++)
        if (!QuizIsCoding(&lesson->questions[i])) bank[bankCount++] = i;

    int count = 0;
    for (int slot = 0; slot < SPEED_QUESTIONS; slot++) {
        bool fromBank = bankCount > 0 && (skillCount == 0 || RandomBelow(seed, 3) == 0);
        if (fromBank) {
            int pick = RandomBelow(seed, bankCount);
            out[count++] = BankItem(lesson, &lesson->questions[bank[pick]]);
            bank[pick] = bank[--bankCount];  // each lesson question at most once per run
        } else if (skillCount > 0) {
            out[count++] = SkillItem(skills[RandomBelow(seed, skillCount)]);
        }
    }
    return count;
}

typedef struct {
    float need;
    SpeedPlanItem item;
} Candidate;

static int ByNeed(const void *a, const void *b)
{
    float na = ((const Candidate *)a)->need, nb = ((const Candidate *)b)->need;
    return na < nb ? 1 : na > nb ? -1 : 0;
}

int SpeedPlanChallenge(const LessonSet *set, long today, uint32_t *seed, SpeedPlanItem *out)
{
    int capacity = SPEED_SKILL_COUNT;
    for (int l = 0; l < set->count; l++) capacity += set->items[l].questionCount;
    Candidate *candidates = malloc((size_t)(capacity > 0 ? capacity : 1) * sizeof *candidates);
    if (!candidates) return 0;
    int count = 0;

    // Lesson questions the learner has met, or could meet: from lessons they've started.
    for (int l = 0; l < set->count; l++) {
        const Lesson *lesson = &set->items[l];
        bool started = ProgressBest(lesson->id) >= 0;
        for (int q = 0; q < lesson->questionCount; q++) {
            const Question *question = &lesson->questions[q];
            if (QuizIsCoding(question)) continue;  // these need the code editor
            SpeedPlanItem item = BankItem(lesson, question);
            const ReviewItem *history = ReviewFind(item.reviewId);
            if (!history && !started) continue;
            float need = ReviewNeed(history, today) + WEAK_SKILL_BOOST * SkillsNeed(question->skills, question->skillCount);
            candidates[count++] = (Candidate){ need + RandomBelow(seed, 400) / 100.0f, item };
        }
    }
    // Skills whose generated questions the learner has answered.
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
        SpeedPlanItem item = SkillItem((SpeedSkill)s);
        const ReviewItem *history = ReviewFind(item.reviewId);
        if (!history) continue;
        SkillWeight skills[MAX_QUESTION_SKILLS];
        int skillCount = SpeedSkillWeights((SpeedSkill)s, skills);
        float need = ReviewNeed(history, today) + WEAK_SKILL_BOOST * SkillsNeed(skills, skillCount);
        candidates[count++] = (Candidate){ need + RandomBelow(seed, 400) / 100.0f, item };
    }
    // No history at all: begin at the beginning, with the first lessons' questions.
    if (count == 0) {
        for (int l = 0; l < set->count && count < SPEED_QUESTIONS; l++) {
            const Lesson *lesson = &set->items[l];
            for (int q = 0; q < lesson->questionCount && count < SPEED_QUESTIONS; q++) {
                if (!QuizIsCoding(&lesson->questions[q]))
                    candidates[count++] = (Candidate){ 0, BankItem(lesson, &lesson->questions[q]) };
            }
        }
    }

    qsort(candidates, (size_t)count, sizeof *candidates, ByNeed);
    int chosen = count < SPEED_QUESTIONS ? count : SPEED_QUESTIONS;
    for (int i = 0; i < chosen; i++) out[i] = candidates[i].item;
    free(candidates);
    Shuffle(out, chosen, seed);  // so the neediest aren't always first
    return chosen;
}
