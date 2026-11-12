// Ways of printing a step's target without doing the step. See step_cheats.h.
#include "step_cheats.h"
#include "line_diff.h"
#include "skills.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 96
#define TOKEN_SIZE 24
#define MAX_FUNCTIONS 48

static bool WordChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

// ---------------------------------------------------------------------------
// Two views of a program, the same length as it, so offsets match:
//  - words: comments blanked. What the program says, strings included.
//  - shape: comments, strings and character literals blanked. Its structure: brackets and
//    names can be taken at face value.

static char *Blanked(const char *code, bool stringsToo)
{
    size_t len = strlen(code);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, code, len + 1);
    for (size_t i = 0; i < len;) {
        size_t end = i;
        bool blank = true;
        if (out[i] == '/' && out[i + 1] == '/') {
            while (end < len && out[end] != '\n') end++;
        } else if (out[i] == '/' && out[i + 1] == '*') {
            end = i + 2;
            while (end < len && !(out[end] == '*' && out[end + 1] == '/')) end++;
            end = end < len ? end + 2 : len;
        } else if (out[i] == '"' || out[i] == '\'') {
            char quote = out[i];
            end = i + 1;
            while (end < len && out[end] != quote && out[end] != '\n') end += out[end] == '\\' && end + 1 < len ? 2 : 1;
            if (end < len && out[end] == quote) end++;
            blank = stringsToo;
        }
        if (end == i) {
            i++;
            continue;
        }
        for (size_t k = i; blank && k < end; k++)
            if (out[k] != '\n') out[k] = ' ';
        i = end;
    }
    return out;
}

// ---------------------------------------------------------------------------
// 1. The answer written in

typedef struct {
    char text[MAX_TOKENS][TOKEN_SIZE];
    double value[MAX_TOKENS];  // numbers only
    int count;
} Tokens;

static bool HasNumber(const Tokens *tokens, double value)
{
    for (int i = 0; i < tokens->count; i++)
        if (fabs(tokens->value[i] - value) < 1e-9) return true;
    return false;
}

static void AddNumber(Tokens *tokens, const char *text, int length)
{
    char number[TOKEN_SIZE];
    snprintf(number, sizeof number, "%.*s", length < TOKEN_SIZE - 1 ? length : TOKEN_SIZE - 1, text);
    double value = atof(number);
    if (tokens->count >= MAX_TOKENS || HasNumber(tokens, value)) return;
    snprintf(tokens->text[tokens->count], TOKEN_SIZE, "%s", number);
    tokens->value[tokens->count++] = value;
}

// Collects the numbers in text: runs of digits, with a fraction if there is one. In code
// (isCode) digits inside names don't count (arr2D), nor do preprocessor lines, nor what
// sits inside a printf conversion ("%3d", "%.2f") or an escape.
static void CollectNumbers(const char *text, bool isCode, Tokens *tokens)
{
    bool inString = false, lineStart = true;
    for (const char *p = text ? text : ""; *p;) {
        if (isCode) {
            if (*p == '\n') lineStart = true;
            else if (lineStart && *p == '#') {
                while (*p && *p != '\n') p++;
                continue;
            } else if (!isspace((unsigned char)*p)) lineStart = false;
            if (*p == '"') inString = !inString;
            if (inString && *p == '\\' && p[1]) {
                p += 2;
                continue;
            }
            if (inString && *p == '%') {  // a conversion: flags, width and precision aren't values
                p++;
                while (*p && strchr("-+ #0123456789.*lhz", *p)) p++;
                continue;
            }
            if (isalpha((unsigned char)*p) || *p == '_') {  // a name, digits and all
                while (WordChar(*p)) p++;
                continue;
            }
        }
        if (!isdigit((unsigned char)*p)) {
            p++;
            continue;
        }
        const char *start = p;
        while (isdigit((unsigned char)*p)) p++;
        if (*p == '.' && isdigit((unsigned char)p[1])) {
            p++;
            while (isdigit((unsigned char)*p)) p++;
        }
        AddNumber(tokens, start, (int)(p - start));
    }
}

// Whether any string literal in the program (words view) contains the word.
static bool InAString(const char *words, const char *word)
{
    for (const char *p = words; (p = strchr(p, '"')) != NULL;) {
        const char *end = p + 1;
        while (*end && *end != '"' && *end != '\n') end += *end == '\\' && end[1] ? 2 : 1;
        size_t length = strlen(word);
        for (const char *at = p + 1; at + length <= end; at++)
            if (strncmp(at, word, length) == 0) return true;
        p = *end ? end + 1 : end;
    }
    return false;
}

