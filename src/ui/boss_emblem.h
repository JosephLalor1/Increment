#ifndef BOSS_EMBLEM_H
#define BOSS_EMBLEM_H

#include "raylib.h"

// What marks exams ("boss" levels) out from the rest of the app.
//
// The halo: a breathing aura, a crown of spikes turning slowly round the exam's disc and
// sparks in orbit. Drawn behind the disc by the journey's exam stations and the exam's intro
// page, so both look like the same opponent.
//
// center and radius are the disc's; color is the emblem's (the accent, or gold once the
// exam is passed); hover (0 to 1) makes it bristle: the spikes grow and turn faster, the
// aura brightens; time is the clock in seconds.
void BossEmblemDraw(Vector2 center, float radius, Color color, float hover, float time);

// The boss levels' page transition: a cover in color over the whole screen with a round hole
// in the middle, its edge set with spikes that point inwards and turn. open is 0 for shut and
// 1 for a hole as large as the screen; time turns the spikes.
void BossIrisDraw(float open, Color color, float time);

#endif
