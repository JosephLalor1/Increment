// Tests for speed_plan.c: lesson speed round plans and short challenge plans, using the
// real lessons, a throwaway progress file and a throwaway review history.
//
// usage: test_speed_plan <throwaway folder>
#include "../src/speed_plan.h"
#include "../src/progress.h"
#include "../src/quiz_attempt.h"
#include "../src/review.h"
#include "../src/skill_progress.h"
#include "raylib.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool QuestionOf(const Lesson *lesson, const Question *q)
{
    return q >= lesson->questions && q < lesson->questions + lesson->questionCount;
}

static const Lesson *LessonById(const LessonSet *set, const char *id)
{
    for (int i = 0; i < set->count; i++) if (strcmp(set->items[i].id, id) == 0) return &set->items[i];
    return NULL;
}

static void TestLessonPlans(const LessonSet *set)
{
    uint32_t seed = 77;
    int bankSeen = 0;
    for (int l = 0; l < set->count; l++) {
        const Lesson *lesson = &set->items[l];
        if (!lesson->speedSkills[0]) continue;
        SpeedSkill skills[SPEED_SKILL_COUNT];
        int skillCount = SpeedSkillsParse(lesson->speedSkills, skills, SPEED_SKILL_COUNT);
        SpeedPlanItem plan[SPEED_QUESTIONS];
        int count = SpeedPlanLesson(lesson, &seed, plan);
        assert(count == SPEED_QUESTIONS);
        for (int i = 0; i < count; i++) {
            if (plan[i].bank) {
                bankSeen++;
                assert(QuestionOf(lesson, plan[i].bank) && !QuizIsCoding(plan[i].bank));
                char id[64];
                LessonQuestionId(lesson, plan[i].bank, id, sizeof id);
                assert(strcmp(id, plan[i].reviewId) == 0);
                for (int j = i + 1; j < count; j++) assert(plan[j].bank != plan[i].bank);  // no repeats
            } else {
                bool known = false;
                for (int s = 0; s < skillCount; s++) known |= skills[s] == plan[i].skill;
                assert(known && strncmp(plan[i].reviewId, "skill:", 6) == 0);
                SpeedSkill back;
                assert(SpeedSkillFromReviewId(plan[i].reviewId, &back) && back == plan[i].skill);
            }
        }
    }
    assert(bankSeen > 0);  // lesson questions do get mixed in
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

    // No history: the first lessons' questions, so there's always something to practise.
    int count = SpeedPlanChallenge(set, today, &seed, plan);
    assert(count == SPEED_QUESTIONS);
    for (int i = 0; i < count; i++) {
        assert(plan[i].bank);
        assert(QuestionOf(&set->items[0], plan[i].bank) || QuestionOf(&set->items[1], plan[i].bank));
    }

    // A started lesson's questions are candidates; an unstarted lesson's are not, unless the
    // learner has answered them somewhere (a practice quiz, say). A missed question is due
    // and comes first, and so does a skill being missed.
    const Lesson *started = LessonById(set, "c_04_arithmetic"), *alsoStarted = LessonById(set, "c_05_decisions");
    const Lesson *other = LessonById(set, "c_11_pointers");
    assert(started && alsoStarted && other);
    ProgressRecord(started->id, 50);
    ProgressRecord(alsoStarted->id, 90);
    char missed[64];
    LessonQuestionId(other, &other->questions[0], missed, sizeof missed);
    ReviewRecord(missed, false, today - 3);
    ReviewRecord("skill:pointers", false, today - 1);
    // A question answered right recently is not due, so new questions outrank it.
    char known[64];
    LessonQuestionId(started, &started->questions[1], known, sizeof known);
    ReviewRecord(known, true, today);

    for (int run = 0; run < 5; run++) {
        count = SpeedPlanChallenge(set, today, &seed, plan);
        assert(count == SPEED_QUESTIONS);
        bool hasMissed = false, hasSkill = false, hasKnown = false;
        for (int i = 0; i < count; i++) {
            hasMissed |= strcmp(plan[i].reviewId, missed) == 0;
            hasSkill |= strcmp(plan[i].reviewId, "skill:pointers") == 0;
            hasKnown |= strcmp(plan[i].reviewId, known) == 0;
            if (plan[i].bank) {
                assert(!QuizIsCoding(plan[i].bank));
                assert(QuestionOf(started, plan[i].bank) || QuestionOf(alsoStarted, plan[i].bank)
                       || strcmp(plan[i].reviewId, missed) == 0);
            } else {
                assert(plan[i].skill == SPEED_POINTERS);
            }
        }
        assert(hasMissed && hasSkill && !hasKnown);
    }

    ProgressLoad("");
    ReviewLoad("");
    remove(progressPath);
    remove(reviewPath);
}

// Challenges lean towards the learner's weakest skills: with loops weak and everything else
// strong, two started lessons' new questions are mostly the loop ones.
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
    const Lesson *loops = LessonById(set, "c_06_loops"), *arrays = LessonById(set, "c_07_arrays");
    assert(loops && arrays);
    ProgressRecord(loops->id, 50);
    ProgressRecord(arrays->id, 50);
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
    int count = SpeedPlanChallenge(set, 20400, &seed, plan), aboutLoops = 0;
    for (int i = 0; i < count; i++) {
        const Question *q = plan[i].bank;
        assert(q);
        for (int s = 0; s < q->skillCount; s++) {
            if (q->skills[s].skill == SKILL_LOOPS) {
                aboutLoops++;
                break;
            }
        }
    }
    assert(count == SPEED_QUESTIONS && aboutLoops >= 8);

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
    LessonSet set;
    assert(LessonsLoad(&set, "assets/lessons"));
    TestLessonPlans(&set);
    TestChallenges(&set, argv[1]);
    TestWeakSkills(&set, argv[1]);
    LessonsUnload(&set);
    puts("speed plan: all tests passed");
    return 0;
}
