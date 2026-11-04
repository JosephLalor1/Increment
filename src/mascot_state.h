#ifndef MASCOT_STATE_H
#define MASCOT_STATE_H

#include <stdbool.h>

typedef enum {
    FOX_HAPPY, FOX_EXCITED, FOX_CODING, FOX_THINKING, FOX_TROPHY, FOX_GRADUATION,
    FOX_SAD, FOX_DETERMINED, FOX_LOVE_LEARNING, FOX_IDEA, FOX_SLEEPY, FOX_CELEBRATING, FOX_EMOTION_COUNT
} FoxEmotion;
typedef enum { FOX_CORRECT, FOX_INCORRECT, FOX_LESSON_COMPLETE, FOX_COURSE_COMPLETE, FOX_TIP, FOX_BREAK, FOX_STATION_COMPLETE } FoxEvent;
typedef enum { FOX_MILESTONE_NONE, FOX_MILESTONE_LESSON, FOX_MILESTONE_COURSE } FoxMilestone;

typedef struct {
    FoxEmotion fallback, current, previous;
    FoxEvent event;
    bool reacting;
    double started, transition;
    unsigned int variation;
} FoxState;

void FoxStateInit(FoxState *state, FoxEmotion fallback, double now);
void FoxStateReact(FoxState *state, FoxEvent event, double now);
FoxEmotion FoxStateUpdate(FoxState *state, FoxEmotion fallback, double now);
const char *FoxStateMessage(const FoxState *state, const char *fallback);
FoxMilestone FoxCompletionTransition(bool lessonBefore, bool lessonAfter, bool courseBefore, bool courseAfter);

#endif
