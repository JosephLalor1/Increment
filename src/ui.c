#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PASSWORD_DISPLAY_MAX 128

UiFonts gFonts;

static Rectangle clipRect;
static bool clipOn;

static char *focused;
static char *frameFocus; // focus at the start of the frame; only it receives input

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
}

void UiUnloadFonts(void)
{
    if (gFonts.ownsText) UnloadFont(gFonts.text);
    if (gFonts.ownsMono) UnloadFont(gFonts.mono);
    gFonts.ownsText = gFonts.ownsMono = false;
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
    return UiButtonEx(r, label, 22, COL_ACCENT, COL_ACCENT_HI, COL_BG, enabled);
}

void UiFocus(char *buf) { focused = buf; }
bool UiHasFocus(const char *buf) { return focused == buf; }

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

#define SYN_DEFAULT  (Color){ 220, 223, 228, 255 }
#define SYN_KEYWORD  (Color){ 198, 120, 221, 255 }
#define SYN_TYPE     (Color){ 86, 182, 194, 255 }
#define SYN_STRING   (Color){ 152, 195, 121, 255 }
#define SYN_NUMBER   (Color){ 209, 154, 102, 255 }
#define SYN_COMMENT  (Color){ 110, 118, 135, 255 }
#define SYN_PREPROC  (Color){ 224, 108, 117, 255 }

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

// Draws one line of code; inBlockComment carries /* ... */ state across lines.
static void DrawCodeLine(const char *s, int len, float x, float y, float size, bool *inBlockComment)
{
    int i = 0;
    int firstNonSpace = 0;
    while (firstNonSpace < len && s[firstNonSpace] == ' ') firstNonSpace++;

    while (i < len) {
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
        DrawToken(s + start, i - start, &x, y, size, c);
    }
}

float UiCodeBlock(const char *code, Vector2 pos, float width, float size, bool draw)
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
        DrawCodeLine(p, len, pos.x + pad, y, size, &inBlockComment);
        y += lineH;
        if (!end) break;
        p = end + 1;
    }
    return h;
}
