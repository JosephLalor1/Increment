#ifndef SKILL_ICONS_H
#define SKILL_ICONS_H

#include "raylib.h"
#include "learning/skills.h"

// A logo for each skill (skills.h), drawn from shapes like the app's icons (icons.h), so it
// scales and takes any colour. Each pictures what the skill is about: braces for program
// structure, a terminal for output, an arrow between boxes for pointers, a bug for
// debugging, and so on. Check them with `--shot out.png gallery`.

// Draws skill's logo centred on centre, fitting a size x size box.
void SkillIconDraw(Skill skill, Vector2 centre, float size, Color ink);

#endif
