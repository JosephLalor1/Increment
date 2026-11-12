// The code editor's editing operations: the text, the caret and selection, undo, bracket
// pairs, indenting and name suggestions. No input and no drawing (editor_widget.c does
// those), so every operation can be unit-tested (tests/test_editor.c). See editor.h.
#include "code/editor.h"
#include "code/editor_internal.h"
#include "code/code_text.h"

#include <ctype.h>
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

int EditorSelectionStart(const Editor *e) { return MinInt(e->cursor, e->anchor); }
int EditorSelectionEnd(const Editor *e) { return MaxInt(e->cursor, e->anchor); }

static int LineStart(const Editor *e, int pos)
{
    while (pos > 0 && e->text[pos - 1] != '\n') pos--;
    return pos;
}

int EditorLineEnd(const Editor *e, int pos)
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
    e->cursor = MinInt(s.cursor, s.len);
    e->anchor = MinInt(s.anchor, s.len);
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
    int a = EditorSelectionStart(e), b = EditorSelectionEnd(e);
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
    for (int i = LineStart(e, e->cursor); i < EditorLineEnd(e, e->cursor); i++) count += e->text[i] == c;
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
    if (CodeIsWordChar(previous) || previous == c || previous == '\\') return false;
    return CountOnLine(e, c) % 2 == 0;  // an odd count means this quote closes one
}

// "}" typed on a blank line, when the block's own "}" (brought by its "{") waits on the next
// line and every brace in the text has its partner: the learner is closing the block by
// hand, so the blank line goes and the caret lands after the brace that is already there,
// instead of leaving one brace too many. Returns whether it did.
static bool JoinsWaitingBrace(Editor *e)
{
    int ls = LineStart(e, e->cursor), le = EditorLineEnd(e, e->cursor);
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
        int a = EditorSelectionStart(e), b = EditorSelectionEnd(e);
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

    int first = EditorLineOf(e, EditorSelectionStart(e));
    int last = EditorLineOf(e, EditorSelectionEnd(e));
    // A selection ending at the start of a line doesn't include that line.
    if (last > first && EditorSelectionEnd(e) == LineStart(e, EditorSelectionEnd(e))) last--;

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
            if (e->cursor > ls) e->cursor -= MinInt(k, e->cursor - ls);
            if (e->anchor > ls) e->anchor -= MinInt(k, e->anchor - ls);
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
    int end = EditorLineEnd(e, pos);
    return MinInt(pos + MaxInt(col, 0), end);
}

void EditorSetCursor(Editor *e, int pos, bool select)
{
    e->cursor = MaxInt(0, MinInt(pos, e->len));
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
        if (EditorHasSelection(e) && !select) pos = EditorSelectionStart(e);
        else if (pos > 0) pos--;
        break;
    case MOVE_RIGHT:
        if (EditorHasSelection(e) && !select) pos = EditorSelectionEnd(e);
        else if (pos < e->len) pos++;
        break;
    case MOVE_UP:
    case MOVE_DOWN:
    case MOVE_PAGE_UP:
    case MOVE_PAGE_DOWN: {
        vertical = true;
        int step = (m == MOVE_UP || m == MOVE_DOWN) ? 1 : MaxInt(1, e->pageLines);
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
        if (pos > 0 && CodeIsWordChar(e->text[pos - 1])) { while (pos > 0 && CodeIsWordChar(e->text[pos - 1])) pos--; }
        else if (pos > 0) pos--;
        break;
    case MOVE_WORD_RIGHT:
        if (pos < e->len && CodeIsWordChar(e->text[pos])) { while (pos < e->len && CodeIsWordChar(e->text[pos])) pos++; }
        else if (pos < e->len) pos++;
        while (pos < e->len && e->text[pos] == ' ') pos++;
        break;
    case MOVE_HOME: {
        int ls = LineStart(e, pos), first = ls;
        while (first < e->len && e->text[first] == ' ') first++;
        pos = (pos == first) ? ls : first;
    } break;
    case MOVE_END:
        pos = EditorLineEnd(e, pos);
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
    int a = EditorSelectionStart(e), b = EditorSelectionEnd(e);
    char *s = malloc((size_t)(b - a) + 1);
    memcpy(s, e->text + a, (size_t)(b - a));
    s[b - a] = '\0';
    return s;
}

void EditorSetMarks(Editor *e, const int *lines, int count)
{
    e->markCount = MinInt(count, EDITOR_MAX_MARKS);
    for (int i = 0; i < e->markCount; i++) e->marks[i] = lines[i];
}

void EditorSetHint(Editor *e, int line, bool before)
{
    e->hintLine = line > 0 ? line : 0;
    e->hintBefore = before;
}

bool EditorLineMarked(const Editor *e, int line1)
{
    for (int i = 0; i < e->markCount; i++) if (e->marks[i] == line1) return true;
    return false;
}
