#ifndef LEVEL_ENTRY_H
#define LEVEL_ENTRY_H

#include "raylib.h"

#include <stdbool.h>

// The way into a level and out of it again.
//
// In: clicking a level zooms into it. The screen as it stood at the click is kept as a
// picture and flown into, towards the clicked point, while a disc in the accent colour
// grows out of that point until it covers everything; the level then appears through an
// opening iris.
//
// Out: the same film backwards. An iris closes on a picture of the level; behind the cover
// the journey is drawn once and kept as a picture, and the disc shrinks into the level's
// station as that picture zooms back out to its real size.
//
// It knows nothing about levels or the journey: main.c starts it, draws it in place of the
// screen while it stands in for one, and does what its cues say (open the level, show the
// journey) at the moments the screen is covered.

typedef enum {
    ENTRY_CUE_NONE,
    ENTRY_CUE_OPEN_LEVEL,    // the zoom in has covered the screen: open the level now
    ENTRY_CUE_SHOW_JOURNEY,  // the iris has closed on the level: draw the journey this frame,
                             // then call LevelExitLand
} LevelEntryCue;

// Starts the zoom into a level, towards a point on screen. Call it once the current screen
// has been drawn this frame: what is on screen now is the picture that gets zoomed.
void LevelEntryStart(Vector2 clickedAt);

// Starts the way out. Call it once the level has been drawn this frame, as above.
void LevelExitStart(void);

// After ENTRY_CUE_SHOW_JOURNEY, with the journey drawn this frame: keeps it as the picture
// to zoom out of, towards the level's station on it, and covers it up again.
void LevelExitLand(Vector2 station);

void LevelEntryUpdate(float dt);  // call every frame

// Whether the animation stands in for the screen: call LevelEntryDraw instead of a screen.
bool LevelEntryStandsIn(void);

// Draws the animation. Each cue is returned once.
LevelEntryCue LevelEntryDraw(void);

// Draws the opening iris over the level; nothing when no entry is going on.
void LevelEntryDrawReveal(void);

void LevelEntryShutdown(void);  // frees the picture

// Dev aid for --shot: the animation started next stays at this moment, in seconds from its
// start.
void LevelEntryDemo(float seconds);

#endif
