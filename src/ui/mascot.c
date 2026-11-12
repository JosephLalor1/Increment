// The fox mascot on screen. See mascot.h.
#include "ui/mascot.h"
#include "ui/ui.h"

#include <stdio.h>
#include <math.h>

#define FACE_FADE_SECONDS 0.22f  // the cross-fade from one face to the next
#define BOUNCE_SECONDS 0.8f      // the little squeeze at the start of a reaction

static Texture2D expressions[FOX_EMOTION_COUNT];  // a picture per face
static Texture2D fox;                             // the plain fox, shown if a face is missing
static FoxState actors[FOX_SLOT_COUNT];           // the mood of the fox in each place

bool MascotAvailable(void)
{
    if (fox.id) return true;
    for (int i = 0; i < FOX_EMOTION_COUNT; i++) if (expressions[i].id) return true;
    return false;
}

void MascotResetInteractions(void)
{
    for (int i = 0; i < FOX_SLOT_COUNT; i++) FoxStateInit(&actors[i], FOX_HAPPY, GetTime());
}

void MascotUnload(void)
{
    if (fox.id) UnloadTexture(fox);
    fox = (Texture2D){ 0 };
    for (int i = 0; i < FOX_EMOTION_COUNT; i++) {
        if (expressions[i].id) UnloadTexture(expressions[i]);
        expressions[i] = (Texture2D){ 0 };
    }
}

// One image of the fox, smoothed for drawing small; an empty texture if it can't be loaded.
static Texture2D LoadFox(const char *assetDir, const char *name)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/mascot/%s", assetDir, name);
    Texture2D image = { 0 };
    if (FileExists(path)) image = LoadTexture(path);
    if (image.id && image.width > 0 && image.height > 0) {
        GenTextureMipmaps(&image);
        SetTextureFilter(image, TEXTURE_FILTER_TRILINEAR);
    } else {
        if (image.id) UnloadTexture(image);
        image = (Texture2D){ 0 };
        TraceLog(LOG_WARNING, "MASCOT: optional image unavailable: %s", path);
    }
    return image;
}

void MascotLoad(const char *assetDir)
{
    MascotUnload();
    static const char *names[FOX_EMOTION_COUNT] = {
        "01_fox_happy.png", "02_fox_excited.png", "03_fox_coding.png", "04_fox_thinking.png",
        "05_fox_trophy.png", "06_fox_graduation.png", "07_fox_sad.png", "08_fox_determined.png",
        "09_fox_love_learning.png", "10_fox_idea.png", "11_fox_sleepy.png", "12_fox_celebrating.png"
    };
    for (int i = 0; i < FOX_EMOTION_COUNT; i++) expressions[i] = LoadFox(assetDir, names[i]);
    fox = LoadFox(assetDir, "increment_fox.png");
    MascotResetInteractions();
}

// The picture for a face, or the nearest thing there is to it.
static Texture2D Expression(FoxEmotion emotion)
{
    if (emotion >= 0 && emotion < FOX_EMOTION_COUNT && expressions[emotion].id) return expressions[emotion];
    if (expressions[FOX_HAPPY].id) return expressions[FOX_HAPPY];
    if (fox.id) return fox;
    for (int i = 0; i < FOX_EMOTION_COUNT; i++) if (expressions[i].id) return expressions[i];
    return (Texture2D){ 0 };
}

// Draws a face as large as fits inside bounds, centred.
static void DrawFox(FoxEmotion emotion, Rectangle bounds, float alpha)
{
    Texture2D image = Expression(emotion);
    if (!image.id || bounds.width <= 0 || bounds.height <= 0) return;
    float scale = bounds.width / image.width;
    if (bounds.height / image.height < scale) scale = bounds.height / image.height;
    float w = image.width * scale, h = image.height * scale;
    Rectangle dest = { bounds.x + (bounds.width - w) / 2, bounds.y + (bounds.height - h) / 2, w, h };
    DrawTexturePro(image, (Rectangle){ 0, 0, (float)image.width, (float)image.height }, dest, (Vector2){ 0, 0 }, 0, Fade(WHITE, alpha));
}

