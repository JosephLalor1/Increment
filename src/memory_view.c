// The level screen's picture of variables, heap blocks and the pointers between them.
// See memory_view.h.
#include "memory_view.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ARROW_SECONDS  0.5f   // an arrow drawing itself to a new target
#define FLASH_SECONDS  0.8f   // a changed value's flash
#define APPEAR_SECONDS 0.3f   // a new box fading in
#define CURVE_STEPS    28

// What an arrow is aimed at, in terms of what is on screen this frame.
typedef struct {
    ProbeTargetKind kind;
    bool heap;     // the target row is a heap block (otherwise a variable)
    int row;       // which variable (in the scan) or block (in the snapshot); -1 for none
    int element;
    bool solid;    // from a run, not a guess from the code
} Aim;

// What is remembered about a variable from frame to frame, by name, so things can move.
typedef struct {
    char name[PROBE_NAME];
    bool seen;                 // still in the code this frame
    float y;                   // where its row is drawn: eases towards its place
    double bornAt;
    char values[PROBE_MAX_CELLS][PROBE_VALUE];  // as last drawn
    double changedAt[PROBE_MAX_CELLS];
    Aim aim;                   // pointers: what the arrow was last aimed at
    double aimedAt;
} Memory;

static Memory memories[PROBE_MAX_VARS];
static int memoryCount;
static float scroll;

// A row as laid out this frame: a variable's box or boxes, or a heap block's.
typedef struct {
    Rectangle cells[PROBE_MAX_CELLS];
    int cellCount;
    const ProbeCell *cell;  // its values from the run, or NULL
} Row;

void MemoryViewReset(void)
{
    memoryCount = 0;
    scroll = 0;
}

static Memory *Remember(const char *name, float y, double now)
{
    for (int i = 0; i < memoryCount; i++)
        if (strcmp(memories[i].name, name) == 0) return &memories[i];
    if (memoryCount >= PROBE_MAX_VARS) return NULL;
    Memory *memory = &memories[memoryCount++];
    memset(memory, 0, sizeof *memory);
    snprintf(memory->name, sizeof memory->name, "%s", name);
    memory->y = y;
    memory->bornAt = now;
    memory->aim.kind = PROBE_TARGET_UNKNOWN;
    memory->aim.row = -1;
    return memory;
}

// Drops the memories of variables that are no longer in the code.
static void ForgetUnseen(void)
{
    int kept = 0;
    for (int i = 0; i < memoryCount; i++)
        if (memories[i].seen) memories[kept++] = memories[i];
    memoryCount = kept;
}

static int ScanIndex(const ProbeScan *scan, const char *name)
{
    for (int i = 0; i < scan->count; i++)
        if (strcmp(scan->vars[i].name, name) == 0) return i;
    return -1;
}

// A colour per pointer, so arrows can be told apart.
static Color PointerColor(int ordinal)
{
    switch (ordinal % 4) {
    case 0:  return COL_ACCENT;
    case 1:  return COL_GOLD;
    case 2:  return COL_GOOD;
    default: return ColorLerp(COL_ACCENT, COL_BAD, 0.55f);
    }
}

// ---------------------------------------------------------------------------
// Boxes

// How wide a row's boxes are: enough for its longest value.
static float CellWidth(const ProbeCell *cell, bool pointer, float size)
{
    float width = 40 * UiTextScale();
    if (pointer || !cell) return width;
    for (int i = 0; i < cell->shown; i++) {
        float need = UiTextWidth(gFonts.mono, cell->values[i], size) + 14;
        if (need > width) width = need;
    }
    return width;
}

// Lays a row's boxes out from (x, y).
static void LayOut(Row *row, const ProbeCell *cell, bool pointer, float x, float y, float height, float size)
{
    float width = CellWidth(cell, pointer, size);
    row->cell = cell;
    row->cellCount = cell && cell->shown > 0 ? cell->shown : 1;
    for (int i = 0; i < row->cellCount; i++) row->cells[i] = (Rectangle){ x + i * (width + 2), y, width, height };
}

