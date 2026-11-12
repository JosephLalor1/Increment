// Reading a program's variables and pointers from its code, and from a run. See memory_probe.h.
#include "level/memory_probe.h"
#include "code/code_text.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Reading the code's shape

static int SkipSpaces(const char *s, int i, int end)
{
    while (i < end && isspace((unsigned char)s[i])) i++;
    return i;
}

// Reads a name at i into out; returns the index after it, or i if there is none there.
static int ReadName(const char *s, int i, int end, char *out, int size)
{
    int n = 0;
    if (i >= end || isdigit((unsigned char)s[i])) {
        out[0] = '\0';
        return i;
    }
    while (i < end && CodeIsWordChar(s[i])) {
        if (n < size - 1) out[n++] = s[i];
        i++;
    }
    out[n] = '\0';
    return i;
}

static bool OneOf(const char *word, const char *const *list)
{
    for (; *list; list++)
        if (strcmp(word, *list) == 0) return true;
    return false;
}

// The body of main: the offsets of its braces. False if there is no main.
static bool FindMain(const char *s, int len, int *open, int *close)
{
    for (int i = 0; i + 4 <= len; i++) {
        if (strncmp(s + i, "main", 4) != 0 || (i > 0 && CodeIsWordChar(s[i - 1])) || CodeIsWordChar(s[i + 4])) continue;
        int at = SkipSpaces(s, i + 4, len);
        if (at >= len || s[at] != '(') continue;
        at = SkipSpaces(s, CodeMatchBracket(s, at, len) + 1, len);
        if (at >= len || s[at] != '{') continue;
        *open = at;
        *close = CodeMatchBracket(s, at, len);
        return true;
    }
    return false;
}

