#ifndef ARROWS_H
#define ARROWS_H

#include "raylib.h"

// Arrows for pictures of memory (the level screen's memory view and the new pieces'
// examples): a pointer's curved arrow and the mark a null pointer ends in.

// A point along the cubic curve a-b-c-d (b and c are its control points), t from 0 to 1.
Vector2 ArrowCurvePoint(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t);

// An arrow along the curve a-b-c-d, drawn as far as progress (0 to 1, eased) so a new aim
// can draw itself: solid with light flowing along it once whole, or dashed for a guess.
// Its head sits at the drawn end. now (seconds) moves the light.
void ArrowCurveDraw(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color, bool solid, float progress, double now);

// The mark a null pointer ends in, from `from` to the right: a short lead (growing with
// progress) and then three shrinking bars, like an earth symbol.
void ArrowGroundDraw(Vector2 from, Color color, float progress);

#endif
