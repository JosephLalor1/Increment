// The skill logos. See skill_icons.h.
//
// Coordinates are in the icon unit box, -0.5 to 0.5 (icon_pen.h). Strokes are a tenth of the
// size, so the shapes are kept simple enough to read at 24 px.
#include "ui/skill_icons.h"
#include "ui/icon_pen.h"

#include <math.h>

// An arrowhead with its tip at x,y, pointing along the angle (radians).
static void Head(float x, float y, float angle, float length)
{
    float back = angle + PI, spread = 0.55f;
    PenLine(x, y, x + cosf(back - spread) * length, y + sinf(back - spread) * length);
    PenLine(x, y, x + cosf(back + spread) * length, y + sinf(back + spread) * length);
}

// A straight arrow from one point to another.
static void Arrow(float x1, float y1, float x2, float y2)
{
    PenLine(x1, y1, x2, y2);
    Head(x2, y2, atan2f(y2 - y1, x2 - x1), 0.12f);
}

// An arc of radius r around x,y from one angle to another (degrees, clockwise on screen, as
// raylib's DrawRing), with an arrowhead at its end.
static void ArcArrow(float x, float y, float r, float from, float to)
{
    PenRing(x, y, r, from, to);
    float end = to * DEG2RAD;
    float tipX = x + cosf(end) * r, tipY = y + sinf(end) * r;
    Head(tipX, tipY, end + PI / 2, 0.13f);  // the tangent, going clockwise
}

// A row of array cells: one outline split by dividers (separate boxes would run together).
static void Cells(float x, float y, float w, float h, int cells)
{
    PenBox(x, y, w, h, 0.03f);
    for (int i = 1; i < cells; i++) PenLine(x + w * i / cells, y, x + w * i / cells, y + h);
}

// A curly brace: opening (facing right, at x) when dir is 1, closing when -1.
static void Brace(float x, int dir)
{
    float d = (float)dir;
    float brace[] = { x + 0.1f * d, -0.38f, x, -0.34f, x, -0.07f, x - 0.1f * d, 0.0f,
                      x, 0.07f, x, 0.34f, x + 0.1f * d, 0.38f };
    PenPoly(brace, 7);
}

