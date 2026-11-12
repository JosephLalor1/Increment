#include "editor.h"
#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Buffer helpers

static void Reserve(Editor *e, int need)
{
    if (need + 1 <= e->cap) return;
    int cap = e->cap ? e->cap : 256;
    while (cap < need + 1) cap *= 2;
    e->text = realloc(e->text, (size_t)cap);
    e->cap = cap;
}

static bool IsWord(char c) { return isalnum((unsigned char)c) || c == '_'; }
static int Min(int a, int b) { return a < b ? a : b; }
static int Max(int a, int b) { return a > b ? a : b; }
static int SelMin(const Editor *e) { return Min(e->cursor, e->anchor); }
static int SelMax(const Editor *e) { return Max(e->cursor, e->anchor); }

static int LineStart(const Editor *e, int pos)
{
    while (pos > 0 && e->text[pos - 1] != '\n') pos--;
    return pos;
}

static int LineEnd(const Editor *e, int pos)
{
    while (pos < e->len && e->text[pos] != '\n') pos++;
    return pos;
}

static void RawInsert(Editor *e, int at, const char *s, int n)
{
    if (n <= 0) return;
    Reserve(e, e->len + n);
    memmove(e->text + at + n, e->text + at, (size_t)(e->len - at + 1));
    memcpy(e->text + at, s, (size_t)n);
    e->len += n;
    e->version++;
}

static void RawDelete(Editor *e, int a, int b)
{
    if (b <= a) return;
    memmove(e->text + a, e->text + b, (size_t)(e->len - b + 1));
    e->len -= b - a;
    e->version++;
}

// Keeps printable ASCII and newlines; tabs become 4 spaces; everything else is dropped.
static char *Filter(const char *s, int *outLen)
{
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 1);
    int o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\t') { memcpy(out + o, "    ", 4); o += 4; }
        else if (c == '\n' || (c >= 32 && c < 127)) out[o++] = (char)c;
    }
    out[o] = '\0';
    *outLen = o;
    return out;
}

// ---------------------------------------------------------------------------
// Undo

static void FreeSnap(EditorSnapshot *s)
{
    free(s->text);
    s->text = NULL;
}

static EditorSnapshot Snap(const Editor *e)
{
    EditorSnapshot s;
    s.text = malloc((size_t)e->len + 1);
    memcpy(s.text, e->text, (size_t)e->len + 1);
    s.len = e->len;
    s.cursor = e->cursor;
    s.anchor = e->anchor;
    return s;
}

static void PushStack(EditorSnapshot *stack, int *count, EditorSnapshot s)
{
    if (*count == EDITOR_MAX_UNDO) {
        FreeSnap(&stack[0]);
        memmove(stack, stack + 1, sizeof stack[0] * (EDITOR_MAX_UNDO - 1));
        (*count)--;
    }
    stack[(*count)++] = s;
}

static void ClearStack(EditorSnapshot *stack, int *count)
{
    for (int i = 0; i < *count; i++) FreeSnap(&stack[i]);
    *count = 0;
}

// Call before changing the text. Consecutive edits of the same kind within a second
// share one undo step, so undo removes a typed word rather than a single letter.
static void BeginEdit(Editor *e, EditKind kind, double now)
{
    bool group = kind != EDIT_OTHER && kind == e->lastKind && now - e->lastEditTime < 1.0;
    e->lastKind = kind;
    e->lastEditTime = now;
    ClearStack(e->redo, &e->redoCount);
    if (!group) PushStack(e->undo, &e->undoCount, Snap(e));
}

static void Restore(Editor *e, EditorSnapshot s)
{
    free(e->text);
    e->text = s.text;
    e->len = s.len;
    e->cap = s.len + 1;
    e->cursor = Min(s.cursor, s.len);
    e->anchor = Min(s.anchor, s.len);
    e->preferredCol = -1;
    e->lastKind = EDIT_NONE;
    e->revealCursor = true;
    e->version++;
}

void EditorUndo(Editor *e)
{
    if (e->readOnly || e->undoCount == 0) return;
    PushStack(e->redo, &e->redoCount, Snap(e));
    Restore(e, e->undo[--e->undoCount]);
}

