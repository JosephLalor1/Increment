// Lines that differ between two pieces of code. See line_diff.h.
#include "line_diff.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// One line of a text: where it starts and how long it is once trimmed, and its line number.
typedef struct {
    const char *start;
    int length;
    int number;
} Line;

static bool Space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// The text's non-blank lines, trimmed. Returns how many (at most LINE_DIFF_MAX_LINES).
static int SplitLines(const char *text, Line *out)
{
    int count = 0, number = 0;
    const char *p = text ? text : "";
    while (*p && count < LINE_DIFF_MAX_LINES) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *s = p, *e = end;
        while (s < e && Space(*s)) s++;
        while (e > s && Space(e[-1])) e--;
        if (e > s) out[count++] = (Line){ s, (int)(e - s), number };
        number++;
        p = *end ? end + 1 : end;
    }
    return count;
}

static bool SameLine(Line a, Line b)
{
    return a.length == b.length && memcmp(a.start, b.start, (size_t)a.length) == 0;
}

int LineDiff(const char *oldText, const char *newText, DiffOp *ops, int maxOps)
{
    static Line oldLines[LINE_DIFF_MAX_LINES], newLines[LINE_DIFF_MAX_LINES];
    int n = SplitLines(oldText, oldLines), m = SplitLines(newText, newLines);

    // Longest common subsequence, filled from the end so the walk below can go forwards.
    int *table = calloc((size_t)(n + 1) * (size_t)(m + 1), sizeof *table);
    if (!table) return 0;
    #define CELL(i, j) table[(i) * (m + 1) + (j)]
    for (int i = n - 1; i >= 0; i--)
        for (int j = m - 1; j >= 0; j--)
            CELL(i, j) = SameLine(oldLines[i], newLines[j]) ? CELL(i + 1, j + 1) + 1
                       : CELL(i + 1, j) > CELL(i, j + 1) ? CELL(i + 1, j) : CELL(i, j + 1);

    int count = 0, i = 0, j = 0;
    while ((i < n || j < m) && count < maxOps) {
        if (i < n && j < m && SameLine(oldLines[i], newLines[j])) {
            ops[count++] = (DiffOp){ DIFF_SAME, oldLines[i++].number, newLines[j++].number };
        } else if (i < n && (j == m || CELL(i + 1, j) >= CELL(i, j + 1))) {
            ops[count++] = (DiffOp){ DIFF_REMOVED, oldLines[i++].number, -1 };
        } else {
            ops[count++] = (DiffOp){ DIFF_ADDED, -1, newLines[j++].number };
        }
    }
    #undef CELL
    free(table);
    return count;
}

bool LineDiffGetLine(const char *text, int line, char *out, int size)
{
    out[0] = '\0';
    const char *p = text ? text : "";
    for (int i = 0; i < line; i++) {
        p = strchr(p, '\n');
        if (!p) return false;
        p++;
    }
    const char *end = strchr(p, '\n');
    if (!end) end = p + strlen(p);
    while (p < end && Space(*p)) p++;
    while (end > p && Space(end[-1])) end--;
    int length = (int)(end - p);
    if (length > size - 1) length = size - 1;
    memcpy(out, p, (size_t)length);
    out[length] = '\0';
    return true;
}
