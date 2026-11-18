#ifndef PIECE_DEMO_VIEW_H
#define PIECE_DEMO_VIEW_H

#include "level/piece_demos.h"

#include "raylib.h"

// Plays an example of what a piece of code does (piece_demos.h) in a panel: the code with
// the running line lit and the piece glowing gold, the expression on the line worked out
// with its values, the variables as boxes that change (arrays as rows, pointers as arrows,
// freed blocks crossed out), and the output typing itself out, with what the user types
// in the accent colour. No words: the run is the explanation.
//
// Everything follows from how long it has been playing, so a replay is starting the clock
// again. It holds on the last frame when it ends.

typedef enum {
    PIECE_DEMO_VIEW_NONE,
    PIECE_DEMO_VIEW_REPLAY,  // the replay button was clicked
    PIECE_DEMO_VIEW_CLOSE,   // the close button was clicked
} PieceDemoViewAction;

// The panel's size for this example (code at the current text size).
Vector2 PieceDemoViewSize(const PieceDemo *demo);

// How long the example takes to play, in seconds.
float PieceDemoViewLength(const PieceDemo *demo);

// Draws the example `seconds` into playing, inside box; piece is the key shown (it glows
// in the code). Returns a button the learner clicked.
PieceDemoViewAction PieceDemoViewDraw(const PieceDemo *demo, const char *piece, Rectangle box, float seconds);

#endif
