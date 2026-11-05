// The Increment logo, drawn from shapes. See brand.h.
#include "brand.h"

static void RoundRect(Rectangle r, float radius, Color c)
{
    float shortest = r.width < r.height ? r.width : r.height;
    if (shortest <= 0) return;
    float roundness = (2.0f * radius) / shortest;
    DrawRectangleRounded(r, roundness > 1 ? 1 : roundness, 16, c);
}

// The fox is designed on a unit square (0..1 across and down the tile). Point maps a
// design point into the tile at origin/size, scaled by `scale` about the head's centre
// so it can pop in on the splash without drifting.
typedef struct {
    Vector2 origin;
    float size;
    float scale;
} Canvas;

static Vector2 Point(Canvas c, float x, float y)
{
    const float centreX = 0.5f, centreY = 0.5f;
    return (Vector2){ c.origin.x + c.size * (centreX + (x - centreX) * c.scale),
                      c.origin.y + c.size * (centreY + (y - centreY) * c.scale) };
}

// raylib only fills triangles wound counter-clockwise on screen; this accepts either order.
static void Triangle(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0) DrawTriangle(a, b, c, col);
    else DrawTriangle(a, c, b, col);
}

// The head: two pointed ears and a face narrowing to the chin. Its outline, left to right:
// ear tip, forehead, ear tip, cheek, chin, cheek.
static void FoxHead(Canvas c, Color ink, Color innerEar)
{
    Vector2 leftEar = Point(c, 0.20f, 0.15f), rightEar = Point(c, 0.80f, 0.15f);
    Vector2 leftBrow = Point(c, 0.39f, 0.34f), rightBrow = Point(c, 0.61f, 0.34f);
    Vector2 leftCheek = Point(c, 0.15f, 0.52f), rightCheek = Point(c, 0.85f, 0.52f);
    Vector2 chin = Point(c, 0.50f, 0.85f);

    // The face is convex, so it's a fan of triangles from the chin.
    Triangle(chin, leftCheek, leftBrow, ink);
    Triangle(chin, leftBrow, rightBrow, ink);
    Triangle(chin, rightBrow, rightCheek, ink);
    Triangle(leftEar, leftBrow, leftCheek, ink);
    Triangle(rightEar, rightBrow, rightCheek, ink);

    // Inner ears: a softer tone set into each ear.
    Triangle(Point(c, 0.245f, 0.250f), Point(c, 0.345f, 0.355f), Point(c, 0.225f, 0.420f), innerEar);
    Triangle(Point(c, 0.755f, 0.250f), Point(c, 0.655f, 0.355f), Point(c, 0.775f, 0.420f), innerEar);
}

// Eyes and nose, cut out of the head in the tile colour.
static void FoxFace(Canvas c, Color tile)
{
    float eye = c.size * c.scale * 0.048f, nose = c.size * c.scale * 0.050f;
    DrawCircleV(Point(c, 0.37f, 0.53f), eye, tile);
    DrawCircleV(Point(c, 0.63f, 0.53f), eye, tile);
    DrawCircleV(Point(c, 0.50f, 0.715f), nose, tile);
}

void BrandDrawLogo(Vector2 p, float size, Color tile, Color ink, LogoAnim a, bool shadow)
{
    // Tile, scaled about its centre.
    float ts = size * a.tile;
    Vector2 centre = { p.x + size / 2, p.y + size / 2 };
    Rectangle t = { centre.x - ts / 2, centre.y - ts / 2, ts, ts };
    float radius = ts * 0.24f;

    if (shadow && ts > 0) {
        for (int i = 3; i >= 1; i--) {
            float grow = size * 0.012f * i;
            Rectangle s = { t.x - grow, t.y - grow + size * 0.035f, t.width + grow * 2, t.height + grow * 2 };
            RoundRect(s, radius + grow, Fade(BLACK, 0.06f));
        }
    }
    RoundRect(t, radius, tile);

    // The fox is placed relative to the full-size tile so it doesn't drift while the tile scales.
    if (a.head <= 0) return;
    Canvas fox = { p, size, a.head };
    FoxHead(fox, ink, ColorLerp(tile, ink, 0.55f));
    // The eyes and nose fade in from the head's colour (a.face 0) to the tile's (1).
    float face = a.face < 0 ? 0 : (a.face > 1 ? 1 : a.face);
    if (face > 0) FoxFace(fox, ColorLerp(ink, tile, face));
}
