#include "mascot_state.h"

static FoxEmotion Reaction(FoxEvent event)
{
    switch (event) {
    case FOX_CORRECT: return FOX_EXCITED;
    case FOX_INCORRECT: return FOX_SAD;
    case FOX_LESSON_COMPLETE: return FOX_TROPHY;
    case FOX_COURSE_COMPLETE: return FOX_GRADUATION;
    case FOX_STATION_COMPLETE: return FOX_CELEBRATING;
    case FOX_BREAK: return FOX_SLEEPY;
    default: return FOX_IDEA;
    }
}

void FoxStateInit(FoxState *state, FoxEmotion fallback, double now)
{
    *state = (FoxState){ .fallback = fallback, .current = fallback, .previous = fallback, .transition = now };
}

void FoxStateReact(FoxState *state, FoxEvent event, double now)
{
    state->previous = state->current;
    state->event = event;
    state->reacting = true;
    state->started = state->transition = now;
    state->current = Reaction(event);
    state->variation++;
}

FoxEmotion FoxStateUpdate(FoxState *state, FoxEmotion fallback, double now)
{
    state->fallback = fallback;
    FoxEmotion desired = fallback;
    if (state->reacting) {
        double age = now - state->started;
        double duration = state->event == FOX_LESSON_COMPLETE || state->event == FOX_COURSE_COMPLETE
                           || state->event == FOX_STATION_COMPLETE ? 6.0 : 3.5;
        if (age >= duration) state->reacting = false;
        else {
            desired = Reaction(state->event);
            if (state->event == FOX_INCORRECT && age >= 0.9) desired = FOX_DETERMINED;
        }
    }
    if (desired != state->current) {
        state->previous = state->current;
        state->current = desired;
        state->transition = now;
    }
    return state->current;
}

const char *FoxStateMessage(const FoxState *state, const char *fallback)
{
    static const char *right[] = { "Awesome! You got it!", "Brilliant answer!", "You're getting better every day!" };
    static const char *wrong[] = { "Not quite! You've got this!", "Every mistake helps you learn!", "Keep going, future programmer!" };
    if (!state->reacting) return fallback;
    switch (state->event) {
    case FOX_CORRECT: return right[state->variation % 3];
    case FOX_INCORRECT: return state->current == FOX_DETERMINED ? "You can solve this. Keep trying!" : wrong[state->variation % 3];
    case FOX_LESSON_COMPLETE:
    case FOX_STATION_COMPLETE: return "Another station completed!";
    case FOX_COURSE_COMPLETE: return "You did it! What an amazing milestone!";
    case FOX_BREAK: return "A short break can help you come back refreshed.";
    default: return "Try tracing the code one line at a time.";
    }
}

FoxMilestone FoxCompletionTransition(bool lessonBefore, bool lessonAfter, bool courseBefore, bool courseAfter)
{
    if (!lessonAfter || lessonBefore) return FOX_MILESTONE_NONE;
    if (!courseBefore && courseAfter) return FOX_MILESTONE_COURSE;
    return FOX_MILESTONE_LESSON;
}
