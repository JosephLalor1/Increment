// Plays the example of what a piece of code does. See piece_demo_view.h.
#include "level/piece_demo_view.h"
#include "level/step_novelty.h"
#include "ui/arrows.h"
#include "ui/easing.h"
#include "ui/icons.h"
#include "ui/ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INTRO_SECONDS  0.5f   // the code settling in before the first line runs
#define FRAME_SECONDS  1.1f   // a line running
#define TYPING_SECONDS 0.8f   // more for a line where the user types
#define EVAL_SECONDS   0.45f  // more for a line whose expression is worked out
#define GLIDE_SECONDS  0.22f  // the running line's light moving to the next line
#define FLASH_SECONDS  0.8f   // a changed value's flash
#define ARROW_SECONDS  0.5f   // an arrow drawing itself to a new target

#define HEADER_H   46
#define PAD        14
#define BOX_H      30
#define ARC_ROOM   30         // under a row of boxes, for arrows between them
#define SLOT_GAP   18
#define NULL_ROOM  40         // beside a pointer that can be null, for its ground mark
#define MEMORY_MAX_W 380
#define MAX_LINES  24
#define LINE_CHARS 160
#define MAX_SLOTS  16

// ---------------------------------------------------------------------------
// Timing

static float FrameLength(const DemoFrame *frame)
{
    return FRAME_SECONDS + (frame->typed ? TYPING_SECONDS : 0) + (frame->eval ? EVAL_SECONDS : 0);
}

static float FrameStart(const PieceDemo *demo, int n)
{
    float start = INTRO_SECONDS;
    for (int i = 0; i < n; i++) start += FrameLength(&demo->frames[i]);
    return start;
}

float PieceDemoViewLength(const PieceDemo *demo)
{
    return FrameStart(demo, PieceDemoFrameCount(demo));
}

// The frame playing at `seconds` (-1 before the first), and how long it has been playing.
static int FrameAt(const PieceDemo *demo, float seconds, float *age)
{
    int frames = PieceDemoFrameCount(demo), current = -1;
    for (int i = 0; i < frames; i++)
        if (seconds >= FrameStart(demo, i)) current = i;
    *age = current >= 0 ? seconds - FrameStart(demo, current) : seconds;
    return current;
}

// ---------------------------------------------------------------------------
// Measuring

typedef struct {
    float size;    // code text
    float lineH;
    float gutter;  // line numbers
    float small;   // names over the boxes
} Metrics;

static Metrics Measure(void)
{
    Metrics m;
    m.size = 17 * UiTextScale();
    m.lineH = m.size * 1.5f;
    m.gutter = UiTextWidth(gFonts.mono, "00", m.size * 0.85f) + 18;
    m.small = 13 * UiTextScale();
    return m;
}

// Splits code into lines. Returns how many.
static int SplitLines(const char *code, char lines[][LINE_CHARS], int max)
{
    int count = 0;
    for (const char *p = code; count < max;) {
        const char *end = strchr(p, '\n');
        int length = end ? (int)(end - p) : (int)strlen(p);
        snprintf(lines[count++], LINE_CHARS, "%.*s", length < LINE_CHARS - 1 ? length : LINE_CHARS - 1, p);
        if (!end) break;
        p = end + 1;
    }
    return count;
}

// The worked-out expression's bubble: the expression, an arrow, the result.
static float BubbleWidth(const DemoFrame *frame, float size)
{
    return UiTextWidth(gFonts.mono, frame->eval, size) + 34 + UiTextWidth(gFonts.mono, frame->result, size) + 24;
}

// ---------------------------------------------------------------------------
// Memory: a slot per variable that ever appears, laid out once so nothing jumps around

typedef struct {
    char name[DEMO_NAME];
    bool block;       // an unnamed block
    bool pointer;     // a pointer (to something or to nothing)
    bool canBeNull;
    int cells;        // the most boxes it ever has
    float cellW;
    Rectangle area;   // its name and boxes, relative to the memory area
} Slot;

typedef struct {
    Slot slots[MAX_SLOTS];
    int count;
    float width, height;
} Layout;

static Slot *SlotNamed(Layout *layout, const char *name)
{
    for (int i = 0; i < layout->count; i++)
        if (strcmp(layout->slots[i].name, name) == 0) return &layout->slots[i];
    return NULL;
}