// Appends a token to a note, comma-separated; false if it doesn't fit.
static bool AddToNote(char *note, const char *token, bool quoted)
{
    size_t used = strlen(note);
    if (used + strlen(token) + 5 >= STEP_CHEAT_NOTE) return false;
    snprintf(note + used, STEP_CHEAT_NOTE - used, quoted ? "%s\"%s\"" : "%s%s", used ? ", " : "", token);
    return true;
}

// The target's numbers and words that the step itself never spells out, found written in
// the learner's code. Fills note with them and returns whether there are enough to matter.
static bool AnswerWritten(const Step *step, const char *words, const char *solutionWords, const char *starterWords,
                          char *note)
{
    note[0] = '\0';
    // Words of three letters or more: any one of them in a string is the answer typed in.
    bool wordFound = false;
    for (const char *p = step->expected ? step->expected : ""; *p;) {
        if (!isalpha((unsigned char)*p)) {
            p++;
            continue;
        }
        char word[TOKEN_SIZE];
        int length = 0;
        while (isalpha((unsigned char)*p)) {
            if (length < TOKEN_SIZE - 1) word[length++] = *p;
            p++;
        }
        word[length] = '\0';
        if (length < 3 || InAString(solutionWords, word) || InAString(starterWords, word)) continue;
        if (step->input && strstr(step->input, word)) continue;
        if (InAString(words, word) && !strstr(note, word)) wordFound |= AddToNote(note, word, true);
    }
    if (wordFound) return true;

    // Numbers: those the step's own code and input never mention had to be computed.
    Tokens target = { .count = 0 }, known = { .count = 0 }, written = { .count = 0 };
    CollectNumbers(step->expected, false, &target);
    CollectNumbers(solutionWords, true, &known);
    CollectNumbers(starterWords, true, &known);
    CollectNumbers(step->input, false, &known);
    CollectNumbers(words, true, &written);
    int computed = 0, found = 0;
    for (int i = 0; i < target.count; i++) {
        double value = target.value[i];
        if (value == 0 || value == 1 || HasNumber(&known, value)) continue;
        computed++;
        if (HasNumber(&written, value) && AddToNote(note, target.text[i], false)) found++;
    }
    // One or two computed numbers: all of them. More: over half. So a single literal that
    // happens to match one of several (a loop bound) proves nothing.
    return computed > 0 && (computed <= 2 ? found == computed : found * 2 > computed);
}

// ---------------------------------------------------------------------------
// 2. A function left as it came

typedef struct {
    char name[48];
    int ordinal;          // which function of that name (overloads, same-named methods)
    int bodyStart, bodyEnd;
} Function;

static int MatchBracket(const char *s, int open)
{
    char opener = s[open], closer = opener == '(' ? ')' : '}';
    int depth = 0, i = open;
    for (; s[i]; i++) {
        if (s[i] == opener) depth++;
        else if (s[i] == closer && --depth == 0) return i;
    }
    return i;
}

// Finds every function and method defined in the program (shape view): a name, its
// bracketed parameters, then a body in braces (a constructor's initialiser list between
// them is stepped over). Control statements look the same and are told apart by name.
static int FindFunctions(const char *shape, Function *out, int max)
{
    static const char *const notFunctions[] = { "if", "for", "while", "switch", "catch", "return", "sizeof", NULL };
    int count = 0;
    for (int i = 0; shape[i];) {
        if (!(isalpha((unsigned char)shape[i]) || shape[i] == '_' || shape[i] == '~')) {
            i++;
            continue;
        }
        int start = i++;
        while (WordChar(shape[i])) i++;
        int nameEnd = i, at = i;
        while (shape[at] == ' ' || shape[at] == '\t') at++;
        if (shape[at] != '(') continue;
        at = MatchBracket(shape, at);
        if (!shape[at]) break;
        at++;
        while (isspace((unsigned char)shape[at])) at++;
        if (strncmp(shape + at, "const", 5) == 0 && !WordChar(shape[at + 5])) at += 5;
        while (isspace((unsigned char)shape[at])) at++;
        if (shape[at] == ':' && shape[at + 1] != ':') {  // ": x(nx), y(ny)" before a constructor's body
            while (shape[at] && shape[at] != '{' && shape[at] != ';') at = shape[at] == '(' ? MatchBracket(shape, at) + 1 : at + 1;
        }
        if (shape[at] != '{') continue;
        char name[48];
        snprintf(name, sizeof name, "%.*s", nameEnd - start < 47 ? nameEnd - start : 47, shape + start);
        bool control = false;
        for (int k = 0; notFunctions[k]; k++) control |= strcmp(name, notFunctions[k]) == 0;
        if (control || count >= max) continue;
        Function *function = &out[count];
        snprintf(function->name, sizeof function->name, "%s", name);
        function->ordinal = 0;
        for (int k = 0; k < count; k++) function->ordinal += strcmp(out[k].name, name) == 0;
        function->bodyStart = at + 1;
        function->bodyEnd = MatchBracket(shape, at);
        count++;
    }
    return count;
}

