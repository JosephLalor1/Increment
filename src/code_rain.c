// Falling code characters for the speed round. See code_rain.h.
#include "code_rain.h"
#include "ui.h"

#include <math.h>

#define MAX_STREAMS 420
#define GLYPH_SIZE  20.0f

typedef struct {
    float x, head;   // column position and the y of the leading character
    float speed;     // pixels per second at intensity 0
    int length;      // characters in the trail
    unsigned glyphs; // picks the trail's characters
} Stream;

static Stream streams[MAX_STREAMS];
static int streamCount;
static float spawnDebt;      // fractional streams owed by the spawn rate
static float flickerClock;   // advances faster with intensity, to swap characters
static unsigned randomState = 2463534242u;
static Rectangle rainArea;

static const char glyphSet[] = "{}[]()<>*&=+-;:!?%#01234567890xyzpqint*&->/|^~";

static unsigned NextRandom(void)
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

static float RandomBetween(float lo, float hi) { return lo + (hi - lo) * (float)(NextRandom() % 10000) / 10000.0f; }

void CodeRainClear(void)
{
    streamCount = 0;
    spawnDebt = 0;
}

// A new stream above the top edge, in a random column.
static void Spawn(void)
{
    if (streamCount >= MAX_STREAMS) return;
    float column = GLYPH_SIZE * 0.9f;
    int columns = (int)(rainArea.width / column);
    if (columns < 1) return;
    Stream *s = &streams[streamCount++];
    s->x = rainArea.x + (float)(NextRandom() % (unsigned)columns) * column;
    s->length = 5 + (int)(NextRandom() % 14);
    s->head = rainArea.y - RandomBetween(0, GLYPH_SIZE * 6);
    s->speed = RandomBetween(70, 170);
    s->glyphs = NextRandom();
}

void CodeRainUpdate(Rectangle area, float dt, float intensity)
{
    rainArea = area;
    float pace = 0.5f + 4.5f * intensity * intensity;  // everything speeds up together
    flickerClock += dt * (1 + 14 * intensity);
    spawnDebt += dt * (5 + 140 * intensity * intensity);
    while (spawnDebt >= 1) {
        Spawn();
        spawnDebt -= 1;
    }
    for (int i = 0; i < streamCount; i++) {
        Stream *s = &streams[i];
        s->head += s->speed * pace * dt;
        if (s->head - s->length * GLYPH_SIZE > area.y + area.height) streams[i--] = streams[--streamCount];  // gone off the bottom
    }
}

static Color Mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

// ---------------------------------------------------------------------------
// The journey's effects: which are on, and the ambience

static bool ambienceOn = true, trailOn = true;

#define AMBIENCE_MAX 48

typedef struct {
    Vector2 at;
    float age, life;  // seconds
    float size;
    char glyph;
} AmbientGlyph;

static AmbientGlyph ambience[AMBIENCE_MAX];
static int ambienceCount;
static float ambienceDebt;  // fractional characters owed by the appear rate

void CodeJourneyEffects(bool ambienceEnabled, bool trailEnabled)
{
    ambienceOn = ambienceEnabled;
    trailOn = trailEnabled;
    if (!ambienceOn) ambienceCount = 0;
}

void CodeAmbienceUpdate(Rectangle area, float dt)
{
    if (!ambienceOn) return;
    for (int i = 0; i < ambienceCount; i++) {
        ambience[i].age += dt;
        ambience[i].at.y += 8 * dt;  // the slowest of drifts, a hint of the rain
        if (ambience[i].age >= ambience[i].life) ambience[i--] = ambience[--ambienceCount];
    }
    ambienceDebt += dt * 7;  // characters appearing per second
    while (ambienceDebt >= 1) {
        ambienceDebt -= 1;
        if (ambienceCount == AMBIENCE_MAX) continue;
        ambience[ambienceCount++] = (AmbientGlyph){
            { RandomBetween(area.x, area.x + area.width), RandomBetween(area.y, area.y + area.height) },
            0, RandomBetween(2.5f, 5.0f), RandomBetween(12, 17), glyphSet[NextRandom() % (sizeof glyphSet - 1)] };
    }
}

void CodeAmbienceDraw(void)
{
    if (!ambienceOn) return;
    for (int i = 0; i < ambienceCount; i++) {
        float fade = sinf(PI * ambience[i].age / ambience[i].life);  // in, then out again
        char glyph[2] = { ambience[i].glyph, '\0' };
        DrawTextEx(gFonts.mono, glyph, ambience[i].at, ambience[i].size, 0, Fade(COL_ACCENT, 0.24f * fade));
    }
}

void CodeAmbienceWarm(Rectangle area)
{
    for (int frame = 0; frame < 300; frame++) CodeAmbienceUpdate(area, 1 / 60.0f);
}

