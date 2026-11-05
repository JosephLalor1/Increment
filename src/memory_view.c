// The interactive pointer diagram in lessons. See memory_view.h.
#include "memory_view.h"
#include "memory_model.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VIEWS 8     // diagrams remembered at once (a lesson has a handful)
#define EASE_RATE 12.0f // how quickly boxes and arrows glide to where they belong

// Everything one diagram remembers between frames. Positions are relative to the memory
// area's top-left, so scrolling the lesson doesn't animate anything.
typedef struct {
    const char *source;      // the block's text; identifies the diagram
    MemScript *script;       // NULL when parsing failed
    char error[200];
    int step;
    int selected;            // selected cell, or -1
    bool settled;            // animation state has been set up
    float layoutScale;       // scale the cell widths were measured at
    float cellWidth[MEM_MAX_CELLS];
    Vector2 pos[MEM_MAX_CELLS];       // each box's top-left, gliding towards its place
    float alpha[MEM_MAX_CELLS];       // fades boxes in and out as variables come and go
    float flash[MEM_MAX_CELLS];       // a pulse when the value changes
    MemValue shown[MEM_MAX_CELLS];    // the value last drawn, to spot changes
    Vector2 arrowEnd[MEM_MAX_CELLS];  // where each pointer's arrow ends (glides when retargeted)
    float arrowGrow[MEM_MAX_CELLS];   // 0..1: a new arrow draws itself out from the pointer
    float travel;                     // 0..1: the dot running along the selected pointer's arrow
} View;

static View views[MAX_VIEWS];
static int nextSlot;
static int demoStep = -1, demoCell = -1;

void MemoryViewDemo(int step, int selectedCell)
{
    demoStep = step;
    demoCell = selectedCell;
}

void MemoryViewShutdown(void)
{
    for (int i = 0; i < MAX_VIEWS; i++) {
        free(views[i].script);
        views[i] = (View){ 0 };
    }
}