void SkillIconDraw(Skill skill, Vector2 centre, float size, Color ink)
{
    PenBegin(centre, size, ink);

    switch (skill) {
    // --- C fundamentals ---
    case SKILL_PROGRAM_STRUCTURE:  // { . }: a block of code
        Brace(-0.24f, 1);
        Brace(0.24f, -1);
        PenDot(0, 0, 0.06f);
        break;
    case SKILL_OUTPUT: {  // a terminal with a prompt
        PenBox(-0.42f, -0.32f, 0.84f, 0.64f, 0.08f);
        float prompt[] = { -0.24f, -0.12f, -0.12f, 0.0f, -0.24f, 0.12f };
        PenPoly(prompt, 3);
        PenLine(-0.02f, 0.13f, 0.2f, 0.13f);
    } break;
    case SKILL_INPUT:  // a keyboard
        PenBox(-0.44f, -0.24f, 0.88f, 0.48f, 0.08f);
        for (int key = 0; key < 5; key++) PenDot(-0.26f + key * 0.13f, -0.07f, 0.04f);
        PenLine(-0.16f, 0.11f, 0.16f, 0.11f);
        break;
    case SKILL_TYPES:  // boxes of different sizes: char, int, double
        PenBox(-0.4f, -0.38f, 0.22f, 0.18f, 0.03f);
        PenBox(-0.4f, -0.09f, 0.48f, 0.18f, 0.03f);
        PenBox(-0.4f, 0.2f, 0.8f, 0.18f, 0.03f);
        break;
    case SKILL_OPERATORS:  // + - x =
        PenLine(-0.32f, -0.2f, -0.08f, -0.2f);
        PenLine(-0.2f, -0.32f, -0.2f, -0.08f);
        PenLine(0.08f, -0.2f, 0.32f, -0.2f);
        PenLine(-0.29f, 0.11f, -0.11f, 0.29f);
        PenLine(-0.29f, 0.29f, -0.11f, 0.11f);
        PenLine(0.08f, 0.15f, 0.32f, 0.15f);
        PenLine(0.08f, 0.25f, 0.32f, 0.25f);
        break;
    case SKILL_CONDITIONS: {  // a decision diamond that branches two ways
        float diamond[] = { 0, -0.42f, 0.2f, -0.2f, 0, 0.02f, -0.2f, -0.2f, 0, -0.42f };
        PenPoly(diamond, 5);
        PenLine(0, 0.02f, -0.28f, 0.32f);
        PenLine(0, 0.02f, 0.28f, 0.32f);
        PenDot(-0.3f, 0.34f, 0.07f);
        PenDot(0.3f, 0.34f, 0.07f);
    } break;
    case SKILL_LOOPS:  // two arrows chasing each other round
        ArcArrow(0, 0, 0.3f, 200, 330);
        ArcArrow(0, 0, 0.3f, 20, 150);
        break;
    case SKILL_ARRAYS:  // a row of cells, the first marked
        Cells(-0.44f, -0.15f, 0.88f, 0.3f, 3);
        PenLine(-0.36f, 0.32f, -0.2f, 0.32f);
        break;
    case SKILL_STRINGS:  // quote marks over characters ending in \0
        PenLine(-0.36f, -0.44f, -0.39f, -0.3f);
        PenLine(-0.2f, -0.44f, -0.23f, -0.3f);
        PenLine(0.23f, -0.44f, 0.2f, -0.3f);
        PenLine(0.39f, -0.44f, 0.36f, -0.3f);
        Cells(-0.44f, -0.12f, 0.88f, 0.3f, 3);
        PenDot(-0.29f, 0.03f, 0.05f);
        PenDot(0.0f, 0.03f, 0.05f);
        PenRing(0.29f, 0.03f, 0.06f, 0, 360);  // the null character at the end
        break;
    case SKILL_FUNCTIONS:  // a box with an argument going in and a result coming out
        PenBox(-0.15f, -0.2f, 0.3f, 0.4f, 0.06f);
        Arrow(-0.44f, 0, -0.22f, 0);
        Arrow(0.15f, 0, 0.42f, 0);
        PenDot(0, 0, 0.05f);
        break;
    case SKILL_SCOPE:  // nested blocks
        PenBox(-0.42f, -0.36f, 0.84f, 0.72f, 0.1f);
        PenBox(-0.22f, -0.16f, 0.44f, 0.32f, 0.06f);
        PenDot(0, 0, 0.06f);
        break;

    // --- Pointers and memory ---
    case SKILL_POINTERS:  // a pointer aimed at a variable
        PenDot(-0.36f, 0, 0.09f);
        Arrow(-0.36f, 0, 0.04f, 0);
        PenBox(0.12f, -0.18f, 0.34f, 0.36f, 0.05f);
        break;
    case SKILL_INDIRECTION:  // a pointer to a pointer to a value
        PenDot(-0.38f, -0.3f, 0.09f);
        Arrow(-0.38f, -0.3f, 0.04f, -0.3f);
        PenBox(0.12f, -0.46f, 0.34f, 0.32f, 0.05f);
        PenDot(0.29f, -0.3f, 0.06f);
        Arrow(0.29f, -0.3f, 0.29f, 0.08f);
        PenBox(0.12f, 0.16f, 0.34f, 0.3f, 0.05f);
        break;
    case SKILL_PARAMETER_PASSING:  // a value passed into a function's parentheses
        PenRing(0.26f, 0.08f, 0.52f, 148, 212);
        PenRing(-0.26f, 0.08f, 0.52f, -32, 32);
        Arrow(0, -0.46f, 0, -0.12f);
        PenDot(0, 0.12f, 0.08f);
        break;
    case SKILL_POINTER_ARITHMETIC:  // a pointer hopping along an array
        Cells(-0.44f, 0.1f, 0.88f, 0.3f, 3);
        PenRing(-0.145f, -0.02f, 0.145f, 180, 360);
        Head(0.0f, 0.0f, PI / 2, 0.11f);
        PenLine(0.2f, -0.3f, 0.42f, -0.3f);
        PenLine(0.31f, -0.41f, 0.31f, -0.19f);
        break;
    case SKILL_DYNAMIC_MEMORY:  // blocks piled on the heap, and one more being allocated
        PenBox(-0.42f, 0.14f, 0.6f, 0.22f, 0.04f);
        PenBox(-0.32f, -0.12f, 0.36f, 0.22f, 0.04f);
        PenLine(0.14f, -0.26f, 0.4f, -0.26f);
        PenLine(0.27f, -0.39f, 0.27f, -0.13f);
        break;
    case SKILL_MEMORY_SAFETY: {  // a shield with a tick
        float shield[] = { 0, -0.42f, 0.32f, -0.3f, 0.3f, 0.06f, 0, 0.42f, -0.3f, 0.06f, -0.32f, -0.3f, 0, -0.42f };
        PenPoly(shield, 7);
        float tick[] = { -0.14f, 0.0f, -0.03f, 0.12f, 0.15f, -0.12f };
        PenPoly(tick, 3);
    } break;
    case SKILL_FUNCTION_POINTERS:  // a pointer aimed at a function, f
        PenDot(-0.4f, 0, 0.09f);
        Arrow(-0.4f, 0, -0.04f, 0);
        PenRing(0.3f, -0.22f, 0.12f, 180, 300);
        PenLine(0.18f, -0.22f, 0.18f, 0.38f);
        PenLine(0.06f, -0.02f, 0.34f, -0.02f);
        break;

    // --- C++ and objects ---
    case SKILL_CPP_BASICS:  // C++
        PenRing(-0.2f, 0, 0.22f, 40, 320);
        PenLine(0.04f, 0, 0.22f, 0);
        PenLine(0.13f, -0.09f, 0.13f, 0.09f);
        PenLine(0.26f, 0, 0.44f, 0);
        PenLine(0.35f, -0.09f, 0.35f, 0.09f);
        break;
    case SKILL_REFERENCES:  // two names for the same variable
        PenDot(-0.38f, -0.26f, 0.07f);
        PenDot(-0.38f, 0.26f, 0.07f);
        PenLine(-0.38f, -0.26f, 0.0f, 0);
        PenLine(-0.38f, 0.26f, 0.0f, 0);
        PenBox(0.06f, -0.18f, 0.36f, 0.36f, 0.05f);
        break;
    case SKILL_CLASSES:  // a class diagram: name, data, methods
        PenBox(-0.32f, -0.4f, 0.64f, 0.8f, 0.06f);
        PenLine(-0.32f, -0.14f, 0.32f, -0.14f);
        PenLine(-0.32f, 0.13f, 0.32f, 0.13f);
        PenLine(-0.12f, -0.27f, 0.12f, -0.27f);
        PenLine(-0.17f, 0.0f, 0.13f, 0.0f);
        PenLine(-0.17f, 0.27f, 0.07f, 0.27f);
        break;
    case SKILL_CONSTRUCTORS: {  // an object being built: a cube
        float cube[] = { 0, -0.4f, 0.35f, -0.2f, 0.35f, 0.2f, 0, 0.4f, -0.35f, 0.2f, -0.35f, -0.2f, 0, -0.4f };
        PenPoly(cube, 7);
        PenLine(-0.35f, -0.2f, 0, 0);
        PenLine(0.35f, -0.2f, 0, 0);
        PenLine(0, 0, 0, 0.4f);
    } break;
    case SKILL_ENCAPSULATION:  // a capsule with something kept inside
        PenBox(-0.42f, -0.2f, 0.84f, 0.4f, 0.2f);
        PenLine(0, -0.2f, 0, 0.2f);
        PenDot(-0.2f, 0, 0.07f);
        break;
    case SKILL_COPYING: {  // a value copied into a second box
        PenBox(-0.44f, -0.38f, 0.36f, 0.36f, 0.05f);
        PenBox(0.08f, 0.02f, 0.36f, 0.36f, 0.05f);
        PenDot(-0.26f, -0.2f, 0.06f);
        PenDot(0.26f, 0.2f, 0.06f);
        float path[] = { -0.26f, 0.05f, -0.26f, 0.2f, 0.0f, 0.2f };
        PenPoly(path, 3);
        Head(0.0f, 0.2f, 0, 0.1f);
    } break;
    case SKILL_DATA_STRUCTURES:  // linked nodes
        PenRing(0, -0.26f, 0.12f, 0, 360);
        PenRing(-0.27f, 0.24f, 0.12f, 0, 360);
        PenRing(0.27f, 0.24f, 0.12f, 0, 360);
        PenLine(-0.07f, -0.15f, -0.19f, 0.13f);
        PenLine(0.07f, -0.15f, 0.19f, 0.13f);
        break;

    // --- Thinking skills ---
    case SKILL_TRACING:  // lines of code with the line being run marked
        PenLine(-0.1f, -0.26f, 0.38f, -0.26f);
        PenLine(-0.1f, 0.0f, 0.3f, 0.0f);
        PenLine(-0.1f, 0.26f, 0.34f, 0.26f);
        PenTri(-0.44f, -0.13f, -0.44f, 0.13f, -0.24f, 0.0f);
        break;
    case SKILL_DEBUGGING:  // a bug
        PenBox(-0.16f, -0.12f, 0.32f, 0.46f, 0.16f);
        PenDot(0, -0.22f, 0.1f);
        PenLine(0, -0.02f, 0, 0.26f);
        PenLine(-0.05f, -0.3f, -0.14f, -0.42f);
        PenLine(0.05f, -0.3f, 0.14f, -0.42f);
        for (int side = -1; side <= 1; side += 2) {
            float s = (float)side;
            PenLine(0.17f * s, 0.0f, 0.36f * s, -0.1f);
            PenLine(0.17f * s, 0.12f, 0.38f * s, 0.12f);
            PenLine(0.17f * s, 0.24f, 0.36f * s, 0.34f);
        }
        break;
    case SKILL_PROBLEM_SOLVING:  // a light bulb
        PenRing(0, -0.1f, 0.26f, 135, 405);
        PenLine(-0.18f, 0.08f, -0.11f, 0.2f);
        PenLine(0.18f, 0.08f, 0.11f, 0.2f);
        PenLine(-0.11f, 0.22f, 0.11f, 0.22f);
        PenLine(-0.07f, 0.34f, 0.07f, 0.34f);
        break;

    case SKILL_COUNT:
        break;
    }
}
