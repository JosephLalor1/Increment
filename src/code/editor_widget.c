// The code editor on screen: draws an Editor's text, line numbers, selection, marks, hint
// and name suggestions, and turns this frame's mouse and keyboard into the editing
// operations of editor.c. See editor.h.
#include "code/editor.h"
#include "code/editor_internal.h"
#include "code/code_text.h"
#include "ui/ui.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float charW, lineH, gutter;
    Rectangle text;  // visible text area
} Layout;

static Layout MakeLayout(const Editor *e, Rectangle r)
{
    Layout L;
    float size = e->fontSize;
    float sp = UiSpacing(gFonts.mono, size);
    L.charW = MeasureTextEx(gFonts.mono, "MMMMMMMMMM", size, sp).x / 10.0f + sp * 0.1f;
    L.lineH = roundf(size * 1.45f);
    int digits = 1;
    for (int n = EditorLineCount(e); n >= 10; n /= 10) digits++;
    if (digits < 2) digits = 2;
    L.gutter = e->code ? (digits + 2) * L.charW : 6;
    L.text = (Rectangle){ r.x + L.gutter + 8, r.y + 8, r.width - L.gutter - 16, r.height - 16 };
    return L;
}

// The longest line's length in characters, for horizontal scrolling.
static int LongestLine(const Editor *e)
{
    int longest = 0;
    for (int i = 0, col = 0; i <= e->len; i++) {
        if (i == e->len || e->text[i] == '\n') { longest = MaxInt(longest, col); col = 0; }
        else col++;
    }
    return longest;
}

// Where the scroll bars go and how much they scroll: a vertical bar on the right edge and
// a horizontal one along the bottom, each only when the text doesn't fit that way.
Vector2 EditorTextPoint(const Editor *e, Rectangle r, int pos)
{
    Layout L = MakeLayout(e, r);
    return (Vector2){ L.text.x - e->scrollX + EditorColOf(e, pos) * L.charW,
                      L.text.y - e->scrollY + EditorLineOf(e, pos) * L.lineH + L.lineH / 2 };
}

typedef struct {
    Rectangle vertical, horizontal;
    bool showVertical, showHorizontal;
    float contentH, contentW;  // the whole text's size, as the bars scroll it
} ScrollBars;

static ScrollBars MakeScrollBars(const Editor *e, Rectangle r, const Layout *L)
{
    ScrollBars b;
    b.contentH = EditorLineCount(e) * L->lineH + L->lineH;  // as far as scrollY can go, plus the view
    b.contentW = LongestLine(e) * L->charW + L->charW * 4;
    // Shown only when the text itself overflows (the extra line and columns of slack the
    // editor allows past the end don't count).
    b.showVertical = EditorLineCount(e) * L->lineH > L->text.height;
    b.showHorizontal = LongestLine(e) * L->charW > L->text.width;
    b.vertical = (Rectangle){ r.x + r.width - 13, r.y + 5, 10, r.height - 10 - (b.showHorizontal ? 12 : 0) };
    b.horizontal = (Rectangle){ L->text.x, r.y + r.height - 13, r.x + r.width - L->text.x - (b.showVertical ? 18 : 6), 10 };
    return b;
}

static int PosFromPoint(const Editor *e, const Layout *L, Vector2 m)
{
    int line = (int)floorf((m.y - L->text.y + e->scrollY) / L->lineH);
    int lines = EditorLineCount(e);
    if (line < 0) return 0;
    if (line >= lines) return e->len;
    int col = (int)roundf((m.x - L->text.x + e->scrollX) / L->charW);
    return EditorPosOf(e, line, col);
}

static void SelectWordAt(Editor *e, int pos)
{
    int a = pos, b = pos;
    while (a > 0 && CodeIsWordChar(e->text[a - 1])) a--;
    while (b < e->len && CodeIsWordChar(e->text[b])) b++;
    if (a == b && b < e->len) b++;  // not on a word: select the single character
    e->anchor = a;
    e->cursor = b;
}

