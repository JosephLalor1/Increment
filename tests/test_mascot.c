// The fox's reactions over time (mascot_state.c).
#include "ui/mascot_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Reactions(void)
{
    FoxState state;
    FoxStateInit(&state, FOX_THINKING, 0);
    assert(FoxStateUpdate(&state, FOX_THINKING, 0) == FOX_THINKING);
    FoxStateReact(&state, FOX_CORRECT, 1);
    assert(FoxStateUpdate(&state, FOX_THINKING, 1.1) == FOX_EXCITED);
    assert(state.previous == FOX_THINKING && state.transition == 1);
    const char *firstMessage = FoxStateMessage(&state, "fallback");
    FoxStateReact(&state, FOX_CORRECT, 2);
    assert(strcmp(firstMessage, FoxStateMessage(&state, "fallback")) != 0);
    assert(FoxStateUpdate(&state, FOX_THINKING, 6) == FOX_THINKING && !state.reacting);
    assert(strcmp(FoxStateMessage(&state, "fallback"), "fallback") == 0);
    FoxStateReact(&state, FOX_INCORRECT, 10);
    assert(FoxStateUpdate(&state, FOX_THINKING, 10.5) == FOX_SAD);
    assert(FoxStateUpdate(&state, FOX_THINKING, 11) == FOX_DETERMINED);
    assert(strstr(FoxStateMessage(&state, "fallback"), "Keep trying"));
    assert(FoxStateUpdate(&state, FOX_THINKING, 14) == FOX_THINKING);
    FoxStateReact(&state, FOX_TIP, 40);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 41) == FOX_IDEA);
    FoxStateReact(&state, FOX_BREAK, 50);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 51) == FOX_SLEEPY);
    assert(FoxStateUpdate(&state, FOX_HAPPY, 54) == FOX_HAPPY);
    FoxStateInit(&state, FOX_HAPPY, 60); // account boundary drops reactions
    assert(!state.reacting && state.current == FOX_HAPPY);
}

int main(void)
{
    Reactions();
    puts("All mascot tests passed");
    return 0;
}
