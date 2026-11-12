// Page pieces shared by screens: content column, scroll views, top bar. See layout.h.
#include "layout.h"
#include "brand.h"
#include "ui.h"

#include <stddef.h>

float ContentWidthIn(float areaWidth)
{
    float w = areaWidth - 80.0f;
    return w > MAX_WIDTH ? MAX_WIDTH : w;
}

float ContentWidth(void) { return ContentWidthIn((float)GetScreenWidth()); }

float ContentX(void) { return (GetScreenWidth() - ContentWidth()) / 2.0f; }

Rectangle ScrollView(void)
{
    return (Rectangle){ 0, TOPBAR_H, (float)GetScreenWidth(), GetScreenHeight() - TOPBAR_H };
}

void UpdateScroll(float *scroll, float contentH, float viewH)
{
    *scroll -= GetMouseWheelMove() * 60.0f;
    if (!UiAnyFocus()) {
        if (IsKeyDown(KEY_DOWN)) *scroll += 12.0f;
        if (IsKeyDown(KEY_UP)) *scroll -= 12.0f;
        if (IsKeyPressed(KEY_PAGE_DOWN)) *scroll += viewH * 0.9f;
        if (IsKeyPressed(KEY_PAGE_UP)) *scroll -= viewH * 0.9f;
        if (IsKeyPressed(KEY_HOME)) *scroll = 0;
        if (IsKeyPressed(KEY_END)) *scroll = contentH;
    }

    float max = contentH - viewH;
    if (max < 0) max = 0;
    if (*scroll > max) *scroll = max;
    if (*scroll < 0) *scroll = 0;
}

bool PageScrollBar(Rectangle view, float *scroll, float contentH)
{
    Rectangle track = { view.x + view.width - 16, view.y + 6, 12, view.height - 12 };
    return UiScrollBar(track, scroll, contentH, view.height);
}

void BeginView(Rectangle v)
{
    BeginScissorMode((int)v.x, (int)v.y, (int)v.width, (int)v.height);
    UiSetClip(v);
}

void EndView(void)
{
    EndScissorMode();
    UiClearClip();
}

bool TopBar(const char *title)
{
    DrawRectangle(0, 0, GetScreenWidth(), (int)TOPBAR_H, COL_PANEL);
    DrawRectangle(0, (int)TOPBAR_H - 1, GetScreenWidth(), 1, COL_PANEL_HI);
    bool back = UiButtonIconEx((Rectangle){ 16, 12, 96, 40 }, ICON_BACK, "Back", 20, COL_PANEL_HI,
                               COL_SECONDARY_HOVER, COL_TEXT, true);
    UiText(gFonts.text, title, 132, 17, 26, COL_TEXT);

    if (UiAnyFocus()) {
        if (IsKeyPressed(KEY_ESCAPE) && !UiEscapeClaimed()) UiFocus(NULL);
        return back;
    }
    return back || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
}

void ProgressBar(Rectangle r, float t, Color track, Color fill)
{
    UiRoundRect(r, r.height / 2, track);
    if (t > 0) UiRoundRect((Rectangle){ r.x, r.y, r.width * t, r.height }, r.height / 2, fill);
}

float TrackBadge(const char *trackName, float x, float y)
{
    float w = UiTextWidth(gFonts.bold, trackName, 17) + 22;
    UiRoundRect((Rectangle){ x, y, w, 28 }, 14, COL_BG);
    UiTextCentered(gFonts.bold, trackName, x + w / 2, y + 4, 17, COL_ACCENT);
    return w;
}

void DrawLogoLockup(float cx, float top, float logoSize)
{
    float textSize = logoSize * 0.84f;
    float textW = MeasureTextEx(gFonts.bold, "Increment", textSize, 0).x;
    float gap = logoSize * 0.28f;
    float x = cx - (logoSize + gap + textW) / 2;
    BrandDrawLogo((Vector2){ x, top }, logoSize, COL_BRAND, WHITE, LOGO_STATIC, true);
    UiText(gFonts.bold, "Increment", x + logoSize + gap, top + logoSize / 2 - textSize * 0.62f, textSize, COL_TEXT);
}