// The slot a pointer's target names: the variable, or an object's first member.
static Slot *SlotAimedAt(Layout *layout, const char *target)
{
    Slot *slot = SlotNamed(layout, target);
    size_t length = strlen(target);
    for (int i = 0; !slot && i < layout->count; i++)
        if (strncmp(layout->slots[i].name, target, length) == 0 && layout->slots[i].name[length] == '.')
            slot = &layout->slots[i];
    return slot;
}

static float LabelHeight(const Metrics *m) { return m->small + 6; }

static Rectangle CellBox(const Slot *slot, int cell, const Metrics *m, Vector2 origin)
{
    return (Rectangle){ origin.x + slot->area.x + cell * (slot->cellW + 2), origin.y + slot->area.y + LabelHeight(m),
                        slot->cellW, BOX_H };
}

static void LayOutMemory(const PieceDemo *demo, const Metrics *m, Layout *layout)
{
    memset(layout, 0, sizeof *layout);
    int frames = PieceDemoFrameCount(demo);
    for (int f = 0; f < frames; f++) {
        DemoVar vars[DEMO_VARS];
        int count = DemoMemoryParse(demo->frames[f].memory, vars, DEMO_VARS);
        for (int v = 0; v < count; v++) {
            Slot *slot = SlotNamed(layout, vars[v].name);
            if (!slot) {
                if (layout->count >= MAX_SLOTS) continue;
                slot = &layout->slots[layout->count++];
                snprintf(slot->name, sizeof slot->name, "%s", vars[v].name);
                slot->block = vars[v].block;
                slot->cellW = 34;
            }
            bool pointer = vars[v].kind == DEMO_POINTER || vars[v].kind == DEMO_NULL;
            slot->pointer |= pointer;
            slot->canBeNull |= vars[v].kind == DEMO_NULL;
            int cells = pointer ? 1 : vars[v].cellCount;
            if (cells > slot->cells) slot->cells = cells;
            for (int c = 0; !pointer && c < vars[v].cellCount; c++)
                slot->cellW = fmaxf(slot->cellW, UiTextWidth(gFonts.mono, vars[v].cells[c], m->size * 0.9f) + 16);
        }
    }
    // Variables first, in the order they appear, then unnamed blocks on rows of their own.
    float x = 0, y = 0, rowH = LabelHeight(m) + BOX_H + ARC_ROOM;
    bool blocks = false;
    for (int i = 0; i < layout->count; i++) blocks |= layout->slots[i].block;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1 && blocks && x > 0) {
            x = 0;
            y += rowH;
        }
        for (int i = 0; i < layout->count; i++) {
            Slot *slot = &layout->slots[i];
            if (slot->block != (pass == 1)) continue;
            float boxesW = slot->cells * (slot->cellW + 2) - 2 + (slot->canBeNull ? NULL_ROOM : 0);
            float labelW = slot->block ? 0 : UiTextWidth(gFonts.mono, slot->name, m->small) + 20;
            float width = fmaxf(boxesW, labelW);
            if (x > 0 && x + width > MEMORY_MAX_W) {
                x = 0;
                y += rowH;
            }
            slot->area = (Rectangle){ x, y, width, LabelHeight(m) + BOX_H };
            x += width + SLOT_GAP;
            layout->width = fmaxf(layout->width, x - SLOT_GAP);
        }
    }
    layout->height = layout->count ? y + rowH : 0;
}

// ---------------------------------------------------------------------------
// Size

// Everything the panel's size depends on.
typedef struct {
    Metrics m;
    char lines[MAX_LINES][LINE_CHARS];
    int lineCount;
    float codeW, codeH;
    Layout memory;
    float consoleW, consoleH;
    float rightW;
} Plan;

// How many lines the whole output (with what is typed) takes.
static int OutputLines(const PieceDemo *demo, float *widest, float size)
{
    char line[LINE_CHARS] = "";
    int lines = 1, length = 0;
    *widest = 0;
    for (int f = 0; f < PieceDemoFrameCount(demo); f++)
        for (int part = 0; part < 2; part++) {
            const char *text = part == 0 ? demo->frames[f].typed : demo->frames[f].printed;
            for (; text && *text; text++) {
                if (*text == '\n') {
                    lines++;
                    length = 0;
                } else if (length < LINE_CHARS - 1) {
                    line[length++] = *text;
                }
                line[length] = '\0';
                *widest = fmaxf(*widest, UiTextWidth(gFonts.mono, line, size));
            }
        }
    return lines;
}

