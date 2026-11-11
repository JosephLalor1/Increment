// The level screen's visual effects. See level_fx.h.
#include "level_fx.h"
#include "ui.h"

#include <math.h>
#include <string.h>

#define MAX_PARTICLES 600
#define TWO_PI 6.2831853f

typedef enum {
    P_SPARK,     // a small glowing dot that falls
    P_GLYPH,     // a code character drifting upwards
    P_CONFETTI,  // a tumbling scrap of colour
    P_RING,      // an expanding ring
    P_FLY,       // a code character flying along a curve from `from` to `to`
} ParticleKind;

typedef struct {
    ParticleKind kind;
    Vector2 pos, vel;
    Vector2 from, bend, to;  // P_FLY's curve
    float age, life;         // a negative age is a delay before it appears
    float size, angle, spin;
    Color color;
    char glyph;
} Particle;

// What the oscilloscope line is feeling.
typedef enum { MOOD_IDLE, MOOD_BUSY, MOOD_GOOD, MOOD_BAD } Mood;

static struct {
    Particle particles[MAX_PARTICLES];
    int count;
    LevelFxLayout at;
    float time;
    bool running, checking;
    float trickle;       // time until the next character flies during a run

    Mood mood;
    float amplitude;     // the oscilloscope's, easing towards its mood's
    float heat;          // 0 to 1: recent typing
    float matchShown;    // the match bar's eased value
    float matchShine;    // 1 when the bar has just filled, fading

    // Timed overlays: seconds since each began; negative when not showing.
    float wipe, vignette, glitch, sweep, hop, lightning, rewind;
    float flash;         // strength 0 to 1, fading
    Color flashColor;
    Vector2 lightningFrom;
    float targetPulse;   // the target rings' flash after a pass
} fx;

static const char glyphs[] = "{}();=+*&%<>#01[]";

// ---------------------------------------------------------------------------
// Small helpers

static float Rand01(void) { return GetRandomValue(0, 10000) / 10000.0f; }
static float RandIn(float a, float b) { return a + (b - a) * Rand01(); }
static float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
static float EaseOut(float t) { t = 1 - Clamp01(t); return 1 - t * t * t; }

static Vector2 RandomPointIn(Rectangle r, float inset)
{
    return (Vector2){ RandIn(r.x + inset, r.x + r.width - inset), RandIn(r.y + inset, r.y + r.height - inset) };
}

static Particle *Spawn(ParticleKind kind, Vector2 pos, float life, Color color)
{
    if (fx.count >= MAX_PARTICLES) return &fx.particles[GetRandomValue(0, MAX_PARTICLES - 1)];  // reuse one
    Particle *p = &fx.particles[fx.count++];
    memset(p, 0, sizeof *p);
    p->kind = kind;
    p->pos = pos;
    p->life = life;
    p->color = color;
    p->size = 3;
    p->glyph = glyphs[GetRandomValue(0, (int)sizeof glyphs - 2)];
    return p;
}

static void Sparks(Vector2 at, int count, Color color, float speed)
{
    for (int i = 0; i < count; i++) {
        Particle *p = Spawn(P_SPARK, at, RandIn(0.35f, 0.8f), color);
        float angle = RandIn(0, TWO_PI);
        p->vel = (Vector2){ cosf(angle) * RandIn(0.3f, 1) * speed, sinf(angle) * RandIn(0.3f, 1) * speed - speed * 0.4f };
        p->size = RandIn(1.5f, 3.5f);
    }
}

static void Ring(Vector2 at, float size, float delay, Color color)
{
    Particle *p = Spawn(P_RING, at, 0.7f, color);
    p->age = -delay;
    p->size = size;
}

// A code character flies in an arc from one point to another, after a delay.
static void Fly(Vector2 from, Vector2 to, Color color, float delay)
{
    Particle *p = Spawn(P_FLY, from, RandIn(0.45f, 0.8f), color);
    p->from = from;
    p->to = to;
    p->bend = (Vector2){ (from.x + to.x) / 2 + RandIn(-40, 40), fminf(from.y, to.y) - RandIn(40, 160) };
    p->age = -delay;
    p->size = RandIn(14, 20);
}

// The part of the editor where the code usually is.
static Rectangle CodeArea(void)
{
    Rectangle code = fx.at.editor;
    code.height *= 0.55f;
    return code;
}

