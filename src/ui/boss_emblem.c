// The exam ("boss" level) look: the halo of aura, turning spikes and orbiting sparks, and the
// spiked iris its pages open and close through. See boss_emblem.h.
#include "ui/boss_emblem.h"

#include <math.h>

#define SPIKES 12
#define SPARKS 3

void BossEmblemDraw(Vector2 center, float radius, Color color, float hover, float time)
{
    // The aura breathes in and out, brighter while hovered.
    float breath = (sinf(time * 2.0f) + 1) / 2;
    for (int ring = 3; ring >= 1; ring--) {
        float spread = radius + 10 + ring * (9 + 4 * breath);
        DrawCircleV(center, spread, Fade(color, (0.05f + 0.04f * hover) * (4 - ring) / 3.0f));
    }

    // A crown of spikes turning round the disc: longer and quicker while hovered.
    float turn = time * (0.25f + 0.9f * hover);
    float inner = radius + 8, outer = radius + 18 + 7 * hover + 2 * breath;
    for (int i = 0; i < SPIKES; i++) {
        float angle = turn + i * 2 * PI / SPIKES, half = PI / SPIKES * 0.45f;
        Vector2 tip = { center.x + cosf(angle) * outer, center.y + sinf(angle) * outer };
        Vector2 left = { center.x + cosf(angle - half) * inner, center.y + sinf(angle - half) * inner };
        Vector2 right = { center.x + cosf(angle + half) * inner, center.y + sinf(angle + half) * inner };
        // raylib only fills a triangle wound anticlockwise; give both windings, one is drawn.
        DrawTriangle(left, tip, right, Fade(color, 0.55f + 0.35f * hover));
        DrawTriangle(right, tip, left, Fade(color, 0.55f + 0.35f * hover));
    }

    // Sparks in orbit, each at its own speed and distance.
    for (int i = 0; i < SPARKS; i++) {
        float speed = 0.9f + i * 0.45f, distance = radius + 28 + i * 7;
        float angle = -time * speed + i * 2.1f;
        Vector2 spark = { center.x + cosf(angle) * distance, center.y + sinf(angle) * distance * 0.92f };
        DrawCircleV(spark, 5 + 2 * hover, Fade(color, 0.18f));
        DrawCircleV(spark, 2.4f + hover, Fade(color, 0.9f));
    }
}

void BossIrisDraw(float open, Color color, float time)
{
    float width = (float)GetScreenWidth(), height = (float)GetScreenHeight();
    Vector2 centre = { width / 2, height / 2 };
    float reach = sqrtf(centre.x * centre.x + centre.y * centre.y) * 1.08f;  // to the corners
    float hole = reach * open;
    DrawRing(centre, hole, reach * 1.3f, 0, 360, 96, color);
    if (hole <= 0) return;
    // Spikes round the hole's edge, pointing in: shorter as it opens wide.
    int spikes = 28;
    float length = 26 * (1 - open * 0.6f), turn = time * 0.8f;
    for (int i = 0; i < spikes; i++) {
        float angle = turn + i * 2 * PI / spikes, half = PI / spikes * 0.55f;
        Vector2 tip = { centre.x + cosf(angle) * (hole - length), centre.y + sinf(angle) * (hole - length) };
        Vector2 left = { centre.x + cosf(angle - half) * (hole + 1), centre.y + sinf(angle - half) * (hole + 1) };
        Vector2 right = { centre.x + cosf(angle + half) * (hole + 1), centre.y + sinf(angle + half) * (hole + 1) };
        DrawTriangle(left, right, tip, color);  // one winding of the two is drawn (see above)
        DrawTriangle(right, left, tip, color);
    }
    DrawRing(centre, hole - length - 2, hole - length + 1, 0, 360, 96, Fade(color, 0.35f));
}
