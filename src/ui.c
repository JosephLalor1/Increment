// Themes, fonts, immediate-mode widgets, word wrap and code blocks. See ui.h.
#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PASSWORD_DISPLAY_MAX 128

UiFonts gFonts;
UiTheme gTheme;

// ---------------------------------------------------------------------------
// Themes

static const UiTheme darkBase = {
    .bg = { 24, 26, 33, 255 },        .panel = { 34, 37, 46, 255 },     .panelHi = { 46, 50, 62, 255 },
    .text = { 225, 228, 235, 255 },   .dim = { 150, 156, 170, 255 },
    .onAccent = { 24, 26, 33, 255 },
    .good = { 90, 200, 130, 255 },    .goodBg = { 34, 70, 52, 255 },
    .bad = { 235, 95, 95, 255 },      .badBg = { 82, 36, 40, 255 },
    .codeBg = { 16, 17, 22, 255 },    .noteBg = { 38, 45, 64, 255 },
    .synDefault = { 220, 223, 228, 255 }, .synKeyword = { 198, 120, 221, 255 }, .synType = { 86, 182, 194, 255 },
    .synString = { 152, 195, 121, 255 },  .synNumber = { 209, 154, 102, 255 },  .synComment = { 110, 118, 135, 255 },
    .synPreproc = { 224, 108, 117, 255 },
};

static const UiTheme lightBase = {
    .bg = { 243, 244, 248, 255 },     .panel = { 255, 255, 255, 255 },  .panelHi = { 228, 232, 241, 255 },
    .text = { 30, 33, 40, 255 },      .dim = { 100, 106, 120, 255 },
    .onAccent = { 255, 255, 255, 255 },
    .good = { 22, 140, 70, 255 },     .goodBg = { 214, 245, 225, 255 },
    .bad = { 205, 40, 40, 255 },      .badBg = { 252, 224, 224, 255 },
    .codeBg = { 235, 237, 243, 255 }, .noteBg = { 226, 236, 252, 255 },
    .synDefault = { 40, 44, 52, 255 },    .synKeyword = { 166, 38, 164, 255 },  .synType = { 1, 122, 170, 255 },
    .synString = { 60, 130, 60, 255 },    .synNumber = { 152, 94, 1, 255 },     .synComment = { 140, 142, 150, 255 },
    .synPreproc = { 200, 70, 60, 255 },
};

// Each accent has a bright variant for dark mode and a deeper one for light mode,
// so it stays readable as text on either background.
static const struct {
    const char *name;
    Color dark, darkHi, light, lightHi;
} accents[UI_ACCENT_COUNT] = {
    { "Blue",   { 98, 160, 255, 255 },  { 130, 180, 255, 255 }, { 37, 99, 235, 255 },  { 29, 78, 216, 255 } },
    { "Purple", { 167, 139, 250, 255 }, { 196, 181, 253, 255 }, { 124, 58, 237, 255 }, { 109, 40, 217, 255 } },
    { "Green",  { 52, 211, 153, 255 },  { 110, 231, 183, 255 }, { 5, 140, 100, 255 },  { 4, 115, 82, 255 } },
    { "Orange", { 251, 146, 60, 255 },  { 253, 186, 116, 255 }, { 214, 82, 12, 255 },  { 180, 62, 12, 255 } },
    { "Pink",   { 244, 114, 182, 255 }, { 249, 168, 212, 255 }, { 210, 36, 115, 255 }, { 180, 22, 90, 255 } },
};

void UiSetTheme(bool light, int accent)
{
    if (accent < 0 || accent >= UI_ACCENT_COUNT) accent = 0;
    gTheme = light ? lightBase : darkBase;
    gTheme.accent = light ? accents[accent].light : accents[accent].dark;
    gTheme.accentHi = light ? accents[accent].lightHi : accents[accent].darkHi;
    gTheme.secondaryHover = ColorLerp(gTheme.panelHi, gTheme.accent, light ? 0.18f : 0.3f);
    gTheme.brand = accents[accent].light;  // white pluses need the deeper shade for contrast
}