// Where element n of a row is: one of its boxes, or the place beyond them where it would
// be (a pointer can be aimed past the end of an array).
static Rectangle ElementBox(const Row *row, int element, bool *beyond)
{
    *beyond = element < 0 || element >= row->cellCount;
    if (!*beyond) return row->cells[element];
    int place = element < 0 ? -1 : row->cellCount;  // just off either end, however far it is
    Rectangle box = row->cells[0];
    box.x += place * (box.width + 2);
    return box;
}

// One box: its fill (flashing if the value just changed), its value, and its outline.
static void DrawBox(Rectangle box, const char *value, Color ink, float flash, float alpha, Color outline, float size)
{
    UiRoundRect(box, 6, Fade(ColorLerp(COL_PANEL_HI, COL_ACCENT, flash * 0.7f), alpha));
    if (outline.a) DrawRectangleRoundedLinesEx(box, 0.3f, 6, 1.5f, Fade(outline, alpha));
    if (value && value[0])
        UiTextCentered(gFonts.mono, value, box.x + box.width / 2, box.y + (box.height - size) / 2 - 1, size, Fade(ink, alpha));
}

// ---------------------------------------------------------------------------
// Arrows

static Vector2 Curve(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t)
{
    float u = 1 - t;
    return (Vector2){ u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                      u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y };
}

// An arrow along a curve, drawn as far as `progress` (0 to 1): solid with light flowing
// along it, or dashed for a guess. Its head sits at the drawn end.
static void DrawArrow(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color, bool solid, float progress, double now)
{
    float reach = progress * progress * (3 - 2 * progress);  // eases in and out
    Vector2 previous = a, tip = a, beforeTip = a;
    for (int i = 1; i <= CURVE_STEPS; i++) {
        float t = (float)i / CURVE_STEPS;
        if (t > reach) break;
        Vector2 point = Curve(a, b, c, d, t);
        if (solid || i % 2) DrawLineEx(previous, point, solid ? 2.5f : 2, Fade(color, solid ? 0.95f : 0.6f));
        beforeTip = previous;
        previous = tip = point;
    }
    float dx = tip.x - beforeTip.x, dy = tip.y - beforeTip.y, length = sqrtf(dx * dx + dy * dy);
    if (length > 0.01f) {
        dx /= length;
        dy /= length;
        Vector2 left = { tip.x - dx * 10 - dy * 5, tip.y - dy * 10 + dx * 5 };
        Vector2 right = { tip.x - dx * 10 + dy * 5, tip.y - dy * 10 - dx * 5 };
        DrawTriangle(tip, right, left, Fade(color, solid ? 1 : 0.7f));
        DrawTriangle(tip, left, right, Fade(color, solid ? 1 : 0.7f));  // either winding
    }
    if (solid && progress >= 1) {  // light running from the pointer to what it points at
        for (int k = 0; k < 2; k++) {
            float t = fmodf((float)now * 0.6f + k * 0.5f, 1.0f);
            DrawCircleV(Curve(a, b, c, d, t), 3, Fade(ColorLerp(color, WHITE, 0.6f), 0.9f * sinf(t * PI)));
        }
    }
}

// The mark a null pointer ends in: a short lead and three shrinking bars, like an earth
// symbol.
static void DrawGround(Vector2 from, Color color, float progress)
{
    Vector2 end = { from.x + 26 * progress, from.y };
    DrawLineEx(from, end, 2.5f, color);
    if (progress < 1) return;
    for (int i = 0; i < 3; i++) {
        float half = 9 - i * 3.0f;
        DrawLineEx((Vector2){ end.x + i * 5, end.y - half }, (Vector2){ end.x + i * 5, end.y + half }, 2.5f, color);
    }
}

// Whether two aims are at the same thing (however sure).
static bool SameAim(Aim a, Aim b)
{
    return a.kind == b.kind && a.heap == b.heap && a.row == b.row && a.element == b.element;
}

// What a pointer holding `address` is aimed at, from the run.
static Aim AimFromRun(const ProbeScan *scan, const ProbeSnapshot *snapshot, unsigned long long address, bool solid)
{
    ProbeTarget target = ProbeResolve(snapshot, address);
    Aim aim = { target.kind, false, -1, target.element, solid };
    if (target.kind == PROBE_TARGET_VAR) {
        aim.row = ScanIndex(scan, snapshot->vars[target.index].name);
        if (aim.row < 0) aim.kind = PROBE_TARGET_WILD;  // a variable the code no longer has
    } else if (target.kind == PROBE_TARGET_HEAP || target.kind == PROBE_TARGET_FREED) {
        aim.heap = true;
        aim.row = target.index;
    }
    return aim;
}

