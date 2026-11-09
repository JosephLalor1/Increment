// The journey's background styles. See ambience.h.
//
// Every style but Particles is a "field": a function giving how strongly each character cell
// should show (0 to 1) at the current moment. The cell grid turns any field into soft,
// fizzling characters, so a new style only has to describe its shape.
#include "ambience.h"
#include "ui.h"

#include <math.h>
#include <string.h>

static const char glyphSet[] = "{}[]()<>*&=+-;:!?%#0123456789xyzpqint/|^~";
static const char *styleNames[AMBIENCE_STYLE_COUNT] = {
    "Off", "Particles", "Swirl", "Downpour", "Mandala", "Life", "Waves", "Fox",
};

static AmbienceStyle style = AMBIENCE_PARTICLES;
static float clock;  // seconds the background has been running
static unsigned randomState = 0x2545f491u;

static unsigned NextRandom(void)
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

static float Random01(void) { return (float)(NextRandom() % 10000) / 10000.0f; }
static float RandomBetween(float lo, float hi) { return lo + (hi - lo) * Random01(); }
static char RandomGlyph(void) { return glyphSet[NextRandom() % (sizeof glyphSet - 1)]; }
static float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

const char *AmbienceStyleName(AmbienceStyle s) { return s < AMBIENCE_STYLE_COUNT ? styleNames[s] : ""; }
AmbienceStyle AmbienceGetStyle(void) { return style; }

// ---------------------------------------------------------------------------
// Particles: single characters that appear, drift down a little and fade

#define PARTICLE_MAX 48

typedef struct {
    Vector2 at;
    float age, life;  // seconds
    float size;
    char glyph;
} Particle;

static Particle particles[PARTICLE_MAX];
static int particleCount;
static float particleDebt;  // fractional particles owed by the appear rate

static void UpdateParticles(Rectangle area, float dt)
{
    for (int i = 0; i < particleCount; i++) {
        particles[i].age += dt;
        particles[i].at.y += 8 * dt;  // the slowest of drifts, a hint of the rain
        if (particles[i].age >= particles[i].life) particles[i--] = particles[--particleCount];
    }
    particleDebt += dt * 7;  // characters appearing per second
    while (particleDebt >= 1) {
        particleDebt -= 1;
        if (particleCount == PARTICLE_MAX) continue;
        particles[particleCount++] = (Particle){
            { RandomBetween(area.x, area.x + area.width), RandomBetween(area.y, area.y + area.height) },
            0, RandomBetween(2.5f, 5.0f), RandomBetween(12, 17), RandomGlyph() };
    }
}

static void DrawParticles(void)
{
    for (int i = 0; i < particleCount; i++) {
        float fade = sinf(PI * particles[i].age / particles[i].life);  // in, then out again
        char glyph[2] = { particles[i].glyph, '\0' };
        DrawTextEx(gFonts.mono, glyph, particles[i].at, particles[i].size, 0, Fade(COL_ACCENT, 0.24f * fade));
    }
}

// ---------------------------------------------------------------------------
// The cell grid shared by the other styles

#define CELL       18.0f   // pixels per character cell
#define MAX_COLS   200
#define MAX_ROWS   120
#define CELL_ALPHA 0.24f   // the strongest a cell shows: as faint as the particles

typedef struct {
    float shown;     // current strength, easing towards the field's value
    float pace;      // how quickly this cell eases (each cell differs, so shapes fizzle)
    float shimmer;   // a slow per-cell flicker so even steady shapes breathe
    char glyph;
} Cell;

static Cell cells[MAX_ROWS][MAX_COLS];
static int cols, rows;
static Rectangle gridArea;

static void ResetGrid(Rectangle area)
{
    gridArea = area;
    cols = (int)(area.width / CELL) + 1;
    rows = (int)(area.height / CELL) + 1;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) cells[r][c] = (Cell){ 0, RandomBetween(1.2f, 4.5f), Random01(), RandomGlyph() };
    }
}

// ---------------------------------------------------------------------------
// Fields: how strongly each cell shows. u and v are the cell's position across the area,
// 0 to 1; aspect is the area's width over its height, so shapes aren't stretched.

static float Smooth(float edge0, float edge1, float x)
{
    float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3 - 2 * t);
}

static float SwirlField(float u, float v, float aspect)
{
    float x = (u - 0.5f) * aspect, y = v - 0.5f;
    float radius = sqrtf(x * x + y * y), angle = atan2f(y, x);
    float arms = cosf(3 * angle - radius * 14 + clock * 0.7f);  // three arms winding outwards
    float arm = powf(fmaxf(arms, 0), 5);
    return arm * Smooth(0.03f, 0.12f, radius) * (1 - Smooth(0.45f, 0.75f, radius));
}

static float DownpourField(int col, int row)
{
    // Each column has its own speed and phase, from a hash of its number, so the streaks
    // never line up; a long tail fades behind each head.
    unsigned h = (unsigned)col * 2654435761u;
    float speed = 0.35f + (h % 1000) / 1000.0f * 0.6f;   // screens per second
    float phase = ((h >> 10) % 1000) / 1000.0f;
    float head = fmodf(clock * speed + phase, 1.4f) - 0.2f;  // a pause above the top between drops
    float behind = head - (float)row / rows;
    float tail = 0.28f + ((h >> 20) % 100) / 100.0f * 0.25f;
    if (behind < 0 || behind > tail) return 0;
    return 1 - behind / tail;
}

