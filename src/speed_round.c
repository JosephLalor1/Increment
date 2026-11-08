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
    "arithmetic", "loops", "arrays", "pointers", "pointer_to_pointer",
    "pass_by_pointer", "pointer_arithmetic", "references",
};
static const char *skillTitles[SPEED_SKILL_COUNT] = {
    "Arithmetic", "Loops", "Arrays", "Pointers", "Pointers to pointers",
    "Pass by pointer", "Pointer arithmetic", "References",
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
    snprintf(text, sizeof text, "x = %d, y = %d%s%s", v[0], v[1], extra[0] ? ", " : "", extra);
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

void SpeedGenerate(SpeedSkill skill, int difficulty, uint32_t *seed, SpeedQuestion *out)
{
    memset(out, 0, sizeof *out);
    out->skill = skill;
    switch (skill) {
    case SPEED_ARITHMETIC:         Arithmetic(out, difficulty, seed); break;
    case SPEED_LOOPS:              Loops(out, difficulty, seed); break;
    case SPEED_ARRAYS:             Arrays(out, difficulty, seed); break;
    case SPEED_POINTERS:           Pointers(out, difficulty, seed); break;
    case SPEED_POINTER_TO_POINTER: PointerToPointer(out, difficulty, seed); break;
    case SPEED_PASS_BY_POINTER:    PassByPointer(out, difficulty, seed); break;
    case SPEED_POINTER_ARITHMETIC: PointerArithmetic(out, difficulty, seed); break;
    default:                       References(out, difficulty, seed); break;
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

static void NextQuestion(SpeedRound *round, double now)
{
    SpeedSkill skill = round->skills[Rand(&round->seed, 0, round->skillCount - 1)];
    SpeedGenerate(skill, round->number, &round->seed, &round->question);
    round->phaseStart = now;
}

void SpeedRoundStart(SpeedRound *round, const SpeedSkill *skills, int skillCount, uint32_t seed, double now)
{
    memset(round, 0, sizeof *round);
    if (skillCount > SPEED_SKILL_COUNT) skillCount = SPEED_SKILL_COUNT;
    for (int i = 0; i < skillCount; i++) round->skills[i] = skills[i];
    round->skillCount = skillCount > 0 ? skillCount : 1;  // no skills given: arithmetic (skills[0] is 0)
    round->seed = seed ? seed : 1;
    round->phase = SPEED_COUNTING_IN;
    round->picked = -1;
    NextQuestion(round, now);  // ready behind the count-in
}

void SpeedRoundUpdate(SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN && now - round->phaseStart >= SPEED_COUNT_IN) {
        round->phase = SPEED_PLAYING;
        round->phaseStart += SPEED_COUNT_IN;
    }
    if (round->phase == SPEED_PLAYING && now - round->phaseStart >= SPEED_SECONDS) {
        round->phase = SPEED_FAILED;
        round->picked = -1;
        round->endedAt = round->phaseStart + SPEED_SECONDS;
    }
}

SpeedOutcome SpeedRoundAnswer(SpeedRound *round, int choice, double now)
{
    SpeedRoundUpdate(round, now);  // an answer after the time limit is too late
    if (round->phase != SPEED_PLAYING || choice < 0 || choice >= SPEED_CHOICES) return SPEED_IGNORED;
    if (choice != round->question.correct) {
        round->phase = SPEED_FAILED;
        round->picked = choice;
        round->endedAt = now;
        return SPEED_WRONG;
    }
    round->number++;
    if (round->number >= SPEED_QUESTIONS) {
        round->number = SPEED_QUESTIONS - 1;  // stays a valid question number
        round->phase = SPEED_WON;
        round->endedAt = now;
    } else {
        NextQuestion(round, now);  // no breather: the next question starts now
    }
    return SPEED_RIGHT;
}

double SpeedRoundTimeLeft(const SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN) return SPEED_SECONDS;
    if (round->phase != SPEED_PLAYING) return 0;
    double left = SPEED_SECONDS - (now - round->phaseStart);
    return left < 0 ? 0 : left;
}

float SpeedRoundTension(const SpeedRound *round, double now)
{
    if (round->phase != SPEED_PLAYING) return 0;
    float t = (float)((now - round->phaseStart) / SPEED_SECONDS);
    return t < 0 ? 0 : t > 1 ? 1 : t;
}
