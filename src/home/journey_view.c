// The journey: a winding path of a track's levels. See journey_view.h.
#include "home/journey_view.h"
#include "ui/mascot.h"
#include "learning/progress.h"
#include "learning/roadmap.h"
#include "ui/ui.h"

#include <math.h>
#include <string.h>

// Four beats repeat: centre, right, centre, left. Vertical spacing follows the text.
static Vector2 Station(float x, float y, float width, int number)
{
    static const float sway[] = { 0, 1, 0, -1 };
    return (Vector2){ x + width / 2 + sway[number % 4] * width * 0.25f, y };
}

static float StationStep(const Lesson *lesson, float scale)
{
    float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, 264, 20 * scale, COL_TEXT, false);
    float h = 100 * scale + titleH + 48;
    return h > 190 * scale ? h : 190 * scale;
}

float JourneyViewFocus(const LessonSet *set, Track track)
{
    int done, total;
    int next = RoadmapNext(set, track, &done, &total);
    if (next < 0) return 0;
    float offset = 0;
    for (int i = 0; i < next; i++)
        if (set->items[i].track == track) offset += StationStep(&set->items[i], UiTextScale());
    return offset;
}

// Cubic segments have vertical tangents at each station, making a continuous winding trail.
static void Trail(Vector2 a, Vector2 b, bool complete)
{
    Vector2 previous = a;
    float middle = (a.y + b.y) / 2;
    for (int i = 1; i <= 36; i++) {
        float t = i / 36.0f, u = 1 - t;
        Vector2 point = {
            u*u*u*a.x + 3*u*u*t*a.x + 3*u*t*t*b.x + t*t*t*b.x,
            u*u*u*a.y + 3*u*u*t*middle + 3*u*t*t*middle + t*t*t*b.y
        };
        DrawLineEx(previous, point, 14, COL_PANEL_HI);
        DrawLineEx(previous, point, 7, Fade(COL_ACCENT, complete ? 0.7f : 0.22f));  // walked: a stronger accent
        previous = point;
    }
}

static void Guide(Vector2 station, float x, float width, float scale, int done, int total)
{
    float size = 94 * scale;
    // Keep the fox and speech bubble on the opposite side of an off-centre station.
    bool left = station.x > x + width * 0.6f;
    float guideX = left ? station.x - 230 : station.x + 108;
    float bubbleW = 176;
    float bubbleX = left ? guideX - bubbleW - 12 : guideX + size + 12;
    FoxEmotion emotion = done == 0 ? FOX_HAPPY : done == total - 1 ? FOX_DETERMINED
                          : done > total / 2 ? FOX_LOVE_LEARNING : FOX_CODING;
    const char *message = MascotMessage(FOX_JOURNEY, done == 0 ? "Welcome to your coding adventure!"
                        : done == total - 1 ? "You're almost there!" : "Let's tackle the next level!");
    float textH = UiWrappedText(gFonts.text, message, (Vector2){ 0, 0 }, bubbleW - 24, 16 * scale, COL_TEXT, false);
    Rectangle bubble = { bubbleX, station.y - (textH + 22) / 2, bubbleW, textH + 22 };
    UiRoundRect(bubble, 12, COL_PANEL);
    UiWrappedText(gFonts.text, message, (Vector2){ bubble.x + 12, bubble.y + 11 }, bubbleW - 24, 16 * scale, COL_TEXT, true);
    Rectangle fox = { guideX, station.y - size / 2 + 2, size, size };
    MascotDrawActor(FOX_JOURNEY, fox, emotion);
    MascotOfferTip(FOX_JOURNEY, fox);  // click the fox for a learning tip
}

// How hovered each station is, from 0 (not at all) to 1 (fully), by lesson index. It eases
// towards the mouse's actual state over a few frames, so lifting and highlighting glide
// instead of snapping.
#define MAX_HOVER_STATIONS 512
static float hoverAmount[MAX_HOVER_STATIONS];

static float EaseHover(int lessonIndex, bool over)
{
    if (lessonIndex < 0 || lessonIndex >= MAX_HOVER_STATIONS) return over ? 1.0f : 0.0f;
    float *amount = &hoverAmount[lessonIndex];
    float step = 1.0f - expf(-GetFrameTime() * 14.0f);  // same feel at any frame rate
    *amount += ((over ? 1.0f : 0.0f) - *amount) * step;
    if (fabsf(*amount - (over ? 1.0f : 0.0f)) < 0.001f) *amount = over ? 1.0f : 0.0f;
    return *amount;
}