static const Function *SameFunction(const Function *list, int count, const Function *wanted)
{
    for (int i = 0; i < count; i++)
        if (list[i].ordinal == wanted->ordinal && strcmp(list[i].name, wanted->name) == 0) return &list[i];
    return NULL;
}

// Whether two function bodies say the same thing, however they are spaced.
static bool SameBody(const char *a, const Function *fa, const char *b, const Function *fb)
{
    int i = fa->bodyStart, j = fb->bodyStart;
    for (;;) {
        while (i < fa->bodyEnd && isspace((unsigned char)a[i])) i++;
        while (j < fb->bodyEnd && isspace((unsigned char)b[j])) j++;
        if (i >= fa->bodyEnd || j >= fb->bodyEnd) return i >= fa->bodyEnd && j >= fb->bodyEnd;
        if (a[i++] != b[j++]) return false;
    }
}

// How many of the functions found have this name.
static int Definitions(const Function *list, int count, const char *name)
{
    int found = 0;
    for (int i = 0; i < count; i++) found += strcmp(list[i].name, name) == 0;
    return found;
}

// How many times the name is followed by "(" in the program (shape view): its calls, plus
// its definitions. A destructor is never called by name, so it has only its definitions.
static int Mentions(const char *shape, const char *name)
{
    size_t length = strlen(name);
    int found = 0;
    for (const char *p = shape; (p = strstr(p, name)) != NULL; p += length) {
        bool wholeWord = (p == shape || !(WordChar(p[-1]) || p[-1] == '~')) && !WordChar(p[length]);
        const char *after = p + length;
        while (*after == ' ') after++;
        found += wholeWord && *after == '(';
    }
    return found;
}

