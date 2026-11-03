#ifndef BRAND_H
#define BRAND_H

#include "raylib.h"

// The Increment logo: a rounded tile holding two plus signs, the second larger and
// stepping up and to the right, i.e. "++", improving one step at a time.
// Drawn from shapes so it is sharp at any size and can take the user's accent colour.

#define BRAND_BLUE (Color){ 59, 110, 246, 255 }  // default tile colour (app icon)

// Animation state for the splash screen; all 1 for a static logo.
typedef struct {
    float tile;   // tile scale, 0..1 (may overshoot)
    float plus1;  // first plus scale
    float plus2;  // second plus scale
} LogoAnim;

#define LOGO_STATIC (LogoAnim){ 1, 1, 1 }

// Draws the logo in the square at topLeft with the given side length.
// shadow adds a soft drop shadow, for use on a background (not for icon files).
void BrandDrawLogo(Vector2 topLeft, float size, Color tile, Color ink, LogoAnim anim, bool shadow);

#endif
