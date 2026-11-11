// Recording steps and speed round answers into skill XP (and the review schedule). See answers.h.
#include "answers.h"
#include "review.h"
#include "skill_progress.h"

// Snapshots each skill's XP, applies the answer, then fills in the XP after it, so the
// achievement animation can show the change.
static SkillGains RecordSkills(const SkillWeight *skills, int skillCount, bool correct, SkillAnswer answer)
{
    SkillGains gains = { .count = 0, .correct = correct };
    bool tracking = SkillsTracking();
    for (int i = 0; tracking && i < skillCount && gains.count < MAX_QUESTION_SKILLS; i++) {
        if (skills[i].skill >= SKILL_COUNT || skills[i].weight <= 0) continue;  // SkillsRecord skips these too
        float xp = SkillProgressOf(skills[i].skill)->xp;
        gains.items[gains.count++] = (SkillGain){ skills[i].skill, xp, xp };
    }
    SkillsRecord(skills, skillCount, correct, answer);
    for (int i = 0; i < gains.count; i++) gains.items[i].xpAfter = SkillProgressOf(gains.items[i].skill)->xp;
    return gains;
}

SkillGains AnswerRecordStep(const SkillWeight *skills, int skillCount, int difficulty, bool passedBefore)
{
    SkillAnswer answer = { difficulty, passedBefore ? 1 : 0, 0, -1 };  // untimed
    return RecordSkills(skills, skillCount, true, answer);
}

SkillGains AnswerRecordTimed(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty,
                             bool correct, float timeLeft)
{
    // The question's record before this answer weighs the XP; then the answer joins it.
    const ReviewItem *before = ReviewFind(reviewId);
    SkillAnswer answer = { difficulty, before ? before->right : 0, before ? before->wrong : 0, timeLeft < 0 ? 0 : timeLeft };
    SkillGains gains = RecordSkills(skills, skillCount, correct, answer);
    ReviewRecord(reviewId, correct, ReviewToday());
    return gains;
}
