#ifndef SPEED_PLAN_H
#define SPEED_PLAN_H

#include "lesson.h"
#include "skills.h"
#include "speed_round.h"

// What goes into a speed round, decided before it starts. Data only (it reads progress and
// the review history, and draws nothing), so it's unit-tested (tests/test_speed_plan.c).

// A lesson's speed round: generated questions for its "@speed" skills, with some of the
// lesson's own multiple-choice questions mixed in (about one in three; all of them if it has
// no skills). Returns the number of questions (SPEED_QUESTIONS, or fewer if the lesson
// hasn't enough to ask).
int SpeedPlanLesson(const Lesson *lesson, uint32_t *seed, SpeedPlanItem *out);

// A short challenge: the learner's most necessary questions right now, chosen Anki-style by
// ReviewNeed, with a boost for questions in the learner's weaker skills (SkillWeakness), so
// challenges keep working on what they find hardest. Questions due for review come first (the more overdue and the more often
// missed, the sooner), then ones being missed, then new questions from lessons the learner
// has started; skills whose generated questions they've been missing join in too. With no
// history at all it starts from the first lessons' questions. A little randomness varies
// the picks between equals, and the chosen questions come in a shuffled order. Returns the
// number of questions (up to SPEED_QUESTIONS).
int SpeedPlanChallenge(const LessonSet *set, long today, uint32_t *seed, SpeedPlanItem *out);

// The skills a speed round skill's generated questions exercise, with weights: always code
// tracing, plus the topic itself. Returns how many (at most MAX_QUESTION_SKILLS).
int SpeedSkillWeights(SpeedSkill skill, SkillWeight *out);

// The review id a skill's generated questions are recorded under: "skill:<name>".
void SpeedSkillReviewId(SpeedSkill skill, char *out, int size);
// The skill a "skill:<name>" id stands for; false for other ids.
bool SpeedSkillFromReviewId(const char *id, SpeedSkill *skill);

#endif
