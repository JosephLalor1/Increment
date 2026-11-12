#ifndef EASING_H
#define EASING_H

// Curves that animations share. Each takes progress from 0 to 1 and returns how far along
// the movement should be. Header-only, so effects can use them without a link dependency.

// t held between 0 and 1.
static inline float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

// Fast at first, settling gently at 1.
static inline float EaseOutCubic(float t)
{
    t = 1 - Clamp01(t);
    return 1 - t * t * t;
}

#endif
