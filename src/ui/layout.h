#ifndef LAYOUT_H
#define LAYOUT_H

#include "raylib.h"
#include <stdbool.h>

// Page-level layout shared by all screens.
#define TOPBAR_H  64.0f
#define MAX_WIDTH 880.0f

float ContentWidth(void);   // width of the centred content column
float ContentX(void);       // left edge of the content column
Rectangle ScrollView(void); // the area below the top bar

// Wheel/keyboard scrolling clamped to contentH (measured on the previous frame).
// Keys are ignored while a text box has focus.
void UpdateScroll(float *scroll, float contentH, float viewH);
// The scroll bar for a page scrolled with UpdateScroll, along the right edge of view (drag
// it, or click its track to jump). Call after EndView, so it's drawn over the page.
bool PageScrollBar(Rectangle view, float *scroll, float contentH);  // true while hovered or dragged
void BeginView(Rectangle v); // clip drawing and hit-testing to v
void EndView(void);

// Draws the top bar; returns true when the user asks to go back (button, Esc, or
// Backspace). While a text box has focus, Esc only releases focus and Backspace is ignored.
bool TopBar(const char *title);

void ProgressBar(Rectangle r, float t, Color track, Color fill);

// The logo tile with the "Increment" wordmark beside it, centred on cx. Height is logoSize.
void DrawLogoLockup(float cx, float top, float logoSize);

#endif
