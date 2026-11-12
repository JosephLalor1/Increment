#ifndef MASCOT_H
#define MASCOT_H

#include "raylib.h"
#include <stdbool.h>
#include "ui/mascot_state.h"

// The fox mascot on screen: its images, and a fox per place it appears (each with its
// own mood, see mascot_state.h). Without the images in assets/mascot nothing is drawn.

// The places a fox appears. Each keeps its own reactions.
typedef enum { FOX_JOURNEY, FOX_EDITOR, FOX_DASHBOARD, FOX_SLOT_COUNT } FoxSlot;

// Load once after InitWindow, using the same resolved assets folder as lessons and fonts.
// Missing or invalid images leave the mascot unavailable; drawing is then a no-op.
void MascotLoad(const char *assetDir);
void MascotUnload(void);
bool MascotAvailable(void);

// Drops every fox's reaction (at sign-out, so one learner's don't greet the next).
void MascotResetInteractions(void);

// Starts a reaction for the fox in `slot`.
void MascotReact(FoxSlot slot, FoxEvent event);

// Draws that fox inside `bounds` (contained, keeping its shape), showing `fallback` unless
// it is reacting. A change of face cross-fades.
void MascotDrawActor(FoxSlot slot, Rectangle bounds, FoxEmotion fallback);

// The fox's words for its current reaction, or `fallback` when it isn't reacting.
const char *MascotMessage(FoxSlot slot, const char *fallback);

// The fox with a speech bubble beside it saying `message` (or its reaction), `width` wide
// from x, y. Returns the height used, 0 if there is no room or no mascot.
float MascotCompanionActor(FoxSlot slot, float x, float y, float width, float size, float textSize,
                           FoxEmotion fallback, const char *message);

// Makes the fox drawn in `image` clickable: each click shows a learning tip, and every third
// one suggests a short break instead. Call after drawing it.
void MascotOfferTip(FoxSlot slot, Rectangle image);

// Confetti falling over `area`, `age` seconds after it started (it lasts 3.5 s); more of it
// for a graduation.
void MascotConfetti(Rectangle area, double age, bool graduation);

#endif
