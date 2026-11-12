// The achievement animation after an answer. See skill_reward.h.
//
// Two small particle lists: badges (one per skill, stacked up from the bottom-right corner)
// and sparks (glowing characters flying from the answer to a badge). Each spark knows its
// badge by id, since badges come and go; when it lands, the badge's ring moves a step
// towards the skill's new XP.
#include "app/skill_reward.h"
#include "ui/easing.h"
#include "core/random_stream.h"
#include "ui/skill_icons.h"
#include "ui/ui.h"

#include <math.h>
#include <stdio.h>

#define MAX_BADGES   5
#define MAX_SPARKS   96
#define CARD_W       236.0f
#define CARD_H       62.0f
#define CARD_GAP     10.0f
#define MARGIN       28.0f
#define RING_R       23.0f    // the level ring's outer radius
#define POP_SECONDS  0.35f    // a badge popping in
#define HOLD_SECONDS 1.6f     // showing after its last spark lands
#define LEAVE_SECONDS 0.3f    // slipping away

typedef struct {
    int id;
    Skill skill;
    bool correct;          // the latest answer: tints its sparks and XP text
    float xpStart;         // XP when the badge appeared (its "+n XP" counts from here)
    float xpFrom, xpTo;    // this answer's change, which the landing sparks step through
    float xpShown;         // what the ring shows, easing after the landed share
    int sparksSent, sparksLanded;
    float age;             // seconds since it popped in
    float idle;            // seconds since its last spark landed
    float leave;           // 0 showing, rising to 1 as it slips away
    float pulse;           // 1 when a spark lands, fading
    float levelFlash;      // 1 on a level up, fading
    int levelUp;           // the level it reached, shown while flashing (0: none)
    float slotY;           // the card's top, easing towards its slot
} Badge;

typedef struct {
    int badge;             // the target badge's id
    Vector2 from, bend;    // start and the curve's control point (screen)
    float delay, age, duration;
    float size;
    bool correct;
    char glyph;
} Spark;

static const char glyphSet[] = "{}[]()<>*&=+;:01xp#%";

static Badge badges[MAX_BADGES];
static int badgeCount;
static Spark sparks[MAX_SPARKS];
static int sparkCount;
static int nextBadgeId = 1;
static bool frozen;  // --shot: hold the demo still
static RandomStream randomState = 0x2f6b91c3u;

// Overshoots a little before settling at 1: the "pop".
static float BackOut(float t)
{
    t = Clamp01(t) - 1;
    return 1 + t * t * (2.7f * t + 1.7f);
}

// How far through its current level an XP total is, 0 to 1.
static float LevelShare(float xp)
{
    int level = SkillLevel(xp);
    float from = SkillXpForLevel(level), to = SkillXpForLevel(level + 1);
    return to > from ? Clamp01((xp - from) / (to - from)) : 0;
}

static Badge *BadgeById(int id)
{
    for (int i = 0; i < badgeCount; i++) if (badges[i].id == id) return &badges[i];
    return NULL;
}

// The card's top for a slot: slot 0 is the bottom one.
static float SlotTop(int slot) { return GetScreenHeight() - MARGIN - CARD_H - slot * (CARD_H + CARD_GAP); }

// How far the card has slid in: 0 off screen to the right, 1 in place.
static float SlideIn(const Badge *b)
{
    return EaseOutCubic(b->age / (POP_SECONDS * 0.8f)) * (1 - b->leave * b->leave);
}

static Rectangle CardRect(const Badge *b)
{
    float x = GetScreenWidth() - MARGIN - CARD_W;
    x += (1 - SlideIn(b)) * (CARD_W + MARGIN + 8);
    return (Rectangle){ x, b->slotY, CARD_W, CARD_H };
}

static Vector2 RingCentre(const Badge *b)
{
    Rectangle card = CardRect(b);
    return (Vector2){ card.x + 12 + RING_R, card.y + CARD_H / 2 };
}

static void RemoveBadge(int index)
{
    for (int i = index + 1; i < badgeCount; i++) badges[i - 1] = badges[i];
    badgeCount--;
}

// The badge for a skill: the one still showing, or a new one (the oldest makes room).
static Badge *BadgeFor(Skill skill, float xp)
{
    for (int i = 0; i < badgeCount; i++) {
        Badge *b = &badges[i];
        if (b->skill != skill) continue;
        b->leave = 0;  // coming back if it was slipping away
        return b;
    }
    if (badgeCount == MAX_BADGES) RemoveBadge(0);
    Badge *b = &badges[badgeCount];
    *b = (Badge){ .id = nextBadgeId++, .skill = skill, .xpStart = xp, .xpFrom = xp, .xpTo = xp, .xpShown = xp };
    b->slotY = SlotTop(badgeCount);
    badgeCount++;
    return b;
}

