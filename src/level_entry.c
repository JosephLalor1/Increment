// The zoom into a level, the iris that opens on it, and both backwards on the way out.
// See level_entry.h.
#include "level_entry.h"
#include "ui.h"

#include "rlgl.h"

#include <math.h>

#define ZOOM_SECONDS   0.45f  // the zoom, in or out
#define REVEAL_SECONDS 0.40f  // the iris opening on the level
#define CLOSE_SECONDS  0.30f  // the iris closing on it
#define ZOOM_DEPTH     6.0f   // how many times larger the picture is when it is covered
#define STREAK_COUNT   36     // speed lines rushing past

typedef enum {
    ENTRY_IDLE,
    ENTRY_ZOOMING_IN,   // in: flying into the journey's picture
    ENTRY_OPENING,      // in: the iris opening over the live level
    ENTRY_CLOSING,      // out: the iris closing over the level's picture
    ENTRY_ZOOMING_OUT,  // out: flying back out of the journey's picture
} EntryPhase;

static struct {
    EntryPhase phase;
    float time;         // seconds since the animation (in or out) started
    float demoTime;     // --shot: a fixed moment to show (see LevelEntryDemo)
    bool demo;
    Vector2 point;      // the point zoomed into, or out of
    Texture2D picture;  // the screen being zoomed, or closed on
} entry;

static float Now(void) { return entry.demo ? entry.demoTime : entry.time; }

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// Keeps what has been drawn this frame as the picture.
static void TakePicture(void)
{
    if (entry.picture.id) UnloadTexture(entry.picture);
    rlDrawRenderBatchActive();  // what was drawn this frame isn't in the framebuffer until flushed
    Image screen = LoadImageFromScreen();
    entry.picture = LoadTextureFromImage(screen);  // without one, the animation is the disc alone
    UnloadImage(screen);
    if (entry.picture.id) SetTextureFilter(entry.picture, TEXTURE_FILTER_BILINEAR);
}

void LevelEntryStart(Vector2 clickedAt)
{
    TakePicture();
    entry.point = clickedAt;
    entry.time = 0;
    entry.phase = ENTRY_ZOOMING_IN;
}

void LevelExitStart(void)
{
    TakePicture();
    entry.time = 0;
    entry.phase = ENTRY_CLOSING;
}

void LevelExitLand(Vector2 station)
{
    if (entry.phase != ENTRY_ZOOMING_OUT) return;
    TakePicture();
    entry.point = station;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), COL_ACCENT);  // still covered this frame
}

void LevelEntryUpdate(float dt)
{
    if (entry.phase != ENTRY_IDLE) entry.time += dt;
}

bool LevelEntryStandsIn(void)
{
    return entry.phase == ENTRY_ZOOMING_IN || entry.phase == ENTRY_CLOSING || entry.phase == ENTRY_ZOOMING_OUT;
}

// The distance from a point to the screen's farthest corner: a disc that large covers it all.
static float ReachFrom(Vector2 point, float width, float height)
{
    float dx = fmaxf(point.x, width - point.x), dy = fmaxf(point.y, height - point.y);
    return sqrtf(dx * dx + dy * dy);
}

// The zoom at t: 0 the picture as it is, 1 flown right into its point and covered.
static void DrawZoom(float t)
{
    float width = (float)GetScreenWidth(), height = (float)GetScreenHeight();
    // The zoom is gentle near the picture and rushes near the cover; the point glides to
    // the middle of the screen, quickest near the picture.
    float rush = t * t * t, glide = 1 - (1 - t) * (1 - t);
    float zoom = 1 + (ZOOM_DEPTH - 1) * rush;
    Vector2 at = { Lerp(entry.point.x, width / 2, glide), Lerp(entry.point.y, height / 2, glide) };
    if (entry.picture.id) {
        Rectangle whole = { 0, 0, (float)entry.picture.width, (float)entry.picture.height };
        Rectangle zoomed = { at.x - entry.point.x * zoom, at.y - entry.point.y * zoom, width * zoom, height * zoom };
        DrawTexturePro(entry.picture, whole, zoomed, (Vector2){ 0, 0 }, 0, WHITE);
    }

    // Speed lines fly outwards from the point, longer and brighter as the zoom rushes.
    float reach = ReachFrom(at, width, height);
    for (int i = 0; i < STREAK_COUNT; i++) {
        float angle = i * 2.399963f;                      // the golden angle spreads them evenly
        float spread = fmodf(i * 0.618034f, 1.0f);        // each line at its own distance
        float from = reach * (0.12f + 0.88f * spread) * (0.35f + 0.9f * t);
        float length = 24 + 320 * rush * (0.5f + spread);
        Vector2 direction = { cosf(angle), sinf(angle) };
        DrawLineEx((Vector2){ at.x + direction.x * from, at.y + direction.y * from },
                   (Vector2){ at.x + direction.x * (from + length), at.y + direction.y * (from + length) },
                   2 + 2 * rush, Fade(COL_ACCENT, 0.5f * t));
    }

    // The disc covers the screen at 1 and is gone into the point at 0, a bright rim round it.
    float radius = reach * 1.04f * rush;
    DrawCircleSector(at, radius + 5, 0, 360, 96, Fade(COL_ACCENT_HI, 0.6f));
    DrawCircleSector(at, radius, 0, 360, 96, COL_ACCENT);
}

