// Tests for speed_plan.c: level speed round plans and short challenge plans, using made-up
// levels, a throwaway progress file and a throwaway review history.
//
// usage: test_speed_plan <throwaway folder>
#include "speed/speed_plan.h"
#include "learning/progress.h"
#include "learning/review.h"
#include "learning/skill_progress.h"
#include "raylib.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void MakeLevel(Lesson *lesson, const char *id, const char *speedSkills)
{
    memset(lesson, 0, sizeof *lesson);
    snprintf(lesson->id, sizeof lesson->id, "%s", id);
    snprintf(lesson->speedSkills, sizeof lesson->speedSkills, "%s", speedSkills);
    lesson->stepCount = 2;
}

// Every item is a generated question for a skill, recorded under "skill:<name>".
static void CheckItems(const SpeedPlanItem *plan, int count)
{
    for (int i = 0; i < count; i++) {
        SpeedSkill back;
        assert(strncmp(plan[i].reviewId, "skill:", 6) == 0);
        assert(SpeedSkillFromReviewId(plan[i].reviewId, &back) && back == plan[i].skill);
    }
}

static int CountSkill(const SpeedPlanItem *plan, int count, SpeedSkill skill)
{
    int n = 0;
    for (int i = 0; i < count; i++) n += plan[i].skill == skill;
    return n;
}

static void TestLevelPlans(const LessonSet *set)
{
    uint32_t seed = 77;
    SpeedPlanItem plan[SPEED_QUESTIONS];
    int count = SpeedPlanLesson(&set->items[0], &seed, plan);  // arithmetic and loops
    assert(count == SPEED_QUESTIONS);
    CheckItems(plan, count);
    assert(CountSkill(plan, count, SPEED_ARITHMETIC) + CountSkill(plan, count, SPEED_LOOPS) == count);
    Lesson none;
    MakeLevel(&none, "c_none", "");
    assert(SpeedPlanLesson(&none, &seed, plan) == 0);  // no skills, no round
}

static void TestChallenges(const LessonSet *set, const char *folder)
{
    char progressPath[1024], reviewPath[1024];
    snprintf(progressPath, sizeof progressPath, "%s/plan_progress.txt", folder);
    snprintf(reviewPath, sizeof reviewPath, "%s/plan_review.txt", folder);
    remove(progressPath);
    remove(reviewPath);
    ProgressLoad(progressPath);
    ReviewLoad(reviewPath);
    long today = 20400;
    uint32_t seed = 3;
    SpeedPlanItem plan[SPEED_QUESTIONS];

    // No history: the first level's skills, so there's always something to practise.
    int count = SpeedPlanChallenge(set, today, &seed, plan);
    assert(count == SPEED_QUESTIONS);
    CheckItems(plan, count);
    assert(CountSkill(plan, count, SPEED_ARITHMETIC) > 0 && CountSkill(plan, count, SPEED_LOOPS) > 0);
    assert(CountSkill(plan, count, SPEED_ARITHMETIC) + CountSkill(plan, count, SPEED_LOOPS) == count);

    // A started level's skills join in; a skill answered in a speed round does too, even when
    // its level hasn't been started. The first level's skills, untouched, no longer do.
    ProgressRecordStep("c_arrays", 0);
    ReviewRecord("skill:pointers", false, today - 1);
    for (int run = 0; run < 5; run++) {
        count = SpeedPlanChallenge(set, today, &seed, plan);
        assert(count == SPEED_QUESTIONS);
        CheckItems(plan, count);
        assert(CountSkill(plan, count, SPEED_POINTERS) > 0 && CountSkill(plan, count, SPEED_ARRAYS) > 0);
        assert(CountSkill(plan, count, SPEED_POINTERS) + CountSkill(plan, count, SPEED_ARRAYS) == count);
    }

    LessonSet empty = { 0 };
    ProgressLoad("");
    ReviewLoad("");
    assert(SpeedPlanChallenge(&empty, today, &seed, plan) == 0);  // nothing to ask about
    remove(progressPath);
    remove(reviewPath);
}

// Challenges lean towards the learner's weakest skills: with loops weak and everything else
// strong, loops is among the skills practised, and most often.
static void TestWeakSkills(const LessonSet *set, const char *folder)
{
    char progressPath[1024], reviewPath[1024], skillsPath[1024];
    snprintf(progressPath, sizeof progressPath, "%s/weak_progress.txt", folder);
    snprintf(reviewPath, sizeof reviewPath, "%s/weak_review.txt", folder);
    snprintf(skillsPath, sizeof skillsPath, "%s/weak_skills.txt", folder);
    remove(progressPath);
    remove(reviewPath);
    remove(skillsPath);
    ProgressLoad(progressPath);
    ReviewLoad(reviewPath);
    SkillsLoad(skillsPath);
    long today = 20400;
    // Six skills met, all answered right today (none due).
    const char *met[] = { "skill:arithmetic", "skill:loops", "skill:arrays", "skill:pointers", "skill:strings",
                          "skill:functions" };
    for (int i = 0; i < 6; i++) ReviewRecord(met[i], true, today);
    SkillAnswer answer = { 2, 0, 0, -1 };
    for (int s = 0; s < SKILL_COUNT; s++) {  // strong at everything...
        SkillWeight one = { (Skill)s, 1 };
        for (int i = 0; i < 30; i++) SkillsRecord(&one, 1, true, answer);
    }
    SkillWeight loopSkill = { SKILL_LOOPS, 1 };  // ...but loops
    for (int i = 0; i < 40; i++) SkillsRecord(&loopSkill, 1, false, answer);
    assert(SkillWeakness(SKILL_LOOPS) > 0.8f && SkillWeakness(SKILL_ARRAYS) < 0.3f);

    uint32_t seed = 21;
    SpeedPlanItem plan[SPEED_QUESTIONS];
    int count = SpeedPlanChallenge(set, today, &seed, plan);
    assert(count == SPEED_QUESTIONS);
    int loops = CountSkill(plan, count, SPEED_LOOPS);
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) assert(CountSkill(plan, count, (SpeedSkill)s) <= loops);
    assert(loops >= 3);

    // Generated questions' skills: tracing first, then the topic.
    SkillWeight weights[MAX_QUESTION_SKILLS];
    int n = SpeedSkillWeights(SPEED_POINTER_TO_POINTER, weights);
    assert(n == 3 && weights[0].skill == SKILL_TRACING && weights[1].skill == SKILL_INDIRECTION);

    ProgressLoad("");
    ReviewLoad("");
    SkillsLoad("");
    remove(progressPath);
    remove(reviewPath);
    remove(skillsPath);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetTraceLogLevel(LOG_ERROR);
    LessonSet set = { calloc(3, sizeof(Lesson)), 3 };
    assert(set.items);
    MakeLevel(&set.items[0], "c_first", "arithmetic loops");
    MakeLevel(&set.items[1], "c_pointers", "pointers");
    MakeLevel(&set.items[2], "c_arrays", "arrays");
    TestLevelPlans(&set);
    TestChallenges(&set, argv[1]);
    TestWeakSkills(&set, argv[1]);
    free(set.items);
    puts("speed plan: all tests passed");
    return 0;
}