// A code character leaves the editor for the output panel.
static void FlyToOutput(float delay)
{
    if (fx.at.editor.width <= 0) return;  // nothing has been laid out yet (the level's first frame)
    Fly(RandomPointIn(CodeArea(), 30), RandomPointIn(fx.at.output, 24), fx.checking ? COL_GOLD : COL_ACCENT, delay);
}

// ---------------------------------------------------------------------------
// Events

void LevelFxReset(void)
{
    memset(&fx, 0, sizeof fx);
    fx.wipe = fx.vignette = fx.glitch = fx.sweep = fx.hop = fx.lightning = fx.rewind = -1;
}

void LevelFxSetLayout(LevelFxLayout layout) { fx.at = layout; }

void LevelFxActivity(bool running, bool checking)
{
    fx.running = running;
    fx.checking = checking;
    if (running || checking) fx.mood = MOOD_BUSY;
    else if (fx.mood == MOOD_BUSY) fx.mood = MOOD_IDLE;  // it ended with nothing to say (a step's first run)
}

void LevelFxKey(Vector2 caret)
{
    Sparks(caret, 3, COL_ACCENT, 70);
    fx.heat = fminf(1, fx.heat + 0.14f);
}

void LevelFxLine(Vector2 at, bool matches)
{
    Sparks(at, matches ? 6 : 4, matches ? COL_GOOD : COL_BAD, matches ? 120 : 80);
    if (matches) Ring(at, 16, 0, COL_GOOD);
}

void LevelFxLightning(Vector2 from)
{
    fx.lightningFrom = from;
    fx.lightning = 0;
}

void LevelFxHint(Vector2 button, Vector2 line, bool pointsAtLine)
{
    // The bulb lights up: rays burst from it, and its light travels to the hinted line,
    // where it lands in widening rings.
    for (int i = 0; i < 10; i++) {
        float angle = i * TWO_PI / 10;
        Particle *p = Spawn(P_SPARK, button, 0.45f, COL_GOLD);
        p->vel = (Vector2){ cosf(angle) * 150, sinf(angle) * 150 - 40 };
        p->size = 2.5f;
    }
    Ring(button, 30, 0, COL_GOLD);
    if (!pointsAtLine) return;
    for (int i = 0; i < 16; i++) Fly(button, (Vector2){ line.x + RandIn(0, 120), line.y + RandIn(-6, 6) }, COL_GOLD, i * 0.02f);
    Ring(line, 26, 0.5f, COL_GOLD);
    Ring(line, 48, 0.62f, COL_GOLD);
}

void LevelFxRewind(Vector2 button)
{
    // The code is pulled back into the button, and the editor rewinds like a tape.
    if (fx.at.editor.width <= 0) return;
    for (int i = 0; i < 34; i++) Fly(RandomPointIn(CodeArea(), 20), button, COL_DIM, i * 0.008f);
    Ring(button, 34, 0.35f, COL_ACCENT);
    fx.rewind = 0;
}

static void Flash(Color color, float strength)
{
    fx.flashColor = color;
    fx.flash = strength;
}