void MascotReact(FoxSlot slot, FoxEvent event)
{
    if (slot >= 0 && slot < FOX_SLOT_COUNT) FoxStateReact(&actors[slot], event, GetTime());
}

void MascotDrawActor(FoxSlot slot, Rectangle bounds, FoxEmotion fallback)
{
    if (slot < 0 || slot >= FOX_SLOT_COUNT) return;
    FoxState *state = &actors[slot];
    double now = GetTime();
    FoxEmotion emotion = FoxStateUpdate(state, fallback, now);
    float blend = (float)((now - state->transition) / FACE_FADE_SECONDS);
    if (blend > 1) blend = 1;
    if (blend < 0) blend = 0;
    // A happy reaction starts with a small bounce: the fox shrinks a touch and springs back.
    if (state->reacting && state->event != FOX_INCORRECT) {
        float age = (float)(now - state->started);
        if (age < BOUNCE_SECONDS) {
            float inset = 0.035f * (1 - sinf(age / BOUNCE_SECONDS * PI));
            bounds.x += bounds.width * inset;
            bounds.y += bounds.height * inset;
            bounds.width *= 1 - 2 * inset;
            bounds.height *= 1 - 2 * inset;
        }
    }
    if (blend < 1) DrawFox(state->previous, bounds, 1 - blend);
    DrawFox(emotion, bounds, blend);
}

const char *MascotMessage(FoxSlot slot, const char *fallback)
{
    if (slot < 0 || slot >= FOX_SLOT_COUNT) return fallback;
    FoxStateUpdate(&actors[slot], actors[slot].fallback, GetTime());
    return FoxStateMessage(&actors[slot], fallback);
}

void MascotConfetti(Rectangle area, double age, bool graduation)
{
    if (age < 0 || age > 3.5) return;
    int count = graduation ? 64 : 30;
    // Each piece's path depends only on its number and the age, so nothing is stored.
    for (int i = 0; i < count; i++) {
        float travel = (float)age * (90 + i % 5 * 18);
        float x = area.x + (i * 97 % 997) / 997.0f * area.width + sinf((float)age * 2 + i) * 16;
        float y = area.y - (i * 37 % 130) + travel;
        if (x < area.x || x > area.x + area.width || y < area.y || y > area.y + area.height) continue;
        Color color = i % 3 == 0 ? COL_GOOD : i % 3 == 1 ? COL_ACCENT : COL_DIM;
        DrawRectanglePro((Rectangle){ x, y, 5, 9 }, (Vector2){ 2, 4 }, (float)age * 80 + i * 23, Fade(color, 0.65f));
    }
}

float MascotCompanionActor(FoxSlot slot, float x, float y, float width, float size, float textSize,
                           FoxEmotion fallback, const char *message)
{
    if (slot < 0 || slot >= FOX_SLOT_COUNT || !MascotAvailable() || width - size - 16 <= 32) return 0;
    FoxStateUpdate(&actors[slot], fallback, GetTime());
    const char *text = MascotMessage(slot, message);
    float bubbleW = width - size - 16;
    float textH = UiWrappedText(gFonts.text, text, (Vector2){ 0, 0 }, bubbleW - 32, textSize, COL_TEXT, false);
    float bubbleH = textH + 28, h = size > bubbleH ? size : bubbleH;
    Rectangle image = { x, y + (h - size) / 2, size, size };
    MascotDrawActor(slot, image, fallback);
    Rectangle bubble = { x + size + 16, y + (h - bubbleH) / 2, bubbleW, bubbleH };
    UiRoundRect(bubble, 12, COL_PANEL);
    UiWrappedText(gFonts.text, text, (Vector2){ bubble.x + 16, bubble.y + 14 }, bubbleW - 32, textSize, COL_TEXT, true);
    return h;
}

void MascotOfferTip(FoxSlot slot, Rectangle image)
{
    if (slot < 0 || slot >= FOX_SLOT_COUNT || !MascotAvailable() || !UiMouseOver(image)) return;
    UiWantHand();
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        MascotReact(slot, actors[slot].variation % 3 == 2 ? FOX_BREAK : FOX_TIP);
    }
}
