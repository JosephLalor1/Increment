// What a step shows for the first time. See step_novelty.h.
#include "level/step_novelty.h"
#include "code/code_text.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PIECES 1024  // pieces looked at in one program
#define MAX_SEEN   512   // different pieces met before a step

// The words of C and C++ and their standard libraries that the levels teach. Names the
// learner chooses (variables, functions, classes) aren't pieces. (completion.c has its own,
// shorter list: the words worth offering while typing, with their shapes.)
static const char *const languageWords[] = {
    // C
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float",
    "for", "goto", "if", "int", "long", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
    "union", "unsigned", "void", "while", "main", "NULL", "printf", "scanf", "puts", "putchar", "getchar", "fgets",
    "malloc", "calloc", "realloc", "free", "strlen", "strcpy", "strncpy", "strcmp", "strcat", "sqrt", "pow", "abs",
    "exit", "rand", "srand", "time",
    // C++
    "bool", "true", "false", "cout", "cin", "endl", "std", "string", "class", "public", "private", "protected", "new",
    "delete", "nullptr", "using", "namespace", "this", "virtual", "template", "typename", "vector", "operator", "friend",
    "try", "catch", "throw",
};

// Types that a star after them makes a pointer type.
static const char *const typeWords[] = { "int", "char", "float", "double", "void", "long", "short", "bool", "unsigned" };

// Operators that are pieces, longest first so "<<=" isn't read as "<<".
static const char *const operators[] = {
    "->", "::", "<<", ">>", "++", "--", "+=", "-=", "*=", "/=", "%=", "==", "!=", "<=", ">=", "&&", "||",
};

static bool InList(const char *const *list, int count, const char *word, int length)
{
    for (int i = 0; i < count; i++)
        if ((int)strlen(list[i]) == length && strncmp(list[i], word, (size_t)length) == 0) return true;
    return false;
}
#define COUNT(array) ((int)(sizeof(array) / sizeof(array)[0]))

static void Add(CodePiece *out, int *count, int max, int start, int length, const char *key)
{
    if (*count >= max) return;
    out[*count].start = start;
    out[*count].length = length;
    snprintf(out[*count].key, CODE_PIECE_KEY, "%s", key);
    (*count)++;
}

// The word ending just before position at in code (spaces skipped), copied into word.
static void WordBefore(const char *code, int at, char *word, int size)
{
    int end = at;
    while (end > 0 && (code[end - 1] == ' ' || code[end - 1] == '\t')) end--;
    int start = end;
    while (start > 0 && CodeIsWordChar(code[start - 1])) start--;
    snprintf(word, (size_t)size, "%.*s", end - start, code + start);
}

// A preprocessor line from at (its '#'): "#include" and the header named. Returns where the
// line ends.
static int Directive(const char *code, int at, CodePiece *out, int *count, int max)
{
    int i = at + 1;
    while (code[i] == ' ') i++;
    int wordStart = i;
    while (CodeIsWordChar(code[i])) i++;
    char key[CODE_PIECE_KEY];
    snprintf(key, sizeof key, "#%.*s", i - wordStart, code + wordStart);
    Add(out, count, max, at, i - at, key);
    while (code[i] == ' ') i++;
    if (code[i] == '<') {
        int close = i;
        while (code[close] && code[close] != '>' && code[close] != '\n') close++;
        if (code[close] == '>' && close - i + 1 < CODE_PIECE_KEY) {
            snprintf(key, sizeof key, "%.*s", close - i + 1, code + i);
            Add(out, count, max, i, close - i + 1, key);
        }
    }
    while (code[i] && code[i] != '\n') i++;
    return i;
}

// The conversions and escapes inside a string or character literal from open (its quote)
// to close (the closing quote).
static void InsideLiteral(const char *code, int open, int close, CodePiece *out, int *count, int max)
{
    char key[CODE_PIECE_KEY];
    for (int i = open + 1; i < close; i++) {
        if (code[i] == '\\' && i + 1 < close) {
            snprintf(key, sizeof key, "\\%c", code[i + 1]);
            Add(out, count, max, i, 2, key);
            i++;
        } else if (code[i] == '%' && code[open] == '"') {
            int end = i + 1;
            while (end < close && strchr("-+ #0123456789.*", code[end])) end++;  // flags, width, precision
            while (end < close && strchr("hlLz", code[end])) end++;               // length
            if (end < close && strchr("diouxXfFeEgGcsp%", code[end]) && end - i + 1 < CODE_PIECE_KEY) {
                snprintf(key, sizeof key, "%.*s", end - i + 1, code + i);
                Add(out, count, max, i, end - i + 1, key);
                i = end;
            }
        }
    }
}

