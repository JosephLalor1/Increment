#ifndef SPEED_PLAN_H
#define SPEED_PLAN_H

#include "lesson.h"
#include "skills.h"
#include "speed_round.h"

// What goes into a speed round, decided before it starts. Data only (it reads progress and
// the review history, and draws nothing), so it's unit-tested (tests/test_speed_plan.c).

// A level's speed round: SPEED_QUESTIONS generated questions for its "@speed" skills, picked
// at random. Returns the number of questions (0 when the level names no skills).
int SpeedPlanLesson(const Lesson *lesson, uint32_t *seed, SpeedPlanItem *out);

// A short challenge: generated questions for the skills the learner most needs to practise
// right now, chosen Anki-style by ReviewNeed from their speed round history, with a boost for
// skills where they're weaker (SkillWeakness). Skills tested by levels they've started join
// in even before they've been answered; with no history at all it starts with the first
// level's skills. The neediest few skills share the questions, in a shuffled order. Returns
// the number of questions (SPEED_QUESTIONS, or 0 when there are no skills to ask about).
int SpeedPlanChallenge(const LessonSet *set, long today, uint32_t *seed, SpeedPlanItem *out);

// The skills a speed round skill's generated questions exercise, with weights: always code
// tracing, plus the topic itself. Returns how many (at most MAX_QUESTION_SKILLS).
int SpeedSkillWeights(SpeedSkill skill, SkillWeight *out);

// The review id a skill's generated questions are recorded under: "skill:<name>".
void SpeedSkillReviewId(SpeedSkill skill, char *out, int size);
// The skill a "skill:<name>" id stands for; false for other ids.
bool SpeedSkillFromReviewId(const char *id, SpeedSkill *skill);

#endif
