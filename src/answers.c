// Recording answers into the review schedule and skill XP together. See answers.h.
#include "answers.h"
#include "review.h"
#include "skill_progress.h"

// The question's record before this answer, for weighing the XP, then the answer itself.
// Returns each skill's XP before and after, for the achievement animation.
static SkillGains Record(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty, bool correct,
                         float timeLeft)
{
    SkillGains gains = { .count = 0, .correct = correct };
    bool tracking = SkillsTracking();
    for (int i = 0; tracking && i < skillCount && gains.count < MAX_QUESTION_SKILLS; i++) {
        if (skills[i].skill >= SKILL_COUNT || skills[i].weight <= 0) continue;  // SkillsRecord skips these too
        float xp = SkillProgressOf(skills[i].skill)->xp;
        gains.items[gains.count++] = (SkillGain){ skills[i].skill, xp, xp };
    }

    const ReviewItem *before = ReviewFind(reviewId);
    SkillAnswer answer = { difficulty, before ? before->right : 0, before ? before->wrong : 0, timeLeft };
    SkillsRecord(skills, skillCount, correct, answer);
    ReviewRecord(reviewId, correct, ReviewToday());

    for (int i = 0; i < gains.count; i++) gains.items[i].xpAfter = SkillProgressOf(gains.items[i].skill)->xp;
    return gains;
}

SkillGains AnswerRecordQuestion(const Lesson *lesson, const Question *question, bool correct)
{
    char id[64];
    LessonQuestionId(lesson, question, id, sizeof id);
    return Record(id, question->skills, question->skillCount, question->difficulty, correct, -1);
}

SkillGains AnswerRecordTimed(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty,
                             bool correct, float timeLeft)
{
    return Record(reviewId, skills, skillCount, difficulty, correct, timeLeft < 0 ? 0 : timeLeft);
}
