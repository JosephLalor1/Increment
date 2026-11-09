#ifndef CODE_RAIN_H
#define CODE_RAIN_H

#include "raylib.h"

#include <stdbool.h>

// Columns of code characters falling behind the speed round. One intensity, from 0 (a few
// slow, faint streams in the accent colour) to 1 (a dense, fast, flickering downpour that
// has turned the warning colour), drives everything. It acts at once, so dropping it back
// to 0 calms the rain immediately. Colours come from the theme.

// Moves the rain on by dt seconds over area, adding new streams as the intensity asks.
void CodeRainUpdate(Rectangle area, float dt, float intensity);
void CodeRainDraw(float intensity);
void CodeRainClear(void);  // no streams, for a fresh start

// The journey's two background effects, each of which the learner can turn off in Settings
// (both on by default). A disabled effect's functions below do nothing and draw nothing.
void CodeJourneyEffects(bool ambience, bool trail);

// Ambience: tiny, faint code characters that appear at random behind the journey, drift
// slightly and fade away over a few seconds. Update every frame with the area they live in.
void CodeAmbienceUpdate(Rectangle area, float dt);
void CodeAmbienceDraw(void);
void CodeAmbienceWarm(Rectangle area);  // dev aid for --shot: as if it had been running a while

// A short, translucent trail of code characters left behind the mouse (on the journey).
// Characters drop off along the path as the mouse moves; each drifts down and fades out in
// about half a second. Update every frame: spawning stops while spawning is false, but the
// characters already there still fade.
void CodeTrailUpdate(Vector2 mouse, float dt, bool spawning);
void CodeTrailDraw(void);  // in screen coordinates, in the accent colour
// Dev aid for --shot: a trail as if the mouse had just moved from `from` to `to`.
void CodeTrailDemo(Vector2 from, Vector2 to);

#endif