#define PRESSED(k) (IsKeyPressed(k) || IsKeyPressedRepeat(k))

static void HandleKeys(Editor *e)
{
    double now = GetTime();
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    // While names are suggested, the list takes Up, Down, Tab and Esc, and Enter once the
    // learner has chosen one (before that, Enter is still a new line: nobody should get a
    // name they didn't ask for at the end of a line).
    bool listTookKey = false;
    if (EditorSuggesting(e)) {
        bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        if (PRESSED(KEY_DOWN) || PRESSED(KEY_UP)) {
            EditorChooseSuggestion(e, PRESSED(KEY_DOWN) ? 1 : -1);
            listTookKey = true;
        } else if (!ctrl && (IsKeyPressed(KEY_TAB) || (enter && e->suggestionPicked))) {
            EditorAcceptSuggestion(e, e->suggestionChosen, now);
            listTookKey = true;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            EditorDismissSuggestions(e);
            listTookKey = true;
        }
    }
    if (listTookKey) {
        while (GetCharPressed() != 0) {}  // nothing else happens on this frame's keys
        return;
    }

    // Typing or deleting part of a word brings suggestions for it; anything else that
    // changes the text or moves the caret leaves them out of date (see EditorSuggesting).
    bool wordEdited = false;
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (!ctrl && ch >= 32 && ch < 127) {
            EditorTypeChar(e, (char)ch, now);
            wordEdited = true;
        }
    }

    if (!ctrl && (PRESSED(KEY_ENTER) || PRESSED(KEY_KP_ENTER))) EditorNewline(e, now);
    if (PRESSED(KEY_BACKSPACE)) {
        if (ctrl && !EditorHasSelection(e)) EditorMoveCursor(e, MOVE_WORD_LEFT, true);
        EditorBackspace(e, now);
        wordEdited = !ctrl;
    }
    if (PRESSED(KEY_DELETE)) {
        if (ctrl && !EditorHasSelection(e)) EditorMoveCursor(e, MOVE_WORD_RIGHT, true);
        EditorDeleteForward(e, now);
    }
    if (PRESSED(KEY_TAB) && !ctrl) EditorIndent(e, shift, now);

    if (PRESSED(KEY_LEFT)) EditorMoveCursor(e, ctrl ? MOVE_WORD_LEFT : MOVE_LEFT, shift);
    if (PRESSED(KEY_RIGHT)) EditorMoveCursor(e, ctrl ? MOVE_WORD_RIGHT : MOVE_RIGHT, shift);
    if (PRESSED(KEY_UP)) EditorMoveCursor(e, MOVE_UP, shift);
    if (PRESSED(KEY_DOWN)) EditorMoveCursor(e, MOVE_DOWN, shift);
    if (PRESSED(KEY_HOME)) EditorMoveCursor(e, ctrl ? MOVE_DOC_START : MOVE_HOME, shift);
    if (PRESSED(KEY_END)) EditorMoveCursor(e, ctrl ? MOVE_DOC_END : MOVE_END, shift);
    if (PRESSED(KEY_PAGE_UP)) EditorMoveCursor(e, MOVE_PAGE_UP, shift);
    if (PRESSED(KEY_PAGE_DOWN)) EditorMoveCursor(e, MOVE_PAGE_DOWN, shift);

    if (ctrl) {
        if (IsKeyPressed(KEY_A)) EditorSelectAll(e);
        if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_X)) {
            char *s = EditorCopySelection(e);
            if (s) {
                SetClipboardText(s);
                free(s);
                if (IsKeyPressed(KEY_X)) EditorDeleteSelection(e, now);
            }
        }
        if (IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip) EditorInsertText(e, clip, now);
        }
        if (PRESSED(KEY_Z)) { if (shift) EditorRedo(e); else EditorUndo(e); }
        if (PRESSED(KEY_Y)) EditorRedo(e);
    }
    if (wordEdited) EditorSuggest(e);
}