// How close value is to target, as a soft line: 1 on it, falling to 0 about width away.
static float Line(float value, float target, float width)
{
    float d = (value - target) / width;
    return expf(-d * d);
}

// A mandala woven into a spider's web, centred on the area and always moving: ten spokes
// turning slowly, web threads strung straight between them that drift outwards and fade at
// the rim, a five-petal flower turning one way, a ring of ten smaller petals turning the
// other, and a beating heart. Worked in character cells (cells are square), since the
// characters are its resolution: lines are about one cell wide, and the whole thing is
// large enough for its parts to stay apart.
static float MandalaField(int col, int row)
{
    const int spokes = 10;
    const float sector = 2 * PI / spokes, thin = 0.6f;   // line half-width, in cells
    const float rim = rows * 0.75f;                      // the outer radius, in cells
    float x = col + 0.5f - cols / 2.0f, y = row + 0.5f - rows / 2.0f;
    float radius = sqrtf(x * x + y * y), angle = atan2f(y, x);
    if (radius > rim * 1.05f) return 0;
    float r = radius / rim;  // 0 at the centre, 1 at the rim

    // Where this cell sits within its spoke's sector (from the sector's middle).
    float turned = angle - clock * 0.06f;
    float local = fmodf(turned + 100 * PI, sector) - sector / 2;
    float fromSpoke = (sector / 2 - fabsf(local)) * radius;  // cells to the nearest spoke

    float spoke = Line(fromSpoke, 0, thin) * Smooth(0.1f, 0.16f, r) * (1 - Smooth(0.9f, 1.0f, r));

    // Web threads: straight chords between neighbouring spokes. A point on the chord at
    // distance d from the centre satisfies radius x cos(local) = d x cos(sector / 2). They
    // drift slowly outwards, appearing in the middle of the web and fading at the rim.
    float chord = radius * cosf(local) / cosf(sector / 2) / rim;
    const float threadGap = 0.13f;
    float drift = fmodf(clock * 0.02f, threadGap);
    float nearest = roundf((chord - drift) / threadGap) * threadGap + drift;
    float thread = Line(chord * rim, nearest * rim, thin) * Smooth(0.42f, 0.52f, nearest) * (1 - Smooth(0.85f, 1.0f, nearest));

    // Petal outlines (radius = middle + depth x cos(petals x angle)), counter-rotating.
    float flower = 0.22f + 0.08f * cosf(5 * (angle + clock * 0.18f));
    float ring = 0.38f + 0.035f * cosf(10 * (angle - clock * 0.12f));
    float petals = fmaxf(Line(radius, flower * rim, thin), 0.85f * Line(radius, ring * rim, thin));
    float heart = Line(radius, (0.07f + 0.015f * sinf(clock * 1.3f)) * rim, thin);

    return fmaxf(fmaxf(spoke * 0.75f, thread * 0.8f), fmaxf(petals, heart));
}

static float WavesField(float u, float v)
{
    float strongest = 0;
    for (int k = 0; k < 3; k++) {
        float middle = 0.25f + 0.25f * k;
        float crest = middle + 0.07f * sinf(u * (7 + k * 2) + clock * (0.6f + 0.25f * k) + k * 2);
        float distance = fabsf(v - crest);
        strongest = fmaxf(strongest, expf(-distance * distance / 0.0009f));
    }
    return strongest;
}

// Conway's Game of Life on the cell grid. Each generation lasts LIFE_STEP seconds; a board
// that dies down or stops changing is sprinkled with fresh life.
#define LIFE_STEP 0.45f

static unsigned char life[MAX_ROWS][MAX_COLS], nextLife[MAX_ROWS][MAX_COLS];
static float lifeTimer;
static int lifeQuiet;  // generations in a row with almost no change

static void SeedLife(float density)
{
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (Random01() < density) life[r][c] = 1;
}

static void StepLife(void)
{
    int population = 0, changes = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int neighbours = 0;
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++)
                    if (dr || dc) neighbours += life[(r + dr + rows) % rows][(c + dc + cols) % cols];  // wraps around
            nextLife[r][c] = neighbours == 3 || (life[r][c] && neighbours == 2);
            population += nextLife[r][c];
            changes += nextLife[r][c] != life[r][c];
        }
    }
    memcpy(life, nextLife, sizeof life);
    lifeQuiet = changes < cols * rows / 400 + 2 ? lifeQuiet + 1 : 0;
    if (population < cols * rows / 25 || lifeQuiet > 12) {
        SeedLife(0.12f);
        lifeQuiet = 0;
    }
}

// The mascot's face: ears and a triangular face, with the eyes and nose left empty, on a
// canvas from -0.5 to 0.5. Characters along the outline show more strongly than the inside.
static bool InTriangle(float x, float y, float ax, float ay, float bx, float by, float cx, float cy)
{
    float d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by);
    float d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy);
    float d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay);
    bool negative = d1 < 0 || d2 < 0 || d3 < 0, positive = d1 > 0 || d2 > 0 || d3 > 0;
    return !(negative && positive);
}