void EditorRedo(Editor *e)
{
    if (e->readOnly || e->redoCount == 0) return;
    PushStack(e->undo, &e->undoCount, Snap(e));
    Restore(e, e->redo[--e->redoCount]);
}

// ---------------------------------------------------------------------------
// Setup

void EditorInit(Editor *e, const char *text, bool code)
{
    memset(e, 0, sizeof *e);
    e->code = code;
    e->fontSize = 19;
    e->preferredCol = -1;
    EditorSetText(e, text ? text : "");
}

void EditorFree(Editor *e)
{
    free(e->text);
    ClearStack(e->undo, &e->undoCount);
    ClearStack(e->redo, &e->redoCount);
    e->text = NULL;
    e->len = e->cap = 0;
}

void EditorSetText(Editor *e, const char *text)
{
    int n;
    char *clean = Filter(text, &n);
    free(e->text);
    e->text = clean;
    e->len = n;
    e->cap = n + 1;
    e->cursor = e->anchor = 0;
    e->scrollX = e->scrollY = 0;
    e->preferredCol = -1;
    e->lastKind = EDIT_NONE;
    e->markCount = 0;
    e->hintLine = 0;
    ClearStack(e->undo, &e->undoCount);
    ClearStack(e->redo, &e->redoCount);
    e->version++;
}

void EditorReplaceAll(Editor *e, const char *text, double now)
{
    if (e->readOnly) { EditorSetText(e, text); return; }
    BeginEdit(e, EDIT_OTHER, now);
    int n;
    char *clean = Filter(text, &n);
    RawDelete(e, 0, e->len);
    RawInsert(e, 0, clean, n);
    free(clean);
    e->cursor = e->anchor = 0;
    e->revealCursor = true;
}

// ---------------------------------------------------------------------------
// Editing

bool EditorHasSelection(const Editor *e) { return e->cursor != e->anchor; }

static bool RemoveSelection(Editor *e)
{
    if (!EditorHasSelection(e)) return false;
    int a = SelMin(e), b = SelMax(e);
    RawDelete(e, a, b);
    e->cursor = e->anchor = a;
    return true;
}

static void InsertAtCursor(Editor *e, const char *s, int n)
{
    RemoveSelection(e);
    RawInsert(e, e->cursor, s, n);
    e->cursor += n;
    e->anchor = e->cursor;
    e->preferredCol = -1;
    e->revealCursor = true;
}

void EditorInsertText(Editor *e, const char *s, double now)
{
    if (e->readOnly) return;
    int n;
    char *clean = Filter(s, &n);
    if (n > 0 || EditorHasSelection(e)) {
        BeginEdit(e, EDIT_OTHER, now);
        InsertAtCursor(e, clean, n);
    }
    free(clean);
}

// ---------------------------------------------------------------------------
// Name suggestions (code mode)

void EditorDismissSuggestions(Editor *e)
{
    e->suggestions.count = 0;
    e->suggestionBox = (Rectangle){ 0 };
}

void EditorSuggest(Editor *e)
{
    EditorDismissSuggestions(e);
    if (!e->code || e->readOnly || EditorHasSelection(e)) return;
    CompletionFind(e->text, e->len, e->cursor, &e->suggestions);
    e->suggestedVersion = e->version;
    e->suggestedCursor = e->cursor;
    e->suggestionChosen = 0;
    e->suggestionPicked = false;
}

bool EditorSuggesting(const Editor *e)
{
    return e->suggestions.count > 0 && e->suggestedVersion == e->version && e->suggestedCursor == e->cursor
        && !EditorHasSelection(e);
}

void EditorChooseSuggestion(Editor *e, int step)
{
    if (!EditorSuggesting(e)) return;
    int count = e->suggestions.count;
    e->suggestionChosen = ((e->suggestionChosen + step) % count + count) % count;
    e->suggestionPicked = true;
}

bool EditorAcceptSuggestion(Editor *e, int index, double now)
{
    if (!EditorSuggesting(e) || index < 0 || index >= e->suggestions.count) return false;
    Completion chosen = e->suggestions.items[index];
    int typed = e->suggestions.typed;
    BeginEdit(e, EDIT_OTHER, now);
    InsertAtCursor(e, chosen.name + typed, (int)strlen(chosen.name) - typed);
    InsertAtCursor(e, chosen.after, (int)strlen(chosen.after));
    e->cursor -= chosen.caretBack;  // inside the shape's brackets or quotes
    e->anchor = e->cursor;
    EditorDismissSuggestions(e);
    return true;
}