void LevelFxEmit(LevelFxEvent event)
{
    Rectangle out = fx.at.output;
    switch (event) {
    case FX_STEP_OPENED:
        fx.count = 0;
        fx.wipe = 0;
        fx.mood = MOOD_IDLE;
        fx.matchShown = fx.matchShine = 0;
        break;
    case FX_RUN_STARTED:
        for (int i = 0; i < 26; i++) FlyToOutput(i * 0.012f);
        fx.mood = MOOD_BUSY;
        break;
    case FX_COMPILE_FAILED:
        Flash(COL_BAD, 0.16f);
        // fall through - a compile error is a wrong run with a flash
    case FX_OUTPUT_WRONG:
    case FX_REFUSED:
        fx.glitch = fx.vignette = 0;
        fx.mood = MOOD_BAD;
        for (int i = 0; i < 18; i++) {  // red sparks shaken loose from the panel's top edge
            Particle *p = Spawn(P_SPARK, (Vector2){ RandIn(out.x, out.x + out.width), out.y }, RandIn(0.5f, 1.1f), COL_BAD);
            p->vel = (Vector2){ RandIn(-30, 30), RandIn(20, 120) };
            p->size = RandIn(1.5f, 3);
        }
        if (event == FX_REFUSED) Ring(fx.at.badge, 46, 0, COL_BAD);
        break;
    case FX_PASSED: {
        static const float ringSizes[] = { 60, 110, 180 };
        for (int i = 0; i < 3; i++) Ring(fx.at.badge, ringSizes[i], i * 0.09f, COL_GOOD);
        Ring(fx.at.pip, 34, 0.15f, COL_ACCENT);
        for (int i = 0; i < 80; i++) {  // confetti thrown from the tick
            Color colors[] = { COL_ACCENT, COL_GOOD, COL_GOLD, COL_TEXT };
            Particle *p = Spawn(P_CONFETTI, fx.at.badge, RandIn(1.0f, 1.9f), colors[i % 4]);
            float angle = RandIn(-TWO_PI * 0.5f, 0);  // upwards, either way
            p->vel = (Vector2){ cosf(angle) * RandIn(120, 520), sinf(angle) * RandIn(200, 620) };
            p->size = RandIn(5, 10);
            p->spin = RandIn(-12, 12);
        }
        for (int i = 0; i < 28; i++) {  // the code lifts off
            Particle *p = Spawn(P_GLYPH, RandomPointIn(fx.at.editor, 30), RandIn(0.9f, 1.6f), COL_GOOD);
            p->vel = (Vector2){ RandIn(-20, 20), RandIn(-160, -60) };
            p->size = RandIn(14, 22);
            p->age = -RandIn(0, 0.3f);
        }
        Flash(COL_GOOD, 0.22f);
        fx.sweep = fx.hop = 0;
        fx.targetPulse = 1;
        fx.mood = MOOD_GOOD;
        fx.heat = 1;
    } break;
    }
}

// ---------------------------------------------------------------------------
// Update

static void Advance(float *timer, float dt, float duration)
{
    if (*timer < 0) return;
    *timer += dt;
    if (*timer > duration) *timer = -1;
}

void LevelFxUpdate(float dt)
{
    if (dt > 0.05f) dt = 0.05f;  // a long frame (a compile starting) shouldn't fling things about
    fx.time += dt;

    // Characters keep flying for as long as something runs.
    if (fx.running || fx.checking) {
        fx.trickle -= dt;
        if (fx.trickle <= 0) {
            FlyToOutput(0);
            fx.trickle = 0.045f;
        }
    }

    for (int i = 0; i < fx.count;) {
        Particle *p = &fx.particles[i];
        p->age += dt;
        if (p->age >= p->life) {
            *p = fx.particles[--fx.count];
            continue;
        }
        if (p->age >= 0 && p->kind != P_RING && p->kind != P_FLY) {
            float gravity = p->kind == P_GLYPH ? -30 : p->kind == P_CONFETTI ? 620 : 380;
            p->vel.y += gravity * dt;
            if (p->kind == P_CONFETTI) p->vel.x *= 1 - 1.4f * dt;  // air slows it sideways
            p->pos.x += p->vel.x * dt;
            p->pos.y += p->vel.y * dt;
            p->angle += p->spin * dt;
        }
        i++;
    }

    static const float amplitudes[] = { 2.5f, 11, 7, 10 };
    fx.amplitude += (amplitudes[fx.mood] - fx.amplitude) * fminf(1, dt * 6);
    fx.heat = fmaxf(0, fx.heat - dt * 0.45f);
    fx.flash = fmaxf(0, fx.flash - dt * 0.7f);
    fx.matchShine = fmaxf(0, fx.matchShine - dt * 0.8f);
    fx.targetPulse = fmaxf(0, fx.targetPulse - dt * 0.9f);
    Advance(&fx.wipe, dt, 0.45f);
    Advance(&fx.vignette, dt, 0.7f);
    Advance(&fx.glitch, dt, 0.4f);
    Advance(&fx.sweep, dt, 0.7f);
    Advance(&fx.hop, dt, 1.2f);
    Advance(&fx.lightning, dt, 0.4f);
    Advance(&fx.rewind, dt, 0.45f);
}

// ---------------------------------------------------------------------------
// Drawn in the level screen's order