// Draws the arrow from a pointer's box to its aim. `lane` spreads arrows apart; rows are
// this frame's layout.
static void DrawAim(Rectangle from, Aim aim, const Row *stack, const Row *heap, Color color, int lane, float progress,
                    double now)
{
    Vector2 start = { from.x + from.width / 2, from.y + from.height / 2 };
    DrawCircleV(start, 4.5f, color);
    if (aim.kind == PROBE_TARGET_UNKNOWN) return;
    if (aim.kind == PROBE_TARGET_NULL) {
        DrawGround((Vector2){ from.x + from.width, start.y }, Fade(COL_DIM, aim.solid ? 1 : 0.6f), progress);
        return;
    }
    if (aim.kind == PROBE_TARGET_WILD || aim.row < 0) {  // nowhere known: a stub and a question mark
        Color stub = aim.kind == PROBE_TARGET_WILD ? COL_BAD : COL_DIM;
        Vector2 end = { from.x + from.width + 24 * progress, start.y };
        DrawLineEx((Vector2){ from.x + from.width, start.y }, end, 2, Fade(stub, 0.8f));
        if (progress >= 1) UiText(gFonts.bold, "?", end.x + 5, end.y - 10, 18, stub);
        return;
    }

    bool beyond;
    Rectangle box = ElementBox(aim.heap ? &heap[aim.row] : &stack[aim.row], aim.element, &beyond);
    if (aim.kind == PROBE_TARGET_FREED || beyond) color = COL_BAD;
    if (beyond) DrawRectangleRoundedLinesEx(box, 0.3f, 6, 1.5f, Fade(COL_BAD, 0.5f));  // where it would be

    // Leave the pointer's box to the right. A variable's only box is entered from its right
    // side, a block's first box from its left; any other element from above or below, so
    // the arrow doesn't cross its neighbours.
    float bulge = 46 + lane * 16.0f;
    Vector2 end, pull;
    bool single = aim.heap ? aim.element == 0 : (stack[aim.row].cellCount == 1 && !beyond);
    if (single && aim.heap) {
        end = (Vector2){ box.x - 2, box.y + box.height / 2 };
        pull = (Vector2){ end.x - bulge, end.y };
    } else if (single) {
        end = (Vector2){ box.x + box.width + 2, box.y + box.height / 2 };
        pull = (Vector2){ end.x + bulge, end.y };
    } else {
        bool fromAbove = start.y < box.y;
        end = (Vector2){ box.x + box.width / 2, fromAbove ? box.y - 2 : box.y + box.height + 2 };
        pull = (Vector2){ end.x, end.y + (fromAbove ? -34 : 34) };
    }
    Vector2 push = { from.x + from.width + bulge, start.y };
    DrawArrow((Vector2){ from.x + from.width, start.y }, push, pull, end, color, aim.solid, progress, now);

    // The target lights up as the arrow lands.
    float landed = progress >= 1 ? 1 : 0;
    if (landed > 0) DrawRectangleRoundedLinesEx(box, 0.3f, 6, 2, Fade(color, aim.solid ? 0.9f : 0.5f));
}

// ---------------------------------------------------------------------------
// The picture