static View *ViewFor(const char *source)
{
    for (int i = 0; i < MAX_VIEWS; i++) {
        if (views[i].source == source) return &views[i];
    }
    View *v = &views[nextSlot];
    nextSlot = (nextSlot + 1) % MAX_VIEWS;
    free(v->script);
    *v = (View){ 0 };
    v->source = source;
    v->selected = -1;
    v->script = malloc(sizeof *v->script);
    if (v->script && !MemScriptParse(source, v->script, v->error, sizeof v->error)) {
        free(v->script);
        v->script = NULL;
    }
    if (v->script && demoStep >= 0) {
        v->step = demoStep < v->script->stepCount ? demoStep : v->script->stepCount - 1;
        v->selected = demoCell < v->script->cellCount ? demoCell : -1;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Layout: where every live box belongs at the current step

typedef struct {
    float s;          // text scale
    float boxH, labelH, titleH, addressH, pad, gap, rowLabelH, frameHeaderH;
    float valueSize, labelSize, addressSize;
} Metrics;

static Metrics MetricsFor(float scale)
{
    Metrics m;
    m.s = scale;
    m.boxH = 44 * scale;
    m.labelH = 20 * scale;
    m.titleH = 22 * scale;
    m.addressH = 20 * scale;
    m.pad = 12 * scale;
    m.gap = 34 * scale;  // room for arrows to arch between neighbours
    m.rowLabelH = 24 * scale;
    m.frameHeaderH = 24 * scale;
    m.valueSize = 18 * scale;
    m.labelSize = 16 * scale;
    m.addressSize = 13 * scale;
    return m;
}

// Height of one group: its title (arrays, structs), the cell labels, the boxes, the addresses.
static float GroupHeight(const Metrics *m) { return m->titleH + m->labelH + m->boxH + m->addressH; }

// The label over a box, written like its declaration: "int i", "int *ptr", "Node *head".
// Array elements and struct fields keep their own labels ("[2]", "next").
static void BoxLabel(const MemScript *s, int cell, char *out, size_t size)
{
    const MemCell *c = &s->cells[cell];
    const MemGroup *g = &s->groups[c->group];
    if (g->shape != MEM_VAR || g->region == MEM_HEAP) {
        snprintf(out, size, "%s", c->label);
        return;
    }
    const char *stars = strchr(c->type, '*');
    if (stars) snprintf(out, size, "%.*s %s%s", (int)(stars - c->type), c->type, stars, g->name);
    else snprintf(out, size, "%s %s", c->type, g->name);
}

// Widest value or label each box ever shows, so boxes don't change size between steps.
static void MeasureCells(View *v, const Metrics *m)
{
    if (v->layoutScale == m->s) return;
    v->layoutScale = m->s;
    const MemScript *s = v->script;
    for (int c = 0; c < s->cellCount; c++) {
        char label[64];
        BoxLabel(s, c, label, sizeof label);
        float w = UiTextWidth(gFonts.mono, label, m->labelSize) + 18 * m->s;
        for (int step = 0; step < s->stepCount; step++) {
            float valueW = UiTextWidth(gFonts.mono, s->steps[step].state.values[c].text, m->valueSize) + 26 * m->s;
            if (valueW > w) w = valueW;
        }
        v->cellWidth[c] = w < 78 * m->s ? 78 * m->s : w;
    }
}

static float GroupWidth(const View *v, const MemGroup *g)
{
    float w = 0;
    for (int i = 0; i < g->cellCount; i++) w += v->cellWidth[g->firstCell + i];
    return w;
}

// Whether a group is drawn at this step: it exists, or it's gone (its function returned)
// but a pointer still holds its address. Such a "ghost" stays, greyed, so the dangling
// arrow visibly points at memory that no longer belongs to anything.
static bool Shown(const MemScript *s, const MemState *st, int group)
{
    if (st->live[group]) return true;
    for (int c = 0; c < s->cellCount; c++) {
        MemValue v = st->values[c];
        if (v.kind == MEM_POINTER && s->cells[v.target].group == group && st->live[s->cells[c].group]) return true;
    }
    return false;
}

// Places groups left to right from (x, *y), wrapping at width; boxes' top-left go in out.
// Returns the x after the last group.
static float FlowGroups(const View *v, const Metrics *m, const int *groups, int count, float startX, float *y,
                        float width, Vector2 *out)
{
    const MemScript *s = v->script;
    float x = startX;
    for (int i = 0; i < count; i++) {
        const MemGroup *g = &s->groups[groups[i]];
        float w = GroupWidth(v, g);
        if (x > startX && x + w > width) {  // wrap
            x = startX;
            *y += GroupHeight(m) + m->gap;
        }
        float cellX = x;
        for (int c = 0; c < g->cellCount; c++) {
            out[g->firstCell + c] = (Vector2){ cellX, *y + m->titleH + m->labelH };
            cellX += v->cellWidth[g->firstCell + c];
        }
        x += w + m->gap;
    }
    return x;
}

// A stack frame is at least wide enough for its longest label, "name() returned".
static float FrameLabelWidth(const MemScript *s, const Metrics *m, int frame)
{
    char label[MEM_NAME_SIZE + 16];
    snprintf(label, sizeof label, "%s() returned", s->frames[frame]);
    return UiTextWidth(gFonts.mono, label, m->labelSize) + 2 * m->pad;
}

// Target positions for the current step. Returns the memory area's height and sets the
// y of the heap row's label (or -1 with no heap).
static float LayOut(const View *v, const Metrics *m, float width, Vector2 *target, float *heapLabelY)
{
    const MemScript *s = v->script;
    const MemState *st = &s->steps[v->step].state;
    float y = m->rowLabelH;
    float x = 0;
    int groups[MEM_MAX_GROUPS];

    // The stack: one framed section per function, main first.
    for (int f = 0; f < s->frameCount; f++) {
        int count = 0;
        for (int g = 0; g < s->groupCount; g++) {
            if (s->groups[g].region == MEM_STACK && s->groups[g].frame == f && Shown(s, st, g)) groups[count++] = g;
        }
        if (!count) continue;
        float frameW = 0;
        for (int i = 0; i < count; i++) frameW += GroupWidth(v, &s->groups[groups[i]]) + m->gap;
        frameW += 2 * m->pad - m->gap;
        float labelW = FrameLabelWidth(s, m, f);
        if (frameW < labelW) frameW = labelW;
        if (x > 0 && x + frameW > width) {
            x = 0;
            y += m->frameHeaderH + GroupHeight(m) + 2 * m->pad + m->gap;
        }
        float frameStart = x;
        float rowY = y + m->frameHeaderH + m->pad;
        float rowEnd = FlowGroups(v, m, groups, count, x + m->pad, &rowY, width - m->pad, target);
        x = fmaxf(rowEnd + m->pad, frameStart + frameW + m->gap);
        y = rowY - m->frameHeaderH - m->pad;
    }
    y += m->frameHeaderH + GroupHeight(m) + 2 * m->pad;

    // The heap, if anything was ever allocated (freed blocks stay, greyed).
    int count = 0;
    for (int g = 0; g < s->groupCount; g++) {
        if (s->groups[g].region == MEM_HEAP && Shown(s, st, g)) groups[count++] = g;
    }
    *heapLabelY = -1;
    if (count) {
        y += m->gap * 1.5f;  // room for the arrows coming down from the stack
        *heapLabelY = y;
        y += m->rowLabelH;
        FlowGroups(v, m, groups, count, 0, &y, width, target);
        y += GroupHeight(m);
    }
    return y;
}

// ---------------------------------------------------------------------------
// Animation

static float Ease(float dt) { return 1.0f - expf(-dt * EASE_RATE); }

static bool SameValue(MemValue a, MemValue b)
{
    return a.kind == b.kind && a.target == b.target && strcmp(a.text, b.text) == 0;
}

static Rectangle BoxAt(const View *v, const Metrics *m, int cell, Vector2 origin)
{
    return (Rectangle){ origin.x + v->pos[cell].x, origin.y + v->pos[cell].y, v->cellWidth[cell], m->boxH };
}

// Where a pointer's arrow should end: just above the target's labels (and its array or
// struct title), or below its address when the target is higher up the diagram, so the
// arrow never covers text.
static Vector2 ArrowTarget(const View *v, const Metrics *m, int from, int to)
{
    float targetX = v->pos[to].x + v->cellWidth[to] / 2;
    bool above = v->pos[to].y + m->boxH < v->pos[from].y - 4;
    if (above) return (Vector2){ targetX, v->pos[to].y + m->boxH + m->addressH };
    bool titled = v->script->groups[v->script->cells[to].group].shape != MEM_VAR;
    return (Vector2){ targetX, v->pos[to].y - m->labelH - (titled ? m->titleH : 0) - 2 * m->s };
}

static void Animate(View *v, const Metrics *m, const Vector2 *target)
{
    const MemScript *s = v->script;
    const MemState *st = &s->steps[v->step].state;
    float dt = GetFrameTime();
    float k = v->settled ? Ease(dt) : 1.0f;

    for (int c = 0; c < s->cellCount; c++) {
        bool live = Shown(s, st, s->cells[c].group);
        if (live) {
            if (v->alpha[c] <= 0.01f || !v->settled) v->pos[c] = target[c];  // appear in place
            v->pos[c].x += (target[c].x - v->pos[c].x) * k;
            v->pos[c].y += (target[c].y - v->pos[c].y) * k;
        }
        float wantAlpha = live ? 1.0f : 0.0f;
        v->alpha[c] = v->settled ? v->alpha[c] + (wantAlpha - v->alpha[c]) * Ease(dt * 0.8f) : wantAlpha;

        MemValue now = st->values[c];
        if (v->settled && live && !SameValue(now, v->shown[c])) v->flash[c] = 1.0f;
        v->shown[c] = now;
        v->flash[c] = v->flash[c] > 0 ? v->flash[c] - dt * 1.1f : 0;
    }

    // Arrows: grow out when a pointer first points somewhere; glide when it's retargeted.
    for (int c = 0; c < s->cellCount; c++) {
        MemValue val = st->values[c];
        bool points = val.kind == MEM_POINTER && st->live[s->cells[c].group];
        if (!points) {
            v->arrowGrow[c] = v->settled ? fmaxf(0, v->arrowGrow[c] - dt * 4) : 0;
            continue;
        }
        Vector2 want = ArrowTarget(v, m, c, val.target);
        if (v->arrowGrow[c] <= 0.001f || !v->settled) v->arrowEnd[c] = want;
        v->arrowEnd[c].x += (want.x - v->arrowEnd[c].x) * k;
        v->arrowEnd[c].y += (want.y - v->arrowEnd[c].y) * k;
        v->arrowGrow[c] = v->settled ? fminf(1, v->arrowGrow[c] + dt * 2.6f) : 1;
    }
    v->travel = fmodf(v->travel + dt * 0.7f, 1.0f);
    v->settled = true;
}

// ---------------------------------------------------------------------------
// Drawing

static Vector2 Bezier(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t)
{
    float u = 1 - t;
    float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return (Vector2){ a.x * w0 + b.x * w1 + c.x * w2 + d.x * w3, a.y * w0 + b.y * w1 + c.y * w2 + d.y * w3 };
}

// One pointer's arrow: the cubic curve start-c1-c2-end, drawn out to `grow`, with a head.
static void DrawArrow(Vector2 start, Vector2 c1, Vector2 c2, Vector2 end, float grow, float scale, Color color,
                      bool dangling, float dot)
{
    if (grow <= 0.01f) return;
    const int segments = 28;
    float thick = 2.6f * scale;
    Vector2 previous = start;
    for (int i = 1; i <= segments; i++) {
        float t = grow * i / segments;
        Vector2 p = Bezier(start, c1, c2, end, t);
        DrawLineEx(previous, p, thick, color);
        DrawCircleV(p, thick / 2, color);  // round joints keep the curve smooth
        previous = p;
    }
    DrawCircleV(start, 4.5f * scale, color);

    Vector2 tip = Bezier(start, c1, c2, end, grow);
    Vector2 back = Bezier(start, c1, c2, end, fmaxf(0, grow - 0.04f));
    Vector2 dir = { tip.x - back.x, tip.y - back.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.001f) {
        dir.x /= len;
        dir.y /= len;
        float head = 11 * scale;
        Vector2 left = { tip.x - dir.x * head - dir.y * head * 0.55f, tip.y - dir.y * head + dir.x * head * 0.55f };
        Vector2 right = { tip.x - dir.x * head + dir.y * head * 0.55f, tip.y - dir.y * head - dir.x * head * 0.55f };
        if (dangling) {  // a cross: the arrow leads nowhere valid
            float x = 6 * scale;
            DrawLineEx((Vector2){ tip.x - x, tip.y - x }, (Vector2){ tip.x + x, tip.y + x }, thick, color);
            DrawLineEx((Vector2){ tip.x - x, tip.y + x }, (Vector2){ tip.x + x, tip.y - x }, thick, color);
        } else {
            DrawTriangle(tip, left, right, color);
            DrawTriangle(tip, right, left, color);  // either winding fills
        }
    }
    if (dot >= 0 && grow >= 0.99f) {  // "following the pointer": a dot travelling to the target
        Vector2 p = Bezier(start, c1, c2, end, dot);
        DrawCircleV(p, 6 * scale, ColorAlpha(COL_ACCENT_HI, 0.9f));
    }
}

static Color ValueColor(MemValue v)
{
    switch (v.kind) {
    case MEM_GARBAGE: return COL_DIM;
    case MEM_NULL:    return COL_BAD;
    case MEM_POINTER: return COL_ACCENT;
    default:          return COL_TEXT;
    }
}

// Is this cell the selected one, what the selected pointer points at, or a pointer to it?
static bool Related(const View *v, const MemState *st, int cell)
{
    if (v->selected < 0) return false;
    if (cell == v->selected) return true;
    MemValue sel = st->values[v->selected], mine = st->values[cell];
    return (sel.kind == MEM_POINTER && sel.target == cell) || (mine.kind == MEM_POINTER && mine.target == v->selected);
}

// Draws the boxes, labels and addresses, and handles clicks on boxes. Returns the clicked
// cell, or -1.
static int DrawCells(View *v, const Metrics *m, Vector2 origin)
{
    const MemScript *s = v->script;
    const MemState *st = &s->steps[v->step].state;
    int clicked = -1;
    for (int g = 0; g < s->groupCount; g++) {
        const MemGroup *group = &s->groups[g];
        int first = group->firstCell;
        float alpha = v->alpha[first];
        if (alpha <= 0.01f) continue;
        bool gone = !st->live[g];  // a ghost: its function returned, but a pointer still holds its address
        if (st->freed[g] || gone) alpha *= 0.45f;
        Rectangle firstBox = BoxAt(v, m, first, origin);

        // Title over arrays and structs, written like the declaration: "float a[4]", "Node n";
        // heap blocks have no name, so just their type: "int[5]", "Node".
        char title[64] = "";
        bool heap = group->region == MEM_HEAP;
        if (group->shape == MEM_ARRAY) {
            if (heap) snprintf(title, sizeof title, "%s[%d]", group->type, group->cellCount);
            else snprintf(title, sizeof title, "%s %s[%d]", group->type, group->name, group->cellCount);
        } else if (group->shape == MEM_STRUCT) {
            if (heap) snprintf(title, sizeof title, "%s", group->type);
            else snprintf(title, sizeof title, "%s %s", group->type, group->name);
        }
        if (title[0]) {
            UiText(gFonts.mono, title, firstBox.x, firstBox.y - m->labelH - m->titleH + 2, m->labelSize,
                   Fade(COL_TEXT, alpha));
        }
        if (st->freed[g] || gone) {  // under the boxes, right-aligned: arrows never end there
            const char *tag = gone ? "gone" : "freed";
            UiText(gFonts.bold, tag, firstBox.x + GroupWidth(v, group) - UiTextWidth(gFonts.bold, tag, m->addressSize) - 4 * m->s,
                   firstBox.y + m->boxH + 3 * m->s, m->addressSize, Fade(COL_BAD, 0.9f * v->alpha[first]));
        }

        for (int c = first; c < first + group->cellCount; c++) {
            Rectangle box = BoxAt(v, m, c, origin);
            MemValue value = st->values[c];
            bool over = UiMouseOver(box) && Shown(s, st, g);
            if (over) {
                UiWantHand();
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = c;
            }
            bool related = Related(v, st, c);
            Color fill = over ? COL_SECONDARY_HOVER : COL_CODE_BG;
            if (related) fill = ColorLerp(fill, COL_ACCENT, 0.16f);
            if (v->flash[c] > 0) fill = ColorLerp(fill, COL_ACCENT, 0.45f * v->flash[c]);
            DrawRectangleRounded(box, 0.18f, 6, Fade(fill, alpha));
            Color border = related ? COL_ACCENT : COL_PANEL_HI;
            DrawRectangleRoundedLinesEx(box, 0.18f, 6, (related ? 2.5f : 1.2f) * m->s, Fade(border, alpha));

            // The label above (the declaration, [i] or field) and the address below.
            char label[64];
            BoxLabel(s, c, label, sizeof label);
            UiTextCentered(gFonts.mono, label, box.x + box.width / 2, box.y - m->labelH + 1, m->labelSize,
                           Fade(related ? COL_ACCENT : COL_TEXT, alpha));
            char address[16];
            snprintf(address, sizeof address, "%d", s->cells[c].address);
            // At the left, leaving the right of the bottom edge free for arrows going down.
            UiText(gFonts.mono, address, box.x + 6 * m->s, box.y + m->boxH + 4 * m->s, m->addressSize,
                   Fade(COL_DIM, alpha * 0.85f));

            float textW = UiTextWidth(gFonts.mono, value.text, m->valueSize);
            UiText(gFonts.mono, value.text, box.x + (box.width - textW) / 2, box.y + (m->boxH - m->valueSize) / 2,
                   m->valueSize, Fade(ValueColor(value), alpha));
        }
    }
    return clicked;
}

// Frame outlines around each function's variables, with its name; and the row labels.
static void DrawRegions(const View *v, const Metrics *m, Vector2 origin, float width, float heapLabelY)
{
    const MemScript *s = v->script;
    const MemState *st = &s->steps[v->step].state;
    UiText(gFonts.bold, "Stack", origin.x, origin.y, m->labelSize, COL_DIM);
    for (int f = 0; f < s->frameCount; f++) {
        float left = 1e9f, right = -1e9f, top = 1e9f, alpha = 0;
        bool returned = true;  // only ghosts are left: the function has returned
        for (int g = 0; g < s->groupCount; g++) {
            const MemGroup *group = &s->groups[g];
            if (group->region != MEM_STACK || group->frame != f) continue;
            if (st->live[g]) returned = false;
            for (int c = group->firstCell; c < group->firstCell + group->cellCount; c++) {
                if (v->alpha[c] <= 0.01f) continue;
                left = fminf(left, v->pos[c].x);
                right = fmaxf(right, v->pos[c].x + v->cellWidth[c]);
                top = fminf(top, v->pos[c].y);
                alpha = fmaxf(alpha, v->alpha[c]);
            }
        }
        if (alpha <= 0.01f) continue;
        if (returned) alpha *= 0.55f;
        Rectangle frame = { origin.x + left - m->pad, origin.y + top - m->titleH - m->labelH - m->frameHeaderH - m->pad,
                            right - left + 2 * m->pad, m->frameHeaderH + GroupHeight(m) + 2 * m->pad };
        frame.width = fmaxf(frame.width, FrameLabelWidth(s, m, f));
        if (frame.x + frame.width > origin.x + width) frame.width = origin.x + width - frame.x;
        DrawRectangleRounded(frame, 0.08f, 8, Fade(COL_BG, 0.55f * alpha));
        DrawRectangleRoundedLinesEx(frame, 0.08f, 8, 1.2f * m->s, Fade(returned ? COL_BAD : COL_PANEL_HI, alpha));
        char name[MEM_NAME_SIZE + 16];
        snprintf(name, sizeof name, returned ? "%s() returned" : "%s()", s->frames[f]);
        UiText(gFonts.mono, name, frame.x + m->pad, frame.y + 5 * m->s, m->labelSize,
               Fade(returned ? COL_BAD : COL_ACCENT, alpha));
    }
    if (heapLabelY >= 0) UiText(gFonts.bold, "Heap", origin.x, origin.y + heapLabelY, m->labelSize, COL_DIM);
}

static void DrawArrows(View *v, const Metrics *m, Vector2 origin)
{
    const MemScript *s = v->script;
    const MemState *st = &s->steps[v->step].state;
    for (int c = 0; c < s->cellCount; c++) {
        if (v->arrowGrow[c] <= 0.01f || v->alpha[c] <= 0.01f) continue;
        MemValue val = st->values[c];
        int targetGroup = val.kind == MEM_POINTER ? s->cells[val.target].group : -1;
        bool dangling = targetGroup >= 0 && (!st->live[targetGroup] || st->freed[targetGroup]);
        bool highlighted = v->selected < 0 || c == v->selected || val.target == v->selected;
        int sourceGroup = s->cells[c].group;
        bool sourceDead = st->freed[sourceGroup] || !st->live[sourceGroup];  // nothing should follow it now
        Color color = dangling ? COL_BAD : COL_ACCENT;
        if (!highlighted || sourceDead) color = Fade(color, 0.35f);  // let the selected box's arrows stand out
        // The route keeps clear of labels: to a lower row, out of the box's bottom edge (the
        // address sits at the left, out of the way) and down onto the target; along a row,
        // out of the side facing the target and arching over; to a higher row, out of the
        // side and up into the target from below.
        Rectangle box = { origin.x + v->pos[c].x, origin.y + v->pos[c].y, v->cellWidth[c], m->boxH };
        Vector2 end = { origin.x + v->arrowEnd[c].x, origin.y + v->arrowEnd[c].y };
        float reach = 46 * m->s;
        int side = end.x < box.x + box.width / 2 - 4 ? -1 : 1;
        Vector2 start, c1, c2;
        if (end.y > box.y + box.height + m->addressH) {
            start = (Vector2){ box.x + box.width * 0.72f, box.y + box.height };
            float drop = fminf(reach * 1.6f, (end.y - start.y) * 0.6f);
            c1 = (Vector2){ start.x, start.y + drop };
            c2 = (Vector2){ end.x, end.y - drop };
        } else if (end.y > box.y - m->labelH - m->titleH - 8 * m->s) {
            start = (Vector2){ side > 0 ? box.x + box.width : box.x, box.y + box.height / 2 };
            float lift = 30 * m->s + fabsf(end.x - start.x) * 0.12f;
            c1 = (Vector2){ start.x + side * reach, start.y - lift };
            c2 = (Vector2){ end.x, end.y - lift };
        } else {
            start = (Vector2){ side > 0 ? box.x + box.width : box.x, box.y + box.height / 2 };
            c1 = (Vector2){ start.x + side * reach * 1.4f, start.y };
            c2 = (Vector2){ end.x, end.y + reach * 1.6f };
        }
        float dot = (c == v->selected && !dangling) ? v->travel : -1;
        DrawArrow(start, c1, c2, end, v->arrowGrow[c] * v->alpha[c], m->s, Fade(color, v->alpha[c]), dangling, dot);
    }
}

// The example's code, one clickable row per step. Returns the clicked step, or -1.
static int DrawCode(const View *v, float x, float y, float w, float rowH, float size, float scale)
{
    const MemScript *s = v->script;
    int clicked = -1;
    UiRoundRect((Rectangle){ x, y, w, rowH * s->stepCount + 12 * scale }, 8, COL_CODE_BG);
    for (int i = 0; i < s->stepCount; i++) {
        Rectangle row = { x, y + 6 * scale + i * rowH, w, rowH };
        bool current = i == v->step, over = UiMouseOver(row);
        if (over) {
            UiWantHand();
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = i;
        }
        if (current) {
            DrawRectangleRec(row, Fade(COL_ACCENT, 0.16f));
            DrawRectangleRec((Rectangle){ row.x, row.y, 4 * scale, row.height }, COL_ACCENT);
        } else if (over) {
            DrawRectangleRec(row, Fade(COL_SECONDARY_HOVER, 0.7f));
        }
        char number[12];
        snprintf(number, sizeof number, "%d", i + 1);
        UiText(gFonts.mono, number, x + 16 * scale, row.y + (rowH - size) / 2, size * 0.85f,
               current ? COL_ACCENT : COL_DIM);
        float codeX = x + 48 * scale, codeY = row.y + (rowH - size) / 2;
        if (i <= v->step) {
            UiCodeLine(s->steps[i].code, codeX, codeY, size);
        } else {
            UiText(gFonts.mono, s->steps[i].code, codeX, codeY, size, Fade(COL_DIM, 0.8f));  // not run yet
        }
    }
    return clicked;
}

static float DrawError(const View *v, float x, float y, float w, float scale)
{
    char message[300];
    snprintf(message, sizeof message, "This pointer diagram couldn't be loaded: %s", v->error);
    float h = UiWrappedText(gFonts.text, message, (Vector2){ 0, 0 }, w - 32, 18 * scale, COL_BAD, false) + 24;
    UiRoundRect((Rectangle){ x, y, w, h }, 8, COL_BAD_BG);
    UiWrappedText(gFonts.text, message, (Vector2){ x + 16, y + 12 }, w - 32, 18 * scale, COL_BAD, true);
    return h + 16;
}

float MemoryViewDraw(const char *source, float x, float y, float w, float scale)
{
    View *v = ViewFor(source);
    if (!v->script) return DrawError(v, x, y, w, scale);
    const MemScript *s = v->script;
    Metrics m = MetricsFor(scale);
    MeasureCells(v, &m);
    float pad = 20 * scale, innerX = x + pad, innerW = w - 2 * pad;

    // Lay out the memory area first: the panel's height depends on it.
    Vector2 target[MEM_MAX_CELLS] = { 0 };
    float heapLabelY;
    float memoryH = LayOut(v, &m, innerW, target, &heapLabelY);
    Animate(v, &m, target);

    char explain[MEM_EXPLAIN_SIZE * 2], heading[96];
    if (v->selected >= 0) {
        char name[48];
        MemCellName(s, v->selected, name, sizeof name);
        snprintf(heading, sizeof heading, "About %s", name);
        MemDescribeCell(s, v->step, v->selected, explain, sizeof explain);
    } else {
        snprintf(heading, sizeof heading, "Step %d of %d", v->step + 1, s->stepCount);
        snprintf(explain, sizeof explain, "%s", s->steps[v->step].explain);
    }
    float textSize = 19 * scale;
    float explainH = UiWrappedText(gFonts.text, explain, (Vector2){ 0, 0 }, innerW - 32, textSize, COL_TEXT, false);
    float codeSize = 18 * scale, rowH = 30 * scale;
    float titleH = 30 * scale, hintH = 24 * scale;
    float codeH = rowH * s->stepCount + 12 * scale;
    float boxH = 44 * scale + explainH;
    float height = pad + titleH + hintH + 12 * scale + codeH + 22 * scale + memoryH + 18 * scale + boxH + pad;

    UiRoundRect((Rectangle){ x, y, w, height }, 12, COL_PANEL);
    DrawRectangleRounded((Rectangle){ x, y, 5 * scale, height }, 1, 4, COL_ACCENT);

    // Title, hint and the step buttons.
    float top = y + pad;
    const char *title = s->title[0] ? s->title : "Pointer diagram";
    UiText(gFonts.bold, title, innerX, top, 22 * scale, COL_TEXT);
    float buttonW = 108 * scale, buttonH = 34 * scale;
    Rectangle next = { innerX + innerW - buttonW, top - 4 * scale, buttonW, buttonH };
    Rectangle back = { next.x - buttonW - 8 * scale, next.y, buttonW, buttonH };
    int goTo = -1;
    if (UiButtonIconEx(back, ICON_BACK, "Back", 16 * scale, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, v->step > 0)) {
        goTo = v->step - 1;
    }
    bool atEnd = v->step >= s->stepCount - 1;
    if (UiButtonIconEx(next, atEnd ? ICON_RETRY : ICON_NEXT, atEnd ? "Again" : "Next", 16 * scale, COL_ACCENT,
                       COL_ACCENT_HI, COL_ON_ACCENT, true)) {
        goTo = atEnd ? 0 : v->step + 1;
    }
    UiText(gFonts.text, "Click a line of code to run up to it, or click a box to see what it holds.", innerX,
           top + titleH, 16 * scale, COL_DIM);

    float codeY = top + titleH + hintH + 12 * scale;
    int clickedStep = DrawCode(v, innerX, codeY, innerW, rowH, codeSize, scale);
    if (clickedStep >= 0) goTo = clickedStep;

    Vector2 origin = { innerX, codeY + codeH + 22 * scale };
    DrawRegions(v, &m, origin, innerW, heapLabelY);
    DrawArrows(v, &m, origin);
    int clickedCell = DrawCells(v, &m, origin);

    // The explanation: the selected box's, or the step's.
    Rectangle box = { innerX, origin.y + memoryH + 18 * scale, innerW, boxH };
    UiRoundRect(box, 8, COL_NOTE_BG);
    IconDraw(v->selected >= 0 ? ICON_INFO : ICON_BOOK, (Vector2){ box.x + 26 * scale, box.y + 22 * scale }, 18 * scale,
             COL_ACCENT);
    UiText(gFonts.bold, heading, box.x + 44 * scale, box.y + 12 * scale, 17 * scale, COL_ACCENT);
    if (v->selected >= 0) {
        float headingW = UiTextWidth(gFonts.bold, heading, 17 * scale);
        UiText(gFonts.text, "(click the box again for this step's explanation)", box.x + 56 * scale + headingW,
               box.y + 13 * scale, 15 * scale, COL_DIM);
    }
    UiWrappedText(gFonts.text, explain, (Vector2){ box.x + 16, box.y + 38 * scale }, innerW - 32, textSize, COL_TEXT, true);

    // Act on clicks after drawing, so this frame is drawn from one consistent state.
    if (clickedCell >= 0) {
        v->selected = clickedCell == v->selected ? -1 : clickedCell;  // click again to go back to the step
        v->travel = 0;
    } else if (goTo >= 0 && goTo != v->step) {
        v->step = goTo;
        // Keep a selected box selected while it still exists.
        if (v->selected >= 0 && !s->steps[goTo].state.live[s->cells[v->selected].group]) v->selected = -1;
    }
    return height + 16;
}
