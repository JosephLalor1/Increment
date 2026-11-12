#ifndef MOUSE_TRAIL_H
#define MOUSE_TRAIL_H

#include "raylib.h"

#include <stdbool.h>

// The journey's mouse trail: moving the mouse is like pushing through a soup of code
// characters that only shows where it's stirred. Behind the cursor a wake of characters
// peels off to either side and spreads like water; ahead of it a faint bow wave is pushed
// outwards; around it a few more are stirred up, the more the faster it moves. Every
// character drifts, slows as if in liquid, fizzles and fades. Nothing appears while the
// mouse is still. Translucent, in the accent colour.

// Moves the trail on by dt. While spawning is false (the mouse is off the journey) nothing new
// appears, but what's there keeps moving and fading.
void MouseTrailUpdate(Vector2 mouse, float dt, bool spawning);
void MouseTrailDraw(void);  // in screen coordinates

// Turns the trail on or off (a per-account setting; on by default). While off, nothing is
// added or drawn.
void MouseTrailEnable(bool enabled);

// Dev aid for --shot: the trail as if the mouse had just swept from `from` to `to`.
void MouseTrailDemo(Vector2 from, Vector2 to);

#endif
