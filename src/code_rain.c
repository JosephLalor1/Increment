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
