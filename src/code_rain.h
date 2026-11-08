#ifndef CODE_RAIN_H
#define CODE_RAIN_H

#include "raylib.h"

// Columns of code characters falling behind the speed round. One intensity, from 0 (a few
// slow, faint streams in the accent colour) to 1 (a dense, fast, flickering downpour that
// has turned the warning colour), drives everything. It acts at once, so dropping it back
// to 0 calms the rain immediately. Colours come from the theme.

// Moves the rain on by dt seconds over area, adding new streams as the intensity asks.
void CodeRainUpdate(Rectangle area, float dt, float intensity);
void CodeRainDraw(float intensity);
void CodeRainClear(void);  // no streams, for a fresh start

#endif
