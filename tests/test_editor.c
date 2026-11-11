// Tests for the editing operations in src/editor.c. Run via test.bat.
#include "../src/editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while (0)

#define CHECK_TEXT(e, want)                                                          \
    do {                                                                             \
        if (strcmp((e)->text, (want)) != 0) {                                        \
            printf("FAIL %s:%d: text is \"%s\", want \"%s\"\n", __FILE__, __LINE__,  \
                   (e)->text, (want));                                               \
            failures++;                                                              \
        }                                                                            \
    } while (0)

static void Type(Editor *e, const char *s, double now)
{
    for (; *s; s++) {
        if (*s == '\n') EditorNewline(e, now);
        else EditorTypeChar(e, *s, now);
    }
}

static void TestTypingAndCursor(void)
{
    Editor e;
    EditorInit(&e, "", true);
    Type(&e, "int x;", 0);
    CHECK_TEXT(&e, "int x;");
    CHECK(e.cursor == 6 && e.anchor == 6);

    EditorMoveCursor(&e, MOVE_HOME, false);
    CHECK(e.cursor == 0);
    EditorMoveCursor(&e, MOVE_WORD_RIGHT, false);
    CHECK(e.cursor == 4);  // after "int "
    EditorMoveCursor(&e, MOVE_END, true);
    CHECK(e.anchor == 4 && e.cursor == 6);
    char *sel = EditorCopySelection(&e);
    CHECK(sel && strcmp(sel, "x;") == 0);
    free(sel);

    Type(&e, "y = 1;", 0);  // typing replaces the selection
    CHECK_TEXT(&e, "int y = 1;");
    EditorFree(&e);
}

static void TestAutoIndent(void)
{
    Editor e;
    EditorInit(&e, "", true);
    Type(&e, "int main() {\nreturn 0;\n}", 0);
    CHECK_TEXT(&e, "int main() {\n    return 0;\n}");

    // Enter between braces opens a block with the caret on the indented middle line.
    EditorSetText(&e, "if (x) {}");
    EditorSetCursor(&e, 8, false);
    EditorNewline(&e, 0);
    CHECK_TEXT(&e, "if (x) {\n    \n}");
    CHECK(EditorLineOf(&e, e.cursor) == 1 && EditorColOf(&e, e.cursor) == 4);

    // Indentation is kept on ordinary lines.
    EditorSetText(&e, "    a;");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorNewline(&e, 0);
    CHECK_TEXT(&e, "    a;\n    ");

    // Smart home: first press goes to the first non-space, second to column 0.
    EditorSetText(&e, "    abc");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorMoveCursor(&e, MOVE_HOME, false);
    CHECK(e.cursor == 4);
    EditorMoveCursor(&e, MOVE_HOME, false);
    CHECK(e.cursor == 0);

    // Plain (non-code) editors don't auto-indent.
    Editor p;
    EditorInit(&p, "    a", false);
    EditorMoveCursor(&p, MOVE_DOC_END, false);
    EditorNewline(&p, 0);
    CHECK_TEXT(&p, "    a\n");
    EditorFree(&p);
    EditorFree(&e);
}

static void TestBackspaceAndDelete(void)
{
    Editor e;
    EditorInit(&e, "        x", true);
    EditorSetCursor(&e, 8, false);
    EditorBackspace(&e, 0);  // whole indent step
    CHECK_TEXT(&e, "    x");
    EditorBackspace(&e, 0);
    CHECK_TEXT(&e, "x");
    EditorBackspace(&e, 0);  // at the start: nothing happens
    CHECK_TEXT(&e, "x");

    EditorSetText(&e, "ab\ncd");
    EditorSetCursor(&e, 3, false);
    EditorBackspace(&e, 0);  // joins the lines
    CHECK_TEXT(&e, "abcd");
    EditorDeleteForward(&e, 0);
    CHECK_TEXT(&e, "abd");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorDeleteForward(&e, 0);  // at the end: nothing happens
    CHECK_TEXT(&e, "abd");
    EditorFree(&e);
}

