#ifndef LEVEL_FX_H
#define LEVEL_FX_H

#include "raylib.h"

#include <stdbool.h>

// The level screen's visual effects: everything that moves but means nothing to the logic.
// The level screen says what happened (a run started, the output was wrong, the step was
// passed) and where its panels are; this module answers with particles, beams, flashes and
// living panel backdrops. It never reads the step, the code or progress, so the effects can
// be changed freely without touching how levels work. Colours are COL_* only.
//
// What it draws:
//  - always: an oscilloscope line along the output panel that changes mood with what is going
//    on, slow rings turning behind the target, a shimmering progress line under the top bar,
//    a glow round the editor that warms up as the learner types, sparks at the caret;
//  - a run: code characters fly from the editor into the output panel while a scan beam
//    sweeps the code (downwards, or upwards in gold for the check's second run);
//  - each output line as it appears: green sparks for a match, red for a miss;
//  - wrong output: glitch bars over the output, falling red sparks, a red pulse at the
//    screen's edges; a compile error adds lightning from the faulty line;
//  - a pass: shockwave rings, confetti, rising code, a green wash across the screen;
//  - a new step: a wipe across the screen;
//  - a hint: the bulb bursts into rays and its light flies to the hinted line;
//  - a reset: the code is sucked back into the button while the editor rewinds like a tape.

typedef enum {
    FX_STEP_OPENED,
    FX_RUN_STARTED,
    FX_OUTPUT_WRONG,    // the learner's run didn't print the target
    FX_COMPILE_FAILED,  // the learner's code didn't compile
    FX_REFUSED,         // the target was printed, but not by honest means (step_check.h)
    FX_PASSED,
} LevelFxEvent;

// Where things are this frame, so effects start and land in the right places.
typedef struct {
    Rectangle editor, target, output;
    Vector2 badge;  // the pass tick's corner of the output panel
    Vector2 pip;    // the current step's circle in the top bar
} LevelFxLayout;

void LevelFxReset(void);                      // drops every effect (opening a level)
void LevelFxSetLayout(LevelFxLayout layout);  // call every frame
void LevelFxEmit(LevelFxEvent event);
void LevelFxActivity(bool running, bool checking);  // what is being compiled or run right now
void LevelFxKey(Vector2 caret);               // the learner typed: sparks at the caret
void LevelFxLine(Vector2 at, bool matches);   // an output line appeared at this point
void LevelFxLightning(Vector2 from);          // a compile error on the line at this point
// A hint was revealed: from the hint button to the hinted line (if the hint points at one).
void LevelFxHint(Vector2 button, Vector2 line, bool pointsAtLine);
void LevelFxRewind(Vector2 button);           // the code was reset: back into the reset button
void LevelFxUpdate(float dt);

// Drawn by the level screen in its own order:
void LevelFxTargetBackdrop(Rectangle panel);  // behind the target panel's lines
void LevelFxOutputBackdrop(Rectangle panel);  // behind the output panel's lines
void LevelFxEditorGlow(void);                 // over the editor: beam and typing glow
// A thin bar showing how much of the output matches the target (0 to 1); it eases to the
// new value and shines when full.
void LevelFxMatchBar(Rectangle bar, float fraction);
void LevelFxProgressLine(float y, float fraction);  // under the top bar: steps passed
void LevelFxSpinner(Vector2 centre);          // dots circling while the second run goes on
float LevelFxHop(void);                       // how high the mascot is jumping, in pixels
void LevelFxDrawOver(void);                   // everything that goes on top, last

#endif