void MemoryViewDraw(Rectangle area, const ProbeScan *scan, const ProbeSnapshot *snapshot, bool fresh)
{
    double now = GetTime();
    float scale = UiTextScale(), size = 16 * scale, nameSize = 15 * scale;
    float boxH = 28 * scale, rowGap = 12 * scale;
    bool ran = snapshot && snapshot->valid;

    // The names sit in a column on the left, right-aligned against the boxes.
    float nameW = 0;
    for (int i = 0; i < scan->count; i++) nameW = fmaxf(nameW, UiTextWidth(gFonts.mono, scan->vars[i].name, nameSize));
    float boxX = area.x + nameW + 18;

    if (CheckCollisionPointRec(GetMousePosition(), area)) scroll -= GetMouseWheelMove() * (boxH + rowGap);
    float contentH = scan->count * (boxH + rowGap) + 8;
    if (ran) contentH = fmaxf(contentH, snapshot->blockCount * (boxH + rowGap) + 8);
    scroll = fmaxf(0, fminf(scroll, contentH - area.height));

    // Lay out the variables, each easing towards its place in the list.
    Row stack[PROBE_MAX_VARS], heap[PROBE_MAX_BLOCKS];
    Memory *memory[PROBE_MAX_VARS];
    float stackRight = boxX, glide = 1 - expf(-GetFrameTime() * 14);
    for (int i = 0; i < memoryCount; i++) memories[i].seen = false;
    for (int i = 0; i < scan->count; i++) {
        const ProbeVar *var = &scan->vars[i];
        float place = area.y + 6 + i * (boxH + rowGap);
        memory[i] = Remember(var->name, place, now);
        if (memory[i]) {
            memory[i]->seen = true;
            memory[i]->y += (place - memory[i]->y) * glide;
        }
        int found = ran ? ProbeFindVar(snapshot, var->name) : -1;
        LayOut(&stack[i], found >= 0 ? &snapshot->vars[found] : NULL, var->pointerDepth > 0, boxX,
               (memory[i] ? memory[i]->y : place) - scroll, boxH, size);
        Rectangle last = stack[i].cells[stack[i].cellCount - 1];
        stackRight = fmaxf(stackRight, last.x + last.width);
    }
    ForgetUnseen();
    for (int i = 0; i < scan->count; i++) memory[i] = Remember(scan->vars[i].name, 0, now);  // they may have moved

    // Heap blocks in their own column on the right.
    float heapX = fmaxf(stackRight + 120, area.x + area.width * 0.56f);
    int blocks = ran ? snapshot->blockCount : 0;
    for (int i = 0; i < blocks; i++) {
        const ProbeCell *block = &snapshot->blocks[i];
        LayOut(&heap[i], block, block->pointerDepth > 0, heapX, area.y + 6 + i * (boxH + rowGap) - scroll, boxH, size);
    }

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    if (blocks > 0) {  // the heap's side of the picture, set apart
        Rectangle region = { heapX - 12, area.y, area.x + area.width - heapX + 12, area.height };
        UiRoundRect(region, 8, Fade(COL_PANEL_HI, 0.25f));
        for (float y = region.y + 6; y < region.y + region.height - 6; y += 12)
            DrawRectangle((int)region.x, (int)y, 2, 6, Fade(COL_DIM, 0.5f));
    }

    // Variables: name, then a box per element. Values from a run that is out of date are dim.
    Color valueInk = fresh ? COL_TEXT : COL_DIM;
    for (int i = 0; i < scan->count; i++) {
        const ProbeVar *var = &scan->vars[i];
        const Row *row = &stack[i];
        float alpha = memory[i] ? fminf(1, (float)(now - memory[i]->bornAt) / APPEAR_SECONDS) : 1;
        float nameY = row->cells[0].y + (boxH - nameSize) / 2 - 1;
        UiText(gFonts.mono, var->name, boxX - 10 - UiTextWidth(gFonts.mono, var->name, nameSize), nameY, nameSize,
               Fade(COL_DIM, alpha));
        for (int e = 0; e < row->cellCount; e++) {
            const char *value = row->cell ? row->cell->values[e] : var->pointerDepth > 0 ? "" : "?";
            if (row->cell && row->cell->kind == PROBE_OPAQUE && row->cell->pointerDepth == 0) value = "{}";
            float flash = 0;
            if (memory[i] && row->cell && var->pointerDepth == 0) {  // a value that changed since the last run flashes
                if (strcmp(memory[i]->values[e], value) != 0) {
                    if (memory[i]->values[e][0]) memory[i]->changedAt[e] = now;
                    snprintf(memory[i]->values[e], PROBE_VALUE, "%s", value);
                }
                float age = (float)(now - memory[i]->changedAt[e]);
                flash = memory[i]->changedAt[e] > 0 && age < FLASH_SECONDS ? 1 - age / FLASH_SECONDS : 0;
            }
            DrawBox(row->cells[e], value, row->cell ? valueInk : COL_DIM, flash, alpha, BLANK, size);
        }
        if (row->cell && row->cell->count > row->cellCount) {  // more elements than are shown
            Rectangle last = row->cells[row->cellCount - 1];
            UiText(gFonts.mono, "..", last.x + last.width + 5, last.y + (boxH - size) / 2, size, COL_DIM);
        }
    }

    // Heap blocks: a freed one is red and crossed out.
    for (int i = 0; i < blocks; i++) {
        const ProbeCell *block = &snapshot->blocks[i];
        for (int e = 0; e < heap[i].cellCount; e++) {
            Rectangle box = heap[i].cells[e];
            const char *value = block->kind == PROBE_OPAQUE && block->pointerDepth == 0 ? "{}" : block->values[e];
            DrawBox(box, block->freed ? "" : value, valueInk, 0, fresh ? 1 : 0.6f, block->freed ? COL_BAD : Fade(COL_DIM, 0.6f), size);
            if (block->freed) {
                DrawLineEx((Vector2){ box.x + 6, box.y + 6 }, (Vector2){ box.x + box.width - 6, box.y + box.height - 6 }, 2, COL_BAD);
                DrawLineEx((Vector2){ box.x + box.width - 6, box.y + 6 }, (Vector2){ box.x + 6, box.y + box.height - 6 }, 2, COL_BAD);
            }
        }
        if (block->count > heap[i].cellCount) {
            Rectangle last = heap[i].cells[heap[i].cellCount - 1];
            UiText(gFonts.mono, "..", last.x + last.width + 5, last.y + (boxH - size) / 2, size, COL_DIM);
        }
    }

    // Arrows, over the boxes. A pointer variable's arrow follows the run when the run is
    // current, and otherwise the code's own word for where it points (or, failing that,
    // where it pointed when last run), dashed.
    int ordinal = 0;
    for (int i = 0; i < scan->count; i++) {
        const ProbeVar *var = &scan->vars[i];
        if (var->pointerDepth == 0) continue;
        Color color = PointerColor(ordinal);
        int lane = ordinal++ % 4;
        const ProbeCell *cell = stack[i].cell;
        if (var->isArray) {  // an array of pointers: each element's arrow, as the run left it
            for (int e = 0; cell && e < stack[i].cellCount; e++)
                DrawAim(stack[i].cells[e], AimFromRun(scan, snapshot, cell->pointers[e], fresh), stack, heap, color, lane, 1, now);
            continue;
        }
        Aim aim = { PROBE_TARGET_UNKNOWN, false, -1, 0, false };
        if (cell && fresh) {
            aim = AimFromRun(scan, snapshot, cell->pointers[0], true);
        } else if (var->guess.kind == PROBE_TARGET_VAR || var->guess.kind == PROBE_TARGET_NULL) {
            aim = (Aim){ var->guess.kind, false, var->guess.index, var->guess.element, false };
        } else if (cell) {
            aim = AimFromRun(scan, snapshot, cell->pointers[0], false);
            if (var->guess.kind == PROBE_TARGET_FREED && aim.kind == PROBE_TARGET_HEAP) aim.kind = PROBE_TARGET_FREED;
        }
        float progress = 1;
        if (memory[i]) {
            if (!SameAim(memory[i]->aim, aim)) memory[i]->aimedAt = now;  // a new target: draw the arrow afresh
            memory[i]->aim = aim;
            progress = fminf(1, (float)(now - memory[i]->aimedAt) / ARROW_SECONDS);
        }
        DrawAim(stack[i].cells[0], aim, stack, heap, color, lane, progress, now);
    }
    // Pointers kept in heap blocks (the rows of a 2D array, say) point on from there.
    for (int i = 0; i < blocks; i++) {
        const ProbeCell *block = &snapshot->blocks[i];
        if (block->pointerDepth == 0 || block->freed) continue;
        for (int e = 0; e < heap[i].cellCount; e++) {
            Aim aim = AimFromRun(scan, snapshot, block->pointers[e], fresh);
            if (aim.heap && aim.row == i) continue;
            DrawAim(heap[i].cells[e], aim, stack, heap, PointerColor(ordinal + e), e % 4, 1, now);
        }
    }
    EndScissorMode();
}
