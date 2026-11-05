#ifndef BRAND_H
#define BRAND_H

#include "raylib.h"

// The Increment logo: a rounded tile holding a minimalist white fox head (the app's
// mascot): pointed ears with softer inner ears, a face narrowing to the chin, and eyes
// and nose cut out in the tile colour.
// Drawn from shapes so it is sharp at any size and can take the user's accent colour.

#define BRAND_BLUE (Color){ 59, 110, 246, 255 }  // default tile colour (app icon)

// Animation state for the splash screen; all 1 for a static logo.
typedef struct {
    float tile;  // tile scale, 0..1 (may overshoot)
    float head;  // fox head scale, 0..1 (may overshoot)
    float face;  // eyes and nose, 0 (hidden) to 1 (shown)
} LogoAnim;

#define LOGO_STATIC (LogoAnim){ 1, 1, 1 }

// Draws the logo in the square at topLeft with the given side length.
// shadow adds a soft drop shadow, for use on a background (not for icon files).
void BrandDrawLogo(Vector2 topLeft, float size, Color tile, Color ink, LogoAnim anim, bool shadow);

#endif
