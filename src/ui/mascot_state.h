#ifndef MASCOT_STATE_H
#define MASCOT_STATE_H

#include <stdbool.h>

// The fox's mood over time, with no drawing, so it can be unit-tested (tests/test_mascot.c).
// A fox normally shows the face its screen asks for (the "fallback"). Something happening
// (FoxStateReact) puts on a reaction face for a few seconds, after which it returns to
// the fallback. Callers pass the time in; nothing here reads a clock.

// The faces, in the order of the image files in assets/mascot.
typedef enum {
    FOX_HAPPY, FOX_EXCITED, FOX_CODING, FOX_THINKING, FOX_TROPHY, FOX_GRADUATION,
    FOX_SAD, FOX_DETERMINED, FOX_LOVE_LEARNING, FOX_IDEA, FOX_SLEEPY, FOX_CELEBRATING, FOX_EMOTION_COUNT
} FoxEmotion;

// What a fox can react to.
typedef enum {
    FOX_CORRECT,    // a step passed
    FOX_INCORRECT,  // a wrong run: sad at first, then determined
    FOX_TIP,        // the fox was clicked, or a hint was asked for
    FOX_BREAK,      // every third click suggests a rest
} FoxEvent;

typedef struct {
    FoxEmotion fallback;  // the face to return to
    FoxEmotion current;   // the face showing now
    FoxEmotion previous;  // the face before it, to cross-fade from
    FoxEvent event;       // what is being reacted to, while reacting
    bool reacting;
    double started;       // when the reaction began
    double transition;    // when the face last changed
    unsigned int variation;  // counts reactions, so repeated ones pick different messages
} FoxState;

void FoxStateInit(FoxState *state, FoxEmotion fallback, double now);
void FoxStateReact(FoxState *state, FoxEvent event, double now);

// Ends a reaction that has run its time and returns the face to show at `now`.
FoxEmotion FoxStateUpdate(FoxState *state, FoxEmotion fallback, double now);

// The fox's words for its reaction, or `fallback` when it isn't reacting.
const char *FoxStateMessage(const FoxState *state, const char *fallback);

#endif
