// Speed round questions and timing. See speed_round.h.
//
// Every generator builds its snippet line by line while running it on a tiny model of the
// variables, so the answer and the per-line notes come from the same steps as the code.
// Alongside the true model each one runs a "misconception" model (a by-value call that
// changes the caller, a pointer move that is ignored, counting array indexes from 1...),
// whose result becomes the most tempting wrong answer. tests/test_speed_round.c compiles
// generated snippets with gcc and g++ to check the answers.
#include "speed_round.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *skillNames[SPEED_SKILL_COUNT] = {
    "variables", "arithmetic", "conditions", "loops", "arrays", "strings", "functions", "pointers",
    "pointer_to_pointer", "pass_by_pointer", "pointer_arithmetic", "malloc", "function_pointers",
    "references", "new_delete", "objects", "linked_list",
};
static const char *skillTitles[SPEED_SKILL_COUNT] = {
    "Variables", "Arithmetic", "Conditions", "Loops", "Arrays", "Strings", "Functions", "Pointers",
    "Pointers to pointers", "Pass by pointer", "Pointer arithmetic", "malloc and free", "Function pointers",
    "References", "new and delete", "Objects", "Linked lists",
};

const char *SpeedSkillName(SpeedSkill skill) { return skill < SPEED_SKILL_COUNT ? skillNames[skill] : ""; }
const char *SpeedSkillTitle(SpeedSkill skill) { return skill < SPEED_SKILL_COUNT ? skillTitles[skill] : ""; }

