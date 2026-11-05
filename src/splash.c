// The launch animation. See splash.h.
#include "splash.h"
#include "brand.h"
#include "ui.h"

#include <math.h>

static float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

// Progress (0..1) of a phase that starts at `start` and lasts `duration` seconds.
static float Phase(float t, float start, float duration) { return Clamp01((t - start) / duration); }

static float EaseOutCubic(float x) { return 1 - powf(1 - x, 3); }

// Overshoots slightly past 1 then settles: gives the "pop".
static float EaseOutBack(float x)
{
    if (x <= 0) return 0;
    const float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * powf(x - 1, 3) + c1 * powf(x - 1, 2);
}

bool SplashScreen(float t)
{
    // Any key or click skips. The splash is short, but nobody likes waiting twice.
    if (t > 0.15f && (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) return true;

    float w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
    float cx = w / 2;
    float size = h * 0.26f;
    if (size > 210) size = 210;

    float out = 1 - EaseOutCubic(Phase(t, SPLASH_DURATION - 0.45f, 0.45f));  // final fade to the login screen

    // Tile springs in, the fox head pops up on it, then the fox opens its eyes.
    LogoAnim anim = {
        EaseOutBack(Phase(t, 0.00f, 0.55f)),
        EaseOutBack(Phase(t, 0.35f, 0.40f)),
        EaseOutCubic(Phase(t, 0.70f, 0.25f)),
    };
    float tileAlpha = Phase(t, 0, 0.2f) * out;
    float top = h / 2 - size * 0.95f;
    BrandDrawLogo((Vector2){ cx - size / 2, top }, size, Fade(COL_BRAND, tileAlpha), Fade(WHITE, out), anim, out > 0.99f);

    // Wordmark and tagline slide up into place.
    float word = EaseOutCubic(Phase(t, 0.85f, 0.45f));
    float wordY = top + size + 34 + (1 - word) * 16;
    UiTextCentered(gFonts.bold, "Increment", cx, wordY, 72, Fade(COL_TEXT, word * out));

    float tag = EaseOutCubic(Phase(t, 1.15f, 0.45f));
    UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, wordY + 92 + (1 - tag) * 10, 24,
                   Fade(COL_DIM, tag * out));

    return t >= SPLASH_DURATION;
}