void LevelFxTargetBackdrop(Rectangle panel)
{
    // A target turning slowly in the corner: broken rings, each at its own pace.
    Vector2 centre = { panel.x + panel.width - 70, panel.y + panel.height - 60 };
    Color color = fx.targetPulse > 0 ? ColorLerp(COL_ACCENT, COL_GOOD, fx.targetPulse) : COL_ACCENT;
    BeginScissorMode((int)panel.x, (int)panel.y, (int)panel.width, (int)panel.height);
    for (int ring = 0; ring < 4; ring++) {
        float radius = 22 + ring * 20 + sinf(fx.time * 1.3f + ring) * 2 + fx.targetPulse * 14;
        float turn = fx.time * (ring % 2 ? -14 : 9) * (1 + ring * 0.3f);
        float alpha = 0.10f - ring * 0.018f + fx.targetPulse * 0.25f;
        for (int arc = 0; arc < 3; arc++)
            DrawRing(centre, radius, radius + 2.5f, turn + arc * 120, turn + arc * 120 + 84, 24, Fade(color, alpha));
    }
    DrawCircleV(centre, 5 + fx.targetPulse * 6, Fade(color, 0.16f + fx.targetPulse * 0.4f));
    EndScissorMode();
}

void LevelFxOutputBackdrop(Rectangle panel)
{
    // An oscilloscope line along the bottom: calm when idle, racing during a run, a smooth
    // wave after a pass, jagged after a miss.
    static const float speeds[] = { 1.2f, 9, 3, 14 };
    Color colors[] = { COL_DIM, fx.checking ? COL_GOLD : COL_ACCENT, COL_GOOD, COL_BAD };
    Color color = Fade(colors[fx.mood], fx.mood == MOOD_IDLE ? 0.3f : 0.55f);
    float baseline = panel.y + panel.height - 26, left = panel.x + 14, width = panel.width - 28;
    Vector2 previous = { left, baseline };
    for (int i = 1; i <= 64; i++) {
        float t = i / 64.0f, x = left + t * width;
        float wave = sinf(t * 18 + fx.time * speeds[fx.mood]) * 0.7f + sinf(t * 41 - fx.time * speeds[fx.mood] * 0.6f) * 0.3f;
        if (fx.mood == MOOD_BAD) wave = (wave > 0 ? 1 : -1) * powf(fabsf(wave), 0.3f);  // squared off: harsh
        float fadeAtEnds = sinf(t * 3.14159f);
        Vector2 point = { x, baseline + wave * fx.amplitude * fadeAtEnds };
        DrawLineEx(previous, point, 2, color);
        previous = point;
    }
}

void LevelFxEditorGlow(void)
{
    Rectangle r = fx.at.editor;
    if (fx.heat > 0.01f) {  // typing warms the frame
        for (int i = 0; i < 3; i++) {
            Rectangle glow = { r.x - i * 2, r.y - i * 2, r.width + i * 4, r.height + i * 4 };
            DrawRectangleRoundedLinesEx(glow, 16 / fminf(glow.width, glow.height), 8, 2, Fade(COL_ACCENT, fx.heat * (0.5f - i * 0.15f)));
        }
    }
    if (fx.rewind >= 0) {  // a reset: tracking lines race up the editor, as on a rewinding tape
        float t = fx.rewind / 0.45f, strength = 1 - t;
        BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
        DrawRectangleRec(r, Fade(COL_TEXT, 0.06f * strength));
        for (int i = 0; i < 6; i++) {
            float y = r.y + r.height * fmodf(1.3f - t * 2.4f + i * 0.21f + 4, 1);
            DrawRectangle((int)r.x, (int)y, (int)r.width, 2 + i % 3, Fade(COL_TEXT, 0.28f * strength));
            DrawRectangle((int)(r.x + (i % 2 ? 14 : -14) * strength), (int)y + 6, (int)r.width, 8, Fade(COL_ACCENT, 0.10f * strength));
        }
        EndScissorMode();
    }
    if (!fx.running && !fx.checking) return;
    // A beam sweeps the code: down for the learner's run, up in gold for the second run.
    float phase = fmodf(fx.time * 1.1f, 1);
    if (fx.checking) phase = 1 - phase;
    Color color = fx.checking ? COL_GOLD : COL_ACCENT;
    float y = r.y + phase * r.height, trail = 70;
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    if (fx.checking) DrawRectangleGradientV((int)r.x, (int)y, (int)r.width, (int)trail, Fade(color, 0.22f), Fade(color, 0));
    else DrawRectangleGradientV((int)r.x, (int)(y - trail), (int)r.width, (int)trail, Fade(color, 0), Fade(color, 0.22f));
    DrawRectangle((int)r.x, (int)y - 1, (int)r.width, 2, Fade(color, 0.9f));
    EndScissorMode();
}