// The list of suggested names under the word being typed (above it when there is no room
// below), kept inside the editor. Remembers where it is for clicks.
static void DrawSuggestions(Editor *e, Rectangle r, const Layout *L)
{
    const CompletionList *list = &e->suggestions;
    int longest = 0;
    for (int i = 0; i < list->count; i++)
        longest = MaxInt(longest, (int)(strlen(list->items[i].name) + strlen(list->items[i].after)));
    float rowH = L->lineH + 4;
    Rectangle box = { 0, 0, longest * L->charW + 28, list->count * rowH + 8 };
    float wordX = L->text.x - e->scrollX + (EditorColOf(e, e->cursor) - list->typed) * L->charW;
    float lineY = L->text.y + EditorLineOf(e, e->cursor) * L->lineH - e->scrollY;
    box.x = fmaxf(r.x + 4, fminf(wordX - 14, r.x + r.width - box.width - 4));
    box.y = lineY + L->lineH + 4;
    if (box.y + box.height > r.y + r.height - 4) box.y = lineY - box.height - 4;
    e->suggestionBox = box;

    DrawRectangleRounded((Rectangle){ box.x + 2, box.y + 4, box.width, box.height }, 0.12f, 8, Fade(BLACK, 0.25f));
    UiRoundRect(box, 8, COL_PANEL);
    DrawRectangleRoundedLinesEx(box, 8 * 2 / fminf(box.width, box.height), 8, 1.5f, Fade(COL_ACCENT, 0.6f));
    float sp = UiSpacing(gFonts.mono, e->fontSize);
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < list->count; i++) {
        const Completion *item = &list->items[i];
        Rectangle row = { box.x + 4, box.y + 4 + i * rowH, box.width - 8, rowH };
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (hovered) UiWantHand();
        if (i == e->suggestionChosen) UiRoundRect(row, 6, Fade(COL_ACCENT, 0.28f));
        else if (hovered) UiRoundRect(row, 6, COL_PANEL_HI);
        // The letters already typed in the accent colour, the rest of the name plain, and
        // the shape that comes with it dim.
        float x = row.x + 10, y = row.y + 4;
        char part[COMPLETION_NAME];
        snprintf(part, sizeof part, "%.*s", list->typed, item->name);
        DrawTextEx(gFonts.mono, part, (Vector2){ x, y }, e->fontSize, sp, COL_ACCENT);
        DrawTextEx(gFonts.mono, item->name + list->typed, (Vector2){ x + list->typed * L->charW, y }, e->fontSize, sp, COL_TEXT);
        DrawTextEx(gFonts.mono, item->after, (Vector2){ x + strlen(item->name) * L->charW, y }, e->fontSize, sp, COL_DIM);
    }
}