static void AddSpark(const Badge *b, Vector2 from, float delay, bool correct)
{
    if (sparkCount == MAX_SPARKS) return;
    // Burst outwards in a random direction first, then curve in to the badge.
    float angle = RandomBetween(&randomState, 0, 2 * PI), reach = RandomBetween(&randomState, 60, 150);
    Vector2 start = { from.x + RandomBetween(&randomState, -14, 14), from.y + RandomBetween(&randomState, -10, 10) };
    Vector2 bend = { start.x + cosf(angle) * reach, start.y + sinf(angle) * reach };
    sparks[sparkCount++] = (Spark){ b->id, start, bend, delay, 0, RandomBetween(&randomState, 0.55f, 0.85f),
                                    RandomBetween(&randomState, 14, 19), correct, RandomChar(&randomState, glyphSet) };
}

void SkillRewardShow(Vector2 from, const SkillGains *gains)
{
    if (!gains || gains->count <= 0) return;
    frozen = false;
    for (int i = 0; i < gains->count; i++) {
        const SkillGain *gain = &gains->items[i];
        if (gain->skill >= SKILL_COUNT) continue;
        Badge *b = BadgeFor(gain->skill, gain->xpBefore);
        b->correct = gains->correct;
        b->xpFrom = b->xpShown;
        b->xpTo = gain->xpAfter;
        b->idle = 0;
        // More XP, more sparks; a wrong answer sends a few.
        float change = fabsf(gain->xpAfter - gain->xpBefore);
        int count = gains->correct ? 4 + (int)(change / 2) : 3 + (int)(change / 3);
        int most = gains->correct ? 14 : 6;
        if (count > most) count = most;
        b->sparksSent = count;
        b->sparksLanded = 0;
        for (int s = 0; s < count; s++) AddSpark(b, from, i * 0.08f + s * 0.035f + RandomBetween(&randomState, 0, 0.06f), gains->correct);
    }
}

