#ifndef EDITOR_H
#define EDITOR_H

#include "completion.h"

#include "raylib.h"
#include <stdbool.h>

// A multi-line text editor. In code mode it shows line numbers, highlights C/C++ syntax
// and auto-indents, pairs brackets and suggests names as they are typed (a list under the
// caret: Up/Down choose, Tab accepts, Enter accepts once one was chosen, Esc closes it, and a
// click picks one). The text is plain ASCII with '\n' line endings; tabs are stored as
// 4 spaces.
//
// The editing functions below are independent of input and drawing, so they can be
// unit-tested (tests/test_editor.c). `now` is the time in seconds; edits of the same
// kind less than a second apart are undone together.

#define EDITOR_MAX_UNDO  200
#define EDITOR_MAX_MARKS 64

typedef struct {
    char *text;
    int len;
    int cursor, anchor;
} EditorSnapshot;

typedef enum { EDIT_NONE, EDIT_TYPE, EDIT_DELETE, EDIT_OTHER } EditKind;

typedef enum {
    MOVE_LEFT, MOVE_RIGHT, MOVE_UP, MOVE_DOWN,
    MOVE_WORD_LEFT, MOVE_WORD_RIGHT,
    MOVE_HOME, MOVE_END,            // smart home: first non-space, then column 0
    MOVE_DOC_START, MOVE_DOC_END,
    MOVE_PAGE_UP, MOVE_PAGE_DOWN,
} EditorMotion;

typedef struct {
    char *text;
    int len, cap;
    int cursor;        // byte offset of the caret
    int anchor;        // other end of the selection; equal to cursor when nothing is selected
    int preferredCol;  // column kept while moving up/down, or -1
    bool code;         // line numbers, syntax colours, auto-indent
    bool readOnly;
    float fontSize;
    const char *placeholder;  // shown when empty and unfocused (not owned)

    float scrollX, scrollY;
    int pageLines;     // visible lines, for page up/down (updated while drawing)
    bool revealCursor; // scroll the caret into view on the next draw
    unsigned version;  // incremented on every text change

    EditorSnapshot undo[EDITOR_MAX_UNDO];
    int undoCount;
    EditorSnapshot redo[EDITOR_MAX_UNDO];
    int redoCount;
    EditKind lastKind;
    double lastEditTime;

    int marks[EDITOR_MAX_MARKS];  // 1-based line numbers flagged (compiler errors)
    int markCount;
    int hintLine;      // 1-based line a hint points at, 0 for none (see EditorSetHint)
    bool hintBefore;   // the hint is a gap before hintLine, where a line belongs

    // Name suggestions for the word at the caret (see EditorSuggest). They are for one
    // moment of the text: any other edit or caret move makes them out of date.
    CompletionList suggestions;
    unsigned suggestedVersion;  // the text version and caret they were found for
    int suggestedCursor;
    int suggestionChosen;       // the highlighted one
    bool suggestionPicked;      // the learner moved the highlight, so Enter accepts it too
    Rectangle suggestionBox;    // where the list was drawn, for clicks (width 0: not shown)

    bool dragging;
    double lastClickTime;
    int clickCount;
} Editor;

void EditorInit(Editor *e, const char *text, bool code);
void EditorFree(Editor *e);
void EditorSetText(Editor *e, const char *text);                 // replace and clear history
void EditorReplaceAll(Editor *e, const char *text, double now);  // replace, undoable

void EditorInsertText(Editor *e, const char *s, double now);  // paste: filters to ASCII, tabs to spaces
// Types a character. In code mode: '}' on a blank line dedents; an opening bracket or quote
// brings its closer and leaves the caret between them (or wraps the selection); typing a
// closer that is already there steps over it.
void EditorTypeChar(Editor *e, char c, double now);
void EditorNewline(Editor *e, double now);                    // keeps indentation, +4 after '{'
void EditorBackspace(Editor *e, double now);                  // removes a whole 4-space indent step, or an empty pair "()"
void EditorDeleteForward(Editor *e, double now);
void EditorIndent(Editor *e, bool outdent, double now);       // Tab / Shift+Tab, on all selected lines
void EditorDeleteSelection(Editor *e, double now);

void EditorMoveCursor(Editor *e, EditorMotion m, bool select);
void EditorSetCursor(Editor *e, int pos, bool select);
void EditorSelectAll(Editor *e);
bool EditorHasSelection(const Editor *e);
char *EditorCopySelection(const Editor *e);  // malloc'd, caller frees; NULL if nothing selected

// Name suggestions (code mode). Suggest finds them for the word at the caret, after typing;
// they count as showing until the text or the caret changes again. Accept completes the
// word with one of them, shape and all (`printf("")` with the caret inside), as one edit.
void EditorSuggest(Editor *e);
bool EditorSuggesting(const Editor *e);
void EditorChooseSuggestion(Editor *e, int step);  // moves the highlight down (1) or up (-1), wrapping
bool EditorAcceptSuggestion(Editor *e, int index, double now);  // false if there is no such suggestion
void EditorDismissSuggestions(Editor *e);

void EditorUndo(Editor *e);
void EditorRedo(Editor *e);

int EditorLineCount(const Editor *e);
int EditorLineOf(const Editor *e, int pos);           // 0-based
int EditorColOf(const Editor *e, int pos);
int EditorPosOf(const Editor *e, int line, int col);  // clamped to the text

void EditorSetMarks(Editor *e, const int *lines, int count);
// Points a hint at a line (1-based; 0 clears it), drawn in the accent colour: the line itself,
// or with before set, the gap above it where a new line belongs (line may be one past the
// last). Unlike marks it survives edits; setting the text clears it.
void EditorSetHint(Editor *e, int line, bool before);

// Where the character at text offset pos is drawn (the middle of its left edge) when the
// editor is drawn in r, for effects that point at the caret or at a line.
Vector2 EditorTextPoint(const Editor *e, Rectangle r, int pos);

// Handles mouse and keyboard input for this frame and draws the editor in r.
// Uses its own scissor region, so don't call it inside a scrolling view.
void EditorWidget(Editor *e, Rectangle r);

#endif
