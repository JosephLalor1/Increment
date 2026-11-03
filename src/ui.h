#ifndef UI_H
#define UI_H

#include "raylib.h"
#include <stdbool.h>

// Palette (dark theme)
#define COL_BG          (Color){ 24, 26, 33, 255 }
#define COL_PANEL       (Color){ 34, 37, 46, 255 }
#define COL_PANEL_HI    (Color){ 46, 50, 62, 255 }
#define COL_TEXT        (Color){ 225, 228, 235, 255 }
#define COL_DIM         (Color){ 150, 156, 170, 255 }
#define COL_ACCENT      (Color){ 98, 160, 255, 255 }
#define COL_ACCENT_HI   (Color){ 130, 180, 255, 255 }
#define COL_GOOD        (Color){ 90, 200, 130, 255 }
#define COL_GOOD_BG     (Color){ 34, 70, 52, 255 }
#define COL_BAD         (Color){ 235, 95, 95, 255 }
#define COL_BAD_BG      (Color){ 82, 36, 40, 255 }
#define COL_CODE_BG     (Color){ 16, 17, 22, 255 }
#define COL_NOTE_BG     (Color){ 38, 45, 64, 255 }

typedef struct {
    Font text;
    Font mono;
    bool ownsText;
    bool ownsMono;
} UiFonts;

extern UiFonts gFonts;

void UiLoadFonts(const char *assetDir);
void UiUnloadFonts(void);

// Call UiBeginFrame before any widgets and UiEndFrame after the last one.
void UiBeginFrame(void);
void UiEndFrame(void);
void UiWantHand(void); // request the pointing-hand cursor this frame

// Restrict mouse hit-testing to a region (e.g. a scrolling view).
void UiSetClip(Rectangle r);
void UiClearClip(void);
bool UiMouseOver(Rectangle r);
bool UiClicked(Rectangle r);

float UiSpacing(Font f, float size);
void UiText(Font f, const char *text, float x, float y, float size, Color c);
void UiTextCentered(Font f, const char *text, float centerX, float y, float size, Color c);
void UiRoundRect(Rectangle r, float radius, Color c);

bool UiButton(Rectangle r, const char *label, bool enabled);
bool UiButtonEx(Rectangle r, const char *label, float size, Color base, Color hover, Color fg, bool enabled);

// Single-line text input. buf is also the focus identity, so it must outlive the frame.
// Accepts printable ASCII up to size-1 bytes. Returns true when Enter is pressed while focused.
bool UiTextBox(Rectangle r, char *buf, int size, const char *placeholder, bool password);
void UiFocus(char *buf);       // give keyboard focus to the text box using buf (NULL clears)
bool UiHasFocus(const char *buf);

// Word-wrapped text. Returns the height used. Pass draw=false to only measure.
float UiWrappedText(Font f, const char *text, Vector2 pos, float width, float size, Color c, bool draw);

// Syntax-highlighted code block. Returns the height used.
float UiCodeBlock(const char *code, Vector2 pos, float width, float size, bool draw);

#endif
