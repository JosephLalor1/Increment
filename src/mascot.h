#ifndef MASCOT_H
#define MASCOT_H

#include "raylib.h"
#include <stdbool.h>
#include "mascot_state.h"

typedef enum { FOX_QUIZ, FOX_RESULT, FOX_JOURNEY, FOX_EDITOR, FOX_REVIEW, FOX_READING,
               FOX_DASHBOARD, FOX_CELEBRATION, FOX_PRACTICE, FOX_SLOT_COUNT } FoxSlot;

// Load once after InitWindow, using the same resolved assets folder as lessons/fonts.
// Missing or invalid images leave the mascot unavailable; drawing is then a no-op.
void MascotLoad(const char *assetDir);
void MascotUnload(void);
bool MascotAvailable(void);
void MascotDraw(Rectangle bounds); // contain, preserving alpha and aspect ratio
float MascotCompanion(float x, float y, float width, float size, float textSize, const char *message);
void MascotResetInteractions(void);
void MascotReact(FoxSlot slot, FoxEvent event);
void MascotResetActor(FoxSlot slot, FoxEmotion fallback);
void MascotDrawActor(FoxSlot slot, Rectangle bounds, FoxEmotion fallback);
const char *MascotMessage(FoxSlot slot, const char *fallback);
float MascotCompanionActor(FoxSlot slot, float x, float y, float width, float size, float textSize,
                           FoxEmotion fallback, const char *message);
// Makes the fox drawn in `image` clickable: each click shows a learning tip, and every third
// one suggests a short break instead. Call after drawing it.
void MascotOfferTip(FoxSlot slot, Rectangle image);
void MascotConfetti(Rectangle area, double age, bool graduation);

#endif
