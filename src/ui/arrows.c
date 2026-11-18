// Pointer arrows and null marks. See arrows.h.
#include "ui/arrows.h"

#include <math.h>

#define CURVE_STEPS 28

Vector2 ArrowCurvePoint(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t)
{
    float u = 1 - t;
    return (Vector2){ u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                      u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y };
}

void ArrowCurveDraw(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color, bool solid, float progress, double now)
{
    float reach = progress * progress * (3 - 2 * progress);  // eases in and out
    Vector2 previous = a, tip = a, beforeTip = a;
    for (int i = 1; i <= CURVE_STEPS; i++) {
        float t = (float)i / CURVE_STEPS;
        if (t > reach) break;
        Vector2 point = ArrowCurvePoint(a, b, c, d, t);
        if (solid || i % 2) DrawLineEx(previous, point, solid ? 2.5f : 2, Fade(color, solid ? 0.95f : 0.6f));
        beforeTip = previous;
        previous = tip = point;
    }
    float dx = tip.x - beforeTip.x, dy = tip.y - beforeTip.y, length = sqrtf(dx * dx + dy * dy);
    if (length > 0.01f) {
        dx /= length;
        dy /= length;
        Vector2 left = { tip.x - dx * 10 - dy * 5, tip.y - dy * 10 + dx * 5 };
        Vector2 right = { tip.x - dx * 10 + dy * 5, tip.y - dy * 10 - dx * 5 };
        DrawTriangle(tip, right, left, Fade(color, solid ? 1 : 0.7f));
        DrawTriangle(tip, left, right, Fade(color, solid ? 1 : 0.7f));  // either winding
    }
    if (solid && progress >= 1) {  // light running from the pointer to what it points at
        for (int k = 0; k < 2; k++) {
            float t = fmodf((float)now * 0.6f + k * 0.5f, 1.0f);
            DrawCircleV(ArrowCurvePoint(a, b, c, d, t), 3, Fade(ColorLerp(color, WHITE, 0.6f), 0.9f * sinf(t * PI)));
        }
    }
}

void ArrowGroundDraw(Vector2 from, Color color, float progress)
{
    Vector2 end = { from.x + 26 * progress, from.y };
    DrawLineEx(from, end, 2.5f, color);
    if (progress < 1) return;
    for (int i = 0; i < 3; i++) {
        float half = 9 - i * 3.0f;
        DrawLineEx((Vector2){ end.x + i * 5, end.y - half }, (Vector2){ end.x + i * 5, end.y + half }, 2.5f, color);
    }
}