// A function the step is about that the learner skipped: its body is still the starter's
// though the solution rewrites it, or it is never called, or never written. Writes
// "name()" to note. main is the learner's to use as they like.
static bool FunctionUntouched(const char *words, const char *shape, const char *solutionWords, const char *solutionShape,
                              const char *starterWords, const char *starterShape, char *note)
{
    static Function mine[MAX_FUNCTIONS], solved[MAX_FUNCTIONS], given[MAX_FUNCTIONS];
    int mineCount = FindFunctions(shape, mine, MAX_FUNCTIONS);
    int solvedCount = FindFunctions(solutionShape, solved, MAX_FUNCTIONS);
    int givenCount = FindFunctions(starterShape, given, MAX_FUNCTIONS);
    for (int i = 0; i < givenCount; i++) {
        if (strcmp(given[i].name, "main") == 0) continue;
        const Function *solution = SameFunction(solved, solvedCount, &given[i]);
        const Function *learner = SameFunction(mine, mineCount, &given[i]);
        if (!solution || !learner || SameBody(starterWords, &given[i], solutionWords, solution)) continue;
        if (!SameBody(starterWords, &given[i], words, learner)) continue;
        snprintf(note, STEP_CHEAT_NOTE, "%s()", given[i].name);
        return true;
    }

    // A function the step is about must also be used. For each one the solution defines:
    //  - it calls it more often than the starter does (the step is to call it): so must
    //    the learner;
    //  - the starter calls it and so does the solution: the learner can't have dropped
    //    every call and done its work by hand;
    //  - the starter doesn't define it but names it (it is called there, or it is a class's
    //    destructor), so its name is settled: the learner must have defined it.
    for (int i = 0; i < solvedCount; i++) {
        const char *name = solved[i].name;
        if (strcmp(name, "main") == 0 || solved[i].ordinal > 0) continue;
        int starterDefined = Definitions(given, givenCount, name), learnerDefined = Definitions(mine, mineCount, name);
        int starterCalls = Mentions(starterShape, name) - starterDefined;
        int solutionCalls = Mentions(solutionShape, name) - Definitions(solved, solvedCount, name);
        int learnerCalls = Mentions(shape, name) - learnerDefined;
        // A function written from scratch may have any name: only one the starter already
        // has, or already names, can be asked for by name.
        bool nameSettled = name[0] == '~' || starterCalls > 0;
        bool known = starterDefined > 0 || starterCalls > 0;
        bool uncalled = known && (solutionCalls > starterCalls ? learnerCalls <= starterCalls
                                  : starterCalls > 0 && solutionCalls > 0 && learnerCalls <= 0);
        bool unwritten = starterDefined == 0 && learnerDefined == 0 && nameSettled;
        if (!uncalled && !unwritten) continue;
        snprintf(note, STEP_CHEAT_NOTE, "%s()", name);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 3. Indexing instead of pointer arithmetic

// Finds a use of [ ] in a line of code that isn't an array's declaration, and writes it
// ("arr[i]") to found. A declaration's name follows its type, or a comma in a line that
// starts with one.
static bool IndexingIn(const char *line, char *found, int size)
{
    static const char *const types[] = { "int", "char", "float", "double", "long", "short", "unsigned", "bool", "const",
                                         "static", NULL };
    char first[16] = "";
    sscanf(line, " %15[A-Za-z_]", first);
    bool startsWithType = false;
    for (int k = 0; types[k]; k++) startsWithType |= strcmp(first, types[k]) == 0;

    for (const char *bracket = line; (bracket = strchr(bracket, '[')) != NULL; bracket++) {
        const char *nameEnd = bracket;
        while (nameEnd > line && nameEnd[-1] == ' ') nameEnd--;
        const char *nameStart = nameEnd;
        while (nameStart > line && WordChar(nameStart[-1])) nameStart--;
        if (nameStart == nameEnd) continue;  // "new int*[rows]", "delete[]": not a name being indexed
        const char *before = nameStart;
        while (before > line && (before[-1] == ' ' || before[-1] == '*')) before--;
        const char *wordStart = before;
        while (wordStart > line && WordChar(wordStart[-1])) wordStart--;
        char previous[16];
        snprintf(previous, sizeof previous, "%.*s", (int)(before - wordStart) < 15 ? (int)(before - wordStart) : 15, wordStart);
        bool declared = before > line && before[-1] == ',' && startsWithType;
        for (int k = 0; types[k]; k++) declared |= strcmp(previous, types[k]) == 0;
        if (declared) continue;
        const char *close = strchr(bracket, ']');
        int length = (int)((close ? close + 1 : bracket + 1) - nameStart);
        snprintf(found, (size_t)size, "%.*s", length < size - 1 ? length : size - 1, nameStart);
        return true;
    }
    return false;
}

// Whether any line that `code` adds to the starter indexes; the first is written to found.
static bool AddedLinesIndex(const char *starterShape, const char *shape, char *found, int size)
{
    static DiffOp ops[LINE_DIFF_MAX_LINES * 2];
    int count = LineDiff(starterShape, shape, ops, LINE_DIFF_MAX_LINES * 2);
    char line[1024];
    for (int i = 0; i < count; i++) {
        if (ops[i].kind != DIFF_ADDED) continue;
        LineDiffGetLine(shape, ops[i].newLine, line, sizeof line);
        if (IndexingIn(line, found, size)) return true;
    }
    return false;
}

static bool AboutPointerArithmetic(const Step *step)
{
    for (int i = 0; i < step->skillCount; i++)
        if (step->skills[i].skill == SKILL_POINTER_ARITHMETIC && step->skills[i].weight >= 1.0f) return true;
    return false;
}

// ---------------------------------------------------------------------------

StepCheat StepCheatFind(const Step *step, const char *code)
{
    StepCheat cheat = { CHEAT_NONE, "" };
    if (!step->starter || !step->solution || !code) return cheat;
    char *words = Blanked(code, false), *shape = Blanked(code, true);
    char *solutionWords = Blanked(step->solution, false), *solutionShape = Blanked(step->solution, true);
    char *starterWords = Blanked(step->starter, false), *starterShape = Blanked(step->starter, true);
    char unused[STEP_CHEAT_NOTE];
    if (words && shape && solutionWords && solutionShape && starterWords && starterShape) {
        if (AnswerWritten(step, words, solutionWords, starterWords, cheat.note)) {
            cheat.kind = CHEAT_ANSWER_WRITTEN;
        } else if (FunctionUntouched(words, shape, solutionWords, solutionShape, starterWords, starterShape, cheat.note)) {
            cheat.kind = CHEAT_FUNCTION_UNTOUCHED;
        } else if (AboutPointerArithmetic(step) && !AddedLinesIndex(starterShape, solutionShape, unused, sizeof unused)
                   && AddedLinesIndex(starterShape, shape, cheat.note, sizeof cheat.note)) {
            cheat.kind = CHEAT_INDEXED;
        }
    }
    if (cheat.kind == CHEAT_NONE) cheat.note[0] = '\0';
    free(words);
    free(shape);
    free(solutionWords);
    free(solutionShape);
    free(starterWords);
    free(starterShape);
    return cheat;
}