Vector2 SkillRewardOrigin(void)
{
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return GetMousePosition();
    return (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
}

// Where a spark is, `lag` seconds behind its current age (for its fading tail).
static bool SparkAt(const Spark *s, float lag, Vector2 *at)
{
    const Badge *b = BadgeById(s->badge);
    if (!b) return false;
    float t = Clamp01((s->age - s->delay - lag) / s->duration);
    if (s->age - s->delay - lag < 0) return false;
    float e = t * t;  // speeding up as it nears the badge, like it's being drawn in
    Vector2 to = RingCentre(b);
    float a = (1 - e) * (1 - e), m = 2 * (1 - e) * e, c = e * e;
    *at = (Vector2){ a * s->from.x + m * s->bend.x + c * to.x, a * s->from.y + m * s->bend.y + c * to.y };
    return true;
}

void SkillRewardUpdate(float dt)
{
    if (frozen || dt <= 0) return;

    for (int i = 0; i < sparkCount; i++) {
        Spark *s = &sparks[i];
        s->age += dt;
        if ((RandomNext(&randomState) % 1000) / 1000.0f < 6 * dt) s->glyph = RandomChar(&randomState, glyphSet);  // a flicker of new characters
        Badge *b = BadgeById(s->badge);
        bool landed = s->age >= s->delay + s->duration;
        if (b && landed) {
            b->sparksLanded++;
            b->pulse = 1;
            b->idle = 0;
        }
        if (!b || landed) sparks[i--] = sparks[--sparkCount];
    }

    for (int i = 0; i < badgeCount; i++) {
        Badge *b = &badges[i];
        b->age += dt;
        b->pulse = fmaxf(0, b->pulse - dt * 3);
        b->levelFlash = fmaxf(0, b->levelFlash - dt * 0.9f);
        if (b->levelFlash <= 0) b->levelUp = 0;

        // The ring follows the share of sparks that have landed.
        float landed = b->sparksSent > 0 ? (float)b->sparksLanded / b->sparksSent : 1;
        float target = b->xpFrom + (b->xpTo - b->xpFrom) * Clamp01(landed);
        int levelBefore = SkillLevel(b->xpShown);
        b->xpShown += (target - b->xpShown) * (1 - expf(-10 * dt));
        if (fabsf(target - b->xpShown) < 0.01f) b->xpShown = target;
        int levelNow = SkillLevel(b->xpShown);
        if (levelNow > levelBefore) {
            b->levelFlash = 1;
            b->levelUp = levelNow;
        }

        b->slotY += (SlotTop(i) - b->slotY) * (1 - expf(-14 * dt));
        if (b->sparksLanded >= b->sparksSent) b->idle += dt;
        if (b->idle > HOLD_SECONDS + (b->levelUp ? 0.8f : 0)) b->leave += dt / LEAVE_SECONDS;
        if (b->leave >= 1) RemoveBadge(i--);
    }
}

static void DrawSpark(const Spark *s)
{
    Color tint = s->correct ? COL_ACCENT : COL_BAD;
    char glyph[2] = { s->glyph, 0 };
    // A short fading tail, then the glowing character.
    for (int ghost = 3; ghost >= 0; ghost--) {
        Vector2 at;
        if (!SparkAt(s, ghost * 0.03f, &at)) continue;
        float t = Clamp01((s->age - s->delay) / s->duration);
        float appear = Clamp01((s->age - s->delay) / 0.08f);
        float alpha = appear * (1 - 0.22f * ghost) * (0.55f + 0.45f * (1 - t));
        float size = s->size * (1 - 0.12f * ghost) * (1 - 0.35f * t);
        if (ghost == 0) {
            DrawCircleV(at, size * 0.95f, Fade(tint, 0.10f * alpha));
            DrawCircleV(at, size * 0.5f, Fade(tint, 0.22f * alpha));
        }
        Vector2 corner = { at.x - size * 0.3f, at.y - size * 0.5f };
        DrawTextEx(gFonts.mono, glyph, corner, size, 0, Fade(ghost == 0 ? COL_ACCENT_HI : tint, alpha * (ghost ? 0.5f : 1)));
    }
}

static void DrawBadge(const Badge *b)
{
    Rectangle card = CardRect(b);
    float fade = 1 - b->leave;
    float pop = BackOut(b->age / POP_SECONDS) * (1 - b->leave);
    Vector2 centre = RingCentre(b);
    bool levelling = b->levelUp > 0;
    Color ringColour = levelling ? COL_GOLD : COL_ACCENT;

    UiRoundRect(card, 14, Fade(COL_PANEL, 0.96f * fade));
    DrawRectangleRoundedLinesEx(card, 28 / CARD_H, 10, 1.5f, Fade(levelling ? COL_GOLD : COL_PANEL_HI, fade));

    // The ring: a glow that swells as sparks land, the track, then the level share in colour.
    float r = RING_R * pop;
    if (r > 1) {
        DrawCircleV(centre, r + 3 + 4 * b->pulse, Fade(ringColour, (0.06f + 0.2f * b->pulse) * fade));
        DrawCircleV(centre, r, Fade(COL_BG, fade));
        DrawRing(centre, r - 4, r, 0, 360, 48, Fade(COL_PANEL_HI, fade));
        float share = LevelShare(b->xpShown);
        if (share > 0) DrawRing(centre, r - 4.5f, r + 0.5f, -90, -90 + 360 * share, 48, Fade(ringColour, fade));
        SkillIconDraw(b->skill, centre, r * 1.05f, Fade(b->pulse > 0.5f ? ringColour : COL_TEXT, fade));
    }
    // A level up sends a gold ring out from the badge.
    if (levelling) {
        float spread = 1 - b->levelFlash;
        DrawRing(centre, RING_R + 4 + spread * 26, RING_R + 6 + spread * 26, 0, 360, 48,
                 Fade(COL_GOLD, b->levelFlash * fade));
    }

    float textX = centre.x + RING_R + 16, size = 16;
    UiText(gFonts.bold, SkillTitle(b->skill), textX, card.y + 11, size, Fade(COL_TEXT, fade));
    char line[48];
    float change = b->xpShown - b->xpStart;
    Color changeColour = change < -0.5f ? COL_BAD : b->correct ? COL_GOOD : COL_DIM;
    snprintf(line, sizeof line, "%+.0f XP", change);
    UiText(gFonts.bold, line, textX, card.y + 33, 15, Fade(changeColour, fade));
    float after = UiTextWidth(gFonts.bold, line, 15) + 10;
    if (levelling) snprintf(line, sizeof line, "Level up! %d", b->levelUp);
    else snprintf(line, sizeof line, "Level %d", SkillLevel(b->xpShown));
    UiText(levelling ? gFonts.bold : gFonts.text, line, textX + after, card.y + 33, 15,
           Fade(levelling ? COL_GOLD : COL_DIM, fade));
}

void SkillRewardDraw(void)
{
    for (int i = 0; i < badgeCount; i++) DrawBadge(&badges[i]);
    for (int i = 0; i < sparkCount; i++) DrawSpark(&sparks[i]);
}

void SkillRewardClear(void)
{
    badgeCount = sparkCount = 0;
    frozen = false;
}

void SkillRewardDemo(float seconds)
{
    SkillRewardClear();
    SkillGains gains = { .count = 2, .correct = true };
    gains.items[0] = (SkillGain){ SKILL_TRACING, 118, 136 };
    gains.items[1] = (SkillGain){ SKILL_POINTERS, 41, 55 };  // crosses level 1 at 50 XP
    SkillRewardShow((Vector2){ GetScreenWidth() * 0.45f, GetScreenHeight() * 0.5f }, &gains);
    for (float t = 0; t < seconds; t += 1 / 60.0f) SkillRewardUpdate(1 / 60.0f);
    frozen = true;
}
