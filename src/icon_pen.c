// Drawing helpers shared by the icon sets. See icon_pen.h.
#include "icon_pen.h"

static Vector2 centre;
static float scale;      // pixels per unit
static float thickness;  // stroke width in pixels
static Color ink;

void PenBegin(Vector2 at, float size, Color colour)
{
    centre = at;
    scale = size;
    thickness = size * 0.1f;
    if (thickness < 1.6f) thickness = 1.6f;
    ink = colour;
}

Vector2 PenAt(float x, float y) { return (Vector2){ centre.x + x * scale, centre.y + y * scale }; }

float PenThickness(void) { return thickness; }

void PenLine(float x1, float y1, float x2, float y2)
{
    Vector2 a = PenAt(x1, y1), b = PenAt(x2, y2);
    DrawLineEx(a, b, thickness, ink);
    DrawCircleV(a, thickness / 2, ink);
    DrawCircleV(b, thickness / 2, ink);
}

void PenPoly(const float *xy, int points)
{
    for (int i = 0; i + 1 < points; i++) PenLine(xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3]);
}

void PenRing(float x, float y, float r, float from, float to)
{
    DrawRing(PenAt(x, y), r * scale - thickness / 2, r * scale + thickness / 2, from, to, 36, ink);
}

void PenDot(float x, float y, float r) { DrawCircleV(PenAt(x, y), r * scale, ink); }

void PenTri(float x1, float y1, float x2, float y2, float x3, float y3)
{
    // raylib culls one winding, so draw both.
    DrawTriangle(PenAt(x1, y1), PenAt(x2, y2), PenAt(x3, y3), ink);
    DrawTriangle(PenAt(x1, y1), PenAt(x3, y3), PenAt(x2, y2), ink);
}

void PenBox(float x, float y, float w, float h, float radius)
{
    Rectangle r = { centre.x + x * scale, centre.y + y * scale, w * scale, h * scale };
    float shortest = r.width < r.height ? r.width : r.height;
    DrawRectangleRoundedLinesEx(r, shortest > 0 ? (2 * radius * scale) / shortest : 0, 12, thickness, ink);
}

void PenFillBox(float x, float y, float w, float h, float radius)
{
    Rectangle r = { centre.x + x * scale, centre.y + y * scale, w * scale, h * scale };
    float shortest = r.width < r.height ? r.width : r.height;
    DrawRectangleRounded(r, shortest > 0 ? (2 * radius * scale) / shortest : 0, 12, ink);
}

void PenSector(float x, float y, float r, float from, float to)
{
    DrawCircleSector(PenAt(x, y), r * scale, from, to, 32, ink);
}
