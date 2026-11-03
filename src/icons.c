#include "icons.h"

#include <math.h>

// Icons are designed on a unit box from -0.5 to 0.5, scaled by size around the centre.
static Vector2 ctr;
static float sz;
static float th;  // stroke thickness
static Color ink;

static Vector2 P(float x, float y) { return (Vector2){ ctr.x + x * sz, ctr.y + y * sz }; }

// Line with round caps.
static void Line(float x1, float y1, float x2, float y2)
{
    Vector2 a = P(x1, y1), b = P(x2, y2);
    DrawLineEx(a, b, th, ink);
    DrawCircleV(a, th / 2, ink);
    DrawCircleV(b, th / 2, ink);
}

static void Poly(const float *xy, int points)
{
    for (int i = 0; i + 1 < points; i++) Line(xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3]);
}

static void Ring(float x, float y, float r, float from, float to)
{
    DrawRing(P(x, y), r * sz - th / 2, r * sz + th / 2, from, to, 36, ink);
}

static void Dot(float x, float y, float r) { DrawCircleV(P(x, y), r * sz, ink); }

// Filled triangle in either winding (raylib culls one of them).
static void Tri(float x1, float y1, float x2, float y2, float x3, float y3)
{
    DrawTriangle(P(x1, y1), P(x2, y2), P(x3, y3), ink);
    DrawTriangle(P(x1, y1), P(x3, y3), P(x2, y2), ink);
}

static void Box(float x, float y, float w, float h, float radius)
{
    Rectangle r = { ctr.x + x * sz, ctr.y + y * sz, w * sz, h * sz };
    float shortest = r.width < r.height ? r.width : r.height;
    DrawRectangleRoundedLinesEx(r, shortest > 0 ? (2 * radius * sz) / shortest : 0, 12, th, ink);
}

static void FillBox(float x, float y, float w, float h, float radius)
{
    Rectangle r = { ctr.x + x * sz, ctr.y + y * sz, w * sz, h * sz };
    float shortest = r.width < r.height ? r.width : r.height;
    DrawRectangleRounded(r, shortest > 0 ? (2 * radius * sz) / shortest : 0, 12, ink);
}