// Finds the next statement in [*at, end) at this level: its text is [*start, *stop), without
// the semicolon. Blocks (a function's body, an if's, a loop's, a class) are stepped over,
// along with whatever came before them; an initialiser's braces belong to their statement.
static bool NextStatement(const char *s, int *at, int end, int *start, int *stop)
{
    int i = *at;
    while (i < end) {
        i = SkipSpaces(s, i, end);
        int begin = i, parens = 0;
        bool initialiser = false, found = false;
        for (; i < end; i++) {
            char c = s[i];
            if (c == '(') parens++;
            else if (c == ')') parens--;
            else if (c == '=' && parens == 0) initialiser = true;
            else if (c == ';' && parens <= 0) {
                found = true;
                break;
            } else if (c == '{') {
                int close = CodeMatchBracket(s, i, end);
                if (!initialiser) {  // a block: start again after it
                    i = close + 1;
                    break;
                }
                i = close;
            } else if (c == '}') {
                i++;
                break;
            }
        }
        if (found) {
            *start = begin;
            *stop = i;
            *at = i + 1;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Declarations and where pointers are aimed

int ProbeScanFindVar(const ProbeScan *scan, const char *name)
{
    for (int i = 0; i < scan->count; i++)
        if (strcmp(scan->vars[i].name, name) == 0) return i;
    return -1;
}

static const ProbeTarget unknownTarget = { PROBE_TARGET_UNKNOWN, -1, 0 };

// Where an expression points, as far as the code says: "&x", "&a[2]", "&(a[2])", "arr",
// "arr + 2", another pointer (and "p - 1"), "NULL", "new int", "(int*)malloc(...)".
static ProbeTarget ParseTarget(const ProbeScan *scan, const char *s, int a, int b)
{
    char word[PROBE_NAME];
    a = SkipSpaces(s, a, b);
    if (a < b && s[a] == '(') {  // a cast in front: "(int*)malloc(...)"
        int close = CodeMatchBracket(s, a, b), last = close - 1;
        while (last > a && isspace((unsigned char)s[last])) last--;
        if (close < b && s[last] == '*') a = SkipSpaces(s, close + 1, b);
    }
    if (a >= b) return unknownTarget;
    if (s[a] == '&') {
        a = SkipSpaces(s, a + 1, b);
        while (a < b && s[a] == '(') a = SkipSpaces(s, a + 1, b);
        int after = ReadName(s, a, b, word, sizeof word);
        int var = ProbeScanFindVar(scan, word);
        if (var < 0) return unknownTarget;
        after = SkipSpaces(s, after, b);
        int element = after < b && s[after] == '[' ? atoi(s + after + 1) : 0;
        return (ProbeTarget){ PROBE_TARGET_VAR, var, element };
    }
    if (s[a] == '0' && SkipSpaces(s, a + 1, b) >= b) return (ProbeTarget){ PROBE_TARGET_NULL, -1, 0 };
    int after = ReadName(s, a, b, word, sizeof word);
    if (strcmp(word, "NULL") == 0 || strcmp(word, "nullptr") == 0) return (ProbeTarget){ PROBE_TARGET_NULL, -1, 0 };
    if (strcmp(word, "new") == 0 || strcmp(word, "malloc") == 0 || strcmp(word, "calloc") == 0)
        return (ProbeTarget){ PROBE_TARGET_HEAP, -1, 0 };
    int var = ProbeScanFindVar(scan, word);
    if (var < 0) return unknownTarget;
    const ProbeVar *from = &scan->vars[var];
    ProbeTarget target = from->isArray ? (ProbeTarget){ PROBE_TARGET_VAR, var, 0 }
                       : from->pointerDepth > 0 ? from->guess : unknownTarget;
    after = SkipSpaces(s, after, b);
    if (after >= b) return target;
    if ((s[after] == '+' || s[after] == '-') && target.kind == PROBE_TARGET_VAR) {  // "arr + 2", "ptr - 3"
        int step = atoi(s + SkipSpaces(s, after + 1, b));
        target.element += s[after] == '+' ? step : -step;
        return target;
    }
    return unknownTarget;  // something more involved: an element, a call, a member
}

// Adds a variable (a local takes the place of a global of the same name). Returns its
// index, or -1 if there is no room.
static int AddVar(ProbeScan *scan, const char *name, ProbeKind kind, int pointerDepth, bool isArray, bool global,
                  int line)
{
    int index = ProbeScanFindVar(scan, name);
    if (index < 0) {
        if (scan->count >= PROBE_MAX_VARS) return -1;
        index = scan->count++;
    }
    ProbeVar *var = &scan->vars[index];
    snprintf(var->name, sizeof var->name, "%s", name);
    var->kind = kind;
    var->pointerDepth = pointerDepth;
    var->isArray = isArray;
    var->global = global;
    var->line = line;
    var->guess = unknownTarget;
    if (pointerDepth > 0) scan->hasPointers = true;
    return index;
}

// Words that start a statement but never a declaration, so `return x;` or `delete p;`
// isn't taken for a variable of a type called "return".
static const char *const notTypes[] = {
    "return", "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "goto", "delete",
    "new", "using", "typedef", "sizeof", "throw", "cout", "cin", "cerr", "std", "void", "extern", "class", "enum",
    "union", "template", "namespace", "public", "private", "friend", "operator", NULL,
};
static const char *const qualifiers[] = { "const", "static", "volatile", "register", "struct", NULL };
static const char *const basicTypes[] = {
    "int", "char", "float", "double", "long", "short", "unsigned", "signed", "bool", "size_t", NULL,
};

// Reads the statement s[start, stop) as a declaration, adding its variables. Returns
// whether it was one.
static bool ParseDeclaration(ProbeScan *scan, const char *s, int start, int stop, bool global, int line)
{
    char word[PROBE_NAME];
    int i = SkipSpaces(s, start, stop), after = ReadName(s, i, stop, word, sizeof word);
    while (word[0] && OneOf(word, qualifiers)) {
        i = SkipSpaces(s, after, stop);
        after = ReadName(s, i, stop, word, sizeof word);
    }
    if (!word[0] || OneOf(word, notTypes)) return false;

    ProbeKind kind = PROBE_OPAQUE;
    if (OneOf(word, basicTypes)) {  // "int", "unsigned int", "long long"...
        kind = PROBE_INT;
        while (word[0] && (OneOf(word, basicTypes) || strcmp(word, "const") == 0)) {
            if (strcmp(word, "char") == 0) kind = PROBE_CHAR;
            else if (strcmp(word, "float") == 0 || strcmp(word, "double") == 0) kind = PROBE_FLOAT;
            else if (strcmp(word, "bool") == 0) kind = PROBE_BOOL;
            i = SkipSpaces(s, after, stop);
            after = ReadName(s, i, stop, word, sizeof word);
        }
    } else {  // a struct or class name: not a variable already in use ("a * b;" is no declaration)
        if (ProbeScanFindVar(scan, word) >= 0) return false;
        i = SkipSpaces(s, after, stop);
    }

    // The declarators: "*name", "name[5]", "name = value", "name(arguments)", comma-separated.
    bool any = false;
    while (i < stop) {
        int depth = 0;
        i = SkipSpaces(s, i, stop);
        while (i < stop && s[i] == '*') {
            depth++;
            i = SkipSpaces(s, i + 1, stop);
        }
        bool reference = i < stop && s[i] == '&';
        if (reference) i = SkipSpaces(s, i + 1, stop);
        after = ReadName(s, i, stop, word, sizeof word);
        if (!word[0] || OneOf(word, notTypes)) return any;
        i = SkipSpaces(s, after, stop);

        bool isArray = false, wanted = !reference;
        if (i < stop && s[i] == '[') {
            isArray = true;
            i = SkipSpaces(s, CodeMatchBracket(s, i, stop) + 1, stop);
            if (i < stop && s[i] == '[') wanted = false;  // more than one dimension
        }
        int valueStart = -1;
        char next = i < stop ? s[i] : ';';
        if (next == '=') valueStart = i + 1;
        else if (next == '(') {  // constructor arguments; anything else with brackets is a function
            if (kind != PROBE_OPAQUE || global || depth > 0) return any;
        } else if (next != ',' && next != ';' && next != '{' && next != '[') {
            return any;  // not a declaration after all ("a b c", "x :: y")
        }
        int end = i;  // the comma that ends this declarator, or the statement's end
        while (end < stop && s[end] != ',') end = strchr("([{", s[end]) ? CodeMatchBracket(s, end, stop) + 1 : end + 1;

        if (wanted) {
            int index = AddVar(scan, word, kind, depth, isArray, global, line);
            if (index >= 0 && depth > 0 && !isArray && valueStart >= 0)
                scan->vars[index].guess = ParseTarget(scan, s, valueStart, end);
        }
        any = true;
        i = end + 1;
    }
    return any;
}

// Moves a pointer's guessed aim along its array.
static void ShiftGuess(ProbeVar *var, int step)
{
    if (var->guess.kind == PROBE_TARGET_VAR) var->guess.element += step;
}

// Reads the statement s[start, stop) for what it does to a pointer's aim: "p = &x",
// "*pp = &x" (aims the pointer pp points at), "p++", "p += 2", "free(p)", "delete p".
static void ParseAssignment(ProbeScan *scan, const char *s, int start, int stop)
{
    char word[PROBE_NAME];
    int i = SkipSpaces(s, start, stop);
    int after = ReadName(s, i, stop, word, sizeof word);
    if (strcmp(word, "delete") == 0 || strcmp(word, "free") == 0) {
        i = after;
        while (i < stop && !CodeIsWordChar(s[i])) i++;  // past "[]", "(" and spaces
        ReadName(s, i, stop, word, sizeof word);
        int var = ProbeScanFindVar(scan, word);
        if (var >= 0 && scan->vars[var].pointerDepth > 0) scan->vars[var].guess = (ProbeTarget){ PROBE_TARGET_FREED, -1, 0 };
        return;
    }
    if (i + 1 < stop && (s[i] == '+' || s[i] == '-') && s[i + 1] == s[i]) {  // "++p"
        ReadName(s, SkipSpaces(s, i + 2, stop), stop, word, sizeof word);
        int var = ProbeScanFindVar(scan, word);
        if (var >= 0 && scan->vars[var].pointerDepth > 0) ShiftGuess(&scan->vars[var], s[i] == '+' ? 1 : -1);
        return;
    }
    int stars = 0;
    while (i < stop && s[i] == '*') {
        stars++;
        i = SkipSpaces(s, i + 1, stop);
    }
    after = ReadName(s, i, stop, word, sizeof word);
    int index = ProbeScanFindVar(scan, word);
    if (index < 0 || scan->vars[index].pointerDepth == 0 || scan->vars[index].isArray) return;
    ProbeVar *var = &scan->vars[index];
    i = SkipSpaces(s, after, stop);
    if (i + 1 >= stop) return;
    bool assigns = s[i] == '=' && s[i + 1] != '=';
    if (stars == 0) {
        if ((s[i] == '+' || s[i] == '-') && s[i + 1] == s[i]) ShiftGuess(var, s[i] == '+' ? 1 : -1);
        else if ((s[i] == '+' || s[i] == '-') && s[i + 1] == '=') ShiftGuess(var, (s[i] == '+' ? 1 : -1) * atoi(s + i + 2));
        else if (assigns) var->guess = ParseTarget(scan, s, i + 1, stop);
    } else if (stars == 1 && assigns && var->guess.kind == PROBE_TARGET_VAR) {
        ProbeVar *aimed = &scan->vars[var->guess.index];
        if (aimed->pointerDepth > 0 && !aimed->isArray) aimed->guess = ParseTarget(scan, s, i + 1, stop);
    }
}

static int LineOf(const char *code, int offset)
{
    int line = 0;
    for (int i = 0; i < offset && code[i]; i++) line += code[i] == '\n';
    return line;
}

void ProbeScanCode(const char *code, ProbeScan *scan)
{
    memset(scan, 0, sizeof *scan);
    scan->injectAt = -1;
    char *clean = code ? CodeBlanked(code, CODE_BLANK_DIRECTIVES) : NULL;
    if (!clean) return;
    int len = (int)strlen(clean), start, stop;

    // Globals: the statements outside every function.
    for (int at = 0; NextStatement(clean, &at, len, &start, &stop);)
        ParseDeclaration(scan, clean, start, stop, true, LineOf(code, start));

    // Main's own statements, in order; a run reports just before its last return.
    int open, close;
    if (FindMain(clean, len, &open, &close)) {
        scan->injectAt = close;
        for (int at = open + 1; NextStatement(clean, &at, close, &start, &stop);) {
            start = SkipSpaces(clean, start, stop);
            if (strncmp(clean + start, "return", 6) == 0 && !CodeIsWordChar(clean[start + 6])) scan->injectAt = start;
            else if (!ParseDeclaration(scan, clean, start, stop, false, LineOf(code, start)))
                ParseAssignment(scan, clean, start, stop);
        }
    }
    free(clean);
}

// ---------------------------------------------------------------------------
// The instrumented copy

// A growing string.
typedef struct {
    char *text;
    size_t len, cap;
} Buffer;

static void Append(Buffer *b, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int need = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (need < 0) return;
    if (b->len + (size_t)need + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        while (cap < b->len + (size_t)need + 1) cap *= 2;
        char *grown = realloc(b->text, cap);
        if (!grown) return;
        b->text = grown;
        b->cap = cap;
    }
    va_start(args, format);
    vsnprintf(b->text + b->len, (size_t)need + 1, format, args);
    va_end(args);
    b->len += (size_t)need;
}

// Goes in front of the program: a table of the heap blocks it asks for. Freeing a block
// only marks it, so a freed block keeps its address and can still be told from a live
// one. In C++, new and delete go through the same table.
static const char *const prelude =
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <stddef.h>\n"
    "#include <string.h>\n"
    "static struct { char *at; unsigned long long size; int freed; } probe_blocks[512];\n"
    "static int probe_count;\n"
    "static void *probe_malloc(unsigned long long n) {\n"
    "    void *p = malloc(n ? n : 1);\n"
    "    if (p && probe_count < 512) {\n"
    "        probe_blocks[probe_count].at = (char *)p;\n"
    "        probe_blocks[probe_count].size = n;\n"
    "        probe_blocks[probe_count].freed = 0;\n"
    "        probe_count++;\n"
    "    }\n"
    "    return p;\n"
    "}\n"
    "static void *probe_calloc(unsigned long long a, unsigned long long b) {\n"
    "    void *p = probe_malloc(a * b);\n"
    "    if (p) memset(p, 0, (size_t)(a * b));\n"
    "    return p;\n"
    "}\n"
    "static void probe_free(void *p) {\n"
    "    int i;\n"
    "    for (i = probe_count - 1; i >= 0; i--)\n"
    "        if (probe_blocks[i].at == (char *)p) { probe_blocks[i].freed = 1; return; }\n"
    "}\n"
    "static int probe_find(void *p, unsigned long long each, void **start, long long *count, int *freed) {\n"
    "    int i;\n"
    "    char *at = (char *)p;\n"
    "    for (i = probe_count - 1; i >= 0; i--) {\n"
    "        char *base = probe_blocks[i].at, *end = base + probe_blocks[i].size;\n"
    "        if (!each || at < base || at > end || probe_blocks[i].size < each) continue;\n"
    "        base += (unsigned long long)(at - base) % each;\n"
    "        *start = base;\n"
    "        *count = (long long)((unsigned long long)(end - base) / each);\n"
    "        *freed = probe_blocks[i].freed;\n"
    "        return 1;\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
    "#ifdef __cplusplus\n"
    "#include <new>\n"
    "void *operator new(std::size_t n) { return probe_malloc(n); }\n"
    "void *operator new[](std::size_t n) { return probe_malloc(n); }\n"
    "void operator delete(void *p) noexcept { probe_free(p); }\n"
    "void operator delete[](void *p) noexcept { probe_free(p); }\n"
    "void operator delete(void *p, std::size_t) noexcept { probe_free(p); }\n"
    "void operator delete[](void *p, std::size_t) noexcept { probe_free(p); }\n"
    "#endif\n";

// Goes after the program's own #includes, so the standard headers are read unchanged.
static const char *const allocatorMacros =
    "\n#define malloc(n) probe_malloc(n)\n"
    "#define calloc(a, b) probe_calloc(a, b)\n"
    "#define free(p) probe_free(p)\n";

static char KindLetter(ProbeKind kind) { return "icfbo"[kind]; }

// Code that prints one value: an address for a pointer, otherwise by its kind.
static void AppendValue(Buffer *b, const char *expression, ProbeKind kind, int pointerDepth)
{
    if (pointerDepth > 0) Append(b, "printf(\" %%llx\", (unsigned long long)(size_t)(%s));", expression);
    else if (kind == PROBE_INT || kind == PROBE_BOOL) Append(b, "printf(\" %%lld\", (long long)(%s));", expression);
    else if (kind == PROBE_CHAR) Append(b, "printf(\" %%d\", (int)(%s));", expression);
    else if (kind == PROBE_FLOAT) Append(b, "printf(\" %%g\", (double)(%s));", expression);
}

// Code that reports the heap block a pointer points into, if it does: where the block's
// elements start, their size, how many there are, whether it was freed, and their values.
// A pointer to pointers reports the blocks those point into as well (one level down).
static void AppendHeap(Buffer *b, const char *pointer, ProbeKind kind, int pointerDepth, int level)
{
    Append(b, "{ __typeof__(%s) probe_p%d = (%s); void *probe_s%d; long long probe_n%d; int probe_f%d;\n", pointer, level,
           pointer, level, level, level);
    Append(b, "if (probe_find((void *)probe_p%d, sizeof(*probe_p%d), &probe_s%d, &probe_n%d, &probe_f%d)) {\n", level, level,
           level, level, level);
    Append(b, "printf(\"@@MEM H %%llx %%d %%lld %c %d %%d\", (unsigned long long)(size_t)probe_s%d, "
              "(int)sizeof(*probe_p%d), probe_n%d, probe_f%d);\n",
           KindLetter(kind), pointerDepth - 1, level, level, level, level);
    char element[160];
    snprintf(element, sizeof element, "((__typeof__(probe_p%d))probe_s%d)[probe_i%d]", level, level, level);
    Append(b, "if (!probe_f%d) for (long long probe_i%d = 0; probe_i%d < probe_n%d && probe_i%d < %d; probe_i%d++) { ", level,
           level, level, level, level, PROBE_MAX_CELLS, level);
    AppendValue(b, element, kind, pointerDepth - 1);
    Append(b, " }\nprintf(\"\\n\");\n");
    if (pointerDepth >= 2 && level == 0) {
        Append(b, "if (!probe_f0) for (long long probe_i0 = 0; probe_i0 < probe_n0 && probe_i0 < %d; probe_i0++) {\n",
               PROBE_MAX_CELLS);
        AppendHeap(b, element, kind, pointerDepth - 1, 1);
        Append(b, "}\n");
    }
    Append(b, "} }\n");
}

// Code that reports every variable, to go just before main returns.
static void AppendReport(Buffer *b, const ProbeScan *scan)
{
    Append(b, "{ printf(\"\\n@@MEM BEGIN\\n\");\n");
    for (int i = 0; i < scan->count; i++) {
        const ProbeVar *var = &scan->vars[i];
        const char *name = var->name;
        if (var->isArray) {
            Append(b, "printf(\"@@MEM V %s %%llx %%d %%d %c %d\", (unsigned long long)(size_t)%s, (int)sizeof(%s[0]), "
                      "(int)(sizeof(%s) / sizeof(%s[0])));\n",
                   name, KindLetter(var->kind), var->pointerDepth, name, name, name, name);
            char element[80];
            snprintf(element, sizeof element, "%s[probe_a]", name);
            Append(b, "for (int probe_a = 0; probe_a < (int)(sizeof(%s) / sizeof(%s[0])) && probe_a < %d; probe_a++) { ", name,
                   name, PROBE_MAX_CELLS);
            AppendValue(b, element, var->kind, var->pointerDepth);
            Append(b, " }\nprintf(\"\\n\");\n");
            if (var->pointerDepth > 0) {
                Append(b, "for (int probe_a = 0; probe_a < (int)(sizeof(%s) / sizeof(%s[0])) && probe_a < %d; probe_a++) {\n",
                       name, name, PROBE_MAX_CELLS);
                AppendHeap(b, element, var->kind, var->pointerDepth, 0);
                Append(b, "}\n");
            }
        } else {
            Append(b, "printf(\"@@MEM V %s %%llx %%d 1 %c %d\", (unsigned long long)(size_t)&%s, (int)sizeof(%s));\n", name,
                   KindLetter(var->kind), var->pointerDepth, name, name);
            AppendValue(b, name, var->kind, var->pointerDepth);
            Append(b, "\nprintf(\"\\n\");\n");
            if (var->pointerDepth > 0) AppendHeap(b, name, var->kind, var->pointerDepth, 0);
        }
    }
    Append(b, "printf(\"@@MEM END\\n\"); fflush(stdout); }\n");
}

// The offset just after the code's last #include line (0 if it has none).
static int AfterIncludes(const char *code, int before)
{
    int after = 0;
    for (int lineStart = 0; lineStart < before;) {
        int i = lineStart;
        while (code[i] == ' ' || code[i] == '\t') i++;
        int lineEnd = lineStart;
        while (code[lineEnd] && code[lineEnd] != '\n') lineEnd++;
        if (code[lineEnd] == '\n') lineEnd++;
        if (strncmp(code + i, "#include", 8) == 0) after = lineEnd;
        if (!code[lineEnd] || lineEnd == lineStart) break;
        lineStart = lineEnd;
    }
    return after < before ? after : 0;
}

char *ProbeInstrument(const char *code)
{
    ProbeScan scan;
    ProbeScanCode(code, &scan);
    if (scan.injectAt < 0) return NULL;
    int includesEnd = AfterIncludes(code, scan.injectAt);
    Buffer b = { 0 };
    Append(&b, "%s", prelude);
    Append(&b, "%.*s", includesEnd, code);
    Append(&b, "%s", allocatorMacros);
    Append(&b, "%.*s", scan.injectAt - includesEnd, code + includesEnd);
    AppendReport(&b, &scan);
    Append(&b, "%s", code + scan.injectAt);
    return b.text;
}

// ---------------------------------------------------------------------------
// Reading the report

static ProbeKind KindOf(char letter)
{
    const char *letters = "icfbo", *found = strchr(letters, letter);
    return found && letter ? (ProbeKind)(found - letters) : PROBE_OPAQUE;
}

// Fills a cell's elements from the rest of its report line: one token each.
static void ReadValues(ProbeCell *cell, const char *rest)
{
    cell->shown = 0;
    bool valueless = cell->kind == PROBE_OPAQUE && cell->pointerDepth == 0;
    int limit = cell->count < PROBE_MAX_CELLS ? cell->count : PROBE_MAX_CELLS;
    while (cell->shown < limit) {
        char token[40];
        int used = 0;
        if (sscanf(rest, " %39s%n", token, &used) != 1) {
            if (!valueless && !cell->freed) break;
            token[0] = '\0';  // no values come for these: empty cells
        }
        rest += used;
        char *value = cell->values[cell->shown];
        value[0] = '\0';
        cell->pointers[cell->shown] = 0;
        if (cell->pointerDepth > 0) {
            cell->pointers[cell->shown] = strtoull(token, NULL, 16);
        } else if (cell->kind == PROBE_CHAR && token[0]) {
            int code = atoi(token);
            if (code == 0) snprintf(value, PROBE_VALUE, "'\\0'");
            else if (code >= 32 && code < 127) snprintf(value, PROBE_VALUE, "'%c'", code);
            else snprintf(value, PROBE_VALUE, "%d", code);
        } else if (cell->kind == PROBE_BOOL && token[0]) {
            snprintf(value, PROBE_VALUE, "%s", atoi(token) ? "true" : "false");
        } else {
            snprintf(value, PROBE_VALUE, "%.*s", PROBE_VALUE - 1, token);
        }
        cell->shown++;
    }
}

bool ProbeRead(const char *output, ProbeSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof *snapshot);
    if (!output) return false;
    const char *begin = NULL;  // the last report, should the program print more than one
    for (const char *at = output; (at = strstr(at, "@@MEM BEGIN")) != NULL; at++) begin = at;
    if (!begin) return false;

    for (const char *line = strchr(begin, '\n'); line; line = strchr(line, '\n')) {
        line++;
        if (strncmp(line, "@@MEM END", 9) == 0) {
            snapshot->valid = true;
            break;
        }
        ProbeCell cell = { 0 };
        char kind;
        int used = 0, freed = 0;
        long long count = 0;
        if (sscanf(line, "@@MEM V %31s %llx %d %lld %c %d%n", cell.name, &cell.address, &cell.elementSize, &count, &kind,
                   &cell.pointerDepth, &used) == 6) {
            if (snapshot->varCount >= PROBE_MAX_VARS) continue;
            cell.count = (int)count;
            cell.kind = KindOf(kind);
            ReadValues(&cell, line + used);
            snapshot->vars[snapshot->varCount++] = cell;
        } else if (sscanf(line, "@@MEM H %llx %d %lld %c %d %d%n", &cell.address, &cell.elementSize, &count, &kind,
                          &cell.pointerDepth, &freed, &used) == 6) {
            bool known = false;
            for (int i = 0; i < snapshot->blockCount; i++) known |= snapshot->blocks[i].address == cell.address;
            if (known || snapshot->blockCount >= PROBE_MAX_BLOCKS) continue;
            cell.count = (int)count;
            cell.kind = KindOf(kind);
            cell.freed = freed != 0;
            ReadValues(&cell, line + used);
            snapshot->blocks[snapshot->blockCount++] = cell;
        }
    }
    if (!snapshot->valid) memset(snapshot, 0, sizeof *snapshot);
    return snapshot->valid;
}

// Whether the address is inside the cell; sets *element to which of its elements.
static bool Inside(const ProbeCell *cell, unsigned long long address, int *element)
{
    unsigned long long size = (unsigned long long)cell->elementSize * (unsigned long long)cell->count;
    if (cell->elementSize <= 0 || address < cell->address || address >= cell->address + size) return false;
    *element = (int)((address - cell->address) / (unsigned long long)cell->elementSize);
    return true;
}

ProbeTarget ProbeResolve(const ProbeSnapshot *snapshot, unsigned long long address)
{
    int element = 0;
    if (address == 0) return (ProbeTarget){ PROBE_TARGET_NULL, -1, 0 };
    for (int i = 0; i < snapshot->varCount; i++)
        if (Inside(&snapshot->vars[i], address, &element)) return (ProbeTarget){ PROBE_TARGET_VAR, i, element };
    for (int i = 0; i < snapshot->blockCount; i++)
        if (Inside(&snapshot->blocks[i], address, &element))
            return (ProbeTarget){ snapshot->blocks[i].freed ? PROBE_TARGET_FREED : PROBE_TARGET_HEAP, i, element };
    return (ProbeTarget){ PROBE_TARGET_WILD, -1, 0 };
}

int ProbeFindVar(const ProbeSnapshot *snapshot, const char *name)
{
    for (int i = 0; i < snapshot->varCount; i++)
        if (strcmp(snapshot->vars[i].name, name) == 0) return i;
    return -1;
}
