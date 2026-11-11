// The app's line icons. See icons.h.
#include "icons.h"
#include "icon_pen.h"

#include <math.h>

void IconDraw(Icon icon, Vector2 c, float size, Color col)
{
    PenBegin(c, size, col);

    switch (icon) {
    case ICON_BACK: {
        float p[] = { 0.12f, -0.3f, -0.18f, 0.0f, 0.12f, 0.3f };
        PenPoly(p, 3);
    } break;
    case ICON_NEXT: {
        PenLine(-0.32f, 0, 0.3f, 0);
        float p[] = { 0.06f, -0.24f, 0.3f, 0.0f, 0.06f, 0.24f };
        PenPoly(p, 3);
    } break;
    case ICON_CHECK: {
        float p[] = { -0.3f, 0.02f, -0.08f, 0.24f, 0.32f, -0.22f };
        PenPoly(p, 3);
    } break;
    case ICON_CROSS:
        PenLine(-0.24f, -0.24f, 0.24f, 0.24f);
        PenLine(-0.24f, 0.24f, 0.24f, -0.24f);
        break;
    case ICON_GEAR:
        // Teeth start outside the hole so the ring's centre stays open.
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4;
            DrawLineEx(PenAt(cosf(a) * 0.26f, sinf(a) * 0.26f), PenAt(cosf(a) * 0.43f, sinf(a) * 0.43f), size * 0.15f, col);
        }
        DrawRing(c, size * 0.12f, size * 0.31f, 0, 360, 36, col);
        break;
    case ICON_LOGOUT: {
        float door[] = { 0.06f, -0.36f, -0.3f, -0.36f, -0.3f, 0.36f, 0.06f, 0.36f };
        PenPoly(door, 4);
        PenLine(-0.06f, 0, 0.4f, 0);
        float head[] = { 0.22f, -0.17f, 0.4f, 0.0f, 0.22f, 0.17f };
        PenPoly(head, 3);
    } break;
    case ICON_POWER:
        PenRing(0, 0.04f, 0.34f, -45, 225);
        PenLine(0, -0.42f, 0, -0.04f);
        break;
    case ICON_USER:
        PenDot(0, -0.17f, 0.17f);
        DrawCircleSector(PenAt(0, 0.42f), 0.34f * size, 180, 360, 32, col);
        break;
    case ICON_KEY:
        PenRing(-0.2f, 0, 0.16f, 0, 360);
        PenLine(-0.04f, 0, 0.4f, 0);
        PenLine(0.26f, 0, 0.26f, 0.15f);
        PenLine(0.38f, 0, 0.38f, 0.18f);
        break;
    case ICON_LOCK:
        PenRing(0, -0.06f, 0.19f, 180, 360);
        PenLine(-0.19f, -0.06f, -0.19f, 0.02f);
        PenLine(0.19f, -0.06f, 0.19f, 0.02f);
        PenFillBox(-0.31f, 0.0f, 0.62f, 0.42f, 0.08f);
        break;
    case ICON_THEME:
        PenRing(0, 0, 0.36f, 0, 360);
        DrawCircleSector(c, 0.36f * size, 90, 270, 32, col);
        break;
    case ICON_TRASH: {
        PenLine(-0.34f, -0.24f, 0.34f, -0.24f);
        float handle[] = { -0.1f, -0.24f, -0.1f, -0.36f, 0.1f, -0.36f, 0.1f, -0.24f };
        PenPoly(handle, 4);
        float bin[] = { -0.25f, -0.14f, -0.2f, 0.38f, 0.2f, 0.38f, 0.25f, -0.14f };
        PenPoly(bin, 4);
        PenLine(-0.07f, -0.04f, -0.06f, 0.26f);
        PenLine(0.07f, -0.04f, 0.06f, 0.26f);
    } break;
    case ICON_INFO:
        PenRing(0, 0, 0.38f, 0, 360);
        PenDot(0, -0.18f, 0.065f);
        PenLine(0, -0.02f, 0, 0.2f);
        break;
    case ICON_BOOK: {
        float left[] = { 0, -0.26f, -0.4f, -0.34f, -0.4f, 0.3f, 0, 0.38f };
        float right[] = { 0, -0.26f, 0.4f, -0.34f, 0.4f, 0.3f, 0, 0.38f };
        PenPoly(left, 4);
        PenPoly(right, 4);
        PenLine(0, -0.26f, 0, 0.38f);
    } break;
    case ICON_RETRY: {
        PenRing(0, 0, 0.32f, -60, 220);
        // Arrow head at the -60 degree end of the arc
        float ex = 0.32f * cosf(-60 * DEG2RAD), ey = 0.32f * sinf(-60 * DEG2RAD);
        PenTri(ex + 0.16f, ey - 0.05f, ex - 0.06f, ey - 0.15f, ex - 0.02f, ey + 0.12f);
    } break;
    case ICON_PLAY:
        PenTri(-0.22f, -0.32f, -0.22f, 0.32f, 0.34f, 0.0f);
        break;
    case ICON_TROPHY:
        DrawCircleSector(PenAt(0, -0.12f), 0.26f * size, 0, 180, 32, col);
        PenFillBox(-0.26f, -0.38f, 0.52f, 0.27f, 0.02f);
        PenRing(-0.26f, -0.2f, 0.11f, 90, 270);
        PenRing(0.26f, -0.2f, 0.11f, -90, 90);
        PenFillBox(-0.05f, 0.1f, 0.1f, 0.18f, 0.0f);
        PenFillBox(-0.22f, 0.27f, 0.44f, 0.1f, 0.03f);
        break;
    case ICON_COPY:
        PenBox(-0.32f, -0.2f, 0.44f, 0.52f, 0.07f);
        PenBox(-0.12f, -0.38f, 0.44f, 0.52f, 0.07f);
        break;
    case ICON_TARGET:
        PenRing(0, 0, 0.38f, 0, 360);
        PenRing(0, 0, 0.2f, 0, 360);
        PenDot(0, 0, 0.07f);
        break;
    case ICON_CODE: {
        float l[] = { -0.18f, -0.26f, -0.4f, 0.0f, -0.18f, 0.26f };
        float r[] = { 0.18f, -0.26f, 0.4f, 0.0f, 0.18f, 0.26f };
        PenPoly(l, 3);
        PenPoly(r, 3);
        PenLine(0.07f, -0.32f, -0.07f, 0.32f);
    } break;
    case ICON_MAIL: {
        PenBox(-0.38f, -0.26f, 0.76f, 0.52f, 0.06f);
        float flap[] = { -0.33f, -0.2f, 0.0f, 0.06f, 0.33f, -0.2f };
        PenPoly(flap, 3);
    } break;
    case ICON_BULB:
        // The glass, its neck and the screw base.
        PenRing(0, -0.1f, 0.26f, 135, 405);
        PenLine(-0.18f, 0.08f, -0.11f, 0.2f);
        PenLine(0.18f, 0.08f, 0.11f, 0.2f);
        PenLine(-0.11f, 0.22f, 0.11f, 0.22f);
        PenLine(-0.07f, 0.34f, 0.07f, 0.34f);
        break;
    case ICON_PLUS:
        PenLine(-0.3f, 0, 0.3f, 0);
        PenLine(0, -0.3f, 0, 0.3f);
        break;
    case ICON_STAR: {
        // Five points, filled as a fan of triangles from the centre.
        Vector2 points[10];
        for (int i = 0; i < 10; i++) {
            float angle = -PI / 2 + i * PI / 5, r = i % 2 ? 0.2f : 0.46f;
            points[i] = (Vector2){ cosf(angle) * r, 0.04f + sinf(angle) * r };
        }
        for (int i = 0; i < 10; i++) {
            Vector2 a = points[i], b = points[(i + 1) % 10];
            PenTri(0, 0.04f, a.x, a.y, b.x, b.y);
        }
    } break;
    case ICON_BOLT: {
        float bolt[] = { 0.1f, -0.42f, -0.2f, 0.06f, 0.04f, 0.06f, -0.08f, 0.42f, 0.22f, -0.08f, -0.02f, -0.08f,
                         0.1f, -0.42f };
        PenPoly(bolt, 7);
    } break;
    case ICON_COUNT:
        break;
    }
}