// The iris: an accent cover with a round hole in the middle of the screen. open is 0 for
// shut and 1 for a hole as large as the screen; rim is how bright the hole's edge is.
static void DrawIris(float open, float rim)
{
    float width = (float)GetScreenWidth(), height = (float)GetScreenHeight();
    Vector2 centre = { width / 2, height / 2 };
    float reach = ReachFrom(centre, width, height) * 1.04f;
    float hole = reach * open;
    DrawRing(centre, hole, reach * 1.2f, 0, 360, 96, COL_ACCENT);
    DrawRing(centre, fmaxf(hole - 5, 0), hole, 0, 360, 96, Fade(COL_ACCENT_HI, 0.6f * rim));
}

static void DrawPicture(void)
{
    if (entry.picture.id) DrawTexture(entry.picture, 0, 0, WHITE);
}

LevelEntryCue LevelEntryDraw(void)
{
    if (entry.phase == ENTRY_ZOOMING_IN) {
        float t = Now() / ZOOM_SECONDS;
        if (t < 1) {
            DrawZoom(t);
            return ENTRY_CUE_NONE;
        }
        // Covered: a plain fill, and the level can open behind it.
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), COL_ACCENT);
        entry.phase = ENTRY_OPENING;
        return ENTRY_CUE_OPEN_LEVEL;
    }
    if (entry.phase == ENTRY_CLOSING) {
        float t = Now() / CLOSE_SECONDS;
        if (t < 1) {  // the iris shuts slowly at first, then snaps closed
            DrawPicture();
            DrawIris(1 - t * t * t, t);
            return ENTRY_CUE_NONE;
        }
        // Shut. The caller draws the journey now and hands it over with LevelExitLand, which
        // covers it; until then the zoom out aims at the middle of the screen.
        entry.point = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
        entry.phase = ENTRY_ZOOMING_OUT;
        return ENTRY_CUE_SHOW_JOURNEY;
    }
    if (entry.phase == ENTRY_ZOOMING_OUT) {  // the zoom in, backwards
        float t = 1 - (Now() - CLOSE_SECONDS) / ZOOM_SECONDS;
        if (t > 0) {
            DrawZoom(t < 1 ? t : 1);
        } else {
            DrawPicture();  // home again; the live screen takes over next frame
            entry.phase = ENTRY_IDLE;
        }
    }
    return ENTRY_CUE_NONE;
}

void LevelEntryDrawReveal(void)
{
    if (entry.phase != ENTRY_OPENING) return;
    float t = (Now() - ZOOM_SECONDS) / REVEAL_SECONDS;
    if (t >= 1) {
        entry.phase = ENTRY_IDLE;
        return;
    }
    if (t < 0) t = 0;
    DrawIris(1 - (1 - t) * (1 - t) * (1 - t), 1 - t);  // opens fast at first, and settles
}

void LevelEntryShutdown(void)
{
    if (entry.picture.id) UnloadTexture(entry.picture);
    entry.picture = (Texture2D){ 0 };
    entry.phase = ENTRY_IDLE;
}

void LevelEntryDemo(float seconds)
{
    entry.demo = true;
    entry.demoTime = seconds;
}