static void MakePlan(const PieceDemo *demo, Plan *plan)
{
    plan->m = Measure();
    const Metrics *m = &plan->m;
    plan->lineCount = SplitLines(demo->code, plan->lines, MAX_LINES);
    float widest = 0;
    for (int i = 0; i < plan->lineCount; i++) widest = fmaxf(widest, UiTextWidth(gFonts.mono, plan->lines[i], m->size));
    for (int f = 0; f < PieceDemoFrameCount(demo); f++) {
        const DemoFrame *frame = &demo->frames[f];
        if (!frame->eval || frame->line > plan->lineCount) continue;
        float lineW = UiTextWidth(gFonts.mono, plan->lines[frame->line - 1], m->size);
        widest = fmaxf(widest, lineW + 18 + BubbleWidth(frame, m->size * 0.9f));
    }
    plan->codeW = m->gutter + widest + PAD * 2;
    plan->codeH = plan->lineCount * m->lineH + PAD * 2;
    LayOutMemory(demo, m, &plan->memory);
    float outputW;
    int outputLines = OutputLines(demo, &outputW, m->size);
    plan->consoleW = fmaxf(200, outputW + PAD * 2 + 14);
    plan->consoleH = fmaxf(2, (float)outputLines) * m->lineH + PAD * 2;
    plan->rightW = fmaxf(plan->consoleW, plan->memory.width + PAD * 2);
}

Vector2 PieceDemoViewSize(const PieceDemo *demo)
{
    static Plan plan;
    MakePlan(demo, &plan);
    float memoryH = plan.memory.count ? plan.memory.height + PAD * 2 + 10 : 0;
    float bodyH = fmaxf(plan.codeH, memoryH + plan.consoleH);
    return (Vector2){ PAD + plan.codeW + 12 + plan.rightW + PAD, HEADER_H + bodyH + PAD };
}

// ---------------------------------------------------------------------------
// Drawing: the code

// Whether a piece's key is the one shown.
static bool IsShownPiece(const char *key, const char *piece) { return strcmp(key, piece) == 0; }

