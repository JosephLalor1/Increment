#ifndef ANSWERS_H
#define ANSWERS_H

#include "skills.h"

#include <stdbool.h>

// Recording what the learner did, everywhere the app checks it (coding steps, speed rounds):
// it earns or loses XP in the skills involved (skills.h), weighed by difficulty and by how
// the learner has done before, and speed round answers also go into the review schedule
// (review.h). Both return each skill's XP before and after (none while signed out), for the
// achievement animation (skill_reward.h). Data only; nothing draws.

// A passed coding step: skills from StepSkills (step_skills.h) and the step's difficulty.
// passedBefore is true when the learner had already passed this step, which earns less.
SkillGains AnswerRecordStep(const SkillWeight *skills, int skillCount, int difficulty, bool passedBefore);

// An answer in a speed round: reviewId is the question's ("skill:<name>" for generated ones),
// skills and difficulty what it exercises, and timeLeft the share of the time that was left
// (0 to 1, or 0 for a timeout). Timed answers earn more XP.
SkillGains AnswerRecordTimed(const char *reviewId, const SkillWeight *skills, int skillCount, int difficulty,
                             bool correct, float timeLeft);

#endif