// ---------------------------------------------------------------------------
// Bracket and quote pairs (code mode)

// The character that closes c, or 0 if c doesn't open a pair. Quotes close themselves.
static char CloserOf(char c)
{
    switch (c) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    case '"': return '"';
    case '\'': return '\'';
    default:  return 0;
    }
}

static bool IsCloser(char c) { return c == ')' || c == ']' || c == '}' || c == '"' || c == '\''; }

// How many times c appears on the caret's line.
static int CountOnLine(const Editor *e, char c)
{
    int count = 0;
    for (int i = LineStart(e, e->cursor); i < LineEnd(e, e->cursor); i++) count += e->text[i] == c;
    return count;
}

// Whether typing the closer c should just step over the same character already after the
// caret: only when that one already completes its pair on this line, so a closer that is
// really missing still gets typed ("f(g(x|)" needs another bracket; "f(x|)" doesn't).
static bool StepsOverCloser(const Editor *e, char c)
{
    if (e->cursor >= e->len || e->text[e->cursor] != c) return false;
    if (c == '"' || c == '\'') return CountOnLine(e, c) % 2 == 0;
    char opener = c == ')' ? '(' : c == ']' ? '[' : '{';
    return CountOnLine(e, c) >= CountOnLine(e, opener);
}

// Whether typing the opener c should bring its closer with it: not in front of a word
// (the learner is wrapping something by hand), and for quotes not when the quote ends a
// string or is an apostrophe after a letter.
static bool PairsUp(const Editor *e, char c)
{
    char next = e->cursor < e->len ? e->text[e->cursor] : '\n';
    if (!strchr(" \n)]};,", next)) return false;
    if (c != '"' && c != '\'') return true;
    char previous = e->cursor > 0 ? e->text[e->cursor - 1] : ' ';
    if (IsWord(previous) || previous == c || previous == '\\') return false;
    return CountOnLine(e, c) % 2 == 0;  // an odd count means this quote closes one
}

// "}" typed on a blank line, when the block's own "}" (brought by its "{") waits on the next
// line and every brace in the text has its partner: the learner is closing the block by
// hand, so the blank line goes and the caret lands after the brace that is already there,
// instead of leaving one brace too many. Returns whether it did.
static bool JoinsWaitingBrace(Editor *e)
{
    int ls = LineStart(e, e->cursor), le = LineEnd(e, e->cursor);
    for (int i = ls; i < le; i++) if (e->text[i] != ' ') return false;
    if (le >= e->len) return false;  // no next line
    int brace = le + 1;
    while (brace < e->len && e->text[brace] == ' ') brace++;
    if (brace >= e->len || e->text[brace] != '}') return false;
    int depth = 0;
    for (int i = 0; i < e->len; i++) depth += (e->text[i] == '{') - (e->text[i] == '}');
    if (depth != 0) return false;
    RawDelete(e, ls, le + 1);  // the blank line and its newline
    e->cursor = e->anchor = brace - (le + 1 - ls) + 1;
    e->preferredCol = -1;
    e->revealCursor = true;
    return true;
}