static void TestIndentSelection(void)
{
    Editor e;
    EditorInit(&e, "a\nb\nc", true);
    EditorSelectAll(&e);
    EditorIndent(&e, false, 0);
    CHECK_TEXT(&e, "    a\n    b\n    c");
    EditorIndent(&e, true, 0);
    CHECK_TEXT(&e, "a\nb\nc");

    // A selection ending at the start of a line leaves that line alone.
    EditorSetCursor(&e, 0, false);
    EditorSetCursor(&e, 4, true);  // "a\nb\n" ends at the start of "c"
    EditorIndent(&e, false, 0);
    CHECK_TEXT(&e, "    a\n    b\nc");

    // Tab with no selection inserts spaces up to the next 4-column stop.
    EditorSetText(&e, "ab");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorIndent(&e, false, 0);
    CHECK_TEXT(&e, "ab  ");

    // A closing brace on an indented blank line steps back one level.
    EditorSetText(&e, "{\n    ");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorTypeChar(&e, '}', 0);
    CHECK_TEXT(&e, "{\n}");
    EditorFree(&e);
}

static void TestPairs(void)
{
    Editor e;
    EditorInit(&e, "", true);
    // An opener brings its closer; typing the closer steps over it.
    Type(&e, "printf(", 0);
    CHECK_TEXT(&e, "printf()");
    CHECK(e.cursor == 7);
    Type(&e, "\"hi", 0);
    CHECK_TEXT(&e, "printf(\"hi\")");
    Type(&e, "\");", 0);
    CHECK_TEXT(&e, "printf(\"hi\");");
    CHECK(e.cursor == e.len);

    // A closer that is really missing is typed, not stepped over.
    EditorSetText(&e, "f(g(x)");
    EditorSetCursor(&e, 5, false);  // f(g(x|)
    EditorTypeChar(&e, ')', 0);
    CHECK_TEXT(&e, "f(g(x))");

    // Brackets, braces and character quotes pair too; an apostrophe after a letter doesn't,
    // and nothing pairs in front of a word.
    EditorSetText(&e, "");
    Type(&e, "a[", 0);
    CHECK_TEXT(&e, "a[]");
    EditorSetText(&e, "c = ");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorTypeChar(&e, '\'', 0);
    CHECK_TEXT(&e, "c = ''");
    EditorSetText(&e, "// don");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorTypeChar(&e, '\'', 0);
    CHECK_TEXT(&e, "// don'");
    EditorSetText(&e, "x");
    EditorSetCursor(&e, 0, false);
    EditorTypeChar(&e, '(', 0);
    CHECK_TEXT(&e, "(x");

    // Backspace between an empty pair removes both; with something inside, only the opener's side.
    EditorSetText(&e, "f()");
    EditorSetCursor(&e, 2, false);
    EditorBackspace(&e, 0);
    CHECK_TEXT(&e, "f");
    EditorSetText(&e, "f(x)");
    EditorSetCursor(&e, 3, false);
    EditorBackspace(&e, 0);
    CHECK_TEXT(&e, "f()");

    // An opener typed over a selection wraps it and keeps it selected.
    EditorSetText(&e, "a + b");
    EditorSelectAll(&e);
    EditorTypeChar(&e, '(', 0);
    CHECK_TEXT(&e, "(a + b)");
    char *inside = EditorCopySelection(&e);
    CHECK(inside && strcmp(inside, "a + b") == 0);
    free(inside);

    // A block typed out in full, closing brace and all, ends with one brace, not two.
    EditorSetText(&e, "");
    Type(&e, "while (1) {\nx++;\n}", 0);
    CHECK_TEXT(&e, "while (1) {\n    x++;\n}");
    CHECK(e.cursor == e.len);
    // But a brace that is genuinely missing is still typed.
    EditorSetText(&e, "{\n    {\n    \n}");
    EditorSetCursor(&e, 12, false);  // the end of the blank line
    EditorTypeChar(&e, '}', 0);
    CHECK_TEXT(&e, "{\n    {\n}\n}");

    // Plain text boxes don't pair.
    Editor plain;
    EditorInit(&plain, "", false);
    EditorTypeChar(&plain, '(', 0);
    CHECK_TEXT(&plain, "(");
    EditorFree(&plain);
    EditorFree(&e);
}