void EditorWidget(Editor *e, Rectangle r)
{
    Layout L = MakeLayout(e, r);
    int lines = EditorLineCount(e);
    e->pageLines = MaxInt(1, (int)(L.text.height / L.lineH) - 1);
    bool over = UiMouseOver(r);
    if (over) UiWantCursor(MOUSE_CURSOR_IBEAM);

    // Mouse: click to place the caret, drag or shift-click to select, double-click for a word.
    // A click on a scroll bar is the bar's (it's handled when the bars are drawn, below).
    Vector2 m = GetMousePosition();
    ScrollBars bars = MakeScrollBars(e, r, &L);
    bool onBar = (bars.showVertical && UiMouseOver(bars.vertical)) || (bars.showHorizontal && UiMouseOver(bars.horizontal));
    // A click on a suggested name (where the list was drawn last frame) picks it.
    bool onSuggestions = EditorSuggesting(e) && e->suggestionBox.width > 0 && CheckCollisionPointRec(m, e->suggestionBox);
    if (onSuggestions && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        float rowH = (e->suggestionBox.height - 8) / e->suggestions.count;
        EditorAcceptSuggestion(e, (int)((m.y - e->suggestionBox.y - 4) / rowH), GetTime());
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !onBar) {
        if (over) {
            UiFocus(e);
            int pos = PosFromPoint(e, &L, m);
            double now = GetTime();
            e->clickCount = (now - e->lastClickTime < 0.35) ? e->clickCount + 1 : 1;
            e->lastClickTime = now;
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (e->clickCount == 2) {
                SelectWordAt(e, pos);
            } else if (e->clickCount >= 3) {  // whole line
                e->anchor = EditorPosOf(e, EditorLineOf(e, pos), 0);
                e->cursor = MinInt(EditorLineEnd(e, pos) + 1, e->len);
            } else {
                EditorSetCursor(e, pos, shift);
                e->dragging = true;
            }
        } else if (UiHasFocus(e)) {
            UiFocus(NULL);
        }
    }
    if (e->dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) EditorSetCursor(e, PosFromPoint(e, &L, m), true);
        else e->dragging = false;
    }
    if (over) {
        float wheel = GetMouseWheelMove();
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) e->scrollX -= wheel * L.charW * 6;
        else e->scrollY -= wheel * L.lineH * 3;
    }

    bool focused = UiHasFocus(e);
    if (UiTakesInput(e)) HandleKeys(e);
    lines = EditorLineCount(e);

    // Keep the caret visible after it moves, then clamp scrolling to the content.
    int maxCols = LongestLine(e);
    if (e->revealCursor) {
        float cy = EditorLineOf(e, e->cursor) * L.lineH;
        float cx = EditorColOf(e, e->cursor) * L.charW;
        if (cy < e->scrollY) e->scrollY = cy;
        if (cy + L.lineH > e->scrollY + L.text.height) e->scrollY = cy + L.lineH - L.text.height;
        if (cx < e->scrollX + L.charW * 2) e->scrollX = cx - L.charW * 2;
        if (cx > e->scrollX + L.text.width - L.charW * 4) e->scrollX = cx - L.text.width + L.charW * 4;
        e->revealCursor = false;
    }
    float maxY = lines * L.lineH - L.text.height + L.lineH;
    float maxX = maxCols * L.charW - L.text.width + L.charW * 4;
    e->scrollY = fmaxf(0, fminf(e->scrollY, fmaxf(0, maxY)));
    e->scrollX = fmaxf(0, fminf(e->scrollX, fmaxf(0, maxX)));

    // --- Drawing ---
    UiRoundRect(r, 8, COL_CODE_BG);
    if (focused) DrawRectangleRoundedLinesEx(r, 16.0f / fminf(r.width, r.height), 8, 1.5f, Fade(COL_ACCENT, 0.7f));

    int first = MaxInt(0, (int)(e->scrollY / L.lineH));
    int visible = (int)(L.text.height / L.lineH) + 2;
    int curLine = EditorLineOf(e, e->cursor);
    int sa = EditorSelectionStart(e), sb = EditorSelectionEnd(e);
    float sp = UiSpacing(gFonts.mono, e->fontSize);
    bool inComment = false;

    BeginScissorMode((int)(L.text.x - 4), (int)r.y + 2, (int)(L.text.width + 8), (int)r.height - 4);
    int ls = 0;
    for (int line = 0; line < lines && line < first + visible; line++) {
        int le = EditorLineEnd(e, ls);
        float y = L.text.y + line * L.lineH - e->scrollY;
        if (line >= first) {
            float x0 = L.text.x - e->scrollX;
            bool hinted = e->hintLine == line + 1 && !e->hintBefore;
            if (EditorLineMarked(e, line + 1)) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_BAD, 0.14f));
            } else if (hinted) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_ACCENT, 0.18f));
            } else if (focused && sa == sb && line == curLine) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_PANEL_HI, 0.55f));
            }
            if (sa < sb && sa <= le && sb >= ls) {  // selection on this line
                int a = MaxInt(sa, ls), b = MinInt(sb, le);
                float w = (b - a) * L.charW + (sb > le ? L.charW * 0.6f : 0);
                DrawRectangle((int)(x0 + (a - ls) * L.charW), (int)y, (int)ceilf(w), (int)L.lineH, Fade(COL_ACCENT, 0.32f));
            }
        }

        // Syntax colouring runs on every line above the view too, to track /* */ state.
        char tmp[1024];
        int n = MinInt(le - ls, (int)sizeof tmp - 1);
        if (e->code) {
            UiCodeToken tokens[256];
            int count = UiCodeTokens(e->text + ls, n, &inComment, tokens, 256);
            if (line >= first) {
                for (int t = 0; t < count; t++) {
                    memcpy(tmp, e->text + ls + tokens[t].start, (size_t)tokens[t].len);
                    tmp[tokens[t].len] = '\0';
                    DrawTextEx(gFonts.mono, tmp, (Vector2){ L.text.x - e->scrollX + tokens[t].start * L.charW, y + 2 },
                               e->fontSize, sp, tokens[t].color);
                }
            }
        } else if (line >= first) {
            memcpy(tmp, e->text + ls, (size_t)n);
            tmp[n] = '\0';
            DrawTextEx(gFonts.mono, tmp, (Vector2){ L.text.x - e->scrollX, y + 2 }, e->fontSize, sp, COL_TEXT);
        }
        ls = le + 1;
    }

    // A hint for a missing line: a bar in the gap where it belongs.
    if (e->hintLine && e->hintBefore) {
        float y = L.text.y + (e->hintLine - 1) * L.lineH - e->scrollY;
        DrawRectangle((int)(L.text.x - 4), (int)y - 2, (int)L.text.width + 8, 4, COL_ACCENT);
    }
    if (e->len == 0 && !focused && e->placeholder) {
        DrawTextEx(gFonts.text, e->placeholder, (Vector2){ L.text.x, L.text.y + 2 }, e->fontSize, 0, COL_DIM);
    }
    if (focused && fmod(GetTime(), 1.0) < 0.6) {
        float cx = L.text.x - e->scrollX + EditorColOf(e, e->cursor) * L.charW;
        float cy = L.text.y + curLine * L.lineH - e->scrollY;
        DrawRectangle((int)cx - 1, (int)cy + 2, 2, (int)L.lineH - 4, COL_TEXT);
    }
    EndScissorMode();

    // Line numbers, with a red dot on lines that have compiler errors.
    if (e->code) {
        BeginScissorMode((int)r.x, (int)r.y + 2, (int)(L.gutter + 4), (int)r.height - 4);
        for (int line = first; line < lines && line < first + visible; line++) {
            float y = L.text.y + line * L.lineH - e->scrollY;
            const char *num = TextFormat("%d", line + 1);
            float w = MeasureTextEx(gFonts.mono, num, e->fontSize * 0.85f, sp).x;
            bool marked = EditorLineMarked(e, line + 1);
            bool hinted = e->hintLine == line + 1 && !e->hintBefore;
            Color c = marked ? COL_BAD : hinted ? COL_ACCENT : (line == curLine && focused ? COL_TEXT : Fade(COL_DIM, 0.7f));
            DrawTextEx(gFonts.mono, num, (Vector2){ r.x + L.gutter - w, y + 4 }, e->fontSize * 0.85f, sp, c);
            if (marked || hinted) DrawCircleV((Vector2){ r.x + 9, y + L.lineH / 2 }, 3.5f, marked ? COL_BAD : COL_ACCENT);
        }
        EndScissorMode();
    }

    bars = MakeScrollBars(e, r, &L);  // the text may have changed since the click handling
    if (bars.showVertical) UiScrollBar(bars.vertical, &e->scrollY, bars.contentH, L.text.height);
    if (bars.showHorizontal) UiScrollBar(bars.horizontal, &e->scrollX, bars.contentW, L.text.width);

    if (focused && EditorSuggesting(e)) {
        DrawSuggestions(e, r, &L);
        UiClaimEscape();  // Esc closes the list instead of leaving the editor
    } else {
        e->suggestionBox = (Rectangle){ 0 };
    }
}