void EditorTypeChar(Editor *e, char c, double now)
{
    if (e->readOnly || c < 32 || c > 126) return;
    BeginEdit(e, EDIT_TYPE, now);

    // An opener typed over a selection wraps it, and keeps it selected.
    if (e->code && CloserOf(c) && EditorHasSelection(e)) {
        int a = SelMin(e), b = SelMax(e);
        char closer = CloserOf(c);
        RawInsert(e, b, &closer, 1);
        RawInsert(e, a, &c, 1);
        e->anchor = a + 1;
        e->cursor = b + 1;
        e->preferredCol = -1;
        e->revealCursor = true;
        return;
    }
    RemoveSelection(e);
    if (e->code && c == '}' && JoinsWaitingBrace(e)) return;
    if (e->code && IsCloser(c) && StepsOverCloser(e, c)) {
        e->cursor++;
        e->anchor = e->cursor;
        e->preferredCol = -1;
        e->revealCursor = true;
        return;
    }
    if (e->code && CloserOf(c) && PairsUp(e, c)) {  // "(" brings ")" and leaves the caret between
        char pair[2] = { c, CloserOf(c) };
        InsertAtCursor(e, pair, 2);
        e->cursor--;
        e->anchor = e->cursor;
        return;
    }

    // A closing brace typed on a line that is only indentation steps back one level.
    if (c == '}' && e->code) {
        int ls = LineStart(e, e->cursor);
        bool blank = true;
        for (int i = ls; i < e->cursor; i++) if (e->text[i] != ' ') blank = false;
        int indent = e->cursor - ls;
        if (blank && indent >= 4) {
            RawDelete(e, e->cursor - 4, e->cursor);
            e->cursor -= 4;
            e->anchor = e->cursor;
        }
    }
    InsertAtCursor(e, &c, 1);
}

void EditorNewline(Editor *e, double now)
{
    if (e->readOnly) return;
    BeginEdit(e, EDIT_OTHER, now);
    RemoveSelection(e);
    if (!e->code) {
        InsertAtCursor(e, "\n", 1);
        return;
    }

    int ls = LineStart(e, e->cursor);
    int indent = 0;
    while (ls + indent < e->cursor && e->text[ls + indent] == ' ') indent++;

    int before = e->cursor - 1;  // last non-space character before the caret
    while (before >= ls && e->text[before] == ' ') before--;
    bool opens = before >= ls && e->text[before] == '{';
    bool closesNext = e->cursor < e->len && e->text[e->cursor] == '}';

    char buf[512];
    int n = 0;
    int inner = opens ? indent + 4 : indent;
    if (inner > 400) inner = 400;
    buf[n++] = '\n';
    for (int i = 0; i < inner; i++) buf[n++] = ' ';
    int caretAt = n;
    if (opens && closesNext) {  // "{|}" becomes a block with the caret inside
        buf[n++] = '\n';
        for (int i = 0; i < indent && i < 400; i++) buf[n++] = ' ';
    }
    int start = e->cursor;
    InsertAtCursor(e, buf, n);
    e->cursor = e->anchor = start + caretAt;
}

void EditorBackspace(Editor *e, double now)
{
    if (e->readOnly) return;
    if (EditorHasSelection(e)) {
        BeginEdit(e, EDIT_DELETE, now);
        RemoveSelection(e);
    } else if (e->cursor > 0) {
        BeginEdit(e, EDIT_DELETE, now);
        int from = e->cursor - 1;
        if (e->code) {  // inside the indentation, remove back to the previous 4-column stop
            int ls = LineStart(e, e->cursor);
            bool blank = true;
            for (int i = ls; i < e->cursor; i++) if (e->text[i] != ' ') blank = false;
            int col = e->cursor - ls;
            if (blank && col > 0) from = e->cursor - ((col - 1) % 4 + 1);
        }
        // Between an empty pair, "(|)", the closer goes with its opener.
        char closer = e->code ? CloserOf(e->text[e->cursor - 1]) : 0;
        if (closer && e->cursor < e->len && e->text[e->cursor] == closer) RawDelete(e, e->cursor, e->cursor + 1);
        RawDelete(e, from, e->cursor);
        e->cursor = e->anchor = from;
    }
    e->preferredCol = -1;
    e->revealCursor = true;
}

void EditorDeleteForward(Editor *e, double now)
{
    if (e->readOnly) return;
    if (EditorHasSelection(e)) {
        BeginEdit(e, EDIT_DELETE, now);
        RemoveSelection(e);
    } else if (e->cursor < e->len) {
        BeginEdit(e, EDIT_DELETE, now);
        RawDelete(e, e->cursor, e->cursor + 1);
    }
    e->preferredCol = -1;
    e->revealCursor = true;
}

void EditorDeleteSelection(Editor *e, double now)
{
    if (e->readOnly || !EditorHasSelection(e)) return;
    BeginEdit(e, EDIT_DELETE, now);
    RemoveSelection(e);
    e->revealCursor = true;
}

