#ifndef ANSWERS_H
#define ANSWERS_H

#include "lesson.h"
#include "skills.h"

#include <stdbool.h>

// Recording an answer, everywhere the app checks one (lesson quizzes, practice quizzes,
// coding questions, speed rounds): it goes into the review schedule (review.h) and earns or
// loses XP in the question's skills (skills.h), weighed by the question's difficulty and how
// the learner has done on it before. Data only; nothing draws.

// An answer to one of a lesson's questions, in a quiz or practice (untimed).
void AnswerRecordQuestion(const Lesson *lesson, const Question *question, bool correct);

// An answer in a speed round: reviewId is the question's (LessonQuestionId, or "skill:<name>"
// for generated ones), skills and difficulty what it exercises, and timeLeft the share of the
// time that was left (0 to 1, or 0 for a timeout). Timed answers earn more XP.
void AnswerRecordTimed(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty, bool correct,
                       float timeLeft);

#endif
