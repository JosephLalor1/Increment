#ifndef SKILLS_VIEW_H
#define SKILLS_VIEW_H

#include <stdbool.h>

// The statistics page's "Your skills" section: every skill (skills.h) grouped by area, with
// its level, XP towards the next level and recent accuracy, and the weakest marked. Sets
// *practise when "Practise your weakest skills" (a short challenge) is pressed.
float SkillsViewDraw(float x, float y, float w, float scale, bool *practise);

#endif