void EditorIndent(Editor *e, bool outdent, double now)
{
    if (e->readOnly) return;

    // No selection and Tab: insert spaces up to the next 4-column stop.
    if (!EditorHasSelection(e) && !outdent) {
        int col = e->cursor - LineStart(e, e->cursor);
        BeginEdit(e, EDIT_TYPE, now);
        InsertAtCursor(e, "    ", 4 - col % 4);
        return;
    }

    int first = EditorLineOf(e, SelMin(e));
    int last = EditorLineOf(e, SelMax(e));
    // A selection ending at the start of a line doesn't include that line.
    if (last > first && SelMax(e) == LineStart(e, SelMax(e))) last--;

    BeginEdit(e, EDIT_OTHER, now);
    // Work bottom-up so earlier line starts don't move.
    for (int line = last; line >= first; line--) {
        int ls = EditorPosOf(e, line, 0);
        if (!outdent) {
            RawInsert(e, ls, "    ", 4);
            if (e->cursor >= ls) e->cursor += 4;
            if (e->anchor >= ls) e->anchor += 4;
        } else {
            int k = 0;
            while (k < 4 && ls + k < e->len && e->text[ls + k] == ' ') k++;
            RawDelete(e, ls, ls + k);
            if (e->cursor > ls) e->cursor -= Min(k, e->cursor - ls);
            if (e->anchor > ls) e->anchor -= Min(k, e->anchor - ls);
        }
    }
    e->revealCursor = true;
}

// ---------------------------------------------------------------------------
// Positions and movement

int EditorLineCount(const Editor *e)
{
    int n = 1;
    for (int i = 0; i < e->len; i++) if (e->text[i] == '\n') n++;
    return n;
}

int EditorLineOf(const Editor *e, int pos)
{
    int line = 0;
    for (int i = 0; i < pos && i < e->len; i++) if (e->text[i] == '\n') line++;
    return line;
}

int EditorColOf(const Editor *e, int pos) { return pos - LineStart(e, pos); }

int EditorPosOf(const Editor *e, int line, int col)
{
    int pos = 0;
    for (int l = 0; l < line && pos < e->len; pos++) if (e->text[pos] == '\n') l++;
    int end = LineEnd(e, pos);
    return Min(pos + Max(col, 0), end);
}

void EditorSetCursor(Editor *e, int pos, bool select)
{
    e->cursor = Max(0, Min(pos, e->len));
    if (!select) e->anchor = e->cursor;
    e->preferredCol = -1;
    e->lastKind = EDIT_NONE;  // moving the caret ends the current typing group
    e->revealCursor = true;
}

void EditorMoveCursor(Editor *e, EditorMotion m, bool select)
{
    int pos = e->cursor;
    bool vertical = false;

    switch (m) {
    case MOVE_LEFT:
        if (EditorHasSelection(e) && !select) pos = SelMin(e);
        else if (pos > 0) pos--;
        break;
    case MOVE_RIGHT:
        if (EditorHasSelection(e) && !select) pos = SelMax(e);
        else if (pos < e->len) pos++;
        break;
    case MOVE_UP:
    case MOVE_DOWN:
    case MOVE_PAGE_UP:
    case MOVE_PAGE_DOWN: {
        vertical = true;
        int step = (m == MOVE_UP || m == MOVE_DOWN) ? 1 : Max(1, e->pageLines);
        if (m == MOVE_UP || m == MOVE_PAGE_UP) step = -step;
        int col = e->preferredCol >= 0 ? e->preferredCol : EditorColOf(e, pos);
        int target = EditorLineOf(e, pos) + step;
        if (target < 0) pos = 0;
        else if (target >= EditorLineCount(e)) pos = e->len;
        else pos = EditorPosOf(e, target, col);
        e->preferredCol = col;
    } break;
    case MOVE_WORD_LEFT:
        while (pos > 0 && e->text[pos - 1] == ' ') pos--;
        if (pos > 0 && IsWord(e->text[pos - 1])) { while (pos > 0 && IsWord(e->text[pos - 1])) pos--; }
        else if (pos > 0) pos--;
        break;
    case MOVE_WORD_RIGHT:
        if (pos < e->len && IsWord(e->text[pos])) { while (pos < e->len && IsWord(e->text[pos])) pos++; }
        else if (pos < e->len) pos++;
        while (pos < e->len && e->text[pos] == ' ') pos++;
        break;
    case MOVE_HOME: {
        int ls = LineStart(e, pos), first = ls;
        while (first < e->len && e->text[first] == ' ') first++;
        pos = (pos == first) ? ls : first;
    } break;
    case MOVE_END:
        pos = LineEnd(e, pos);
        break;
    case MOVE_DOC_START:
        pos = 0;
        break;
    case MOVE_DOC_END:
        pos = e->len;
        break;
    }

    int keepCol = e->preferredCol;
    EditorSetCursor(e, pos, select);
    if (vertical) e->preferredCol = keepCol;
}

