// The journey's liquid mouse trail. See mouse_trail.h.
//
// A small particle system. The trail follows a smoothed copy of the cursor, and characters
// are dropped every few pixels of that smoothed path (so fast moves still give a continuous
// curve), in three kinds: wake pairs flung out to either side behind the cursor, a bow wave
// pushed ahead of it, and a stirred-up halo around it. Each has a velocity that drag slows,
// as if moving through liquid.
#include "home/mouse_trail.h"
#include "core/random_stream.h"
#include "ui/ui.h"

#include <math.h>

#define MAX_DROPS      220
#define WAKE_SPACING   9.0f    // pixels of smoothed travel between wake pairs
#define BOW_SPACING    16.0f   // and between bow waves
#define FOLLOW_RATE    22.0f   // how quickly the smoothed cursor catches up (per second)
#define DRAG           4.5f    // how quickly characters slow down (per second)
#define MAX_JUMP       220.0f  // a bigger jump in one frame starts afresh instead of drawing a streak

typedef struct {
    Vector2 at, velocity;
    float age, life;  // seconds
    float size;
    float strength;   // how opaque at its strongest, before fading (0 to 1)
    char glyph;
} Drop;

static const char glyphSet[] = "{}[]()<>*&=+-;:!?%#0123456789xyzpqint/|^~";

static Drop drops[MAX_DROPS];
static int dropCount;
static bool enabled = true;
static bool frozen;                  // --shot: hold the demo still so the capture shows it
static bool following;               // the smoothed cursor is tracking the mouse
static Vector2 smoothed, velocity;   // the smoothed cursor and its velocity (pixels per second)
static float wakeDebt, bowDebt;      // smoothed travel not yet spent on drops
static float stirDebt;               // halo drops owed by the stirring rate
static RandomStream randomState = 0x6b43a9b5u;

void MouseTrailEnable(bool on)
{
    enabled = on;
    if (!on) dropCount = 0;
}

static void AddDrop(Vector2 at, Vector2 v, float life, float size, float strength)
{
    if (dropCount == MAX_DROPS) {  // the oldest makes room
        for (int i = 1; i < dropCount; i++) drops[i - 1] = drops[i];
        dropCount--;
    }
    drops[dropCount++] = (Drop){ at, v, 0, life, size, strength, RandomChar(&randomState, glyphSet) };
}

// A wake pair at `at`: two characters flung out to either side of the direction of travel
// (a V behind the cursor, like a boat's), drifting back a little.
static void Wake(Vector2 at, Vector2 dir, float speed)
{
    Vector2 side = { -dir.y, dir.x };
    float push = 35 + 0.18f * fminf(speed, 900);
    for (int s = -1; s <= 1; s += 2) {
        float spread = push * RandomBetween(&randomState, 0.7f, 1.2f);
        Vector2 v = { side.x * s * spread - dir.x * 25, side.y * s * spread - dir.y * 25 };
        Vector2 from = { at.x + side.x * s * 3, at.y + side.y * s * 3 };
        AddDrop(from, v, RandomBetween(&randomState, 0.55f, 0.85f), RandomBetween(&randomState, 13, 17), 0.5f);
    }
}

// A bow wave: a few characters on an arc just ahead of the cursor, pushed outwards from it.
static void Bow(Vector2 at, Vector2 dir, float speed)
{
    float ahead = 16 + 0.02f * fminf(speed, 900);
    float heading = atan2f(dir.y, dir.x);
    for (int i = -2; i <= 2; i++) {
        float angle = heading + i * 0.42f;
        Vector2 out = { cosf(angle), sinf(angle) };
        Vector2 from = { at.x + out.x * ahead, at.y + out.y * ahead };
        float push = 50 + 0.25f * fminf(speed, 900);
        AddDrop(from, (Vector2){ out.x * push, out.y * push }, RandomBetween(&randomState, 0.22f, 0.36f), RandomBetween(&randomState, 11, 14),
                0.32f - 0.04f * fabsf((float)i));
    }
}

// One stirred-up character near the cursor, pushed gently away from it.
static void Stir(Vector2 at, float speed)
{
    float angle = RandomBetween(&randomState, 0, 2 * PI), distance = RandomBetween(&randomState, 10, 34);
    Vector2 out = { cosf(angle), sinf(angle) };
    Vector2 from = { at.x + out.x * distance, at.y + out.y * distance };
    float push = 20 + 0.12f * fminf(speed, 900);
    AddDrop(from, (Vector2){ out.x * push, out.y * push }, RandomBetween(&randomState, 0.35f, 0.6f), RandomBetween(&randomState, 11, 15), 0.28f);
}