// ---------------------------------------------------------------------------
// The mouse trail

#define TRAIL_MAX      24
#define TRAIL_SPACING  16.0f   // pixels of mouse travel between characters
#define TRAIL_LIFE     0.55f   // seconds a character lasts
#define TRAIL_SIZE     18.0f

typedef struct {
    Vector2 at;
    float age;       // seconds since it dropped off
    char glyph;
} TrailGlyph;

static TrailGlyph trail[TRAIL_MAX];
static int trailCount;
static Vector2 trailLast;    // where the last character dropped off
static bool trailStarted;    // trailLast is meaningful
static bool trailFrozen;     // --shot: hold the demo trail still so the capture shows it

static void TrailAdd(Vector2 at)
{
    if (trailCount == TRAIL_MAX) {  // drop the oldest
        for (int i = 1; i < trailCount; i++) trail[i - 1] = trail[i];
        trailCount--;
    }
    trail[trailCount++] = (TrailGlyph){ at, 0, glyphSet[NextRandom() % (sizeof glyphSet - 1)] };
}

void CodeTrailUpdate(Vector2 mouse, float dt, bool spawning)
{
    if (trailFrozen) return;
    if (!trailOn) {
        trailCount = 0;
        trailStarted = false;
        return;
    }
    for (int i = 0; i < trailCount; i++) {
        trail[i].age += dt;
        trail[i].at.y += 40 * dt;  // a gentle fall, like the rain
        if (trail[i].age >= TRAIL_LIFE) {
            for (int j = i + 1; j < trailCount; j++) trail[j - 1] = trail[j];
            trailCount--;
            i--;
        }
    }
    if (!spawning) {
        trailStarted = false;  // re-entering the area shouldn't draw a line from where it left
        return;
    }
    if (!trailStarted) {
        trailLast = mouse;
        trailStarted = true;
        return;
    }
    // One character every TRAIL_SPACING pixels along the path, so fast moves leave a line.
    float dx = mouse.x - trailLast.x, dy = mouse.y - trailLast.y;
    float distance = sqrtf(dx * dx + dy * dy);
    int steps = (int)(distance / TRAIL_SPACING);
    if (steps > 8) steps = 8;  // a jump across the window leaves a short trail, not a long one
    for (int s = 1; s <= steps; s++) {
        float t = (float)s / steps;
        TrailAdd((Vector2){ trailLast.x + dx * t, trailLast.y + dy * t });
    }
    if (steps > 0) trailLast = mouse;
}

void CodeTrailDraw(void)
{
    if (!trailOn) return;
    for (int i = 0; i < trailCount; i++) {
        float life = 1 - trail[i].age / TRAIL_LIFE;
        char glyph[2] = { trail[i].glyph, '\0' };
        Vector2 at = { trail[i].at.x - TRAIL_SIZE * 0.3f, trail[i].at.y - TRAIL_SIZE * 0.5f };
        DrawTextEx(gFonts.mono, glyph, at, TRAIL_SIZE, 0, Fade(COL_ACCENT, 0.5f * life));
    }
}

void CodeTrailDemo(Vector2 from, Vector2 to)
{
    trailCount = 0;
    trailStarted = false;
    CodeTrailUpdate(from, 0, true);
    for (int i = 1; i <= 12; i++) {  // the mouse moving along the line, a frame at a time
        Vector2 at = { from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12 };
        CodeTrailUpdate(at, 1 / 30.0f, true);
    }
    trailFrozen = true;
}

// ---------------------------------------------------------------------------
// The rain

void CodeRainDraw(float intensity)
{
    Color base = Mix(COL_ACCENT, COL_BAD, powf(intensity, 1.5f));
    float strength = 0.10f + 0.40f * intensity;
    int flicker = (int)flickerClock;
    for (int i = 0; i < streamCount; i++) {
        const Stream *s = &streams[i];
        for (int k = 0; k < s->length; k++) {
            float y = s->head - k * GLYPH_SIZE;
            if (y < rainArea.y - GLYPH_SIZE || y > rainArea.y + rainArea.height) continue;
            unsigned pick = s->glyphs + (unsigned)k * 2654435761u + (unsigned)(k % 3 == 0 ? flicker : 0) * 40503u;
            char glyph[2] = { glyphSet[pick % (sizeof glyphSet - 1)], '\0' };
            float fade = 1.0f - (float)k / s->length;
            // The leading character glows like the text colour; the trail fades out behind it.
            Color color = k == 0 ? Mix(base, COL_TEXT, 0.6f) : base;
            DrawTextEx(gFonts.mono, glyph, (Vector2){ s->x, y }, GLYPH_SIZE, 0, Fade(color, strength * fade));
        }
    }
}
