#ifndef EDITOR_INTERNAL_H
#define EDITOR_INTERNAL_H

// What the editor's two files share and nobody else needs: editor.c (the editing
// operations) defines these, editor_widget.c (input and drawing) uses them.

#include "code/editor.h"

static inline int MinInt(int a, int b) { return a < b ? a : b; }
static inline int MaxInt(int a, int b) { return a > b ? a : b; }

int EditorSelectionStart(const Editor *e);      // the lower end of the selection (the caret if none)
int EditorSelectionEnd(const Editor *e);        // the upper end
int EditorLineEnd(const Editor *e, int pos);    // the offset of the newline ending pos's line, or the text's end
bool EditorLineMarked(const Editor *e, int line1);  // whether that line (1-based) has an error mark

#endif
