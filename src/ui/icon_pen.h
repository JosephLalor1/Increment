#ifndef ICON_PEN_H
#define ICON_PEN_H

#include "raylib.h"

// The drawing helpers shared by the icon sets (icons.c for the app's line icons, skill_icons.c
// for the skill logos). An icon is designed on a unit box from -0.5 to 0.5; PenBegin sets
// where that box is drawn, how big, and in what colour, and the helpers below take
// coordinates in that unit box.

// Starts an icon centred on `centre`, fitting a size x size box, in colour ink. Strokes are a
// tenth of the size (at least 1.6 px).
void PenBegin(Vector2 centre, float size, Color ink);

Vector2 PenAt(float x, float y);   // a unit-box point on screen

void PenLine(float x1, float y1, float x2, float y2);       // a line with round caps
void PenPoly(const float *xy, int points);                 // connected lines through x,y pairs
void PenRing(float x, float y, float r, float from, float to);  // an arc (degrees), stroke wide
void PenDot(float x, float y, float r);                    // a filled circle
void PenTri(float x1, float y1, float x2, float y2, float x3, float y3);  // filled, either winding
void PenBox(float x, float y, float w, float h, float radius);      // a rounded outline
void PenFillBox(float x, float y, float w, float h, float radius);  // a filled rounded box

#endif
