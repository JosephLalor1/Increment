#ifndef SKILL_REWARD_H
#define SKILL_REWARD_H

#include "raylib.h"
#include "skills.h"

// The achievement animation shown after a passed coding step or a speed round answer. Small
// glowing code characters burst out from where the answer was given and stream down to a
// badge for each skill it exercised, which pops up in the bottom-right corner: the
// skill's logo (skill_icons.h) inside a ring that fills to how far the learner is through the
// skill's current level, rising (or draining, for a wrong answer) as the characters arrive.
// A level up flashes the ring gold. The badges slip away again a moment later.
//
// It is an overlay over every screen: main.c updates and draws it after the current screen,
// so it carries on when the page changes. Screens only call SkillRewardShow.

// Starts the animation for an answer's XP changes (answers.h), from the point `from` on screen.
// Does nothing if gains has no skills (signed out). A skill whose badge is still showing reuses it.
void SkillRewardShow(Vector2 from, const SkillGains *gains);

// Where an answer came from: the mouse if it clicked this frame, otherwise the screen's centre
// (an answer by keyboard).
Vector2 SkillRewardOrigin(void);

void SkillRewardUpdate(float dt);
void SkillRewardDraw(void);
void SkillRewardClear(void);  // drops everything at once (signing out)

// --shot: an answer that earned XP in two skills (one of them levelling up), `seconds` into
// its animation, then held still. INCREMENT_SHOT_REWARD=<ms> with any shot.
void SkillRewardDemo(float seconds);

#endif
