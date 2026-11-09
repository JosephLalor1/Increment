// Recording answers into the review schedule and skill XP together. See answers.h.
#include "answers.h"
#include "review.h"
#include "skill_progress.h"

// The question's record before this answer, for weighing the XP, then the answer itself.
static void Record(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty, bool correct,
                   float timeLeft)
{
    const ReviewItem *before = ReviewFind(reviewId);
    SkillAnswer answer = { difficulty, before ? before->right : 0, before ? before->wrong : 0, timeLeft };
    SkillsRecord(skills, skillCount, correct, answer);
    ReviewRecord(reviewId, correct, ReviewToday());
}

void AnswerRecordQuestion(const Lesson *lesson, const Question *question, bool correct)
{
    char id[64];
    LessonQuestionId(lesson, question, id, sizeof id);
    Record(id, question->skills, question->skillCount, question->difficulty, correct, -1);
}

void AnswerRecordTimed(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty, bool correct,
                       float timeLeft)
{
    Record(reviewId, skills, skillCount, difficulty, correct, timeLeft < 0 ? 0 : timeLeft);
}