void EditorSelectAll(Editor *e)
{
    e->anchor = 0;
    e->cursor = e->len;
    e->lastKind = EDIT_NONE;
}

char *EditorCopySelection(const Editor *e)
{
    if (!EditorHasSelection(e)) return NULL;
    int a = SelMin(e), b = SelMax(e);
    char *s = malloc((size_t)(b - a) + 1);
    memcpy(s, e->text + a, (size_t)(b - a));
    s[b - a] = '\0';
    return s;
}

void EditorSetMarks(Editor *e, const int *lines, int count)
{
    e->markCount = Min(count, EDITOR_MAX_MARKS);
    for (int i = 0; i < e->markCount; i++) e->marks[i] = lines[i];
}

void EditorSetHint(Editor *e, int line, bool before)
{
    e->hintLine = line > 0 ? line : 0;
    e->hintBefore = before;
}

static bool Marked(const Editor *e, int line1)
{
    for (int i = 0; i < e->markCount; i++) if (e->marks[i] == line1) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Widget

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
        if (i == e->len || e->text[i] == '\n') { longest = Max(longest, col); col = 0; }
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
    while (a > 0 && IsWord(e->text[a - 1])) a--;
    while (b < e->len && IsWord(e->text[b])) b++;
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
        longest = Max(longest, (int)(strlen(list->items[i].name) + strlen(list->items[i].after)));
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
    e->pageLines = Max(1, (int)(L.text.height / L.lineH) - 1);
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
                e->cursor = Min(LineEnd(e, pos) + 1, e->len);
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

    int first = Max(0, (int)(e->scrollY / L.lineH));
    int visible = (int)(L.text.height / L.lineH) + 2;
    int curLine = EditorLineOf(e, e->cursor);
    int sa = SelMin(e), sb = SelMax(e);
    float sp = UiSpacing(gFonts.mono, e->fontSize);
    bool inComment = false;

    BeginScissorMode((int)(L.text.x - 4), (int)r.y + 2, (int)(L.text.width + 8), (int)r.height - 4);
    int ls = 0;
    for (int line = 0; line < lines && line < first + visible; line++) {
        int le = LineEnd(e, ls);
        float y = L.text.y + line * L.lineH - e->scrollY;
        if (line >= first) {
            float x0 = L.text.x - e->scrollX;
            bool hinted = e->hintLine == line + 1 && !e->hintBefore;
            if (Marked(e, line + 1)) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_BAD, 0.14f));
            } else if (hinted) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_ACCENT, 0.18f));
            } else if (focused && sa == sb && line == curLine) {
                DrawRectangle((int)(L.text.x - 4), (int)y, (int)L.text.width + 8, (int)L.lineH, Fade(COL_PANEL_HI, 0.55f));
            }
            if (sa < sb && sa <= le && sb >= ls) {  // selection on this line
                int a = Max(sa, ls), b = Min(sb, le);
                float w = (b - a) * L.charW + (sb > le ? L.charW * 0.6f : 0);
                DrawRectangle((int)(x0 + (a - ls) * L.charW), (int)y, (int)ceilf(w), (int)L.lineH, Fade(COL_ACCENT, 0.32f));
            }
        }

        // Syntax colouring runs on every line above the view too, to track /* */ state.
        char tmp[1024];
        int n = Min(le - ls, (int)sizeof tmp - 1);
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
            bool marked = Marked(e, line + 1);
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