void IconDraw(Icon icon, Vector2 c, float size, Color col)
{
    ctr = c;
    sz = size;
    th = size * 0.1f;
    if (th < 1.6f) th = 1.6f;
    ink = col;

    switch (icon) {
    case ICON_BACK: {
        float p[] = { 0.12f, -0.3f, -0.18f, 0.0f, 0.12f, 0.3f };
        Poly(p, 3);
    } break;
    case ICON_NEXT: {
        Line(-0.32f, 0, 0.3f, 0);
        float p[] = { 0.06f, -0.24f, 0.3f, 0.0f, 0.06f, 0.24f };
        Poly(p, 3);
    } break;
    case ICON_CHECK: {
        float p[] = { -0.3f, 0.02f, -0.08f, 0.24f, 0.32f, -0.22f };
        Poly(p, 3);
    } break;
    case ICON_CROSS:
        Line(-0.24f, -0.24f, 0.24f, 0.24f);
        Line(-0.24f, 0.24f, 0.24f, -0.24f);
        break;
    case ICON_GEAR:
        // Teeth start outside the hole so the ring's centre stays open.
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4;
            DrawLineEx(P(cosf(a) * 0.26f, sinf(a) * 0.26f), P(cosf(a) * 0.43f, sinf(a) * 0.43f), size * 0.15f, ink);
        }
        DrawRing(c, size * 0.12f, size * 0.31f, 0, 360, 36, ink);
        break;
    case ICON_LOGOUT: {
        float door[] = { 0.06f, -0.36f, -0.3f, -0.36f, -0.3f, 0.36f, 0.06f, 0.36f };
        Poly(door, 4);
        Line(-0.06f, 0, 0.4f, 0);
        float head[] = { 0.22f, -0.17f, 0.4f, 0.0f, 0.22f, 0.17f };
        Poly(head, 3);
    } break;
    case ICON_POWER:
        Ring(0, 0.04f, 0.34f, -45, 225);
        Line(0, -0.42f, 0, -0.04f);
        break;
    case ICON_USER:
        Dot(0, -0.17f, 0.17f);
        DrawCircleSector(P(0, 0.42f), 0.34f * sz, 180, 360, 32, ink);
        break;
    case ICON_KEY:
        Ring(-0.2f, 0, 0.16f, 0, 360);
        Line(-0.04f, 0, 0.4f, 0);
        Line(0.26f, 0, 0.26f, 0.15f);
        Line(0.38f, 0, 0.38f, 0.18f);
        break;
    case ICON_LOCK:
        Ring(0, -0.06f, 0.19f, 180, 360);
        Line(-0.19f, -0.06f, -0.19f, 0.02f);
        Line(0.19f, -0.06f, 0.19f, 0.02f);
        FillBox(-0.31f, 0.0f, 0.62f, 0.42f, 0.08f);
        break;
    case ICON_THEME:
        Ring(0, 0, 0.36f, 0, 360);
        DrawCircleSector(c, 0.36f * sz, 90, 270, 32, ink);
        break;
    case ICON_TRASH: {
        Line(-0.34f, -0.24f, 0.34f, -0.24f);
        float handle[] = { -0.1f, -0.24f, -0.1f, -0.36f, 0.1f, -0.36f, 0.1f, -0.24f };
        Poly(handle, 4);
        float bin[] = { -0.25f, -0.14f, -0.2f, 0.38f, 0.2f, 0.38f, 0.25f, -0.14f };
        Poly(bin, 4);
        Line(-0.07f, -0.04f, -0.06f, 0.26f);
        Line(0.07f, -0.04f, 0.06f, 0.26f);
    } break;
    case ICON_INFO:
        Ring(0, 0, 0.38f, 0, 360);
        Dot(0, -0.18f, 0.065f);
        Line(0, -0.02f, 0, 0.2f);
        break;
    case ICON_BOOK: {
        float left[] = { 0, -0.26f, -0.4f, -0.34f, -0.4f, 0.3f, 0, 0.38f };
        float right[] = { 0, -0.26f, 0.4f, -0.34f, 0.4f, 0.3f, 0, 0.38f };
        Poly(left, 4);
        Poly(right, 4);
        Line(0, -0.26f, 0, 0.38f);
    } break;
    case ICON_RETRY: {
        Ring(0, 0, 0.32f, -60, 220);
        // Arrow head at the -60 degree end of the arc
        float ex = 0.32f * cosf(-60 * DEG2RAD), ey = 0.32f * sinf(-60 * DEG2RAD);
        Tri(ex + 0.16f, ey - 0.05f, ex - 0.06f, ey - 0.15f, ex - 0.02f, ey + 0.12f);
    } break;
    case ICON_PLAY:
        Tri(-0.22f, -0.32f, -0.22f, 0.32f, 0.34f, 0.0f);
        break;
    case ICON_TROPHY:
        DrawCircleSector(P(0, -0.12f), 0.26f * sz, 0, 180, 32, ink);
        FillBox(-0.26f, -0.38f, 0.52f, 0.27f, 0.02f);
        Ring(-0.26f, -0.2f, 0.11f, 90, 270);
        Ring(0.26f, -0.2f, 0.11f, -90, 90);
        FillBox(-0.05f, 0.1f, 0.1f, 0.18f, 0.0f);
        FillBox(-0.22f, 0.27f, 0.44f, 0.1f, 0.03f);
        break;
    case ICON_COPY:
        Box(-0.32f, -0.2f, 0.44f, 0.52f, 0.07f);
        Box(-0.12f, -0.38f, 0.44f, 0.52f, 0.07f);
        break;
    case ICON_TARGET:
        Ring(0, 0, 0.38f, 0, 360);
        Ring(0, 0, 0.2f, 0, 360);
        Dot(0, 0, 0.07f);
        break;
    case ICON_CODE: {
        float l[] = { -0.18f, -0.26f, -0.4f, 0.0f, -0.18f, 0.26f };
        float r[] = { 0.18f, -0.26f, 0.4f, 0.0f, 0.18f, 0.26f };
        Poly(l, 3);
        Poly(r, 3);
        Line(0.07f, -0.32f, -0.07f, 0.32f);
    } break;
    case ICON_COUNT:
        break;
    }
}