// Where each station's middle was in the last draw, by lesson index.
static Vector2 stationPoint[MAX_HOVER_STATIONS];
static bool stationDrawn[MAX_HOVER_STATIONS];

bool JourneyViewStationPoint(int lessonIndex, Vector2 *point)
{
    if (lessonIndex < 0 || lessonIndex >= MAX_HOVER_STATIONS || !stationDrawn[lessonIndex]) return false;
    *point = stationPoint[lessonIndex];
    return true;
}

static int demoClick = -1;  // --shot: a level to report as clicked on the next draw

void JourneyViewDemoClick(int lessonIndex) { demoClick = lessonIndex; }

int JourneyViewDraw(const LessonSet *set, Track track, float x, float y, float width, float *height,
                    Vector2 *clickedAt)
{
    float top = y, scale = UiTextScale();
    int done, total, selected = -1;
    int next = RoadmapNext(set, track, &done, &total);
    memset(stationDrawn, 0, sizeof stationDrawn);
    int stations = 0;  // every level of the track, written yet or not
    for (int i = 0; i < set->count; i++) stations += set->items[i].track == track;
    if (!stations) {
        *height = UiWrappedText(gFonts.text, "This journey is waiting for its first levels.", (Vector2){ x, y },
                                width, 24 * scale, COL_DIM, true) + 48;
        return -1;
    }
    if (next < 0 && total > 0) {
        y += MascotCompanionActor(FOX_JOURNEY, x + 60, y, width - 120, 84, 21 * scale,
                                  FOX_LOVE_LEARNING, "You're doing amazing! Journey complete.") + 24;
    }
    float firstY = y + 78 * scale;
    float stationY = firstY;
    int ordinal = 0;
    Vector2 previous = { 0 };
    bool previousPassed = false;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        Vector2 point = Station(x, stationY, width, ordinal);
        bool passed = RoadmapLevelComplete(lesson);
        if (ordinal) Trail(previous, point, previousPassed && passed);
        previous = point;
        previousPassed = passed;
        stationY += StationStep(lesson, scale);
        ordinal++;
    }

    stationY = firstY;
    ordinal = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        Vector2 center = Station(x, stationY, width, ordinal);
        float step = StationStep(lesson, scale), radius = 43 * scale;
        bool ready = RoadmapLevelReady(lesson);  // levels not written yet show, but can't be opened
        int stepsDone = RoadmapStepsDone(lesson);  // looked up once: each call searches the saved progress
        bool passed = ready && stepsDone == lesson->stepCount, current = i == next;
        bool gold = ProgressGold(lesson->id);  // won the level's speed round
        Rectangle hit = { center.x - radius - 8, center.y - radius - 12, radius * 2 + 16, radius * 2 + 24 };
        float titleY = center.y + radius + 18;
        float titleH = UiWrappedText(gFonts.text, lesson->title, (Vector2){ 0, 0 }, 264, 20 * scale, COL_TEXT, false);
        Rectangle titleHit = { center.x - 144, titleY - 8, 288, titleH + 34 };
        bool over = ready && ((UiMouseOver(hit) && CheckCollisionPointCircle(GetMousePosition(), center, radius + 8))
                              || UiMouseOver(titleHit));
        if (over) UiWantHand();
        float hover = EaseHover(i, over);
        float restingLift = current ? 2 + sinf((float)GetTime() * 2.5f) * 2 : 0;  // the current station bobs
        float lift = restingLift + (6 - restingLift) * hover;
        radius *= 1 + 0.04f * hover;  // a slight swell as it lifts
        Vector2 face = { center.x, center.y - lift };
        if (i < MAX_HOVER_STATIONS) {
            stationPoint[i] = face;
            stationDrawn[i] = true;
        }

        // Shades of the theme's accent: solid accent for the next level, a soft accent tint
        // with an accent tick for completed ones (distinct in both themes), grey for the rest.
        Color currentColor = COL_ACCENT;
        Color doneColor = ColorLerp(COL_BG, COL_ACCENT, 0.3f);
        Color doneLabel = ColorLerp(COL_ACCENT, COL_DIM, 0.45f);  // softer than the next lesson's
        Color fill = passed ? doneColor : current ? currentColor : COL_PANEL_HI;
        Color ink = passed ? COL_ACCENT : current ? COL_ON_ACCENT : COL_DIM;
        if (current) {
            float pulse = (sinf((float)GetTime() * 2.5f) + 1) / 2;
            DrawCircleV(center, radius + 14 + pulse * 3, Fade(currentColor, 0.10f + pulse * 0.05f));
            DrawRing(center, radius + 7, radius + 9, 0, 360, 64, Fade(currentColor, 0.55f));
        }
        DrawCircleV((Vector2){ center.x, center.y + 9 }, radius + 3, COL_BG);
        DrawCircleV((Vector2){ center.x, center.y + 6 }, radius, passed ? ColorBrightness(doneColor, -0.3f)
                                                                         : current ? ColorBrightness(currentColor, -0.25f) : COL_PANEL);
        DrawCircleV(face, radius, ColorBrightness(fill, 0.08f * hover));
        if (gold) DrawRing(face, radius + 3, radius + 8, 0, 360, 64, COL_GOLD);
        // A level under way: an arc round the station fills with its passed steps.
        if (!gold && !passed && stepsDone > 0) {
            float share = (float)stepsDone / lesson->stepCount;
            DrawRing(face, radius + 3, radius + 7, -90, 270, 64, Fade(COL_ACCENT, 0.18f));
            DrawRing(face, radius + 3, radius + 7, -90, -90 + 360 * share, 64, COL_ACCENT);
        }
        DrawCircleV((Vector2){ face.x - radius * 0.2f, face.y - radius * 0.35f }, radius * 0.48f, Fade(WHITE, passed || current ? 0.09f : 0.025f));
        IconDraw(passed ? ICON_CHECK : !ready ? ICON_LOCK : current ? ICON_PLAY : ICON_CODE,
                 (Vector2){ face.x, face.y - (current ? 9 : 0) }, 32 * scale, Fade(ink, ready ? 1 : 0.5f));
        if (current) UiTextCentered(gFonts.bold, stepsDone == 0 ? "START" : "CONTINUE",
                                    face.x, face.y + 12 * scale, 12 * scale, ink);
        Vector2 badge = { face.x + radius * 0.75f, face.y - radius * 0.7f };
        DrawCircleV(badge, 14 * scale, COL_PANEL);
        UiTextCentered(gFonts.bold, TextFormat("%d", ordinal + 1), badge.x, badge.y - 9 * scale, 15 * scale,
                       passed ? doneLabel : current ? currentColor : COL_DIM);

        // The title plate is see-through, so the trail shows beneath it; hovering fades in a
        // light tint.
        if (hover > 0.01f) UiRoundRect(titleHit, 12, Fade(COL_PANEL_HI, 0.45f * hover));
        if (UiTextWidth(gFonts.text, lesson->title, 20 * scale) <= 264)
            UiTextCentered(gFonts.text, lesson->title, center.x, titleY, 20 * scale, ready ? COL_TEXT : COL_DIM);
        else UiWrappedText(gFonts.text, lesson->title, (Vector2){ center.x - 132, titleY }, 264, 20 * scale,
                           ready ? COL_TEXT : COL_DIM, true);
        const char *status = !ready ? "COMING SOON" : gold ? "GOLD MASTERY" : passed ? "COMPLETED"
                           : stepsDone > 0 ? TextFormat("%d / %d STEPS", stepsDone, lesson->stepCount)
                           : current ? "YOUR NEXT CHALLENGE" : "EXPLORE ANYTIME";
        UiTextCentered(gFonts.text, status, center.x, titleY + titleH + 6, 12 * scale,
                       gold ? COL_GOLD : passed ? doneLabel : current ? currentColor : COL_DIM);
        if (current && MascotAvailable()) Guide(center, x, width, scale, done, total);
        if ((over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || i == demoClick) {
            selected = i;
            demoClick = -1;
            if (clickedAt) *clickedAt = face;
        }

        // Little map markers decorate the opposite bank of the trail.
        if (ordinal % 3 == 2) {
            float markerX = center.x < x + width / 2 ? x + width - 90 : x + 90;
            IconDraw(ICON_CODE, (Vector2){ markerX, center.y + 12 }, 25, Fade(COL_ACCENT, 0.18f));
            DrawCircleV((Vector2){ markerX + 24, center.y - 16 }, 3, Fade(COL_ACCENT, 0.3f));
        }
        stationY += step;
        ordinal++;
    }
    *height = stationY - top + 24;
    return selected;
}