int SpeedSkillsParse(const char *names, SpeedSkill *out, int max)
{
    int count = 0;
    while (names && *names && count < max) {
        while (*names == ' ' || *names == '\t' || *names == ',') names++;
        size_t length = strcspn(names, " \t,");
        if (length == 0) break;
        for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
            if (strlen(skillNames[s]) == length && strncmp(names, skillNames[s], length) == 0) {
                out[count++] = (SpeedSkill)s;
                break;
            }
        }
        names += length;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Building a question

static uint32_t NextRandom(uint32_t *seed)
{
    if (*seed == 0) *seed = 0x9e3779b9u;
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

// A random int from lo to hi inclusive.
static int Rand(uint32_t *seed, int lo, int hi)
{
    return hi <= lo ? lo : lo + (int)(NextRandom(seed) % (uint32_t)(hi - lo + 1));
}

// Adds a line of code with the note shown beside it when the answer is explained.
static void Emit(SpeedQuestion *q, const char *note, const char *format, ...)
{
    if (q->lineCount >= SPEED_MAX_LINES) return;
    va_list args;
    va_start(args, format);
    vsnprintf(q->lines[q->lineCount], SPEED_LINE_SIZE, format, args);
    va_end(args);
    snprintf(q->notes[q->lineCount], SPEED_LINE_SIZE, "%s", note ? note : "");
    q->lineCount++;
}

// Formats a note into a static buffer, for passing to Emit.
static const char *Note(const char *format, ...)
{
    static char note[SPEED_LINE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(note, sizeof note, format, args);
    va_end(args);
    return note;
}

static bool Contains(const int *values, int count, int value)
{
    for (int i = 0; i < count; i++) if (values[i] == value) return true;
    return false;
}

// Sets the asked variable and the four choices: the answer, the tempting wrong answers
// (decoys) first, then near misses, all different, in a random order.
static void Finish(SpeedQuestion *q, uint32_t *seed, const char *asks, int answer, const int *decoys, int decoyCount)
{
    snprintf(q->asks, sizeof q->asks, "%s", asks);
    int wrong[SPEED_CHOICES - 1], wrongCount = 0;
    for (int i = 0; i < decoyCount && wrongCount < SPEED_CHOICES - 1; i++) {
        if (decoys[i] != answer && !Contains(wrong, wrongCount, decoys[i])) wrong[wrongCount++] = decoys[i];
    }
    static const int nearMisses[] = { 1, -1, 2, -2, 10, -10, 3, 5 };
    for (int i = 0; wrongCount < SPEED_CHOICES - 1; i++) {
        int candidate = answer + nearMisses[i % 8] * (1 + i / 8);
        if (candidate != answer && !Contains(wrong, wrongCount, candidate)) wrong[wrongCount++] = candidate;
    }
    q->correct = Rand(seed, 0, SPEED_CHOICES - 1);
    int next = Rand(seed, 0, wrongCount - 1);  // rotate the wrong answers so the decoy moves about
    for (int i = 0; i < SPEED_CHOICES; i++) {
        if (i == q->correct) {
            q->choices[i] = answer;
        } else {
            q->choices[i] = wrong[next];
            next = (next + 1) % wrongCount;
        }
    }
}

// How many lines of operations a question has at this difficulty.
static int OperationCount(int difficulty) { return 2 + difficulty / 3; }

// ---------------------------------------------------------------------------
// Generators, one per skill

static void Arithmetic(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int x = Rand(seed, 4, 20);
    int thought = x;  // misconception: division and % round to the nearest whole number
    int beforeLast = x;
    Emit(q, Note("x = %d", x), "int x = %d;", x);
    int ops = OperationCount(difficulty) + 1;
    for (int i = 0; i < ops; i++) {
        beforeLast = x;
        int kind = Rand(seed, 0, 6), k;
        if (kind == 2 && x > 30) kind = 1;
        if ((kind == 1 || kind == 6) && x < 3) kind = 0;
        switch (kind) {
        case 0: k = Rand(seed, 1, 9); x += k; thought += k; Emit(q, Note("x = %d", x), "x += %d;", k); break;
        case 1: k = Rand(seed, 1, x < 9 ? x : 9); x -= k; thought -= k; Emit(q, Note("x = %d", x), "x -= %d;", k); break;
        case 2: k = Rand(seed, 2, 3); x *= k; thought *= k; Emit(q, Note("x = %d", x), "x *= %d;", k); break;
        case 3:
            k = Rand(seed, 2, 4);
            thought = (thought * 2 + k) / (2 * k);
            x /= k;
            Emit(q, Note("x = %d (whole numbers only)", x), "x /= %d;", k);
            break;
        case 4:
            k = Rand(seed, 3, 7);
            thought = thought / k;  // mixing up % (remainder) with / (how many times)
            x %= k;
            Emit(q, Note("x = %d (the remainder)", x), "x %%= %d;", k);
            break;
        case 5: x++; thought++; Emit(q, Note("x = %d", x), "x++;"); break;
        default: x--; thought--; Emit(q, Note("x = %d", x), "x--;"); break;
        }
    }
    int decoys[] = { thought, beforeLast };
    Finish(q, seed, "x", x, decoys, 2);
}

static void Loops(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int x, decoys[3], n = Rand(seed, 3, 4 + difficulty / 2);
    switch (Rand(seed, 0, 3)) {
    case 0: {
        int start = Rand(seed, 0, 10), k = Rand(seed, 2, 5);
        x = start + n * k;
        Emit(q, Note("x = %d", start), "int x = %d;", start);
        Emit(q, Note("x = %d after %d passes", x, n), "for (int i = 0; i < %d; i++) x += %d;", n, k);
        decoys[0] = start + (n + 1) * k; decoys[1] = start + (n - 1) * k; decoys[2] = start + k;
        break;
    }
    case 1: {
        x = n * (n + 1) / 2;
        Emit(q, Note("x = 0"), "int x = 0;");
        Emit(q, Note("x = %d (1 + 2 + ... + %d)", x, n), "for (int i = 1; i <= %d; i++) x += i;", n);
        decoys[0] = n * (n - 1) / 2; decoys[1] = x + n + 1; decoys[2] = n;
        break;
    }
    case 2: {
        int limit = Rand(seed, 5, 20 + difficulty * 10);
        x = 1;
        while (x < limit) x *= 2;
        Emit(q, Note("x = 1"), "int x = 1;");
        Emit(q, Note("x = %d, the first power of 2 not below %d", x, limit), "while (x < %d) x *= 2;", limit);
        decoys[0] = x / 2; decoys[1] = x * 2; decoys[2] = limit;
        break;
    }
    default: {
        int start = Rand(seed, 0, 9), passes = (n + 1) / 2;
        x = start + passes;
        Emit(q, Note("x = %d", start), "int x = %d;", start);
        Emit(q, Note("x = %d (i = %d, %d, ... : %d passes)", x, n, n - 2, passes), "for (int i = %d; i > 0; i -= 2) x++;", n);
        decoys[0] = start + n; decoys[1] = start + n / 2 + 1; decoys[2] = x - 1;
        break;
    }
    }
    if (difficulty >= 5) {  // one more step after the loop
        int k = Rand(seed, 1, 5);
        x -= k;
        for (int i = 0; i < 3; i++) decoys[i] -= k;
        Emit(q, Note("x = %d", x), "x -= %d;", k);
    }
    Finish(q, seed, "x", x, decoys, 3);
}

// An int array's contents as "{3, 5, 2, 8}".
static const char *ArrayText(const int *a, int n)
{
    static char text[48];
    int used = snprintf(text, sizeof text, "{");
    for (int i = 0; i < n; i++) used += snprintf(text + used, sizeof text - used, i ? ", %d" : "%d", a[i]);
    snprintf(text + used, sizeof text - used, "}");
    return text;
}

static void Arrays(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    enum { N = 4 };
    int a[N], thought[N];  // misconception: a[1] is the first element, so every index is one too far left
    for (int i = 0; i < N; i++) a[i] = thought[i] = Rand(seed, 1, 9);
    Emit(q, Note("a = %s", ArrayText(a, N)), "int a[4] = { %d, %d, %d, %d };", a[0], a[1], a[2], a[3]);
    int ops = OperationCount(difficulty);
    for (int op = 0; op < ops; op++) {
        int i = Rand(seed, 0, N - 1), j = Rand(seed, 0, N - 1), k = Rand(seed, 1, 5);
        int ti = (i + N - 1) % N, tj = (j + N - 1) % N;  // where the misconception looks
        switch (Rand(seed, 0, 2)) {
        case 0: a[i] = a[j] + k; thought[ti] = thought[tj] + k; Emit(q, Note("a = %s", ArrayText(a, N)), "a[%d] = a[%d] + %d;", i, j, k); break;
        case 1: a[i] *= 2; thought[ti] *= 2; Emit(q, Note("a = %s", ArrayText(a, N)), "a[%d] *= 2;", i); break;
        default: a[i]++; thought[ti]++; Emit(q, Note("a = %s", ArrayText(a, N)), "a[%d]++;", i); break;
        }
    }
    int i = Rand(seed, 0, N - 1), j = Rand(seed, 0, N - 1);
    int x = a[i] + a[j];
    Emit(q, Note("x = %d + %d = %d", a[i], a[j], x), "int x = a[%d] + a[%d];", i, j);
    int decoys[] = { thought[(i + N - 1) % N] + thought[(j + N - 1) % N], a[(i + 1) % N] + a[(j + 1) % N], a[i] + a[j] * 2 };
    Finish(q, seed, "x", x, decoys, 3);
}

// "x = 5, y = 3" plus where a pointer points, for the pointer questions' notes. pointerNote
// is often Note()'s buffer, so it is copied before this builds its own.
static const char *TwoVars(const int *v, const char *pointerNote)
{
    static char text[SPEED_LINE_SIZE * 2];  // room for any extra; Emit cuts it to a line
    char extra[SPEED_LINE_SIZE];
    snprintf(extra, sizeof extra, "%s", pointerNote ? pointerNote : "");
    const char *gap = extra[0] == 0 ? "" : extra[0] == '(' ? " " : ", ";  // "x = 1, y = 2 (y is a copy)"
    snprintf(text, sizeof text, "x = %d, y = %d%s%s", v[0], v[1], gap, extra);
    return text;
}

static const char *const varNames[] = { "x", "y" };

// Picks x or y to ask about, preferring one whose true value differs from the misconception's.
static int AskWhich(uint32_t *seed, const int *truth, const int *thought)
{
    int first = Rand(seed, 0, 1);
    return truth[first] != thought[first] || truth[1 - first] == thought[1 - first] ? first : 1 - first;
}

static void Pointers(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int v[2] = { Rand(seed, 1, 9), Rand(seed, 1, 9) }, thought[2] = { v[0], v[1] };
    int target = 0, thoughtTarget = 0;  // misconception: p = &y doesn't move p
    Emit(q, TwoVars(v, NULL), "int x = %d, y = %d;", v[0], v[1]);
    Emit(q, TwoVars(v, "p -> x"), "int *p = &x;");
    int ops = OperationCount(difficulty) + 1, moveAt = Rand(seed, 0, ops - 2);
    for (int op = 0; op < ops; op++) {
        int kind = op == moveAt ? 2 : Rand(seed, 0, 3), k = Rand(seed, 1, 9);
        switch (kind) {
        case 0: v[target] = k; thought[thoughtTarget] = k; Emit(q, TwoVars(v, Note("p -> %s", varNames[target])), "*p = %d;", k); break;
        case 1: v[target] += k; thought[thoughtTarget] += k; Emit(q, TwoVars(v, Note("p -> %s", varNames[target])), "*p += %d;", k); break;
        case 2: target = 1 - target; Emit(q, TwoVars(v, Note("p -> %s", varNames[target])), "p = &%s;", varNames[target]); break;
        default: {
            int other = 1 - target;  // copy through the pointer into the other variable
            v[other] = v[target] + k;
            thought[1 - thoughtTarget] = thought[thoughtTarget] + k;
            Emit(q, TwoVars(v, Note("p -> %s", varNames[target])), "%s = *p + %d;", varNames[other], k);
            break;
        }
        }
    }
    int ask = AskWhich(seed, v, thought);
    int decoys[] = { thought[ask], v[1 - ask], thought[1 - ask] };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void PointerToPointer(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int v[2] = { Rand(seed, 1, 9), Rand(seed, 1, 9) }, thought[2] = { v[0], v[1] };
    int target = 0, thoughtTarget = 0;  // misconception: *pp = &y only changes pp's copy, not p
    Emit(q, TwoVars(v, NULL), "int x = %d, y = %d;", v[0], v[1]);
    Emit(q, TwoVars(v, "p -> x"), "int *p = &x;");
    Emit(q, TwoVars(v, "pp -> p -> x"), "int **pp = &p;");
    int ops = OperationCount(difficulty) + 1, moveAt = Rand(seed, 0, ops - 2);
    for (int op = 0; op < ops; op++) {
        int kind = op == moveAt ? 2 : Rand(seed, 0, 3), k = Rand(seed, 1, 9);
        if (kind == 3 && v[target] > 20) kind = 1;
        switch (kind) {
        case 0: v[target] = k; thought[thoughtTarget] = k;
            Emit(q, TwoVars(v, Note("pp -> p -> %s", varNames[target])), "**pp = %d;", k); break;
        case 1: v[target] += k; thought[thoughtTarget] += k;
            Emit(q, TwoVars(v, Note("pp -> p -> %s", varNames[target])), "**pp += %d;", k); break;
        case 2: target = 1 - target;
            Emit(q, TwoVars(v, Note("pp -> p -> %s", varNames[target])), "*pp = &%s;", varNames[target]); break;
        default: v[target] *= 2; thought[thoughtTarget] *= 2;
            Emit(q, TwoVars(v, Note("pp -> p -> %s", varNames[target])), "*p *= 2;"); break;
        }
    }
    int ask = AskWhich(seed, v, thought);
    int decoys[] = { thought[ask], v[1 - ask], thought[1 - ask] };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void PassByPointer(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    enum { ADD_TEN, ADD_FIVE, DOUBLE_IT, SWAP, CALL_KINDS };
    static const char *definitions[CALL_KINDS] = {
        "void addTen(int *n) { *n += 10; }",
        "void addFive(int n) { n += 5; }",
        "void doubleIt(int *n) { *n *= 2; }",
        "void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }",
    };
    int ops = OperationCount(difficulty) + 1, calls[8], byValueAt = Rand(seed, 0, ops - 1);
    bool used[CALL_KINDS] = { false };
    for (int i = 0; i < ops; i++) {
        calls[i] = i == byValueAt ? ADD_FIVE : Rand(seed, 0, CALL_KINDS - 1);
        used[calls[i]] = true;
    }
    for (int kind = 0; kind < CALL_KINDS; kind++) if (used[kind]) Emit(q, NULL, "%s", definitions[kind]);
    Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;

    int v[2] = { Rand(seed, 1, 9), Rand(seed, 1, 9) }, thought[2] = { v[0], v[1] };  // misconception: addFive changes the caller
    Emit(q, TwoVars(v, NULL), "int x = %d, y = %d;", v[0], v[1]);
    for (int i = 0; i < ops; i++) {
        int who = Rand(seed, 0, 1);
        switch (calls[i]) {
        case ADD_TEN: v[who] += 10; thought[who] += 10; Emit(q, TwoVars(v, NULL), "addTen(&%s);", varNames[who]); break;
        case ADD_FIVE: thought[who] += 5;
            Emit(q, TwoVars(v, Note("(only a copy of %s changed)", varNames[who])), "addFive(%s);", varNames[who]); break;
        case DOUBLE_IT:
            if (v[who] > 40) who = 1 - who;
            v[who] *= 2; thought[who] *= 2;
            Emit(q, TwoVars(v, NULL), "doubleIt(&%s);", varNames[who]); break;
        default: {
            int t = v[0]; v[0] = v[1]; v[1] = t;
            t = thought[0]; thought[0] = thought[1]; thought[1] = t;
            Emit(q, TwoVars(v, NULL), "swap_ints(&x, &y);");
            break;
        }
        }
    }
    int ask = AskWhich(seed, v, thought);
    int decoys[] = { thought[ask], v[1 - ask], thought[1 - ask] };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void PointerArithmetic(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    enum { N = 5 };
    int a[N];
    for (int i = 0; i < N; i++) a[i] = Rand(seed, 1, 9);
    Emit(q, Note("a = %s", ArrayText(a, N)), "int a[] = { %d, %d, %d, %d, %d };", a[0], a[1], a[2], a[3], a[4]);
    int at = 0;  // the index p points at; kept to 0..N-2 so *(p + 1) stays inside
    Emit(q, Note("p -> a[0]"), "int *p = a;");
    int ops = OperationCount(difficulty);
    for (int op = 0; op < ops; op++) {
        int kind = Rand(seed, 0, 4), k = Rand(seed, 1, 9);
        if (kind == 0 && at + 1 > N - 2) kind = 2;
        if (kind == 1 && at + 2 > N - 2) kind = at > 0 ? 2 : 3;
        if (kind == 2 && at == 0) kind = 0;
        switch (kind) {
        case 0: at++; Emit(q, Note("p -> a[%d]", at), "p++;"); break;
        case 1: at += 2; Emit(q, Note("p -> a[%d]", at), "p += 2;"); break;
        case 2: at--; Emit(q, Note("p -> a[%d]", at), "p--;"); break;
        case 3: a[at] = k; Emit(q, Note("p -> a[%d], a = %s", at, ArrayText(a, N)), "*p = %d;", k); break;
        default: a[at + 1] += k; Emit(q, Note("p -> a[%d], a = %s", at, ArrayText(a, N)), "*(p + 1) += %d;", k); break;
        }
    }
    int x, decoys[3];
    switch (Rand(seed, 0, 2)) {
    case 0: x = a[at]; Emit(q, Note("x = a[%d] = %d", at, x), "int x = *p;");
        decoys[0] = a[at + 1]; decoys[1] = at > 0 ? a[at - 1] : a[0] + 1; decoys[2] = a[0]; break;
    case 1: x = a[at + 1]; Emit(q, Note("x = a[%d] = %d", at + 1, x), "int x = *(p + 1);");
        decoys[0] = a[at] + 1; decoys[1] = a[at]; decoys[2] = a[1]; break;
    default: x = a[at] + 1; Emit(q, Note("x = a[%d] + 1 = %d", at, x), "int x = *p + 1;");
        decoys[0] = a[at + 1]; decoys[1] = a[at]; decoys[2] = a[1] + 1; break;
    }
    Finish(q, seed, "x", x, decoys, 3);
}

static void References(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    q->cpp = true;
    // Decide the calls first so only the functions used are shown.
    enum { STEP_ADD, STEP_DOUBLE, STEP_COPY, STEP_ADD_Y, STEP_ADD_ONE, STEP_ADD_TWO, STEP_KINDS };
    int ops = OperationCount(difficulty) + 1, steps[8];
    bool usesAddOne = false, usesAddTwo = false;
    for (int i = 0; i < ops; i++) {
        steps[i] = Rand(seed, 0, STEP_KINDS - 1);
        usesAddOne |= steps[i] == STEP_ADD_ONE;
        usesAddTwo |= steps[i] == STEP_ADD_TWO;
    }
    if (usesAddOne) Emit(q, NULL, "void addOne(int &n) { n++; }");
    if (usesAddTwo) Emit(q, NULL, "void addTwo(int n) { n += 2; }");
    if (usesAddOne || usesAddTwo) Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;

    // v[0] is x (r is another name for it), v[1] is y once declared. The misconception
    // treats r as a separate copy of x, and thinks addTwo changes its argument.
    int v[2] = { Rand(seed, 1, 9), 0 }, thought[2] = { v[0], 0 }, thoughtR = v[0];
    bool hasY = false;
    Emit(q, Note("x = %d", v[0]), "int x = %d;", v[0]);
    Emit(q, Note("x = %d, r is another name for x", v[0]), "int &r = x;");
    for (int i = 0; i < ops; i++) {
        int k = Rand(seed, 1, 6), step = steps[i];
        if ((step == STEP_ADD_Y || step == STEP_ADD_TWO) && !hasY) step = STEP_COPY;
        if (step == STEP_COPY && hasY) step = STEP_ADD;
        if (step == STEP_DOUBLE && v[0] > 30) step = STEP_ADD;
        const char *note;
        switch (step) {
        case STEP_ADD: v[0] += k; thoughtR += k;
            note = hasY ? TwoVars(v, NULL) : Note("x = %d", v[0]);
            Emit(q, note, "r += %d;", k); break;
        case STEP_DOUBLE: v[0] *= 2; thoughtR *= 2;
            note = hasY ? TwoVars(v, NULL) : Note("x = %d", v[0]);
            Emit(q, note, "r *= 2;"); break;
        case STEP_COPY: hasY = true; v[1] = v[0]; thought[1] = thoughtR;
            Emit(q, TwoVars(v, Note("(y is a copy)")), "int y = r;"); break;
        case STEP_ADD_Y: v[1] += k; thought[1] += k; Emit(q, TwoVars(v, NULL), "y += %d;", k); break;
        case STEP_ADD_ONE: v[0]++; thought[0]++;
            note = hasY ? TwoVars(v, NULL) : Note("x = %d", v[0]);
            Emit(q, note, "addOne(x);"); break;
        default: thought[1] += 2;
            Emit(q, TwoVars(v, Note("(addTwo changed a copy)")), "addTwo(y);"); break;
        }
    }
    int ask = hasY ? AskWhich(seed, v, thought) : 0;
    int decoys[] = { thought[ask], ask == 0 ? thoughtR : v[0], hasY ? v[1 - ask] : v[0] + 1 };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void Variables(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconceptions: after y = x, y follows x around like a pointer; a double rounds when
    // it's stored in an int; letters count from 1 ('A' is 1).
    int v[2] = { Rand(seed, 1, 9), Rand(seed, 1, 9) }, thought[2] = { v[0], v[1] };
    bool thoughtLinked = false;
    Emit(q, TwoVars(v, NULL), "int x = %d, y = %d;", v[0], v[1]);
    int ops = OperationCount(difficulty) + 1, copyAt = Rand(seed, 0, ops - 2);
    for (int op = 0; op < ops; op++) {
        int kind = op == copyAt ? 0 : Rand(seed, 1, 4), k = Rand(seed, 1, 9);
        switch (kind) {
        case 0:
            v[1] = v[0]; thought[1] = thought[0]; thoughtLinked = true;
            Emit(q, TwoVars(v, "(y gets a copy of x)"), "y = x;");
            break;
        case 1: v[0] = k; thought[0] = k; Emit(q, TwoVars(v, NULL), "x = %d;", k); break;
        case 2: v[0] += v[1]; thought[0] += thought[1]; Emit(q, TwoVars(v, NULL), "x = x + y;"); break;
        case 3: {
            int whole = Rand(seed, 2, 9), tenths = Rand(seed, 5, 9);
            v[0] = whole; thought[0] = whole + 1;
            Emit(q, TwoVars(v, Note("(an int drops the .%d)", tenths)), "x = %d.%d;", whole, tenths);
            break;
        }
        default: {
            int letter = Rand(seed, 1, 6);
            v[0] = letter; thought[0] = letter + 1;
            Emit(q, TwoVars(v, Note("('%c' is 'A' + %d)", 'A' + letter, letter)), "x = '%c' - 'A';", 'A' + letter);
            break;
        }
        }
        if (thoughtLinked && kind != 0) thought[1] = thought[0];
    }
    int ask = AskWhich(seed, v, thought);
    int decoys[] = { thought[ask], v[1 - ask], thought[1 - ask] };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

// One "if" from the conditions questions, applied to s (x and y). flip reads the test the
// wrong way round, as the misconception model does once. Returns whether the test was true.
static bool ApplyCondition(int kind, int k, int k2, int *s, bool flip)
{
    bool test;
    switch (kind) {
    case 0: test = s[0] > s[1]; if (test != flip) s[0] -= s[1]; else s[1] -= s[0]; break;
    case 1: test = s[0] > k; s[0] = test != flip ? s[0] - k2 : s[0] + k2; break;
    case 2: test = s[0] % 2 == 0 && s[1] > k; if (test != flip) s[0]++; else s[1]++; break;
    default: test = s[0] < k || s[1] < k; if (test != flip) s[1] += k2; else s[0] -= 1; break;
    }
    return test;
}

static void Conditions(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int v[2] = { Rand(seed, 2, 12), Rand(seed, 2, 12) }, thought[2] = { v[0], v[1] };
    Emit(q, TwoVars(v, NULL), "int x = %d, y = %d;", v[0], v[1]);
    int ops = OperationCount(difficulty), flipAt = Rand(seed, 0, ops - 1);  // the misread test
    for (int op = 0; op < ops; op++) {
        int kind = Rand(seed, 0, 3), k = Rand(seed, 2, 9), k2 = Rand(seed, 1, k);
        bool test = ApplyCondition(kind, k, k2, v, false);
        ApplyCondition(kind, k, k2, thought, op == flipAt);
        const char *note = TwoVars(v, test ? "(the test was true)" : "(the test was false)");
        switch (kind) {
        case 0: Emit(q, note, "if (x > y) x -= y; else y -= x;"); break;
        case 1: Emit(q, note, "x = x > %d ? x - %d : x + %d;", k, k2, k2); break;
        case 2: Emit(q, note, "if (x %% 2 == 0 && y > %d) x++; else y++;", k); break;
        default: Emit(q, note, "if (x < %d || y < %d) y += %d; else x -= 1;", k, k, k2); break;
        }
    }
    int ask = AskWhich(seed, v, thought);
    int decoys[] = { thought[ask], v[1 - ask], thought[1 - ask] };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void Strings(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconception: strlen counts the '\0' and sizeof counts only the letters.
    static const char *words[] = { "cat", "code", "hello", "array", "pointer", "string", "fox", "loop" };
    const char *word = words[Rand(seed, 0, 7)];
    int length = (int)strlen(word), size = length + 1;
    char text[16];
    snprintf(text, sizeof text, "%s", word);
    Emit(q, Note("%d letters, then '\\0': %d chars", length, size), "char s[] = \"%s\";", word);
    int x, thought;
    if (Rand(seed, 0, 1)) {
        x = length; thought = length + 1;
        Emit(q, Note("x = %d (letters before the '\\0')", x), "int x = strlen(s);");
    } else {
        x = size; thought = length;
        Emit(q, Note("x = %d (the whole array, '\\0' too)", x), "int x = sizeof(s);");
    }
    int ops = OperationCount(difficulty) - 1;
    for (int op = 0; op < ops; op++) {
        int current = (int)strlen(text), kind = Rand(seed, 0, 2);
        if (kind == 0 && current <= 1) kind = 1;  // nothing left to cut
        switch (kind) {
        case 0: {  // cut the string short, then measure it again
            int at = Rand(seed, 1, current - 1);
            text[at] = '\0';
            Emit(q, Note("s now reads \"%s\"", text), "s[%d] = '\\0';", at);
            x += at; thought += at + 1;
            Emit(q, Note("x = %d", x), "x += strlen(s);");
            break;
        }
        case 1: {
            int at = Rand(seed, 0, current - 1), letter = text[at] - 'a';
            x += letter; thought += letter;
            Emit(q, Note("x = %d ('%c' is 'a' + %d)", x, text[at], letter), "x += s[%d] - 'a';", at);
            break;
        }
        default:
            x += size; thought += (int)strlen(text);
            Emit(q, Note("x = %d (sizeof never changes)", x), "x += sizeof(s);");
            break;
        }
    }
    int decoys[] = { thought, x + 1, x - 1 };
    Finish(q, seed, "x", x, decoys, 3);
}

static void Functions(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconception: calling addOne(x) without storing the result changes x.
    Emit(q, NULL, "int square(int n) { return n * n; }");
    Emit(q, NULL, "int addOne(int n) { n++; return n; }");
    Emit(q, NULL, "int twice(int n) { return n + n; }");
    Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;
    int v[2] = { Rand(seed, 1, 6), 0 }, thought[2] = { v[0], 0 };
    bool hasY = false;
    Emit(q, Note("x = %d", v[0]), "int x = %d;", v[0]);
    int ops = OperationCount(difficulty) + 1, ignoredAt = Rand(seed, 0, ops - 1);
    for (int op = 0; op < ops; op++) {
        int kind = op == ignoredAt ? 0 : Rand(seed, 1, 3);
        if (kind == 1 && (v[0] > 12 || v[0] < -12 || thought[0] > 12 || thought[0] < -12)) kind = 2;
        if (kind == 3 && hasY && v[0] > 50) kind = 2;
        switch (kind) {
        case 0: thought[0]++;
            Emit(q, hasY ? TwoVars(v, "(the result was thrown away)") : Note("x = %d (the result was thrown away)", v[0]),
                 "addOne(x);");
            break;
        case 1: v[0] *= v[0]; thought[0] *= thought[0];
            Emit(q, hasY ? TwoVars(v, NULL) : Note("x = %d", v[0]), "x = square(x);"); break;
        case 2: v[0]++; thought[0]++;
            Emit(q, hasY ? TwoVars(v, NULL) : Note("x = %d", v[0]), "x = addOne(x);"); break;
        default:
            if (!hasY) {
                hasY = true; v[1] = v[0] * 2; thought[1] = thought[0] * 2;
                Emit(q, TwoVars(v, NULL), "int y = twice(x);");
            } else {
                v[0] *= 2; thought[0] *= 2;
                Emit(q, TwoVars(v, NULL), "x = twice(x);");
            }
            break;
        }
    }
    int ask = hasY ? AskWhich(seed, v, thought) : 0;
    int decoys[] = { thought[ask], hasY ? v[1 - ask] : v[0] + 2, v[ask] * 2 };
    Finish(q, seed, varNames[ask], v[ask], decoys, 3);
}

static void FunctionPointers(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconception: op keeps calling the function it started with.
    static const char *names[] = { "Add", "Subtract", "Multiply" };
    Emit(q, NULL, "int Add(int num1, int num2) { return num1 + num2; }");
    Emit(q, NULL, "int Subtract(int num1, int num2) { return num1 - num2; }");
    Emit(q, NULL, "int Multiply(int num1, int num2) { return num1 * num2; }");
    Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;
    int a = Rand(seed, 2, 9), b = Rand(seed, 1, 5);
    int target = 0, x = a + b, thought = x;
    Emit(q, Note("op -> Add"), "int (*op)(int, int) = Add;");
    Emit(q, Note("x = %d", x), "int x = op(%d, %d);", a, b);
    int calls = 2 + difficulty / 4, moveAt = Rand(seed, 0, calls - 1);
    for (int call = 0; call < calls; call++) {
        if (call == moveAt || Rand(seed, 0, 2) == 0) {
            int next = (target + Rand(seed, 1, 2)) % 3;
            if (next == 2 && (x > 30 || x < -30)) next = 1;  // keep the numbers small
            if (next != target) {
                target = next;
                Emit(q, Note("op -> %s", names[target]), "op = %s;", names[target]);
            }
        }
        bool big = x > 30 || x < -30;  // multiplying stays gentle once x is large
        int k = target == 2 ? (big ? 1 : Rand(seed, 2, 3)) : Rand(seed, 1, 9);
        x = target == 0 ? x + k : target == 1 ? x - k : x * k;
        thought += k;
        Emit(q, Note("x = %d (%s)", x, names[target]), "x = op(x, %d);", k);
    }
    int decoys[] = { thought, x + 1, -x };
    Finish(q, seed, "x", x, decoys, 3);
}

// One write through a second pointer q that points at index at of a heap block a (the
// misconception, that q points at a copy, simply doesn't see it).
static void ThroughSecondPointer(SpeedQuestion *q, uint32_t *seed, int *a, int at, const char *blockName)
{
    int k = Rand(seed, 1, 5);
    switch (Rand(seed, 0, 2)) {
    case 0: a[at] *= 2; Emit(q, Note("%s[%d] = %d", blockName, at, a[at]), "*q *= 2;"); break;
    case 1: a[at] += k; Emit(q, Note("%s[%d] = %d", blockName, at, a[at]), "*q += %d;", k); break;
    default: a[at + 1] = k; Emit(q, Note("%s[%d] = %d", blockName, at + 1, a[at + 1]), "q[1] = %d;", k); break;
    }
}

static void Malloc(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    int a[3], thought[3];
    Emit(q, Note("p -> a new block of 3 ints"), "int *p = (int*)malloc(3 * sizeof(int));");
    a[0] = Rand(seed, 1, 9);
    Emit(q, Note("p[0] = %d", a[0]), "p[0] = %d;", a[0]);
    int k = Rand(seed, 1, 5);
    a[1] = a[0] + k;
    Emit(q, Note("p[1] = %d", a[1]), "p[1] = p[0] + %d;", k);
    a[2] = Rand(seed, 1, 9);
    Emit(q, Note("p[2] = %d", a[2]), "p[2] = %d;", a[2]);
    for (int i = 0; i < 3; i++) thought[i] = a[i];
    Emit(q, Note("q -> p[1]"), "int *q = p + 1;");
    int ops = 1 + difficulty / 4;
    for (int op = 0; op < ops; op++) ThroughSecondPointer(q, seed, a, 1, "p");
    int x = a[1] + a[2];
    Emit(q, Note("x = %d + %d = %d", a[1], a[2], x), "int x = p[1] + p[2];");
    Emit(q, Note("the block goes back; x keeps %d", x), "free(p);");
    int decoys[] = { thought[1] + thought[2], a[0] + a[1], x + 1 };
    Finish(q, seed, "x", x, decoys, 3);
}

static void NewDelete(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    q->cpp = true;
    if (Rand(seed, 0, 1)) {  // a single int: changes through a second pointer
        int value = Rand(seed, 1, 9), thought;
        Emit(q, Note("*p = %d (on the heap)", value), "int *p = new int(%d);", value);
        int k = Rand(seed, 1, 5);
        value += k;
        Emit(q, Note("*p = %d", value), "*p += %d;", k);
        thought = value;  // misconception: q points at its own copy
        Emit(q, Note("q -> the same int"), "int *q = p;");
        int ops = 1 + difficulty / 4;
        for (int op = 0; op < ops; op++) {
            k = Rand(seed, 1, 5);
            if (Rand(seed, 0, 1)) { value *= 2; Emit(q, Note("*p = %d", value), "*q *= 2;"); }
            else { value += k; Emit(q, Note("*p = %d", value), "*q += %d;", k); }
        }
        Emit(q, Note("x = %d", value), "int x = *p;");
        Emit(q, Note("the int is freed; x keeps %d", value), "delete p;");
        int decoys[] = { thought, value + 1, value - 1 };
        Finish(q, seed, "x", value, decoys, 3);
        return;
    }
    // An array: indexes count from 0 (the misconception counts from 1).
    enum { N = 3 };
    int a[N], thought[N];
    for (int i = 0; i < N; i++) a[i] = thought[i] = Rand(seed, 1, 9);
    Emit(q, Note("a = %s", ArrayText(a, N)), "int *a = new int[3]{ %d, %d, %d };", a[0], a[1], a[2]);
    int ops = OperationCount(difficulty) - 1;
    for (int op = 0; op < ops; op++) {
        int i = Rand(seed, 0, N - 1), j = Rand(seed, 0, N - 1);
        int ti = (i + N - 1) % N, tj = (j + N - 1) % N;
        a[i] += a[j]; thought[ti] += thought[tj];
        Emit(q, Note("a = %s", ArrayText(a, N)), "a[%d] += a[%d];", i, j);
    }
    int i = Rand(seed, 0, N - 1);
    int x = a[i];
    Emit(q, Note("x = %d", x), "int x = a[%d];", i);
    Emit(q, Note("the array is freed; x keeps %d", x), "delete[] a;");
    int decoys[] = { thought[(i + N - 1) % N], a[(i + 1) % N], x + 1 };
    Finish(q, seed, "x", x, decoys, 3);
}

static void Objects(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconception: Counter b = a; makes b another name for a, not a copy.
    q->cpp = true;
    Emit(q, NULL, "class Counter {");
    Emit(q, NULL, "public:");
    Emit(q, NULL, "    int n;");
    Emit(q, NULL, "    Counter(int start) { n = start; }");
    Emit(q, NULL, "    void add(int k) { n += k; }");
    Emit(q, NULL, "};");
    Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;

    int a = Rand(seed, 1, 9), b = 0, thoughtA = a, thoughtB = 0;
    bool hasB = false, hasP = false;
    Emit(q, Note("a.n = %d", a), "Counter a(%d);", a);
    int ops = OperationCount(difficulty), copyAt = Rand(seed, 0, ops - 2);
    for (int op = 0; op < ops; op++) {
        int kind = op == copyAt && !hasB ? 0 : Rand(seed, 1, 4), k = Rand(seed, 1, 6);
        if (kind == 0 && hasB) kind = 1;
        if (kind == 2 && !hasB) kind = 1;
        if (kind == 4 && a > 30) kind = 1;
        switch (kind) {
        case 0: hasB = true; b = a; thoughtB = thoughtA; Emit(q, Note("b.n = %d (b is a copy)", b), "Counter b = a;"); break;
        case 1: a += k; thoughtA += k; if (hasB) thoughtB = thoughtA;
            Emit(q, Note("a.n = %d", a), "a.add(%d);", k); break;
        case 2: b += k; thoughtB += k; thoughtA = thoughtB;
            Emit(q, Note("b.n = %d, a.n still %d", b, a), "b.add(%d);", k); break;
        case 3:
            if (!hasP) {
                hasP = true;
                Emit(q, Note("p -> a"), "Counter *p = &a;");
            }
            a += k; thoughtA += k; if (hasB) thoughtB = thoughtA;
            Emit(q, Note("a.n = %d", a), "p->add(%d);", k);
            break;
        default: a *= 2; thoughtA *= 2; if (hasB) thoughtB = thoughtA;
            Emit(q, Note("a.n = %d", a), "a.n *= 2;"); break;
        }
    }
    bool askB = hasB && Rand(seed, 0, 1);
    int x = askB ? b : a;
    Emit(q, Note("x = %d", x), askB ? "int x = b.n;" : "int x = a.n;");
    int decoys[] = { askB ? thoughtB : thoughtA, askB ? a : (hasB ? b : x + 2), x + 1 };
    Finish(q, seed, "x", x, decoys, 3);
}

static void LinkedList(SpeedQuestion *q, int difficulty, uint32_t *seed)
{
    // Misconception: p = p->next doesn't move p (it changes the node instead).
    q->cpp = true;
    Emit(q, NULL, "struct Node { int data; Node *next; };");
    Emit(q, NULL, "%s", "");
    q->firstBodyLine = q->lineCount;
    static const char *nodeNames[] = { "a", "b", "c" };
    int data[3] = { Rand(seed, 1, 9), Rand(seed, 1, 9), Rand(seed, 1, 9) }, thought[3];
    Emit(q, Note("a -> b -> c"), "Node c = { %d, nullptr };", data[2]);
    Emit(q, NULL, "Node b = { %d, &c };", data[1]);
    Emit(q, NULL, "Node a = { %d, &b };", data[0]);
    for (int i = 0; i < 3; i++) thought[i] = data[i];
    int at = 0, thoughtAt = 0;
    Emit(q, Note("p -> a"), "Node *p = &a;");
    int ops = OperationCount(difficulty) + 1, moveAt = Rand(seed, 0, ops - 2);
    for (int op = 0; op < ops; op++) {
        int kind = op == moveAt ? 0 : Rand(seed, 0, 2), k = Rand(seed, 1, 9);
        if (kind == 0 && at == 2) kind = 1;
        switch (kind) {
        case 0: at++; Emit(q, Note("p -> %s", nodeNames[at]), "p = p->next;"); break;
        case 1: data[at] += k; thought[thoughtAt] += k;
            Emit(q, Note("%s.data = %d", nodeNames[at], data[at]), "p->data += %d;", k); break;
        default: data[at] = k; thought[thoughtAt] = k;
            Emit(q, Note("%s.data = %d", nodeNames[at], data[at]), "p->data = %d;", k); break;
        }
    }
    int x, thoughtX;
    switch (Rand(seed, 0, 2)) {
    case 0: x = data[at]; thoughtX = thought[thoughtAt];
        Emit(q, Note("x = %s.data = %d", nodeNames[at], x), "int x = p->data;"); break;
    case 1: x = data[1]; thoughtX = thought[1];
        Emit(q, Note("x = b.data = %d", x), "int x = a.next->data;"); break;
    default: x = data[0] + data[at]; thoughtX = thought[0] + thought[thoughtAt];
        Emit(q, Note("x = %d + %d = %d", data[0], data[at], x), "int x = a.data + p->data;"); break;
    }
    int decoys[] = { thoughtX, data[(at + 1) % 3], x + 1 };
    Finish(q, seed, "x", x, decoys, 3);
}

void SpeedGenerate(SpeedSkill skill, int difficulty, uint32_t *seed, SpeedQuestion *out)
{
    memset(out, 0, sizeof *out);
    out->skill = skill;
    switch (skill) {
    case SPEED_VARIABLES:          Variables(out, difficulty, seed); break;
    case SPEED_ARITHMETIC:         Arithmetic(out, difficulty, seed); break;
    case SPEED_CONDITIONS:         Conditions(out, difficulty, seed); break;
    case SPEED_STRINGS:            Strings(out, difficulty, seed); break;
    case SPEED_FUNCTIONS:          Functions(out, difficulty, seed); break;
    case SPEED_MALLOC:             Malloc(out, difficulty, seed); break;
    case SPEED_FUNCTION_POINTERS:  FunctionPointers(out, difficulty, seed); break;
    case SPEED_NEW_DELETE:         NewDelete(out, difficulty, seed); break;
    case SPEED_OBJECTS:            Objects(out, difficulty, seed); break;
    case SPEED_LINKED_LIST:        LinkedList(out, difficulty, seed); break;
    case SPEED_LOOPS:              Loops(out, difficulty, seed); break;
    case SPEED_ARRAYS:             Arrays(out, difficulty, seed); break;
    case SPEED_POINTERS:           Pointers(out, difficulty, seed); break;
    case SPEED_POINTER_TO_POINTER: PointerToPointer(out, difficulty, seed); break;
    case SPEED_PASS_BY_POINTER:    PassByPointer(out, difficulty, seed); break;
    case SPEED_POINTER_ARITHMETIC: PointerArithmetic(out, difficulty, seed); break;
    case SPEED_REFERENCES:         References(out, difficulty, seed); break;
    default:                       Arithmetic(out, difficulty, seed); break;
    }
}

char *SpeedQuestionCode(const SpeedQuestion *q, int from, char *out, int size)
{
    int used = 0;
    out[0] = '\0';
    for (int i = from < 0 ? 0 : from; i < q->lineCount && used < size; i++) {
        used += snprintf(out + used, size - used, "%s%s", i > from ? "\n" : "", q->lines[i]);
    }
    return out;
}

// ---------------------------------------------------------------------------
// The round

bool SpeedRoundIsBank(const SpeedRound *round, int index)
{
    return index >= 0 && index < round->count && round->plan[index].bank != NULL;
}

int SpeedRoundChoiceCount(const SpeedRound *round, int index)
{
    if (index < 0 || index >= round->count) return 0;
    return SpeedRoundIsBank(round, index) ? round->plan[index].bank->choiceCount : SPEED_CHOICES;
}

int SpeedRoundCorrect(const SpeedRound *round, int index)
{
    if (index < 0 || index >= round->count) return -1;
    return SpeedRoundIsBank(round, index) ? round->plan[index].bank->correct : round->generated[index].correct;
}

// Characters of reading in a lesson question: its prompt, code and choices.
static int ReadingLength(const Question *q)
{
    int length = (int)strlen(q->prompt) + (q->code ? (int)strlen(q->code) : 0);
    for (int i = 0; i < q->choiceCount; i++) length += (int)strlen(q->choices[i]);
    return length;
}

double SpeedSecondsFor(const Question *bank, const SpeedQuestion *generated, SpeedFamiliarity familiarity)
{
    double seconds = SPEED_MIN_SECONDS;
    if (bank) {
        int length = ReadingLength(bank);
        if (length > 120) seconds += (length - 120) / 50.0;  // about a second per extra line of reading
        if (bank->code) {
            int lines = 1;
            for (const char *c = bank->code; *c; c++) lines += *c == '\n';
            seconds += lines * 0.5;  // code takes tracing, not just reading
        }
    } else if (generated) {
        int lines = generated->lineCount - generated->firstBodyLine;
        if (lines > 4) seconds += (lines - 4) * 0.8;
    }
    if (familiarity == SPEED_NEW) seconds += 4;
    if (familiarity == SPEED_STRUGGLING) seconds += 6;
    return seconds < SPEED_MIN_SECONDS ? SPEED_MIN_SECONDS : seconds > SPEED_MAX_SECONDS ? SPEED_MAX_SECONDS : seconds;
}

void SpeedRoundStart(SpeedRound *round, const SpeedPlanItem *plan, int count, bool endOnMiss,
                     SpeedFamiliarityLookup familiarity, uint32_t seed, double now)
{
    memset(round, 0, sizeof *round);
    if (count > SPEED_QUESTIONS) count = SPEED_QUESTIONS;
    if (count < 0) count = 0;
    round->count = count;
    round->endOnMiss = endOnMiss;
    if (seed == 0) seed = 1;
    for (int i = 0; i < count; i++) {
        round->plan[i] = plan[i];
        round->picked[i] = -1;
        // Generated questions grow with the run, so its end is the hardest part.
        if (!plan[i].bank) SpeedGenerate(plan[i].skill, i, &seed, &round->generated[i]);
        SpeedFamiliarity known = familiarity ? familiarity(plan[i].reviewId) : SPEED_KNOWN;
        round->seconds[i] = SpeedSecondsFor(plan[i].bank, plan[i].bank ? NULL : &round->generated[i], known);
    }
    round->phase = count > 0 ? SPEED_COUNTING_IN : SPEED_WON;
    round->phaseStart = now;
    round->lastAnswered = -1;
}

// After question number has been answered (or timed out): the next one, or the end.
static void MoveOn(SpeedRound *round, double now)
{
    if (round->number + 1 >= round->count) {
        round->phase = SPEED_WON;
        round->endedAt = now;
        return;
    }
    round->number++;
    round->phaseStart = now;  // no breather: the next question starts now
}

SpeedOutcome SpeedRoundUpdate(SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN && now - round->phaseStart >= SPEED_COUNT_IN) {
        round->phase = SPEED_PLAYING;
        round->phaseStart += SPEED_COUNT_IN;
    }
    if (round->phase != SPEED_PLAYING) return SPEED_IGNORED;
    double limit = round->seconds[round->number];
    if (now - round->phaseStart < limit) return SPEED_IGNORED;
    double timedOutAt = round->phaseStart + limit;
    round->lastAnswered = round->number;
    if (round->endOnMiss) {
        round->phase = SPEED_FAILED;
        round->endedAt = timedOutAt;
    } else {
        MoveOn(round, timedOutAt);
    }
    return SPEED_TIMED_OUT;
}

SpeedOutcome SpeedRoundAnswer(SpeedRound *round, int choice, double now)
{
    if (SpeedRoundUpdate(round, now) == SPEED_TIMED_OUT) return SPEED_IGNORED;  // too late: the timeout counts
    if (round->phase != SPEED_PLAYING || choice < 0 || choice >= SpeedRoundChoiceCount(round, round->number))
        return SPEED_IGNORED;
    round->picked[round->number] = choice;
    round->lastAnswered = round->number;
    if (choice != SpeedRoundCorrect(round, round->number)) {
        if (round->endOnMiss) {
            round->phase = SPEED_FAILED;
            round->endedAt = now;
        } else {
            MoveOn(round, now);
        }
        return SPEED_WRONG;
    }
    MoveOn(round, now);
    return SPEED_RIGHT;
}

int SpeedRoundRight(const SpeedRound *round)
{
    int right = 0;
    for (int i = 0; i < round->count; i++) right += round->picked[i] >= 0 && round->picked[i] == SpeedRoundCorrect(round, i);
    return right;
}

double SpeedRoundTimeLeft(const SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN) return round->count ? round->seconds[0] : 0;
    if (round->phase != SPEED_PLAYING) return 0;
    double left = round->seconds[round->number] - (now - round->phaseStart);
    return left < 0 ? 0 : left;
}

float SpeedRoundTension(const SpeedRound *round, double now)
{
    if (round->phase != SPEED_PLAYING) return 0;
    float t = (float)((now - round->phaseStart) / round->seconds[round->number]);
    return t < 0 ? 0 : t > 1 ? 1 : t;
}