static void DrawCode(const PieceDemo *demo, const char *piece, const Plan *plan, Rectangle area, int frame, float age,
                     float seconds, bool ended)
{
    const Metrics *m = &plan->m;
    UiRoundRect(area, 10, COL_CODE_BG);
    float top = area.y + PAD, codeX = area.x + PAD + m->gutter;

    // The running line's light, gliding from the line before.
    if (frame >= 0) {
        int line = demo->frames[frame].line - 1;
        int before = frame > 0 ? demo->frames[frame - 1].line - 1 : line;
        float glide = EaseOutCubic(Clamp01(age / GLIDE_SECONDS));
        float y = top + (before + (line - before) * glide) * m->lineH;
        float fade = ended ? 1 - Clamp01((seconds - PieceDemoViewLength(demo)) / 0.6f) : 1;
        UiRoundRect((Rectangle){ area.x + 6, y, area.width - 12, m->lineH }, 6, Fade(COL_ACCENT, 0.18f * fade));
        IconDraw(ICON_PLAY, (Vector2){ area.x + 14, y + m->lineH / 2 }, 12, Fade(COL_ACCENT, fade));
    }

    // The piece glowing where it appears: brightest on the running line.
    static CodePiece pieces[256];
    int count = CodePiecesFind(demo->code, pieces, 256);
    float pulse = (sinf(seconds * 4) + 1) / 2;
    for (int i = 0; i < count; i++) {
        if (!IsShownPiece(pieces[i].key, piece)) continue;
        int line = 0;
        const char *lineStart = demo->code;
        for (const char *p = demo->code; p < demo->code + pieces[i].start; p++)
            if (*p == '\n') {
                line++;
                lineStart = p + 1;
            }
        if (line >= plan->lineCount) continue;
        int column = (int)(demo->code + pieces[i].start - lineStart);
        char before[LINE_CHARS], text[LINE_CHARS];
        snprintf(before, sizeof before, "%.*s", column, plan->lines[line]);
        snprintf(text, sizeof text, "%.*s", pieces[i].length, plan->lines[line] + column);
        float x = codeX + UiTextWidth(gFonts.mono, before, m->size), w = UiTextWidth(gFonts.mono, text, m->size);
        bool running = frame >= 0 && demo->frames[frame].line - 1 == line;
        Rectangle glow = { x - 3, top + line * m->lineH + 2, w + 6, m->lineH - 4 };
        UiRoundRect(glow, 5, Fade(COL_GOLD, (running ? 0.28f : 0.12f) + 0.08f * pulse));
        DrawRectangleRoundedLinesEx(glow, 0.4f, 6, running ? 2 : 1.2f, Fade(COL_GOLD, running ? 0.95f : 0.55f));
    }

    for (int i = 0; i < plan->lineCount; i++) {
        float y = top + i * m->lineH + (m->lineH - m->size) / 2;
        float numberSize = m->size * 0.85f;
        UiText(gFonts.mono, TextFormat("%d", i + 1), area.x + PAD + 6, y + (m->size - numberSize) / 2, numberSize,
               Fade(COL_DIM, 0.6f));
        UiCodeLine(plan->lines[i], codeX, y, m->size);
    }

    // The running line's expression worked out: it pops up at the line's end, then its result.
    if (frame >= 0 && demo->frames[frame].eval && !ended) {
        const DemoFrame *f = &demo->frames[frame];
        float pop = PopOvershoot((age - 0.2f) / 0.3f);
        if (pop > 0) {
            float size = m->size * 0.9f;
            float lineW = UiTextWidth(gFonts.mono, plan->lines[f->line - 1], m->size);
            float width = BubbleWidth(f, size), height = m->lineH + 4;
            Rectangle bubble = { codeX + lineW + 18, top + (f->line - 1) * m->lineH - 2, width, height };
            Rectangle drawn = { bubble.x, bubble.y + height * (1 - pop) / 2, bubble.width * pop, height * pop };
            UiRoundRect(drawn, 8, COL_PANEL);
            DrawRectangleRoundedLinesEx(drawn, 0.35f, 8, 1.5f, COL_GOLD);
            if (pop > 0.9f) {
                float textY = bubble.y + (height - size) / 2;
                UiCodeLine(f->eval, bubble.x + 12, textY, size);
                float arrowX = bubble.x + 12 + UiTextWidth(gFonts.mono, f->eval, size) + 8;
                float shown = Clamp01((age - 0.55f) / 0.25f);  // the arrow reaches out, then the result lands
                DrawLineEx((Vector2){ arrowX, bubble.y + height / 2 }, (Vector2){ arrowX + 16 * shown, bubble.y + height / 2 },
                           2, COL_GOLD);
                if (shown >= 1) {
                    IconDraw(ICON_NEXT, (Vector2){ arrowX + 16, bubble.y + height / 2 }, 12, COL_GOLD);
                    float land = PopOvershoot((age - 0.8f) / 0.3f);
                    if (land > 0)
                        UiText(gFonts.mono, f->result, arrowX + 26, textY - (1 - land) * 8, size, Fade(COL_GOLD, Clamp01(land)));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing: memory

static const DemoVar *VarNamed(const DemoVar *vars, int count, const char *name)
{
    for (int i = 0; i < count; i++)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    return NULL;
}

// Whether the frame reads this variable's cell (look: "x", "a[2]").
static bool Looked(const char *look, const char *name, int cell)
{
    if (!look) return false;
    size_t length = strlen(name);
    if (strncmp(look, name, length) != 0) return false;
    if (look[length] == '\0') return true;
    return look[length] == '[' && atoi(look + length + 1) == cell;
}

static bool SameAim(const DemoVar *a, const DemoVar *b)
{
    return a && b && a->kind == b->kind && strcmp(a->target, b->target) == 0 && a->targetCell == b->targetCell;
}

static void DrawMemory(const PieceDemo *demo, Plan *plan, Rectangle area, int frame, float age, float seconds)
{
    const Metrics *m = &plan->m;
    UiRoundRect(area, 10, COL_PANEL_HI);
    Vector2 origin = { area.x + PAD, area.y + PAD };
    if (frame < 0) return;
    DemoVar vars[DEMO_VARS], before[DEMO_VARS];
    int count = DemoMemoryParse(demo->frames[frame].memory, vars, DEMO_VARS);
    int beforeCount = frame > 0 ? DemoMemoryParse(demo->frames[frame - 1].memory, before, DEMO_VARS) : 0;
    const char *look = demo->frames[frame].look;
    float pulse = (sinf(seconds * 6) + 1) / 2;
    float valueSize = m->size * 0.9f;

    for (int s = 0; s < plan->memory.count; s++) {
        const Slot *slot = &plan->memory.slots[s];
        const DemoVar *var = VarNamed(vars, count, slot->name);
        if (!var) continue;
        const DemoVar *old = VarNamed(before, beforeCount, slot->name);
        // A variable that wasn't there a line ago pops in.
        float appear = old ? 1 : Clamp01(age / 0.3f);
        float scale = old ? 1 : PopOvershoot(age / 0.35f);
        if (appear <= 0 || scale <= 0) continue;

        if (!slot->block) {  // its name over its boxes, with a padlock if it can't be changed
            float labelX = origin.x + slot->area.x, labelY = origin.y + slot->area.y;
            if (var->locked) {
                IconDraw(ICON_LOCK, (Vector2){ labelX + 6, labelY + m->small / 2 + 1 }, m->small, Fade(COL_GOLD, appear));
                labelX += m->small + 4;
            }
            UiText(gFonts.mono, slot->name, labelX, labelY, m->small, Fade(COL_DIM, appear));
        }

        int cells = var->kind == DEMO_POINTER || var->kind == DEMO_NULL ? 1 : var->cellCount;
        for (int c = 0; c < cells; c++) {
            Rectangle box = CellBox(slot, c, m, origin);
            Rectangle drawn = { box.x + box.width * (1 - scale) / 2, box.y + box.height * (1 - scale) / 2, box.width * scale,
                                box.height * scale };
            bool changed = !old || (var->kind != DEMO_POINTER && var->kind != DEMO_NULL
                                    && (c >= old->cellCount || strcmp(old->cells[c], var->cells[c]) != 0));
            float flash = old && changed ? 1 - Clamp01(age / FLASH_SECONDS) : 0;
            Color fill = ColorLerp(slot->block ? COL_NOTE_BG : COL_PANEL, COL_GOLD, flash * 0.55f);
            if (var->freed) fill = ColorLerp(COL_PANEL, COL_BAD, 0.25f);
            UiRoundRect(drawn, 6, Fade(fill, appear));
            Color outline = var->freed ? COL_BAD : slot->block ? Fade(COL_ACCENT, 0.6f) : Fade(COL_DIM, 0.35f);
            DrawRectangleRoundedLinesEx(drawn, 0.3f, 6, 1.5f, Fade(outline, appear));
            if (Looked(look, slot->name, c) || (look && strcmp(look, slot->name) == 0)) {
                Rectangle ring = { box.x - 3, box.y - 3, box.width + 6, box.height + 6 };
                DrawRectangleRoundedLinesEx(ring, 0.35f, 6, 2.5f, Fade(COL_GOLD, 0.55f + 0.45f * pulse));
            }
            if (var->kind == DEMO_POINTER || var->kind == DEMO_NULL) {
                DrawCircleV((Vector2){ box.x + box.width / 2, box.y + box.height / 2 }, 4 * scale,
                            Fade(var->kind == DEMO_NULL ? COL_DIM : COL_ACCENT, appear));
                continue;
            }
            const char *value = var->cells[c];
            bool unknown = strcmp(value, "?") == 0 || strcmp(value, "...") == 0;
            float rise = flash > 0 ? (1 - EaseOutCubic(Clamp01(age / 0.3f))) * 6 : 0;  // a new value drops in
            if (scale > 0.9f)
                UiTextCentered(gFonts.mono, value, box.x + box.width / 2, box.y + (box.height - valueSize) / 2 - 1 - rise,
                               valueSize, Fade(unknown ? COL_DIM : var->freed ? COL_BAD : COL_TEXT, appear));
            if (var->freed) {  // crossed out
                float cross = Clamp01(old && old->freed ? 1 : age / 0.35f);
                DrawLineEx((Vector2){ box.x + 4, box.y + 4 },
                           (Vector2){ box.x + 4 + (box.width - 8) * cross, box.y + 4 + (box.height - 8) * cross }, 2, COL_BAD);
            }
        }
    }

    // Arrows: drawn last so they cross over the boxes, each drawing itself to a new target.
    for (int s = 0; s < plan->memory.count; s++) {
        const Slot *slot = &plan->memory.slots[s];
        const DemoVar *var = VarNamed(vars, count, slot->name);
        if (!var || (var->kind != DEMO_POINTER && var->kind != DEMO_NULL)) continue;
        const DemoVar *old = VarNamed(before, beforeCount, slot->name);
        float progress = SameAim(var, old) ? 1 : Clamp01(age / ARROW_SECONDS);
        Rectangle from = CellBox(slot, 0, m, origin);
        if (var->kind == DEMO_NULL) {
            ArrowGroundDraw((Vector2){ from.x + from.width, from.y + from.height / 2 }, COL_DIM, progress);
            continue;
        }
        Slot *targetSlot = SlotAimedAt(&plan->memory, var->target);
        if (!targetSlot) continue;
        Rectangle to = CellBox(targetSlot, var->targetCell > 0 ? var->targetCell : 0, m, origin);
        Vector2 a, b, c, d;
        float fromX = from.x + from.width / 2, toX = to.x + to.width / 2;
        if (fabsf(to.y - from.y) < 4) {  // the same row: a loop underneath
            float depth = fminf(ARC_ROOM - 4, 14 + fabsf(toX - fromX) * 0.1f);
            a = (Vector2){ fromX, from.y + from.height };
            d = (Vector2){ toX, to.y + to.height };
            b = (Vector2){ a.x, a.y + depth * 1.3f };
            c = (Vector2){ d.x, d.y + depth * 1.3f };
        } else if (to.y > from.y) {  // down to a row below
            a = (Vector2){ fromX, from.y + from.height };
            d = (Vector2){ toX, to.y - 2 };
            b = (Vector2){ a.x, a.y + 26 };
            c = (Vector2){ d.x, d.y - 26 };
        } else {  // up to a row above
            a = (Vector2){ fromX, from.y };
            d = (Vector2){ toX, to.y + to.height + 2 };
            b = (Vector2){ a.x, a.y - 26 };
            c = (Vector2){ d.x, d.y + 26 };
        }
        ArrowCurveDraw(a, b, c, d, COL_ACCENT, true, progress, seconds);
    }
}

// ---------------------------------------------------------------------------
// Drawing: the output

// Draws text from (x, *y) on, a line per '\n', in colour; *x and *y end where it stops.
static void DrawRun(const char *text, int length, float left, float *x, float *y, float size, float lineH, Color color)
{
    char piece[LINE_CHARS];
    int start = 0;
    for (int i = 0; i <= length; i++) {
        if (i < length && text[i] != '\n') continue;
        int count = i - start < LINE_CHARS - 1 ? i - start : LINE_CHARS - 1;
        snprintf(piece, sizeof piece, "%.*s", count, text + start);
        UiText(gFonts.mono, piece, *x, *y, size, color);
        *x += UiTextWidth(gFonts.mono, piece, size);
        if (i < length) {  // a new line
            *x = left;
            *y += lineH;
        }
        start = i + 1;
    }
}

static void DrawOutput(const PieceDemo *demo, const Plan *plan, Rectangle area, int frame, float age, float seconds)
{
    const Metrics *m = &plan->m;
    UiRoundRect(area, 10, COL_CODE_BG);
    float left = area.x + PAD, x = left, y = area.y + PAD + (m->lineH - m->size) / 2;
    bool waiting = false;  // the program is waiting for the user to type
    for (int f = 0; f <= frame; f++) {
        const DemoFrame *shown = &demo->frames[f];
        float elapsed = f < frame ? 1e9f : age;
        if (shown->typed) {  // typed in, a letter at a time, in the accent colour
            int length = (int)strlen(shown->typed);
            int letters = (int)(Clamp01((elapsed - 0.15f) / (TYPING_SECONDS * 0.75f)) * length);
            waiting = letters < length;
            DrawRun(shown->typed, letters, left, &x, &y, m->size, m->lineH, COL_ACCENT);
        }
        if (shown->printed) {  // printed after any typing, quickly
            float from = shown->typed ? TYPING_SECONDS + 0.1f : 0.15f;
            int length = (int)strlen(shown->printed);
            int letters = (int)(Clamp01((elapsed - from) / 0.3f) * length);
            DrawRun(shown->printed, letters, left, &x, &y, m->size, m->lineH, COL_TEXT);
        }
    }
    // The caret: blinking, and steady while the program waits for typing.
    bool on = waiting || fmodf(seconds, 1.0f) < 0.5f;
    if (on) DrawRectangle((int)x + 1, (int)(y - 1), 2, (int)(m->size + 2), waiting ? COL_ACCENT : Fade(COL_DIM, 0.7f));
}

// ---------------------------------------------------------------------------
// The panel

PieceDemoViewAction PieceDemoViewDraw(const PieceDemo *demo, const char *piece, Rectangle box, float seconds)
{
    static Plan plan;
    MakePlan(demo, &plan);
    PieceDemoViewAction action = PIECE_DEMO_VIEW_NONE;
    float age;
    int frame = FrameAt(demo, seconds, &age);
    int frames = PieceDemoFrameCount(demo);
    bool ended = seconds > PieceDemoViewLength(demo);

    UiRoundRect((Rectangle){ box.x, box.y + 5, box.width, box.height }, 14, Fade(BLACK, 0.2f));
    UiRoundRect(box, 14, COL_PANEL);
    DrawRectangleRoundedLinesEx(box, 0.04f, 8, 2, COL_GOLD);

    // Header: the piece, a pip per line run, replay and close.
    float headerMid = box.y + HEADER_H / 2 + 2;
    IconDraw(ICON_STAR, (Vector2){ box.x + PAD + 10, headerMid }, 20, COL_GOLD);
    float chipW = UiTextWidth(gFonts.mono, piece, 18) + 22;
    Rectangle chip = { box.x + PAD + 26, headerMid - 15, chipW, 30 };
    UiRoundRect(chip, 8, COL_CODE_BG);
    DrawRectangleRoundedLinesEx(chip, 0.3f, 8, 1.5f, COL_GOLD);
    UiCodeLine(piece, chip.x + 11, headerMid - 18 * 0.5f, 18);

    Rectangle close = { box.x + box.width - PAD - 34, headerMid - 16, 34, 32 };
    Rectangle replay = { close.x - 44, close.y, 38, 32 };
    if (ended) {  // finished: replay beckons
        float pulse = (sinf(seconds * 5) + 1) / 2;
        DrawRectangleRoundedLinesEx((Rectangle){ replay.x - 3, replay.y - 3, replay.width + 6, replay.height + 6 }, 0.3f, 8,
                                    2, Fade(COL_ACCENT, 0.3f + 0.5f * pulse));
    }
    if (UiButtonIconEx(replay, ICON_RETRY, NULL, 18, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, true))
        action = PIECE_DEMO_VIEW_REPLAY;
    if (UiButtonIconEx(close, ICON_CROSS, NULL, 16, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true))
        action = PIECE_DEMO_VIEW_CLOSE;

    float pipsX = chip.x + chip.width + 22, pipsRight = replay.x - 16;
    float spacing = frames > 1 ? fminf(16, (pipsRight - pipsX) / frames) : 0;
    for (int i = 0; i < frames; i++) {
        Vector2 at = { pipsX + i * spacing + 4, headerMid };
        if (i == frame && !ended) DrawCircleV(at, 5.5f, COL_ACCENT);
        else if (i < frame || ended) DrawCircleV(at, 3.5f, Fade(COL_ACCENT, 0.6f));
        else DrawCircleLinesV(at, 3.5f, Fade(COL_DIM, 0.6f));
    }

    // Body: the code on the left; memory and the output on the right.
    float bodyY = box.y + HEADER_H, bodyH = box.height - HEADER_H - PAD;
    Rectangle code = { box.x + PAD, bodyY, plan.codeW, bodyH };
    float rightX = code.x + code.width + 12, rightW = box.x + box.width - PAD - rightX;
    float memoryH = plan.memory.count ? plan.memory.height + PAD * 2 : 0;
    Rectangle output = { rightX, bodyY + (memoryH ? memoryH + 10 : 0), rightW,
                         bodyH - (memoryH ? memoryH + 10 : 0) };
    DrawCode(demo, piece, &plan, code, frame, age, seconds, ended);
    if (memoryH) DrawMemory(demo, &plan, (Rectangle){ rightX, bodyY, rightW, memoryH }, frame, age, seconds);
    DrawOutput(demo, &plan, output, frame, age, seconds);
    return action;
}
