// The Increment logo, drawn from shapes. See brand.h.
#include "brand.h"

static void RoundRect(Rectangle r, float radius, Color c)
{
    float shortest = r.width < r.height ? r.width : r.height;
    if (shortest <= 0) return;
    float roundness = (2.0f * radius) / shortest;
    DrawRectangleRounded(r, roundness > 1 ? 1 : roundness, 16, c);
}

// A plus sign made of two pill-shaped bars. length is the full width of each bar.
static void Plus(Vector2 c, float length, float thick, Color col)
{
    if (length <= 0 || thick <= 0) return;
    RoundRect((Rectangle){ c.x - length / 2, c.y - thick / 2, length, thick }, thick / 2, col);
    RoundRect((Rectangle){ c.x - thick / 2, c.y - length / 2, thick, length }, thick / 2, col);
}

void BrandDrawLogo(Vector2 p, float size, Color tile, Color ink, LogoAnim a, bool shadow)
{
    // Tile, scaled about its centre.
    float ts = size * a.tile;
    Vector2 c = { p.x + size / 2, p.y + size / 2 };
    Rectangle t = { c.x - ts / 2, c.y - ts / 2, ts, ts };
    float radius = ts * 0.24f;

    if (shadow && ts > 0) {
        for (int i = 3; i >= 1; i--) {
            float grow = size * 0.012f * i;
            Rectangle s = { t.x - grow, t.y - grow + size * 0.035f, t.width + grow * 2, t.height + grow * 2 };
            RoundRect(s, radius + grow, Fade(BLACK, 0.06f));
        }
    }
    RoundRect(t, radius, tile);

    // The plus signs are placed relative to the full-size tile so they don't drift while it scales.
    Color faded = ColorLerp(tile, ink, 0.72f);  // first step is softer than the second
    Plus((Vector2){ p.x + size * 0.350f, p.y + size * 0.630f }, size * 0.30f * a.plus1, size * 0.095f * a.plus1, faded);
    Plus((Vector2){ p.x + size * 0.645f, p.y + size * 0.370f }, size * 0.40f * a.plus2, size * 0.120f * a.plus2, ink);
}