static bool InFox(float x, float y)
{
    bool face = InTriangle(x, y, -0.46f, -0.12f, 0.46f, -0.12f, 0, 0.46f);
    bool leftEar = InTriangle(x, y, -0.46f, -0.12f, -0.36f, -0.48f, -0.08f, -0.12f);
    bool rightEar = InTriangle(x, y, 0.46f, -0.12f, 0.36f, -0.48f, 0.08f, -0.12f);
    if (!face && !leftEar && !rightEar) return false;
    float ex = fabsf(x) - 0.16f, ey = y - 0.03f;  // eyes, either side of the middle
    float ny = y - 0.3f;                          // the nose, near the chin
    return ex * ex + ey * ey > 0.0036f && x * x + ny * ny > 0.0030f;
}

static float FoxField(int col, int row)
{
    float size = fminf(rows * 0.85f, cols * 0.55f);  // in cells: as large as fits comfortably
    float x0 = cols / 2.0f, y0 = rows * 0.5f;
    float x = (col - x0) / size, y = (row - y0) / size * 0.95f;
    if (!InFox(x, y)) return 0;
    float step = 1 / size;  // one cell, in canvas units
    bool edge = !InFox(x + step, y) || !InFox(x - step, y) || !InFox(x, y + step) || !InFox(x, y - step);
    float breath = 0.85f + 0.15f * sinf(clock * 0.9f);
    return (edge ? 1.0f : 0.38f) * breath;
}

static float Field(int col, int row)
{
    float u = (col + 0.5f) / cols, v = (row + 0.5f) / rows;
    float aspect = gridArea.height > 0 ? gridArea.width / gridArea.height : 1;
    switch (style) {
    case AMBIENCE_SWIRL:    return SwirlField(u, v, aspect);
    case AMBIENCE_DOWNPOUR: return DownpourField(col, row);
    case AMBIENCE_MANDALA:  return MandalaField(col, row);
    case AMBIENCE_LIFE:     return life[row][col] ? 1 : 0;
    case AMBIENCE_WAVES:    return WavesField(u, v);
    case AMBIENCE_FOX:      return FoxField(col, row);
    default:                return 0;
    }
}

static void UpdateCells(Rectangle area, float dt)
{
    if (area.width != gridArea.width || area.height != gridArea.height || cols == 0) {
        ResetGrid(area);
        memset(life, 0, sizeof life);
        SeedLife(0.25f);
    }
    gridArea = area;
    if (style == AMBIENCE_LIFE) {
        lifeTimer += dt;
        while (lifeTimer >= LIFE_STEP) {
            lifeTimer -= LIFE_STEP;
            StepLife();
        }
    }
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Cell *cell = &cells[r][c];
            cell->shimmer = Clamp01(cell->shimmer + RandomBetween(-1, 1) * dt * 1.5f);
            float target = Field(c, r) * (0.7f + 0.3f * cell->shimmer);
            float change = (target - cell->shown) * fminf(1, dt * cell->pace);
            cell->shown += change;
            // The fizzle: a cell that's appearing or fading swaps characters rapidly; a
            // settled one only now and then.
            float changing = dt > 0 ? fabsf(change) / dt : 0;  // strength gained or lost per second
            float swapsPerSecond = 0.4f + 25 * changing;
            if (Random01() < swapsPerSecond * dt) cell->glyph = RandomGlyph();
        }
    }
}

static void DrawCells(void)
{
    char glyph[2] = { 0, 0 };
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Cell *cell = &cells[r][c];
            if (cell->shown < 0.03f) continue;
            glyph[0] = cell->glyph;
            Vector2 at = { gridArea.x + c * CELL + 2, gridArea.y + r * CELL };
            DrawTextEx(gFonts.mono, glyph, at, CELL * 0.82f, 0, Fade(COL_ACCENT, CELL_ALPHA * cell->shown));
        }
    }
}

// ---------------------------------------------------------------------------

void AmbienceSetStyle(AmbienceStyle newStyle)
{
    if (newStyle < 0 || newStyle >= AMBIENCE_STYLE_COUNT) newStyle = AMBIENCE_PARTICLES;
    if (newStyle == style) return;
    style = newStyle;
    // Let the new style fizzle in from nothing rather than replace the old one at once.
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) cells[r][c].shown = 0;
    particleCount = 0;
}

void AmbienceUpdate(Rectangle area, float dt)
{
    clock += dt;
    if (style == AMBIENCE_OFF) return;
    if (style == AMBIENCE_PARTICLES) UpdateParticles(area, dt);
    else UpdateCells(area, dt);
}

void AmbienceDraw(void)
{
    if (style == AMBIENCE_PARTICLES) DrawParticles();
    else if (style != AMBIENCE_OFF) DrawCells();
}

void AmbienceWarm(Rectangle area)
{
    for (int frame = 0; frame < 600; frame++) AmbienceUpdate(area, 1 / 60.0f);
}