static void TestUndoRedo(void)
{
    Editor e;
    EditorInit(&e, "", true);
    Type(&e, "abc", 0.0);  // one typing burst
    Type(&e, "def", 5.0);  // a separate burst after a pause
    CHECK_TEXT(&e, "abcdef");
    EditorUndo(&e);
    CHECK_TEXT(&e, "abc");
    EditorUndo(&e);
    CHECK_TEXT(&e, "");
    EditorUndo(&e);  // nothing left: no change
    CHECK_TEXT(&e, "");
    EditorRedo(&e);
    CHECK_TEXT(&e, "abc");
    EditorRedo(&e);
    CHECK_TEXT(&e, "abcdef");

    // A new edit clears the redo history.
    EditorUndo(&e);
    Type(&e, "X", 10.0);
    EditorRedo(&e);
    CHECK_TEXT(&e, "abcX");

    // Moving the caret ends a typing group even without a pause.
    EditorSetText(&e, "");
    Type(&e, "ab", 20.0);
    EditorMoveCursor(&e, MOVE_LEFT, false);
    Type(&e, "Z", 20.1);
    CHECK_TEXT(&e, "aZb");
    EditorUndo(&e);
    CHECK_TEXT(&e, "ab");

    // Replacing everything (Reset / Show solution) can be undone.
    EditorReplaceAll(&e, "new text", 30.0);
    CHECK_TEXT(&e, "new text");
    EditorUndo(&e);
    CHECK_TEXT(&e, "ab");
    EditorFree(&e);
}

static void TestPasteAndLines(void)
{
    Editor e;
    EditorInit(&e, "", true);
    EditorInsertText(&e, "a\tb\r\nc\x01\xC3\xA9", 0);  // tab, CRLF, control char, UTF-8
    CHECK_TEXT(&e, "a    b\nc");
    CHECK(EditorLineCount(&e) == 2);
    CHECK(EditorLineOf(&e, e.len) == 1);
    CHECK(EditorPosOf(&e, 1, 0) == 7);
    CHECK(EditorPosOf(&e, 0, 99) == 6);  // clamped to the end of line 0
    CHECK(EditorPosOf(&e, 9, 0) == e.len);

    // Up/down keep the preferred column across a short line.
    EditorSetText(&e, "long line\nab\nlong line");
    EditorSetCursor(&e, 7, false);  // line 0, column 7
    EditorMoveCursor(&e, MOVE_DOWN, false);
    CHECK(EditorLineOf(&e, e.cursor) == 1 && EditorColOf(&e, e.cursor) == 2);
    EditorMoveCursor(&e, MOVE_DOWN, false);
    CHECK(EditorLineOf(&e, e.cursor) == 2 && EditorColOf(&e, e.cursor) == 7);
    EditorMoveCursor(&e, MOVE_DOWN, false);  // past the last line: end of text
    CHECK(e.cursor == e.len);

    // Word movement over punctuation and spaces.
    EditorSetText(&e, "foo(bar, baz)");
    EditorMoveCursor(&e, MOVE_DOC_END, false);
    EditorMoveCursor(&e, MOVE_WORD_LEFT, false);
    CHECK(e.cursor == 12);  // before ")"
    EditorMoveCursor(&e, MOVE_WORD_LEFT, false);
    CHECK(e.cursor == 9);   // start of "baz"
    EditorFree(&e);
}

static void TestReadOnly(void)
{
    Editor e;
    EditorInit(&e, "output", false);
    e.readOnly = true;
    Type(&e, "x", 0);
    EditorBackspace(&e, 0);
    EditorInsertText(&e, "paste", 0);
    EditorSelectAll(&e);
    EditorDeleteSelection(&e, 0);
    CHECK_TEXT(&e, "output");
    char *sel = EditorCopySelection(&e);  // copying still works
    CHECK(sel && strcmp(sel, "output") == 0);
    free(sel);
    EditorFree(&e);
}

int main(void)
{
    TestTypingAndCursor();
    TestAutoIndent();
    TestPairs();
    TestBackspaceAndDelete();
    TestIndentSelection();
    TestUndoRedo();
    TestPasteAndLines();
    TestReadOnly();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All editor tests passed\n");
    return 0;
}
