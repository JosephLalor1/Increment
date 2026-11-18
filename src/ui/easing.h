#ifndef EASING_H
#define EASING_H

// Curves that animations share. Each takes progress from 0 to 1 and returns how far along
// the movement should be. Header-only, so effects can use them without a link dependency.

#include <math.h>

// t held between 0 and 1.
static inline float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

// Fast at first, settling gently at 1.
static inline float EaseOutCubic(float t)
{
    t = 1 - Clamp01(t);
    return 1 - t * t * t;
}

// A pop: grows past full size (to 1.35) by 60% of the way, then settles back to 1. For ticks,
// crosses and badges appearing.
static inline float PopOvershoot(float t)
{
    t = Clamp01(t);
    return t < 0.6f ? t / 0.6f * 1.35f : 1.35f - (t - 0.6f) / 0.4f * 0.35f;
}

// Moves *value a step towards target and returns it: a smooth glide that covers the same
// share of the distance per second at any frame rate (rate: higher is quicker; 14 takes about
// a fifth of a second). dt is the frame time. Snaps once it is close, so it comes to rest.
static inline float EaseToward(float *value, float target, float rate, float dt)
{
    *value += (target - *value) * (1.0f - expf(-dt * rate));
    if (fabsf(*value - target) < 0.001f) *value = target;
    return *value;
}

#endif