Color UiAccentSwatch(int accent, bool light)
{
    if (accent < 0 || accent >= UI_ACCENT_COUNT) accent = 0;
    return light ? accents[accent].light : accents[accent].dark;
}

const char *UiAccentName(int accent)
{
    return (accent >= 0 && accent < UI_ACCENT_COUNT) ? accents[accent].name : "";
}

// Multiplier for lesson-sized text, set from the user's "Lesson text size" setting.
static float textScale = 1.0f;

void UiSetTextScale(float scale) { textScale = scale > 0 ? scale : 1.0f; }
float UiTextScale(void) { return textScale; }

// ---------------------------------------------------------------------------

static Rectangle clipRect;
static bool clipOn;

static const void *focused;
static const void *frameFocus; // focus at the start of the frame; only it receives input

// ASCII, Latin-1, and the typographic punctuation people tend to paste in.
static int *FontCodepoints(int *count)
{
    static int cps[256];
    static const int extra[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2190, 0x2192 };
    int n = 0;
    for (int c = 32; c < 127; c++) cps[n++] = c;
    for (int c = 160; c < 256; c++) cps[n++] = c;
    for (size_t i = 0; i < sizeof extra / sizeof extra[0]; i++) cps[n++] = extra[i];
    *count = n;
    return cps;
}

static Font LoadFirstFont(const char **paths, int pathCount, bool *owned)
{
    for (int i = 0; i < pathCount; i++) {
        if (!FileExists(paths[i])) continue;
        int n;
        int *cps = FontCodepoints(&n);
        Font f = LoadFontEx(paths[i], 64, cps, n);
        if (IsFontValid(f)) {
            GenTextureMipmaps(&f.texture);
            SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
            *owned = true;
            return f;
        }
    }
    TraceLog(LOG_WARNING, "UI: no TTF font found, using raylib default font");
    *owned = false;
    return GetFontDefault();
}

void UiLoadFonts(const char *assetDir)
{
    // Bundled fonts in assets/fonts win; otherwise fall back to Windows system fonts.
    char bundledText[1024], bundledMono[1024];
    strncpy(bundledText, TextFormat("%s/fonts/text.ttf", assetDir), sizeof bundledText - 1);
    bundledText[sizeof bundledText - 1] = '\0';
    strncpy(bundledMono, TextFormat("%s/fonts/mono.ttf", assetDir), sizeof bundledMono - 1);
    bundledMono[sizeof bundledMono - 1] = '\0';

    const char *textPaths[] = { bundledText, "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf" };
    const char *monoPaths[] = { bundledMono, "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/cour.ttf" };
    gFonts.text = LoadFirstFont(textPaths, 3, &gFonts.ownsText);
    gFonts.mono = LoadFirstFont(monoPaths, 3, &gFonts.ownsMono);

    char bundledBold[1024];
    strncpy(bundledBold, TextFormat("%s/fonts/bold.ttf", assetDir), sizeof bundledBold - 1);
    bundledBold[sizeof bundledBold - 1] = '\0';
    const char *boldPaths[] = { bundledBold, "C:/Windows/Fonts/seguisb.ttf", "C:/Windows/Fonts/segoeuib.ttf" };
    gFonts.bold = LoadFirstFont(boldPaths, 3, &gFonts.ownsBold);
    if (!gFonts.ownsBold) gFonts.bold = gFonts.text;  // no bold available: reuse the text font
}

void UiUnloadFonts(void)
{
    if (gFonts.ownsText) UnloadFont(gFonts.text);
    if (gFonts.ownsMono) UnloadFont(gFonts.mono);
    if (gFonts.ownsBold) UnloadFont(gFonts.bold);
    gFonts.ownsText = gFonts.ownsMono = gFonts.ownsBold = false;
}

static int wantedCursor = MOUSE_CURSOR_DEFAULT;
static int currentCursor = MOUSE_CURSOR_DEFAULT;

void UiBeginFrame(void)
{
    wantedCursor = MOUSE_CURSOR_DEFAULT;
    clipOn = false;
    frameFocus = focused;
}

