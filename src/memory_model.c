// Pointer diagram scripts: parsing them into steps, and explaining memory. See memory_model.h.
#include "memory_model.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_START 1000   // addresses are made up but realistic-looking: the stack from 1000,
#define HEAP_START  5000   // the heap from 5000
#define MAX_STRUCTS 6
#define MAX_FIELDS  6
#define MAX_TOKENS  24

typedef struct {
    char name[MEM_NAME_SIZE];
    int fieldCount;
    char fields[MAX_FIELDS][MEM_NAME_SIZE];
    char types[MAX_FIELDS][MEM_TYPE_SIZE];
} StructDef;

typedef struct {
    MemScript *s;
    MemStep *step;          // the step being built, or NULL before the first "> "
    StructDef structs[MAX_STRUCTS];
    int structCount;
    int frameStack[MEM_MAX_FRAMES];  // active frames, innermost last
    int frameDepth;
    int frameStackTop[MEM_MAX_FRAMES];  // stack address when each active frame began
    int stackTop, heapTop;
    char *err;
    size_t errSize;
    int lineNo;
} Parser;

static bool Fail(Parser *p, const char *fmt, ...)
{
    char message[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    snprintf(p->err, p->errSize, "line %d: %s", p->lineNo, message);
    return false;
}

static void Copy(char *dst, size_t size, const char *src) { snprintf(dst, size, "%s", src ? src : ""); }

// ---------------------------------------------------------------------------
// Types

bool MemIsPointerType(const char *type) { return strchr(type, '*') != NULL; }

static const StructDef *FindStruct(const Parser *p, const char *name)
{
    for (int i = 0; i < p->structCount; i++) {
        if (strcmp(p->structs[i].name, name) == 0) return &p->structs[i];
    }
    return NULL;
}

// Bytes a value of this type takes, as on a typical 64-bit PC.
static int TypeSize(const Parser *p, const char *type)
{
    if (MemIsPointerType(type)) return 8;
    if (strcmp(type, "char") == 0 || strcmp(type, "bool") == 0) return 1;
    if (strcmp(type, "short") == 0) return 2;
    if (strcmp(type, "double") == 0 || strcmp(type, "long") == 0) return 8;
    const StructDef *def = FindStruct(p, type);
    if (def) {
        int total = 0;
        for (int i = 0; i < def->fieldCount; i++) {
            int size = TypeSize(p, def->types[i]);
            total = (total + size - 1) / size * size + size;  // each field aligned to its size
        }
        return total ? (total + 7) / 8 * 8 : 8;
    }
    return 4;  // int, float, and anything else
}

// ---------------------------------------------------------------------------
// Groups and cells

static MemState *State(Parser *p) { return &p->step->state; }

// The live group with this name, innermost frame first (so a function's variables hide main's).
static int FindGroup(Parser *p, const char *name)
{
    for (int g = p->s->groupCount - 1; g >= 0; g--) {
        if (State(p)->live[g] && strcmp(p->s->groups[g].name, name) == 0) return g;
    }
    return -1;
}

static int AddCell(Parser *p, int group, const char *label, const char *type, MemRegion region)
{
    MemScript *s = p->s;
    if (s->cellCount >= MEM_MAX_CELLS) return -1;
    MemCell *c = &s->cells[s->cellCount];
    Copy(c->label, sizeof c->label, label);
    Copy(c->type, sizeof c->type, type);
    c->group = group;
    int size = TypeSize(p, type);
    int align = size > 8 ? 8 : size;
    int *top = region == MEM_HEAP ? &p->heapTop : &p->stackTop;
    *top = (*top + align - 1) / align * align;
    c->address = *top;
    *top += size;
    State(p)->values[s->cellCount] = (MemValue){ MEM_GARBAGE, "?", -1 };
    return s->cellCount++;
}

// Creates a variable, array or struct and its cells. Returns the group, or -1 after Fail.
static int AddGroup(Parser *p, const char *name, const char *type, MemRegion region, MemShape shape, int elements)
{
    MemScript *s = p->s;
    if (s->groupCount >= MEM_MAX_GROUPS) return Fail(p, "too many variables (at most %d)", MEM_MAX_GROUPS), -1;
    if (region == MEM_STACK) {
        for (int g = 0; g < s->groupCount; g++) {
            bool sameFrame = s->groups[g].frame == p->frameStack[p->frameDepth - 1];
            if (State(p)->live[g] && sameFrame && strcmp(s->groups[g].name, name) == 0) {
                return Fail(p, "'%s' is already declared", name), -1;
            }
        }
    } else if (FindGroup(p, name) >= 0) {
        return Fail(p, "the name '%s' is already used", name), -1;
    }
    int g = s->groupCount++;
    MemGroup *group = &s->groups[g];
    Copy(group->name, sizeof group->name, name);
    Copy(group->type, sizeof group->type, type);
    group->region = region;
    group->shape = shape;
    group->frame = p->frameStack[p->frameDepth - 1];
    group->firstCell = s->cellCount;
    State(p)->live[g] = true;

    const StructDef *def = shape == MEM_STRUCT ? FindStruct(p, type) : NULL;
    int count = def ? def->fieldCount : elements;
    for (int i = 0; i < count; i++) {
        char label[MEM_NAME_SIZE];
        if (shape == MEM_ARRAY) snprintf(label, sizeof label, "[%d]", i);
        else Copy(label, sizeof label, def ? def->fields[i] : (region == MEM_HEAP ? type : name));
        if (AddCell(p, g, label, def ? def->types[i] : type, region) < 0) {
            return Fail(p, "too many boxes (at most %d)", MEM_MAX_CELLS), -1;
        }
    }
    group->cellCount = count;
    return g;
}

static const MemGroup *GroupOf(const MemScript *s, int cell) { return &s->groups[s->cells[cell].group]; }

// ---------------------------------------------------------------------------
// Places and values

static bool IsNameStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool IsNameChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

// The cell a pointer cell points at, or -1 (with Fail) when it doesn't point anywhere usable.
static int Deref(Parser *p, int cell)
{
    MemScript *s = p->s;
    if (!MemIsPointerType(s->cells[cell].type)) return Fail(p, "'%s' is not a pointer", s->cells[cell].label), -1;
    MemValue v = State(p)->values[cell];
    if (v.kind != MEM_POINTER) return Fail(p, "'%s' doesn't point at anything yet", s->cells[cell].label), -1;
    int g = s->cells[v.target].group;
    if (!State(p)->live[g] || State(p)->freed[g]) return Fail(p, "'%s' is a dangling pointer", s->cells[cell].label), -1;
    return v.target;
}

// Moves from cell by `offset` elements within its array. -1 (with Fail) if that leaves the array.
static int Offset(Parser *p, int cell, int offset)
{
    const MemGroup *g = GroupOf(p->s, cell);
    int index = cell - g->firstCell + offset;
    if (g->shape != MEM_ARRAY && offset != 0) return Fail(p, "pointer arithmetic only works inside an array"), -1;
    if (index < 0 || index >= g->cellCount) return Fail(p, "that goes past the end of the array"), -1;
    return g->firstCell + index;
}

static int FieldOf(Parser *p, int cell, const char *field)
{
    MemScript *s = p->s;
    const MemGroup *g = GroupOf(s, cell);
    if (g->shape != MEM_STRUCT) return Fail(p, "'%s' is not a struct", g->name), -1;
    for (int i = 0; i < g->cellCount; i++) {
        if (strcmp(s->cells[g->firstCell + i].label, field) == 0) return g->firstCell + i;
    }
    return Fail(p, "%s has no field '%s'", g->type, field), -1;
}

// Resolves a place such as p, *p, **pp, a[2], n.next, p->next or p[1] to a cell.
// Returns -1 after Fail.
static int ResolvePlace(Parser *p, const char *expr)
{
    int stars = 0;
    while (*expr == '*') { stars++; expr++; }
    if (*expr == '(') {  // (*p).field is written p->field in scripts
        return Fail(p, "write p->field rather than (*p).field"), -1;
    }
    char name[MEM_NAME_SIZE];
    int n = 0;
    if (!IsNameStart(*expr)) return Fail(p, "expected a name in '%s'", expr), -1;
    while (IsNameChar(*expr) && n < MEM_NAME_SIZE - 1) name[n++] = *expr++;
    name[n] = '\0';
    int g = FindGroup(p, name);
    if (g < 0) return Fail(p, "unknown name '%s'", name), -1;
    int cell = p->s->groups[g].firstCell;
    bool atArrayName = p->s->groups[g].shape == MEM_ARRAY;  // a[i] indexes the array itself

    while (*expr) {
        if (*expr == '[') {
            char *end;
            long index = strtol(expr + 1, &end, 10);
            if (*end != ']') return Fail(p, "expected ] in an index"), -1;
            expr = end + 1;
            // Indexing an array picks an element; indexing a pointer counts from where it points.
            if (atArrayName) {
                cell = Offset(p, cell, (int)index);
            } else {
                cell = Deref(p, cell);
                if (cell >= 0) cell = Offset(p, cell, (int)index);
            }
        } else if (*expr == '.' || (expr[0] == '-' && expr[1] == '>')) {
            bool arrow = *expr == '-';
            expr += arrow ? 2 : 1;
            char field[MEM_NAME_SIZE];
            int f = 0;
            while (IsNameChar(*expr) && f < MEM_NAME_SIZE - 1) field[f++] = *expr++;
            field[f] = '\0';
            if (arrow) cell = Deref(p, cell);
            if (cell >= 0) cell = FieldOf(p, cell, field);
        } else {
            return Fail(p, "can't read '%s'", expr), -1;
        }
        atArrayName = false;
        if (cell < 0) return -1;
    }
    for (int i = 0; i < stars && cell >= 0; i++) cell = Deref(p, cell);
    return cell;
}

static MemValue PointerTo(const MemScript *s, int cell)
{
    MemValue v = { MEM_POINTER, "", cell };
    snprintf(v.text, sizeof v.text, "%d", s->cells[cell].address);
    return v;
}

static bool IsLiteral(const char *token)
{
    return isdigit((unsigned char)token[0]) || token[0] == '-' || token[0] == '\'' || token[0] == '"'
        || token[0] == '.' || strcmp(token, "true") == 0 || strcmp(token, "false") == 0;
}

// Parses a value token. Returns false after Fail.
static bool ParseValue(Parser *p, const char *token, MemValue *out)
{
    if (strcmp(token, "?") == 0) {
        *out = (MemValue){ MEM_GARBAGE, "?", -1 };
        return true;
    }
    if (strcmp(token, "NULL") == 0 || strcmp(token, "nullptr") == 0) {
        *out = (MemValue){ MEM_NULL, "", -1 };
        Copy(out->text, sizeof out->text, token);
        return true;
    }
    if (token[0] == '&') {
        int cell = ResolvePlace(p, token + 1);
        if (cell < 0) return false;
        *out = PointerTo(p->s, cell);
        return true;
    }
    if (IsLiteral(token)) {
        *out = (MemValue){ MEM_PLAIN, "", -1 };
        Copy(out->text, sizeof out->text, token);
        return true;
    }
    // Pointer arithmetic: p+1, p-2.
    const char *sign = strpbrk(token, "+-");
    while (sign && sign[0] == '-' && sign[1] == '>') sign = strpbrk(sign + 2, "+-");
    if (sign) {
        char place[MEM_NAME_SIZE * 2];
        snprintf(place, sizeof place, "%.*s", (int)(sign - token), token);
        int cell = ResolvePlace(p, place);
        if (cell < 0) return false;
        int target = Deref(p, cell);
        if (target < 0) return false;
        int amount = atoi(sign + 1);
        target = Offset(p, target, sign[0] == '+' ? amount : -amount);
        if (target < 0) return false;
        *out = PointerTo(p->s, target);
        return true;
    }
    // Another place: copy its value (q = p, x = *p).
    int cell = ResolvePlace(p, token);
    if (cell < 0) return false;
    *out = State(p)->values[cell];
    return true;
}

// ---------------------------------------------------------------------------
// Commands

static int Tokenize(char *line, char **tokens)
{
    int n = 0;
    for (char *t = strtok(line, " \t"); t && n < MAX_TOKENS; t = strtok(NULL, " \t")) tokens[n++] = t;
    return n;
}

static bool CommandStruct(Parser *p, char **tok, int n)
{
    if (n < 3) return Fail(p, "struct needs a name and fields, like: struct Node data:int next:Node*");
    if (p->structCount >= MAX_STRUCTS) return Fail(p, "too many structs");
    StructDef *def = &p->structs[p->structCount++];
    Copy(def->name, sizeof def->name, tok[1]);
    for (int i = 2; i < n && def->fieldCount < MAX_FIELDS; i++) {
        char *colon = strchr(tok[i], ':');
        if (!colon) return Fail(p, "fields are written name:type, not '%s'", tok[i]);
        *colon = '\0';
        Copy(def->fields[def->fieldCount], MEM_NAME_SIZE, tok[i]);
        Copy(def->types[def->fieldCount], MEM_TYPE_SIZE, colon + 1);
        def->fieldCount++;
    }
    return true;
}

// var / heap (one value, or one per struct field) and array / heaparray (one per element).
static bool CommandCreate(Parser *p, char **tok, int n, MemRegion region, bool array)
{
    if (n < 3) return Fail(p, "%s needs a name and a type", tok[0]);
    const char *name = tok[1], *type = tok[2];
    int values = n - 3;
    if (array && values < 1) return Fail(p, "an array needs one value per element (use ? for unset ones)");
    MemShape shape = array ? MEM_ARRAY : (FindStruct(p, type) ? MEM_STRUCT : MEM_VAR);
    int g = AddGroup(p, name, type, region, shape, array ? values : 1);
    if (g < 0) return false;
    const MemGroup *group = &p->s->groups[g];
    if (values > group->cellCount) return Fail(p, "%d values for %d boxes", values, group->cellCount);
    for (int i = 0; i < values; i++) {
        MemValue v;
        if (!ParseValue(p, tok[3 + i], &v)) return false;
        State(p)->values[group->firstCell + i] = v;
    }
    return true;
}

static bool CommandSet(Parser *p, char **tok, int n)
{
    if (n != 3) return Fail(p, "set needs a place and a value, like: set *p 3");
    int cell = ResolvePlace(p, tok[1]);
    if (cell < 0) return false;
    MemValue v;
    if (!ParseValue(p, tok[2], &v)) return false;
    State(p)->values[cell] = v;
    return true;
}

static bool CommandFree(Parser *p, char **tok, int n)
{
    if (n != 2) return Fail(p, "%s needs the pointer, like: %s p", tok[0], tok[0]);
    int cell = ResolvePlace(p, tok[1]);
    if (cell < 0) return false;
    int target = Deref(p, cell);
    if (target < 0) return false;
    int g = p->s->cells[target].group;
    if (p->s->groups[g].region != MEM_HEAP) return Fail(p, "'%s' doesn't point at heap memory", tok[1]);
    if (target != p->s->groups[g].firstCell) return Fail(p, "free must be given the start of the block");
    State(p)->freed[g] = true;
    return true;
}

static bool CommandFrame(Parser *p, char **tok, int n)
{
    if (n != 2) return Fail(p, "frame needs the function's name");
    if (p->s->frameCount >= MEM_MAX_FRAMES || p->frameDepth >= MEM_MAX_FRAMES) return Fail(p, "too many function calls");
    Copy(p->s->frames[p->s->frameCount], MEM_NAME_SIZE, tok[1]);
    p->frameStackTop[p->frameDepth] = p->stackTop;
    p->frameStack[p->frameDepth++] = p->s->frameCount++;
    return true;
}

static bool CommandReturn(Parser *p)
{
    if (p->frameDepth <= 1) return Fail(p, "return without a matching frame");
    int frame = p->frameStack[--p->frameDepth];
    for (int g = 0; g < p->s->groupCount; g++) {
        if (p->s->groups[g].region == MEM_STACK && p->s->groups[g].frame == frame) State(p)->live[g] = false;
    }
    p->stackTop = p->frameStackTop[p->frameDepth];  // the function's stack space is free again
    return true;
}

static bool AppendExplain(Parser *p, const char *text)
{
    char *explain = p->step->explain;
    size_t used = strlen(explain);
    if (used + strlen(text) + 2 >= MEM_EXPLAIN_SIZE) return Fail(p, "the explanation is too long");
    if (used) strcat(explain, " ");
    strcat(explain, text);
    return true;
}

static bool NewStep(Parser *p, const char *code)
{
    MemScript *s = p->s;
    if (s->stepCount >= MEM_MAX_STEPS) return Fail(p, "too many steps (at most %d)", MEM_MAX_STEPS);
    MemStep *step = &s->steps[s->stepCount];
    if (s->stepCount > 0) step->state = s->steps[s->stepCount - 1].state;  // memory carries on
    Copy(step->code, sizeof step->code, code);
    step->explain[0] = '\0';
    s->stepCount++;
    p->step = step;
    return true;
}

static const char *Rest(const char *line, size_t keywordLength)
{
    line += keywordLength;
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

static bool ParseLine(Parser *p, char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    size_t len = strlen(line);
    while (len && (line[len - 1] == ' ' || line[len - 1] == '\r')) line[--len] = '\0';
    if (!line[0] || line[0] == '#') return true;

    if (line[0] == '>') return NewStep(p, line[1] == ' ' ? line + 2 : line + 1);  // keeps the code's indentation
    if (strncmp(line, "title ", 6) == 0) { Copy(p->s->title, sizeof p->s->title, Rest(line, 6)); return true; }
    if (strncmp(line, "say ", 4) == 0) {
        if (!p->step) return Fail(p, "say must come after a > line");
        return AppendExplain(p, Rest(line, 4));
    }

    char copy[256];
    Copy(copy, sizeof copy, line);
    char *tok[MAX_TOKENS] = { 0 };
    int n = Tokenize(copy, tok);
    if (n == 0) return true;
    if (strcmp(tok[0], "struct") == 0) return CommandStruct(p, tok, n);
    if (!p->step) return Fail(p, "'%s' must come after a > line", tok[0]);
    if (strcmp(tok[0], "var") == 0) return CommandCreate(p, tok, n, MEM_STACK, false);
    if (strcmp(tok[0], "array") == 0) return CommandCreate(p, tok, n, MEM_STACK, true);
    if (strcmp(tok[0], "heap") == 0) return CommandCreate(p, tok, n, MEM_HEAP, false);
    if (strcmp(tok[0], "heaparray") == 0) return CommandCreate(p, tok, n, MEM_HEAP, true);
    if (strcmp(tok[0], "set") == 0) return CommandSet(p, tok, n);
    if (strcmp(tok[0], "free") == 0 || strcmp(tok[0], "delete") == 0) return CommandFree(p, tok, n);
    if (strcmp(tok[0], "frame") == 0) return CommandFrame(p, tok, n);
    if (strcmp(tok[0], "return") == 0) return CommandReturn(p);
    return Fail(p, "unknown command '%s'", tok[0]);
}

bool MemScriptParse(const char *text, MemScript *out, char *err, size_t errSize)
{
    memset(out, 0, sizeof *out);
    Parser p = { .s = out, .err = err, .errSize = errSize, .stackTop = STACK_START, .heapTop = HEAP_START };
    Copy(out->frames[0], MEM_NAME_SIZE, "main");
    out->frameCount = 1;
    p.frameDepth = 1;
    if (err && errSize) err[0] = '\0';

    const char *line = text ? text : "";
    bool ok = true;
    while (ok && *line) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        char buffer[256];
        p.lineNo++;
        if (length >= sizeof buffer) {
            ok = Fail(&p, "line too long");
            break;
        }
        memcpy(buffer, line, length);
        buffer[length] = '\0';
        ok = ParseLine(&p, buffer);
        line = end ? end + 1 : line + length;
    }
    if (ok && out->stepCount == 0) {
        p.lineNo = 1;
        ok = Fail(&p, "no steps: start each step with a '> code' line");
    }
    for (int i = 0; ok && i < out->stepCount; i++) {
        if (!out->steps[i].explain[0]) {
            snprintf(err, errSize, "step %d (%s) has no 'say' explanation", i + 1, out->steps[i].code);
            ok = false;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Explanations

typedef struct {
    char *out;
    size_t size, used;
} Writer;

static void Say(Writer *w, const char *fmt, ...)
{
    if (w->used >= w->size) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(w->out + w->used, w->size - w->used, fmt, args);
    va_end(args);
    if (n > 0) w->used += (size_t)n < w->size - w->used ? (size_t)n : w->size - w->used - 1;
}

static const char *Article(const char *word) { return strchr("aeiouAEIOU", word[0]) ? "an" : "a"; }

void MemCellName(const MemScript *s, int cell, char *out, size_t size)
{
    const MemCell *c = &s->cells[cell];
    const MemGroup *g = GroupOf(s, cell);
    if (g->region == MEM_HEAP) {
        if (g->shape == MEM_ARRAY) snprintf(out, size, "heap %s%s", g->type, c->label);
        else if (g->shape == MEM_STRUCT) snprintf(out, size, "the heap %s's %s", g->type, c->label);
        else snprintf(out, size, "the heap %s", g->type);
    } else if (g->shape == MEM_ARRAY) {
        snprintf(out, size, "%s%s", g->name, c->label);
    } else if (g->shape == MEM_STRUCT) {
        snprintf(out, size, "%s.%s", g->name, c->label);
    } else {
        snprintf(out, size, "%s", g->name);
    }
}

// "int" for "int*", "Node*" for "Node**".
static void PointedType(const char *type, char *out, size_t size)
{
    Copy(out, size, type);
    char *star = strrchr(out, '*');
    if (star) *star = '\0';
    size_t n = strlen(out);
    while (n && out[n - 1] == ' ') out[--n] = '\0';
}

static bool GroupGone(const MemState *st, const MemScript *s, int cell)
{
    int g = s->cells[cell].group;
    return !st->live[g] || st->freed[g];
}

// Whether a pointer reaches a whole struct rather than one field: a Node* holding the
// address of a Node points at the Node (whose first field happens to share its address).
static bool PointsAtWholeStruct(const MemScript *s, int pointerCell, int target)
{
    const MemGroup *g = GroupOf(s, target);
    if (g->shape != MEM_STRUCT || target != g->firstCell) return false;
    char pointed[MEM_TYPE_SIZE];
    PointedType(s->cells[pointerCell].type, pointed, sizeof pointed);
    return strcmp(pointed, g->type) == 0;
}

// What a pointer reaches, in words: "i", "a[2]", or for a struct pointer "the heap Node" / "n".
static void TargetName(const MemScript *s, int pointerCell, int target, char *out, size_t size)
{
    const MemGroup *g = GroupOf(s, target);
    if (!PointsAtWholeStruct(s, pointerCell, target)) MemCellName(s, target, out, size);
    else if (g->region == MEM_HEAP) snprintf(out, size, "the heap %s", g->type);
    else snprintf(out, size, "%s", g->name);
}

// A struct's fields and values: "data 10, nextPtr 5032".
static void StructFields(const MemScript *s, const MemState *st, const MemGroup *g, char *out, size_t size)
{
    out[0] = '\0';
    for (int i = 0; i < g->cellCount; i++) {
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s %s", i ? ", " : "", s->cells[g->firstCell + i].label,
                 st->values[g->firstCell + i].text);
    }
}

// Follows a pointer chain: "*pp is p, which holds 1004. **pp is i, which holds 5."
static void FollowChain(Writer *w, const MemScript *s, const MemState *st, int cell, const char *name)
{
    char expr[48], targetName[48];
    snprintf(expr, sizeof expr, "%s", name);
    for (int depth = 0; depth < 4; depth++) {
        MemValue v = st->values[cell];
        if (v.kind != MEM_POINTER || GroupGone(st, s, v.target)) return;
        char deeper[sizeof expr];
        snprintf(deeper, sizeof deeper, "*%.*s", (int)sizeof expr - 2, expr);
        Copy(expr, sizeof expr, deeper);
        if (PointsAtWholeStruct(s, cell, v.target)) {  // the whole struct: list its fields
            char fields[160];
            TargetName(s, cell, v.target, targetName, sizeof targetName);
            StructFields(s, st, GroupOf(s, v.target), fields, sizeof fields);
            Say(w, " So %s is %s (%s).", expr, targetName, fields);
            return;
        }
        cell = v.target;
        MemCellName(s, cell, targetName, sizeof targetName);
        MemValue t = st->values[cell];
        if (t.kind == MEM_GARBAGE) {
            Say(w, " So %s is %s, which hasn't been given a value yet.", expr, targetName);
            return;
        }
        if (t.kind == MEM_NULL) {
            Say(w, " So %s is %s, which is NULL.", expr, targetName);
            return;
        }
        Say(w, " So %s is %s, which holds %s%s.", expr, targetName, t.kind == MEM_POINTER ? "the address " : "", t.text);
        if (t.kind != MEM_POINTER) return;
    }
}

void MemDescribeCell(const MemScript *s, int step, int cell, char *out, size_t size)
{
    Writer w = { out, size, 0 };
    if (size) out[0] = '\0';
    if (step < 0 || step >= s->stepCount || cell < 0 || cell >= s->cellCount || size < 2) return;
    const MemState *st = &s->steps[step].state;
    const MemCell *c = &s->cells[cell];
    const MemGroup *g = GroupOf(s, cell);
    int groupIndex = c->group;
    char name[48];
    MemCellName(s, cell, name, sizeof name);
    bool pointer = MemIsPointerType(c->type);
    char pointed[MEM_TYPE_SIZE];
    PointedType(c->type, pointed, sizeof pointed);

    // What it is and where it lives.
    if (g->region == MEM_HEAP) {
        if (g->shape == MEM_STRUCT) Say(&w, "This is the %s field of a %s on the heap, at address %d.", c->label, g->type, c->address);
        else if (g->shape == MEM_ARRAY) Say(&w, "This is element %s of an array of %s on the heap, at address %d.", c->label, g->type, c->address);
        else Say(&w, "This is %s %s on the heap, at address %d.", Article(c->type), c->type, c->address);
        if (cell == g->firstCell) Say(&w, " Heap memory has no name of its own: the program reaches it only through pointers.");
    } else if (pointer) {
        Say(&w, "%s is a pointer to %s %s (type %s), stored at address %d.", name, Article(pointed), pointed, c->type, c->address);
    } else if (g->shape == MEM_ARRAY) {
        Say(&w, "%s is element %d of the array %s: %s %s at address %d.", name, cell - g->firstCell, g->name,
            Article(c->type), c->type, c->address);
    } else if (g->shape == MEM_STRUCT) {
        Say(&w, "%s is the %s field of %s: %s %s at address %d.", name, c->label, g->name, Article(c->type), c->type, c->address);
    } else {
        Say(&w, "%s is %s %s, stored at address %d.", name, Article(c->type), c->type, c->address);
    }
    if (g->region == MEM_STACK && g->frame != 0) {
        Say(&w, " It belongs to %s() and disappears when %s returns.", s->frames[g->frame], s->frames[g->frame]);
    }

    if (!st->live[groupIndex]) {
        Say(&w, " But %s has returned, so %s no longer exists: its memory may be reused by the next function call. "
                "It is only shown because a pointer still holds its address.", s->frames[g->frame], name);
    } else if (st->freed[groupIndex]) {
        Say(&w, " This memory has been freed, so the program may reuse it at any moment. Nothing should read or write it any more.");
    } else {
        // What it holds.
        MemValue v = st->values[cell];
        if (v.kind == MEM_GARBAGE) {
            if (pointer) Say(&w, " It hasn't been given a value, so it holds a random address. Using *%s now is undefined behaviour: set it to NULL or to a real address first.", name);
            else Say(&w, " It hasn't been given a value yet, so it holds garbage: whatever was in that memory before.");
        } else if (v.kind == MEM_NULL) {
            Say(&w, " It holds %s: it points at nothing. Dereferencing it would crash the program, so check %s != NULL first.", v.text, name);
        } else if (v.kind == MEM_PLAIN) {
            Say(&w, " It holds the value %s.", v.text);
        } else {
            char targetName[48];
            TargetName(s, cell, v.target, targetName, sizeof targetName);
            Say(&w, " It holds %s, which is the address of %s: the arrow shows where it points.", v.text, targetName);
            if (!st->live[s->cells[v.target].group]) {
                Say(&w, " But %s no longer exists (its function has returned), so %s is a dangling pointer.", targetName, name);
            } else if (st->freed[s->cells[v.target].group]) {
                Say(&w, " But that memory has been freed, so %s is a dangling pointer: using *%s is undefined behaviour.", name, name);
            } else {
                FollowChain(&w, s, st, cell, name);
                const MemGroup *tg = GroupOf(s, v.target);
                if (tg->shape == MEM_ARRAY && v.target + 1 < tg->firstCell + tg->cellCount) {
                    char nextName[48];
                    MemCellName(s, v.target + 1, nextName, sizeof nextName);
                    int step = s->cells[v.target + 1].address - s->cells[v.target].address;
                    Say(&w, " %s+1 would point at the next element, %s, %d bytes further on (the size of one %s).",
                        name, nextName, step, s->cells[v.target].type);
                }
            }
        }
    }

    // Who points here.
    char pointers[160] = "";
    int pointerCount = 0, firstPointerCell = -1;
    char firstPointer[48] = "";
    for (int other = 0; other < s->cellCount; other++) {
        MemValue v = st->values[other];
        if (v.kind != MEM_POINTER || v.target != cell || GroupGone(st, s, other)) continue;
        char otherName[48];
        MemCellName(s, other, otherName, sizeof otherName);
        if (pointerCount == 0) {
            Copy(firstPointer, sizeof firstPointer, otherName);
            firstPointerCell = other;
        }
        size_t used = strlen(pointers);
        snprintf(pointers + used, sizeof pointers - used, "%s%s", pointerCount ? ", " : "", otherName);
        pointerCount++;
    }
    if (pointerCount == 1 && !st->freed[groupIndex] && PointsAtWholeStruct(s, firstPointerCell, cell)) {
        Say(&w, " %s points at this whole %s, so %s->%s is this field.", firstPointer, g->type, firstPointer, c->label);
    } else if (pointerCount == 1 && !st->freed[groupIndex]) {
        Say(&w, " %s points here, so *%s is another name for %s: changing one changes the other.", firstPointer, firstPointer, name);
    } else if (pointerCount == 1) {
        Say(&w, " %s still holds this address, so %s is a dangling pointer.", firstPointer, firstPointer);
    } else if (pointerCount > 1) {
        Say(&w, " Pointing here: %s. Each of them reaches this same memory.", pointers);
    }

    // A heap block nobody can reach any more.
    if (g->region == MEM_HEAP && !st->freed[groupIndex]) {
        bool reachable = false;
        for (int other = 0; other < s->cellCount && !reachable; other++) {
            MemValue v = st->values[other];
            reachable = v.kind == MEM_POINTER && s->cells[v.target].group == groupIndex && !GroupGone(st, s, other);
        }
        if (!reachable) Say(&w, " No pointer holds its address any more, so the program can never free it: this is a memory leak.");
    }
}