void MouseTrailUpdate(Vector2 mouse, float dt, bool spawning)
{
    if (frozen || !enabled) return;

    // Everything already there drifts, slows in the "liquid", fizzles and ages.
    float slow = expf(-DRAG * dt);
    for (int i = 0; i < dropCount; i++) {
        Drop *d = &drops[i];
        d->velocity.x *= slow;
        d->velocity.y *= slow;
        d->at.x += d->velocity.x * dt;
        d->at.y += d->velocity.y * dt + 6 * dt;  // the faintest settling, like the rain
        d->age += dt;
        float moving = sqrtf(d->velocity.x * d->velocity.x + d->velocity.y * d->velocity.y);
        if ((RandomNext(&randomState) % 1000) / 1000.0f < (1.5f + moving * 0.06f) * dt) d->glyph = RandomChar(&randomState, glyphSet);
        if (d->age >= d->life) drops[i--] = drops[--dropCount];
    }

    if (!spawning || dt <= 0) {
        following = false;  // re-entering shouldn't draw a line from where the mouse left
        return;
    }
    float jumpX = mouse.x - smoothed.x, jumpY = mouse.y - smoothed.y;
    if (!following || jumpX * jumpX + jumpY * jumpY > MAX_JUMP * MAX_JUMP) {
        smoothed = mouse;
        velocity = (Vector2){ 0, 0 };
        following = true;
        wakeDebt = bowDebt = stirDebt = 0;
        return;
    }

    // The smoothed cursor eases after the real one, which rounds off jitter and corners.
    Vector2 before = smoothed;
    float follow = 1 - expf(-FOLLOW_RATE * dt);
    smoothed.x += jumpX * follow;
    smoothed.y += jumpY * follow;
    Vector2 step = { smoothed.x - before.x, smoothed.y - before.y };
    float travelled = sqrtf(step.x * step.x + step.y * step.y);
    float blend = 1 - expf(-12 * dt);
    velocity.x += (step.x / dt - velocity.x) * blend;
    velocity.y += (step.y / dt - velocity.y) * blend;
    float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
    if (travelled < 0.01f || speed < 15) return;  // still (or nearly): the soup stays hidden
    Vector2 dir = { velocity.x / speed, velocity.y / speed };

    // Drops along this frame's stretch of the smoothed path, evenly spaced.
    wakeDebt += travelled;
    while (wakeDebt >= WAKE_SPACING) {
        wakeDebt -= WAKE_SPACING;
        float back = wakeDebt / travelled;  // how far back along this frame's step
        Wake((Vector2){ smoothed.x - step.x * back, smoothed.y - step.y * back }, dir, speed);
    }
    bowDebt += travelled;
    while (bowDebt >= BOW_SPACING) {
        bowDebt -= BOW_SPACING;
        Bow(smoothed, dir, speed);
    }
    stirDebt += dt * fminf(speed, 900) * 0.05f;  // the faster, the more is stirred up
    while (stirDebt >= 1) {
        stirDebt -= 1;
        Stir(smoothed, speed);
    }
}

void MouseTrailDraw(void)
{
    if (!enabled) return;
    char glyph[2] = { 0, 0 };
    for (int i = 0; i < dropCount; i++) {
        const Drop *d = &drops[i];
        float t = d->age / d->life;
        float appear = fminf(1, d->age / 0.06f);  // a quick fade in, so nothing pops
        float fade = appear * (1 - t) * (1 - t);   // then a soft ease out
        glyph[0] = d->glyph;
        Vector2 at = { d->at.x - d->size * 0.3f, d->at.y - d->size * 0.5f };
        DrawTextEx(gFonts.mono, glyph, at, d->size, 0, Fade(COL_ACCENT, d->strength * fade));
    }
}

void MouseTrailDemo(Vector2 from, Vector2 to)
{
    dropCount = 0;
    following = false;
    frozen = false;
    // The mouse moving along a gentle curve, a frame at a time.
    for (int i = 0; i <= 30; i++) {
        float t = i / 30.0f;
        Vector2 at = { from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - sinf(t * PI) * 60 };
        MouseTrailUpdate(at, 1 / 60.0f, true);
    }
    frozen = true;
}