void LevelFxMatchBar(Rectangle bar, float fraction)
{
    float before = fx.matchShown;
    fx.matchShown += (fraction - fx.matchShown) * fminf(1, GetFrameTime() * 9);
    if (fraction >= 1 && before < 0.985f && fx.matchShown >= 0.985f) fx.matchShine = 1;
    UiRoundRect(bar, bar.height / 2, COL_PANEL_HI);
    if (fx.matchShown < 0.005f) return;
    Color color = ColorLerp(COL_BAD, COL_GOOD, fx.matchShown * fx.matchShown);
    Rectangle fill = { bar.x, bar.y, bar.width * fx.matchShown, bar.height };
    UiRoundRect(fill, bar.height / 2, color);
    // A glint runs along the filled part; the whole bar glows when it has just filled.
    float glint = fmodf(fx.time * 0.6f, 1.4f) - 0.2f;
    if (glint > 0 && glint < 1) {
        float gx = fill.x + glint * fill.width;
        DrawRectangleGradientH((int)(gx - 30), (int)bar.y, 30, (int)bar.height, Fade(WHITE, 0), Fade(WHITE, 0.5f));
    }
    if (fx.matchShine > 0)
        UiRoundRect((Rectangle){ bar.x - 4, bar.y - 4, bar.width + 8, bar.height + 8 }, bar.height, Fade(COL_GOOD, 0.4f * fx.matchShine));
}

void LevelFxProgressLine(float y, float fraction)
{
    float width = (float)GetScreenWidth(), filled = width * Clamp01(fraction);
    DrawRectangle(0, (int)y, (int)filled, 3, Fade(COL_ACCENT, 0.75f));
    float glint = fmodf(fx.time * 0.35f, 1.3f) * filled;  // a light travelling along what's done
    if (glint < filled) DrawRectangleGradientH((int)(glint - 80), (int)y, 80, 3, Fade(COL_ON_ACCENT, 0), Fade(COL_ON_ACCENT, 0.8f));
    if (filled > 2) DrawCircleV((Vector2){ filled, y + 1.5f }, 4 + sinf(fx.time * 4) * 1.2f, COL_ACCENT);
}

void LevelFxSpinner(Vector2 centre)
{
    for (int i = 0; i < 8; i++) {
        float angle = fx.time * 5 + i * TWO_PI / 8;
        float lead = (float)i / 8;  // the dots grow towards the head of the circle
        DrawCircleV((Vector2){ centre.x + cosf(angle) * 14, centre.y + sinf(angle) * 14 }, 1.5f + lead * 2.5f,
                    Fade(COL_GOLD, 0.25f + lead * 0.75f));
    }
}

float LevelFxHop(void)
{
    if (fx.hop < 0) return 0;
    return fabsf(sinf(fx.hop * 9)) * 22 * (1 - fx.hop / 1.2f);  // three or four bounces, each lower
}

// ---------------------------------------------------------------------------
// On top of everything

static Vector2 CurvePoint(const Particle *p, float t)
{
    float u = 1 - t;
    return (Vector2){ u * u * p->from.x + 2 * u * t * p->bend.x + t * t * p->to.x,
                      u * u * p->from.y + 2 * u * t * p->bend.y + t * t * p->to.y };
}

static void DrawGlyph(char glyph, Vector2 at, float size, Color color)
{
    char text[2] = { glyph, '\0' };
    DrawTextEx(gFonts.mono, text, (Vector2){ at.x - size * 0.3f, at.y - size * 0.5f }, size, 0, color);
}

static void DrawParticle(const Particle *p)
{
    if (p->age < 0) return;
    float t = p->age / p->life, fade = 1 - t;
    switch (p->kind) {
    case P_SPARK:
        DrawCircleV(p->pos, p->size * 2.4f, Fade(p->color, 0.18f * fade));
        DrawCircleV(p->pos, p->size * fade, Fade(p->color, fade));
        break;
    case P_GLYPH:
        DrawGlyph(p->glyph, p->pos, p->size, Fade(p->color, fade * 0.8f));
        break;
    case P_CONFETTI: {
        float flutter = fabsf(cosf(p->angle));  // turning edge-on as it tumbles
        Rectangle scrap = { p->pos.x, p->pos.y, p->size, p->size * (0.25f + 0.75f * flutter) };
        DrawRectanglePro(scrap, (Vector2){ scrap.width / 2, scrap.height / 2 }, p->angle * 40, Fade(p->color, fminf(1, fade * 2.5f)));
    } break;
    case P_RING: {
        float radius = p->size * EaseOut(t);
        DrawRing(p->pos, radius, radius + 3 + 6 * fade, 0, 360, 48, Fade(p->color, fade * 0.7f));
    } break;
    case P_FLY: {
        float eased = t * t * (3 - 2 * t);
        Vector2 at = CurvePoint(p, eased);
        for (int ghost = 1; ghost <= 3; ghost++)  // a short tail
            DrawCircleV(CurvePoint(p, fmaxf(0, eased - ghost * 0.04f)), 2.2f - ghost * 0.5f, Fade(p->color, 0.35f - ghost * 0.09f));
        DrawCircleV(at, p->size * 0.55f, Fade(p->color, 0.14f));
        DrawGlyph(p->glyph, at, p->size, Fade(p->color, 0.35f + 0.65f * sinf(t * 3.14159f)));
    } break;
    }
}