int CodePiecesFind(const char *code, CodePiece *out, int max)
{
    int count = 0;
    char *shape = CodeBlanked(code, CODE_BLANK_STRINGS);   // names and punctuation at face value
    char *words = CodeBlanked(code, CODE_KEEP_STRINGS);    // the literals still there
    if (!shape || !words) {
        free(shape);
        free(words);
        return 0;
    }
    bool lineStart = true;
    for (int i = 0; shape[i] && count < max;) {
        char c = shape[i];
        // A literal (blanked in shape, so looked for in words first): its conversions and
        // escapes.
        if (words[i] == '"' || words[i] == '\'') {
            int close = i + 1;
            while (words[close] && words[close] != words[i] && words[close] != '\n')
                close += words[close] == '\\' && words[close + 1] ? 2 : 1;
            InsideLiteral(words, i, close, out, &count, max);
            i = words[close] ? close + 1 : close;
            lineStart = false;
            continue;
        }
        if (c == '\n') {
            lineStart = true;
            i++;
            continue;
        }
        if (c == ' ' || c == '\t') {
            i++;
            continue;
        }
        if (lineStart && c == '#') {
            i = Directive(shape, i, out, &count, max);
            continue;
        }
        lineStart = false;
        if (isalpha((unsigned char)c) || c == '_') {
            int start = i;
            while (CodeIsWordChar(shape[i])) i++;
            int length = i - start;
            // A type followed by stars: the pointer type is the piece.
            int stars = i;
            while (shape[stars] == ' ') stars++;
            int starEnd = stars;
            while (shape[starEnd] == '*') starEnd++;
            if (starEnd > stars && InList(typeWords, COUNT(typeWords), shape + start, length)
                && length + (starEnd - stars) < CODE_PIECE_KEY) {
                char key[CODE_PIECE_KEY];
                snprintf(key, sizeof key, "%.*s%.*s", length, shape + start, starEnd - stars, "********");
                Add(out, &count, max, start, starEnd - start, key);
                i = starEnd;
            } else if (InList(languageWords, COUNT(languageWords), shape + start, length)) {
                char key[CODE_PIECE_KEY];
                snprintf(key, sizeof key, "%.*s", length, shape + start);
                Add(out, &count, max, start, length, key);
            }
            continue;
        }
        if (isdigit((unsigned char)c)) {
            while (CodeIsWordChar(shape[i]) || shape[i] == '.') i++;
            continue;
        }
        bool matched = false;
        for (int k = 0; k < COUNT(operators) && !matched; k++) {
            int length = (int)strlen(operators[k]);
            if (strncmp(shape + i, operators[k], (size_t)length) == 0) {
                Add(out, &count, max, i, length, operators[k]);
                i += length;
                matched = true;
            }
        }
        if (matched) continue;
        if (c == '[') Add(out, &count, max, i, 1, "[]");
        else if (c == '&' || c == '!' || c == '%' || c == '?' || c == '<' || c == '>') {
            char key[2] = { c, 0 };
            Add(out, &count, max, i, 1, key);
        } else if (c == '*') {
            // A star that follows an operator, a bracket or `return` dereferences; after a
            // name, number or closing bracket it multiplies.
            int before = i;
            while (before > 0 && (shape[before - 1] == ' ' || shape[before - 1] == '\t')) before--;
            char previous = before > 0 ? shape[before - 1] : '\n';
            char word[16];
            WordBefore(shape, i, word, sizeof word);
            bool dereference = !CodeIsWordChar(previous) && previous != ')' && previous != ']';
            if (strcmp(word, "return") == 0) dereference = true;
            if (dereference) Add(out, &count, max, i, 1, "*");
        }
        i++;
    }
    free(shape);
    free(words);
    return count;
}

static bool Seen(char (*seen)[CODE_PIECE_KEY], int seenCount, const char *key)
{
    for (int i = 0; i < seenCount; i++)
        if (strcmp(seen[i], key) == 0) return true;
    return false;
}

// Adds every piece of code to seen.
static void Learn(const char *code, char (*seen)[CODE_PIECE_KEY], int *seenCount)
{
    static CodePiece pieces[MAX_PIECES];
    int count = CodePiecesFind(code ? code : "", pieces, MAX_PIECES);
    for (int i = 0; i < count && *seenCount < MAX_SEEN; i++)
        if (!Seen(seen, *seenCount, pieces[i].key)) snprintf(seen[(*seenCount)++], CODE_PIECE_KEY, "%s", pieces[i].key);
}

// Adds the pieces of code not seen (nor already reported) to out.
static void Report(const char *code, bool inStarter, char (*seen)[CODE_PIECE_KEY], int seenCount, StepNovelty *out)
{
    static CodePiece pieces[MAX_PIECES];
    int count = CodePiecesFind(code ? code : "", pieces, MAX_PIECES);
    for (int i = 0; i < count && out->count < NOVELTY_MAX; i++) {
        if (Seen(seen, seenCount, pieces[i].key)) continue;
        bool reported = false;
        for (int j = 0; j < out->count && !reported; j++) reported = strcmp(out->pieces[j].key, pieces[i].key) == 0;
        if (reported) continue;
        snprintf(out->pieces[out->count].key, CODE_PIECE_KEY, "%s", pieces[i].key);
        out->pieces[out->count++].inStarter = inStarter;
    }
}

void StepNoveltyFind(const LessonSet *set, int lessonIndex, int step, StepNovelty *out)
{
    static char seen[MAX_SEEN][CODE_PIECE_KEY];
    int seenCount = 0;
    memset(out, 0, sizeof *out);
    if (!set || lessonIndex < 0 || lessonIndex >= set->count) return;
    const Lesson *lesson = &set->items[lessonIndex];
    if (step < 0 || step >= lesson->stepCount) return;
    for (int l = 0; l <= lessonIndex; l++) {
        const Lesson *earlier = &set->items[l];
        int steps = l < lessonIndex ? earlier->stepCount : step;
        for (int s = 0; s < steps; s++) {
            Learn(earlier->steps[s].starter, seen, &seenCount);
            Learn(earlier->steps[s].solution, seen, &seenCount);
        }
    }
    Report(lesson->steps[step].starter, true, seen, seenCount, out);
    Report(lesson->steps[step].solution, false, seen, seenCount, out);
}
