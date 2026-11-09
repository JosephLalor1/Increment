#ifndef AMBIENCE_H
#define AMBIENCE_H

#include "raylib.h"

// The journey's background: faint code characters in the accent colour, in one of several
// styles the learner picks in Settings. Particles (the default) are single characters that
// appear at random and drift away. The others are shapes drawn on a grid of character cells:
// each cell eases in and out at its own pace and swaps its character as it does, so every
// shape seems to fizzle into existence and away again rather than switch on and off.

typedef enum {
    AMBIENCE_OFF,
    AMBIENCE_PARTICLES,  // scattered characters appearing and drifting (the default)
    AMBIENCE_SWIRL,      // spiral arms turning slowly around the middle
    AMBIENCE_DOWNPOUR,   // heavy rain: dense streaks falling at different speeds
    AMBIENCE_MANDALA,    // a mandala woven into a web: turning spokes, drifting threads, petals
    AMBIENCE_LIFE,       // Conway's Game of Life, one character per cell
    AMBIENCE_WAVES,      // ribbons rolling across like waves
    AMBIENCE_FOX,        // the mascot's face, in fizzling characters
    AMBIENCE_STYLE_COUNT
} AmbienceStyle;

// Saved settings store the style as a number; 0 and 1 are also the old off / on setting.
const char *AmbienceStyleName(AmbienceStyle style);  // "Particles", "Swirl", ...
void AmbienceSetStyle(AmbienceStyle style);           // out-of-range values mean particles
AmbienceStyle AmbienceGetStyle(void);

// Moves the background on by dt over area (the journey's view), then draws it.
void AmbienceUpdate(Rectangle area, float dt);
void AmbienceDraw(void);
void AmbienceWarm(Rectangle area);  // dev aid for --shot: as if it had been running a while

#endif