// A jagged bolt from the faulty line to the output panel, redrawn differently each frame.
static void DrawLightning(void)
{
    float strength = 1 - fx.lightning / 0.4f;
    Vector2 to = { fx.at.output.x + 6, fx.at.output.y + 60 }, previous = fx.lightningFrom;
    for (int i = 1; i <= 12; i++) {
        float t = i / 12.0f;
        Vector2 point = { fx.lightningFrom.x + (to.x - fx.lightningFrom.x) * t, fx.lightningFrom.y + (to.y - fx.lightningFrom.y) * t };
        if (i < 12) {
            point.x += RandIn(-10, 10);
            point.y += RandIn(-16, 16);
        }
        DrawLineEx(previous, point, 6, Fade(COL_BAD, 0.25f * strength));
        DrawLineEx(previous, point, 2, Fade(COL_TEXT, strength));
        previous = point;
    }
    DrawCircleV(fx.lightningFrom, 6 + 6 * strength, Fade(COL_BAD, 0.6f * strength));
}

void LevelFxDrawOver(void)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    for (int i = 0; i < fx.count; i++) DrawParticle(&fx.particles[i]);
    if (fx.lightning >= 0) DrawLightning();

    if (fx.glitch >= 0) {  // slices of the output panel jump sideways
        Rectangle out = fx.at.output;
        float strength = 1 - fx.glitch / 0.4f;
        for (int i = 0; i < 7; i++) {
            float y = out.y + RandIn(0, out.height - 14), h = RandIn(3, 14), shift = RandIn(-26, 26) * strength;
            DrawRectangle((int)(out.x + shift), (int)y, (int)out.width, (int)h, Fade(i % 2 ? COL_BAD : COL_ACCENT, 0.22f * strength));
        }
    }
    if (fx.vignette >= 0) {  // a red pulse closing in from the edges
        float strength = sinf(fx.vignette / 0.7f * 3.14159f) * 0.38f;
        Color edge = Fade(COL_BAD, strength), none = Fade(COL_BAD, 0);
        int depth = 150;
        DrawRectangleGradientH(0, 0, depth, (int)H, edge, none);
        DrawRectangleGradientH((int)W - depth, 0, depth, (int)H, none, edge);
        DrawRectangleGradientV(0, 0, (int)W, depth, edge, none);
        DrawRectangleGradientV(0, (int)H - depth, (int)W, depth, none, edge);
    }
    if (fx.sweep >= 0) {  // a green wash crosses the screen after a pass
        float t = EaseOut(fx.sweep / 0.7f), band = W * 0.5f, x = -band + t * (W + band);
        float strength = 0.28f * (1 - fx.sweep / 0.7f);
        DrawRectangleGradientH((int)x, 0, (int)(band / 2), (int)H, Fade(COL_GOOD, 0), Fade(COL_GOOD, strength));
        DrawRectangleGradientH((int)(x + band / 2), 0, (int)(band / 2), (int)H, Fade(COL_GOOD, strength), Fade(COL_GOOD, 0));
    }
    if (fx.flash > 0) DrawRectangle(0, 0, (int)W, (int)H, Fade(fx.flashColor, fx.flash));
    if (fx.wipe >= 0) {  // a new step slides in behind a curtain of accent colour
        float t = EaseOut(fx.wipe / 0.45f), band = W * 0.6f, x = -band + t * (W + band);
        float strength = 0.45f * (1 - fx.wipe / 0.45f);
        DrawRectangleGradientH((int)x, 0, (int)band, (int)H, Fade(COL_ACCENT, strength), Fade(COL_ACCENT, 0));
    }
}