void UiEndFrame(void)
{
    if (wantedCursor != currentCursor) {
        SetMouseCursor(wantedCursor);
        currentCursor = wantedCursor;
    }
}

void UiWantHand(void) { wantedCursor = MOUSE_CURSOR_POINTING_HAND; }
void UiWantCursor(int cursor) { wantedCursor = cursor; }

void UiSetClip(Rectangle r) { clipRect = r; clipOn = true; }
void UiClearClip(void) { clipOn = false; }

bool UiMouseOver(Rectangle r)
{
    Vector2 m = GetMousePosition();
    if (clipOn && !CheckCollisionPointRec(m, clipRect)) return false;
    return CheckCollisionPointRec(m, r);
}

bool UiClicked(Rectangle r)
{
    return UiMouseOver(r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

float UiSpacing(Font f, float size)
{
    // The built-in pixel font needs explicit spacing; TTFs carry their own advance widths.
    return (f.texture.id == GetFontDefault().texture.id) ? size / 10.0f : 0.0f;
}

void UiText(Font f, const char *text, float x, float y, float size, Color c)
{
    DrawTextEx(f, text, (Vector2){ x, y }, size, UiSpacing(f, size), c);
}

void UiTextCentered(Font f, const char *text, float centerX, float y, float size, Color c)
{
    Vector2 m = MeasureTextEx(f, text, size, UiSpacing(f, size));
    UiText(f, text, centerX - m.x / 2.0f, y, size, c);
}

void UiRoundRect(Rectangle r, float radius, Color c)
{
    float shortest = r.width < r.height ? r.width : r.height;
    float roundness = shortest > 0 ? (2.0f * radius) / shortest : 0;
    if (roundness > 1.0f) roundness = 1.0f;
    DrawRectangleRounded(r, roundness, 8, c);
}

bool UiButtonEx(Rectangle r, const char *label, float size, Color base, Color hover, Color fg, bool enabled)
{
    bool over = enabled && UiMouseOver(r);
    if (over) UiWantHand();
    UiRoundRect(r, 8, enabled ? (over ? hover : base) : Fade(base, 0.4f));
    Vector2 m = MeasureTextEx(gFonts.text, label, size, UiSpacing(gFonts.text, size));
    UiText(gFonts.text, label, r.x + (r.width - m.x) / 2.0f, r.y + (r.height - m.y) / 2.0f, size,
           enabled ? fg : Fade(fg, 0.4f));
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool UiButton(Rectangle r, const char *label, bool enabled)
{
    return UiButtonEx(r, label, 22, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, enabled);
}

bool UiButtonIconEx(Rectangle r, Icon icon, const char *label, float size, Color base, Color hover, Color fg,
                    bool enabled)
{
    bool over = enabled && UiMouseOver(r);
    if (over) UiWantHand();
    UiRoundRect(r, 8, enabled ? (over ? hover : base) : Fade(base, 0.4f));
    Color c = enabled ? fg : Fade(fg, 0.4f);

    // Centre icon + gap + label as one group.
    float iconSize = size * 0.95f, gap = label ? size * 0.45f : 0;
    float textW = label ? MeasureTextEx(gFonts.text, label, size, UiSpacing(gFonts.text, size)).x : 0;
    float x = r.x + (r.width - (iconSize + gap + textW)) / 2.0f;
    float cy = r.y + r.height / 2.0f;
    IconDraw(icon, (Vector2){ x + iconSize / 2, cy }, iconSize, c);
    if (label) UiText(gFonts.text, label, x + iconSize + gap, cy - size / 2.0f, size, c);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool UiButtonIcon(Rectangle r, Icon icon, const char *label, bool enabled)
{
    return UiButtonIconEx(r, icon, label, 22, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, enabled);
}

float UiIconText(Icon icon, Font f, const char *text, float x, float y, float size, Color c)
{
    float iconSize = size * 0.95f, gap = size * 0.45f;
    // Centre the icon on the visual middle of the text line.
    IconDraw(icon, (Vector2){ x + iconSize / 2, y + size * 0.56f }, iconSize, c);
    UiText(f, text, x + iconSize + gap, y, size, c);
    return iconSize + gap + MeasureTextEx(f, text, size, UiSpacing(f, size)).x;
}

bool UiLink(float x, float y, const char *text, float size)
{
    Vector2 m = MeasureTextEx(gFonts.text, text, size, UiSpacing(gFonts.text, size));
    Rectangle r = { x, y, m.x, m.y };
    bool over = UiMouseOver(r);
    if (over) {
        UiWantHand();
        DrawRectangle((int)x, (int)(y + m.y), (int)m.x, 1, COL_ACCENT_HI);
    }
    UiText(gFonts.text, text, x, y, size, over ? COL_ACCENT_HI : COL_ACCENT);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

void UiFocus(const void *id) { focused = id; }
bool UiHasFocus(const void *id) { return focused == id; }
bool UiTakesInput(const void *id) { return focused == id && frameFocus == id; }
bool UiAnyFocus(void) { return focused != NULL; }

static void InsertText(char *buf, int size, const char *text)
{
    int len = (int)strlen(buf);
    for (const char *c = text; *c && len < size - 1; c++) {
        if (*c >= 32 && *c < 127) buf[len++] = *c;
    }
    buf[len] = '\0';
}

bool UiTextBox(Rectangle r, char *buf, int size, const char *placeholder, bool password)
{
    const float fontSize = 22, pad = 14;
    bool over = UiMouseOver(r);
    if (over) wantedCursor = MOUSE_CURSOR_IBEAM;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (over) focused = buf;
        else if (focused == buf) focused = NULL;
    }

    bool hasFocus = focused == buf;
    bool submitted = false;
    if (hasFocus && frameFocus == buf) {
        int c;
        while ((c = GetCharPressed()) != 0) {
            char ch[2] = { (char)c, '\0' };
            if (c < 127) InsertText(buf, size, ch);
        }
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            size_t len = strlen(buf);
            if (ctrl) buf[0] = '\0';
            else if (len) buf[len - 1] = '\0';
        }
        if (ctrl && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip) InsertText(buf, size, clip);
        }
        submitted = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    }

    UiRoundRect(r, 8, COL_CODE_BG);
    if (hasFocus) DrawRectangleRoundedLinesEx(r, 16.0f / r.height, 8, 2, COL_ACCENT);

    // Masked text is drawn as bullets (U+2022).
    int len = (int)strlen(buf);
    char shown[PASSWORD_DISPLAY_MAX * 3 + 1];
    int unit = password ? 3 : 1;
    int count = len < PASSWORD_DISPLAY_MAX ? len : PASSWORD_DISPLAY_MAX;
    if (password) {
        for (int i = 0; i < count; i++) memcpy(shown + i * 3, "\xE2\x80\xA2", 3);
        shown[count * 3] = '\0';
    } else {
        snprintf(shown, sizeof shown, "%s", buf);
        count = (int)strlen(shown);
    }

    // Keep the end of the text (and the caret) visible by dropping leading characters.
    float inner = r.width - pad * 2 - 4;
    float sp = UiSpacing(gFonts.text, fontSize);
    const char *start = shown;
    while (*start && MeasureTextEx(gFonts.text, start, fontSize, sp).x > inner) start += unit;

    float ty = r.y + (r.height - fontSize) / 2.0f - 2;
    if (len == 0 && !hasFocus) UiText(gFonts.text, placeholder, r.x + pad, ty, fontSize, COL_DIM);
    else UiText(gFonts.text, start, r.x + pad, ty, fontSize, COL_TEXT);

    if (hasFocus && fmod(GetTime(), 1.0) < 0.55) {
        float cx = r.x + pad + MeasureTextEx(gFonts.text, start, fontSize, sp).x + 2;
        DrawRectangle((int)cx, (int)(r.y + 12), 2, (int)(r.height - 24), COL_TEXT);
    }
    return submitted;
}

float UiTextWidth(Font f, const char *text, float size)
{
    return MeasureTextEx(f, text, size, UiSpacing(f, size)).x;
}

void UiFitText(char *text, size_t capacity, Font f, float size, float maxWidth)
{
    if (UiTextWidth(f, text, size) <= maxWidth) return;
    // Keep fewer characters each time (dropping spaces before the cut) until they fit with
    // "..." after them. The kept part plus "..." and its terminator must fit in capacity.
    int keep = (int)strlen(text);
    int mostThatFits = (int)capacity - 4;
    if (keep > mostThatFits + 1) keep = mostThatFits + 1;
    while (keep > 0) {
        keep--;
        while (keep > 0 && text[keep - 1] == ' ') keep--;
        memcpy(text + keep, "...", 4);
        if (UiTextWidth(f, text, size) <= maxWidth) return;
    }
}

float UiWrappedText(Font f, const char *text, Vector2 pos, float width, float size, Color c, bool draw)
{
    float lineH = size * 1.35f;
    float sp = UiSpacing(f, size);
    char line[2048] = "";
    char cand[2048];
    size_t lineLen = 0;
    float y = pos.y;
    const char *p = text;

    while (*p) {
        if (*p == '\n') {
            if (draw) DrawTextEx(f, line, (Vector2){ pos.x, y }, size, sp, c);
            y += lineH;
            lineLen = 0;
            line[0] = '\0';
            p++;
            continue;
        }
        if (*p == ' ') { p++; continue; }

        const char *start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t wordLen = (size_t)(p - start);

        size_t candLen = 0;
        if (lineLen) {
            memcpy(cand, line, lineLen);
            cand[lineLen] = ' ';
            candLen = lineLen + 1;
        }
        if (candLen + wordLen >= sizeof cand) wordLen = sizeof cand - candLen - 1;
        memcpy(cand + candLen, start, wordLen);
        candLen += wordLen;
        cand[candLen] = '\0';

        if (lineLen && MeasureTextEx(f, cand, size, sp).x > width) {
            if (draw) DrawTextEx(f, line, (Vector2){ pos.x, y }, size, sp, c);
            y += lineH;
            memcpy(line, start, wordLen);
            line[wordLen] = '\0';
            lineLen = wordLen;
        } else {
            memcpy(line, cand, candLen + 1);
            lineLen = candLen;
        }
    }
    if (lineLen) {
        if (draw) DrawTextEx(f, line, (Vector2){ pos.x, y }, size, sp, c);
        y += lineH;
    }
    return y - pos.y;
}

// ---------------------------------------------------------------------------
// Syntax highlighting

#define SYN_DEFAULT  (gTheme.synDefault)
#define SYN_KEYWORD  (gTheme.synKeyword)
#define SYN_TYPE     (gTheme.synType)
#define SYN_STRING   (gTheme.synString)
#define SYN_NUMBER   (gTheme.synNumber)
#define SYN_COMMENT  (gTheme.synComment)
#define SYN_PREPROC  (gTheme.synPreproc)

static const char *keywords[] = {
    "break", "case", "const", "continue", "default", "do", "else", "enum", "extern", "for", "goto", "if",
    "inline", "register", "restrict", "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", "true", "false", "NULL",
    "class", "namespace", "using", "public", "private", "protected", "template", "typename", "new", "delete",
    "this", "virtual", "override", "nullptr", "try", "catch", "throw", "static_cast", "const_cast",
    "dynamic_cast", "reinterpret_cast", "operator", "friend", "explicit", "constexpr", "noexcept", "final",
};

static const char *types[] = {
    "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "bool", "auto",
    "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "FILE", "std", "string", "vector", "cout", "cin", "endl",
};

static bool InList(const char *s, int n, const char **list, int count)
{
    for (int i = 0; i < count; i++) {
        if ((int)strlen(list[i]) == n && strncmp(s, list[i], (size_t)n) == 0) return true;
    }
    return false;
}

static void DrawToken(const char *s, int n, float *x, float y, float size, Color c)
{
    char tmp[512];
    if (n > (int)sizeof tmp - 1) n = (int)sizeof tmp - 1;
    memcpy(tmp, s, (size_t)n);
    tmp[n] = '\0';
    float sp = UiSpacing(gFonts.mono, size);
    DrawTextEx(gFonts.mono, tmp, (Vector2){ *x, y }, size, sp, c);
    *x += MeasureTextEx(gFonts.mono, tmp, size, sp).x + sp;
}

int UiCodeTokens(const char *s, int len, bool *inBlockComment, UiCodeToken *out, int max)
{
    int i = 0, count = 0;
    int firstNonSpace = 0;
    while (firstNonSpace < len && s[firstNonSpace] == ' ') firstNonSpace++;

    while (i < len && count < max) {
        int start = i;
        Color c = SYN_DEFAULT;

        if (*inBlockComment) {
            while (i < len && !(s[i] == '*' && i + 1 < len && s[i + 1] == '/')) i++;
            if (i < len) { i += 2; *inBlockComment = false; }
            c = SYN_COMMENT;
        } else if (s[i] == '/' && i + 1 < len && s[i + 1] == '/') {
            i = len;
            c = SYN_COMMENT;
        } else if (s[i] == '/' && i + 1 < len && s[i + 1] == '*') {
            i += 2;
            *inBlockComment = true;
            while (i < len && !(s[i] == '*' && i + 1 < len && s[i + 1] == '/')) i++;
            if (i < len) { i += 2; *inBlockComment = false; }
            c = SYN_COMMENT;
        } else if (s[i] == '#' && i == firstNonSpace) {
            i = len;
            c = SYN_PREPROC;
        } else if (s[i] == '"' || s[i] == '\'') {
            char q = s[i++];
            while (i < len && s[i] != q) {
                if (s[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            c = SYN_STRING;
        } else if (isdigit((unsigned char)s[i])) {
            while (i < len && (isalnum((unsigned char)s[i]) || s[i] == '.')) i++;
            c = SYN_NUMBER;
        } else if (isalpha((unsigned char)s[i]) || s[i] == '_') {
            while (i < len && (isalnum((unsigned char)s[i]) || s[i] == '_')) i++;
            int n = i - start;
            if (InList(s + start, n, keywords, (int)(sizeof keywords / sizeof keywords[0]))) c = SYN_KEYWORD;
            else if (InList(s + start, n, types, (int)(sizeof types / sizeof types[0]))) c = SYN_TYPE;
        } else {
            while (i < len && !isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '"' && s[i] != '\''
                   && s[i] != '/' && s[i] != '#') i++;
            if (i == start) i++;
        }
        out[count++] = (UiCodeToken){ start, i - start, c };
    }
    return count;
}

// Draws one line of code; inBlockComment carries /* ... */ state across lines.
static void DrawCodeLine(const char *s, int len, float x, float y, float size, bool *inBlockComment)
{
    UiCodeToken tokens[256];
    int n = UiCodeTokens(s, len, inBlockComment, tokens, 256);
    for (int t = 0; t < n; t++) DrawToken(s + tokens[t].start, tokens[t].len, &x, y, size, tokens[t].color);
}

static float Block(const char *code, Vector2 pos, float width, float size, bool draw, bool highlight);

float UiCodeBlock(const char *code, Vector2 pos, float width, float size, bool draw)
{
    return Block(code, pos, width, size, draw, true);
}

float UiOutputBlock(const char *text, Vector2 pos, float width, float size, bool draw)
{
    return Block(text, pos, width, size, draw, false);
}

static float Block(const char *code, Vector2 pos, float width, float size, bool draw, bool highlight)
{
    float pad = 16.0f;
    float lineH = size * 1.4f;
    int lines = 1;
    for (const char *p = code; *p; p++) if (*p == '\n') lines++;
    float h = lines * lineH + pad * 2;
    if (!draw) return h;

    UiRoundRect((Rectangle){ pos.x, pos.y, width, h }, 8, COL_CODE_BG);
    bool inBlockComment = false;
    const char *p = code;
    float y = pos.y + pad;
    while (true) {
        const char *end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);
        if (highlight) {
            DrawCodeLine(p, len, pos.x + pad, y, size, &inBlockComment);
        } else {
            float x = pos.x + pad;
            DrawToken(p, len, &x, y, size, COL_TEXT);
        }
        y += lineH;
        if (!end) break;
        p = end + 1;
    }
    return h;
}
