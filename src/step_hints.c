// Code-only hints for a coding step. See step_hints.h.
#include "step_hints.h"
#include "line_diff.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Words a shape keeps: C and C++ keywords and the library names the course uses, so the
// shape still says which statement it is ("for", "printf", "cout") without giving it away.
static const char *const keptWords[] = {
    "auto", "bool", "break", "case", "char", "class", "const", "continue", "default", "delete", "do", "double",
    "else", "enum", "extern", "false", "float", "for", "if", "int", "long", "main", "namespace", "new", "nullptr",
    "private", "protected", "public", "return", "short", "signed", "sizeof", "static", "struct", "switch", "this",
    "true", "typedef", "unsigned", "using", "void", "while", "NULL", "std", "string", "size_t",
    "printf", "scanf", "puts", "putchar", "getchar", "fgets", "stdin", "cout", "cin", "endl", "getline",
    "malloc", "calloc", "realloc", "free", "strlen", "strcpy", "strcat", "strcmp",
};

static bool KeptWord(const char *word, int length)
{
    for (size_t i = 0; i < sizeof keptWords / sizeof keptWords[0]; i++)
        if ((int)strlen(keptWords[i]) == length && strncmp(keptWords[i], word, (size_t)length) == 0) return true;
    return false;
}

// Appends n characters to out (of size `size`, used so far *used), dropping what doesn't fit.
static void Put(char *out, int size, int *used, const char *s, int n)
{
    for (int i = 0; i < n && *used < size - 1; i++) out[(*used)++] = s[i];
    out[*used] = '\0';
}

// Appends "_" unless the shape already ends with one, so a run of blanked text is one "_".
static void Blank(char *out, int size, int *used)
{
    if (*used == 0 || out[*used - 1] != '_') Put(out, size, used, "_", 1);
}

// The inside of a string or character literal starting at p (just after the opening quote):
// keeps escapes, printf conversions and spaces (so the layout shows), blanks the rest. Returns where the literal ends
// (just after the closing quote, or the end of the line).
static const char *ShapeLiteral(const char *p, char quote, char *out, int size, int *used)
{
    while (*p && *p != quote) {
        if (*p == '\\' && p[1]) {
            Put(out, size, used, p, 2);
            p += 2;
        } else if (*p == '%' && quote == '"') {
            // A conversion: flags, width, precision and length, then the letter (or %%).
            const char *start = p++;
            while (*p && strchr("-+ #0123456789.lhz", *p)) p++;
            if (*p && *p != quote) p++;
            Put(out, size, used, start, (int)(p - start));
        } else if (*p == ' ') {
            Put(out, size, used, p, 1);
            p++;
        } else {
            Blank(out, size, used);
            p++;
        }
    }
    if (*p == quote) {
        Put(out, size, used, p, 1);
        p++;
    }
    return p;
}

void StepHintShape(const char *line, char *out, int size)
{
    int used = 0;
    out[0] = '\0';
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '#') {  // #include <stdio.h> and the like: nothing to blank
        Put(out, size, &used, line, (int)strlen(line));
        return;
    }
    const char *p = line;
    while (*p) {
        if (p[0] == '/' && p[1] == '/') break;  // a comment isn't part of the shape
        if (*p == '"' || *p == '\'') {
            Put(out, size, &used, p, 1);
            p = ShapeLiteral(p + 1, *p, out, size, &used);
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            if (KeptWord(start, (int)(p - start))) Put(out, size, &used, start, (int)(p - start));
            else Blank(out, size, &used);
        } else if (isdigit((unsigned char)*p)) {
            while (isalnum((unsigned char)*p) || *p == '.') p++;  // 42, 3.14, 0x1F, 10u
            Blank(out, size, &used);
        } else {
            Put(out, size, &used, p, 1);
            p++;
        }
    }
    while (used > 0 && (out[used - 1] == ' ' || out[used - 1] == '\t')) out[--used] = '\0';
}

// Lines in text, counting a last line that has no newline.
static int CountLines(const char *text)
{
    if (!text || !*text) return 0;
    int lines = 0;
    for (const char *c = text; *c; c++) lines += *c == '\n';
    return lines + (text[strlen(text) - 1] != '\n');
}

StepHint StepHintFind(const char *code, const char *solution)
{
    StepHint hint = { STEP_HINT_NONE, 0, "", "" };
    static DiffOp ops[LINE_DIFF_MAX_LINES * 2];
    int count = LineDiff(code, solution, ops, LINE_DIFF_MAX_LINES * 2);

    // The first run of differences: removed lines, added lines, or both (a changed line).
    int first = 0;
    while (first < count && ops[first].kind == DIFF_SAME) first++;
    if (first == count) return hint;
    int removed = -1, added = -1;
    for (int i = first; i < count && ops[i].kind != DIFF_SAME; i++) {
        if (ops[i].kind == DIFF_REMOVED && removed < 0) removed = i;
        if (ops[i].kind == DIFF_ADDED && added < 0) added = i;
    }

    if (added >= 0) {
        LineDiffGetLine(solution, ops[added].newLine, hint.target, sizeof hint.target);
        StepHintShape(hint.target, hint.shape, sizeof hint.shape);
    }
    if (removed >= 0) {
        hint.kind = added >= 0 ? STEP_HINT_CHANGE : STEP_HINT_REMOVE;
        hint.line = ops[removed].oldLine;
        return hint;
    }
    // Only an addition: it goes before the next line the code still has, or after its end.
    hint.kind = STEP_HINT_ADD;
    hint.line = CountLines(code);
    for (int i = added; i < count; i++) {
        if (ops[i].oldLine >= 0) {
            hint.line = ops[i].oldLine;
            break;
        }
    }
    return hint;
}
