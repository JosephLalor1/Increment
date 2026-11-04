#ifndef UI_H
#define UI_H

#include "icons.h"
#include "raylib.h"
#include <stdbool.h>

// The active palette. Switch it with UiSetTheme; COL_* names read from it.
typedef struct {
    Color bg, panel, panelHi, text, dim;
    Color accent, accentHi, onAccent; // onAccent: text drawn on an accent fill
    Color secondaryHover;             // hover fill for neutral buttons, tinted by the accent
    Color brand;                      // logo tile: the deep variant of the accent in both themes
    Color good, goodBg, bad, badBg, codeBg, noteBg;
    Color synDefault, synKeyword, synType, synString, synNumber, synComment, synPreproc;
} UiTheme;

extern UiTheme gTheme;

#define COL_BG        (gTheme.bg)
#define COL_PANEL     (gTheme.panel)
#define COL_PANEL_HI  (gTheme.panelHi)
#define COL_TEXT      (gTheme.text)
#define COL_DIM       (gTheme.dim)
#define COL_ACCENT    (gTheme.accent)
#define COL_ACCENT_HI (gTheme.accentHi)
#define COL_ON_ACCENT (gTheme.onAccent)
#define COL_SECONDARY_HOVER (gTheme.secondaryHover)
#define COL_BRAND     (gTheme.brand)
#define COL_GOOD      (gTheme.good)
#define COL_GOOD_BG   (gTheme.goodBg)
#define COL_BAD       (gTheme.bad)
#define COL_BAD_BG    (gTheme.badBg)
#define COL_CODE_BG   (gTheme.codeBg)
#define COL_NOTE_BG   (gTheme.noteBg)

#define UI_ACCENT_COUNT 5

void UiSetTheme(bool light, int accent);
Color UiAccentSwatch(int accent, bool light);
const char *UiAccentName(int accent);

typedef struct {
    Font text;
    Font mono;
    Font bold;  // semibold, for the wordmark and big headings
    bool ownsText;
    bool ownsMono;
    bool ownsBold;
} UiFonts;

extern UiFonts gFonts;

void UiLoadFonts(const char *assetDir);
void UiUnloadFonts(void);

// Call UiBeginFrame before any widgets and UiEndFrame after the last one.
void UiBeginFrame(void);
void UiEndFrame(void);
void UiWantHand(void); // request the pointing-hand cursor this frame
void UiWantCursor(int cursor); // request any MOUSE_CURSOR_* this frame

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
bool UiLink(float x, float y, const char *text, float size); // accent-coloured clickable text

// Buttons with an icon before the label (label may be NULL for an icon-only button).
bool UiButtonIcon(Rectangle r, Icon icon, const char *label, bool enabled);
bool UiButtonIconEx(Rectangle r, Icon icon, const char *label, float size, Color base, Color hover, Color fg,
                    bool enabled);

// Draws an icon followed by text, vertically centred on the text line. Returns the total width.
float UiIconText(Icon icon, Font f, const char *text, float x, float y, float size, Color c);

// Single-line text input. buf is also the focus identity, so it must outlive the frame.
// Accepts printable ASCII up to size-1 bytes. Returns true when Enter is pressed while focused.
bool UiTextBox(Rectangle r, char *buf, int size, const char *placeholder, bool password);
void UiFocus(const void *id);  // give keyboard focus to a widget (a text box's buf, or an editor); NULL clears
bool UiHasFocus(const void *id);
// True if id had focus at the start of this frame, so it may handle keys now (see UiTextBox).
bool UiTakesInput(const void *id);
bool UiAnyFocus(void);

// Word-wrapped text. Returns the height used. Pass draw=false to only measure.
float UiWrappedText(Font f, const char *text, Vector2 pos, float width, float size, Color c, bool draw);

// One syntax-highlighting token: a run of characters [start, start + len) in one colour.
typedef struct {
    int start, len;
    Color color;
} UiCodeToken;

// Splits one line of C/C++ into coloured tokens. inBlockComment carries /* */ state
// between lines. Returns the number of tokens written to out.
int UiCodeTokens(const char *line, int len, bool *inBlockComment, UiCodeToken *out, int max);

// Syntax-highlighted code block. Returns the height used.
float UiCodeBlock(const char *code, Vector2 pos, float width, float size, bool draw);
// The same box in plain text, for program output.
float UiOutputBlock(const char *text, Vector2 pos, float width, float size, bool draw);

#endif
