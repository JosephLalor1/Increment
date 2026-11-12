// Checks that a passing program really solves its step. See step_check.h.
#include "step_check.h"
#include "code_skills.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOTS 256   // literal values looked at in one program
#define MAX_GIVENS 32   // values changed for one step
#define NAME_SIZE 48
#define MAX_STRING_SLOT 40  // a string value this long or longer (quotes included) isn't varied

// A place in a program where a variable is set to a literal value: "b = 3" in
// "int a = 10, b = 3;", or one element of "int n[3] = {1, 2, 3};".
typedef struct {
    int start, length;     // the literal in the text
    char name[NAME_SIZE];  // the variable being set
    int ordinal;           // which value of that assignment (elements of an initialiser list)
    bool used;             // already matched to a given
} Slot;

static bool WordChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

// The length of the literal at p if it's one whose value can be changed: a decimal whole
// number or fraction (with an optional f, u or l suffix), or a one-letter character literal.
// 0 for anything else (hex, exponents, escapes).
static int LiteralLength(const char *p)
{
    if (p[0] == '\'') return isalpha((unsigned char)p[1]) && p[2] == '\'' ? 3 : 0;
    if (!isdigit((unsigned char)p[0])) return 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) return 0;
    int n = 0;
    while (isdigit((unsigned char)p[n])) n++;
    if (p[n] == '.' && isdigit((unsigned char)p[n + 1])) {
        n++;
        while (isdigit((unsigned char)p[n])) n++;
    }
    if (p[n] && strchr("fFuUlL", p[n])) n++;
    return WordChar(p[n]) || p[n] == '.' ? 0 : n;
}

static const char *SkipSpace(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

// Finds every slot in code. A literal is a slot when it's the whole value being assigned:
// straight after "=", or an element of an initialiser list, and followed by "," ";" or "}".
// Literals inside calls, conditions, array sizes and expressions are left alone.
static int FindSlots(const char *code, Slot *slots)
{
    int count = 0;
    char lastName[NAME_SIZE] = "", owner[NAME_SIZE] = "";
    int parens = 0, brackets = 0, listDepth = 0, ordinal = 0;
    bool assigning = false;  // between an "=" and the end of its value
    char before = ';';       // the last significant character before this one
    bool lineStart = true;

    for (const char *p = code ? code : ""; *p;) {
        char c = *p;
        if (c == '\n') { lineStart = true; p++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { p++; continue; }
        if (lineStart && c == '#') {  // preprocessor lines aren't code to vary
            while (*p && *p != '\n') p++;
            continue;
        }
        lineStart = false;
        if (c == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (c == '/' && p[1] == '*') {
            const char *end = strstr(p + 2, "*/");
            p = end ? end + 2 : p + strlen(p);
            continue;
        }
        if (c == '"') {
            const char *start = p++;
            bool letters = false;
            while (*p && *p != '"' && *p != '\n') {
                letters |= isalpha((unsigned char)*p) != 0 && (p == start + 1 || p[-1] != '\\');
                p += *p == '\\' && p[1] ? 2 : 1;
            }
            if (*p == '"') p++;
            // A string is a slot when it's the whole value a variable is set to
            // (char name[20] = "Alice";), has letters to change and is short enough to patch.
            // Strings anywhere else (printf formats above all) are left alone.
            int length = (int)(p - start);
            char after = *SkipSpace(p);
            bool whole = after == ';' || (after == ',' && parens == 0);
            if (assigning && before == '=' && whole && letters && parens == 0 && brackets == 0 && listDepth == 0
                && owner[0] && length < MAX_STRING_SLOT && count < MAX_SLOTS) {
                Slot *slot = &slots[count++];
                slot->start = (int)(start - code);
                slot->length = length;
                snprintf(slot->name, sizeof slot->name, "%s", owner);
                slot->ordinal = 0;
                slot->used = false;
            }
            before = '"';
            continue;
        }

        int literal = LiteralLength(p);
        bool valueStart = before == '=' || (listDepth > 0 && (before == ',' || before == '{'));
        if (literal > 0) {
            char after = *SkipSpace(p + literal);
            bool whole = after == ';' || (after == ',' && (listDepth > 0 || parens == 0)) || (after == '}' && listDepth > 0);
            if (assigning && valueStart && whole && parens == 0 && brackets == 0 && owner[0] && count < MAX_SLOTS) {
                Slot *slot = &slots[count++];
                slot->start = (int)(p - code);
                slot->length = literal;
                snprintf(slot->name, sizeof slot->name, "%s", owner);
                slot->ordinal = ordinal;
                slot->used = false;
            }
            if (assigning) ordinal++;
            p += literal;
            before = '0';
            continue;
        }
        if (c == '\'') {  // any other character literal
            p++;
            while (*p && *p != '\'' && *p != '\n') p += *p == '\\' && p[1] ? 2 : 1;
            if (*p == '\'') p++;
            before = '0';
            continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            const char *start = p;
            while (WordChar(*p)) p++;
            if (brackets == 0 && !assigning) snprintf(lastName, sizeof lastName, "%.*s", (int)(p - start), start);
            before = 'a';
            continue;
        }
        if (isdigit((unsigned char)c)) {  // a number that can't be varied: step over all of it
            while (WordChar(*p) || *p == '.') p++;
            before = '0';
            continue;
        }

        if (c == '(') parens++;
        else if (c == ')') parens -= parens > 0;
        else if (c == '[') brackets++;
        else if (c == ']') brackets -= brackets > 0;
        else if (c == '=' && p[1] != '=' && !strchr("=!<>", before) && parens == 0) {
            assigning = true;  // "=" and compound assignments such as "+="
            snprintf(owner, sizeof owner, "%s", lastName);
            ordinal = 0;
            listDepth = 0;
        } else if (c == '{' && assigning) listDepth++;
        else if (c == '}' && listDepth > 0) listDepth--;
        else if (c == ';' || c == '{' || c == '}' || (c == ',' && listDepth == 0 && parens == 0)) {
            assigning = false;  // the statement, block or declarator ends
            owner[0] = '\0';
        }
        if (c == '=' && p[1] == '=') p++;  // "==" is one token
        before = c;
        p++;
    }
    return count;
}

static bool SlotText(const char *code, const Slot *slot, const char *text, int length)
{
    return slot->length == length && strncmp(code + slot->start, text, (size_t)length) == 0;
}

// The first unused slot in code that sets the same variable to the same value as wanted (a
// slot of wantedCode), or NULL.
static Slot *MatchSlot(const char *code, Slot *slots, int count, const char *wantedCode, const Slot *wanted)
{
    for (int i = 0; i < count; i++) {
        Slot *slot = &slots[i];
        if (!slot->used && slot->ordinal == wanted->ordinal && strcmp(slot->name, wanted->name) == 0
            && SlotText(code, slot, wantedCode + wanted->start, wanted->length)) return slot;
    }
    return NULL;
}

// The step's givens: the solution's slots that its starter has too. Returns their indices
// in solutionSlots, in order.
static int FindGivens(const Step *step, Slot *solutionSlots, int solutionCount, int *givens)
{
    static Slot starterSlots[MAX_SLOTS];
    int starterCount = FindSlots(step->starter, starterSlots), count = 0;
    for (int i = 0; i < solutionCount && count < MAX_GIVENS; i++) {
        Slot *match = MatchSlot(step->starter, starterSlots, starterCount, step->solution, &solutionSlots[i]);
        if (!match) continue;
        match->used = true;
        givens[count++] = i;
    }
    return count;
}

// A letter moved delta places along the alphabet, wrapping round, in its own case.
static char ShiftLetter(char letter, int delta)
{
    char base = isupper((unsigned char)letter) ? 'A' : 'a';
    return (char)(base + (letter - base + delta) % 26);
}

// Writes the literal `text` changed by delta (1 or more) into out: a number grows by delta
// (its fraction and suffix kept), a letter moves delta places along the alphabet, and so
// does every letter of a string (its escapes and any % conversions stay as they are, and
// so does its length, so it still fits wherever it is kept).
static void ChangeLiteral(const char *text, int length, int delta, char *out, int size)
{
    if (text[0] == '"') {
        int n = 0;
        for (int i = 0; i < length && n < size - 1; i++) {
            bool kept = i > 0 && (text[i - 1] == '\\' || text[i - 1] == '%');
            out[n++] = isalpha((unsigned char)text[i]) && !kept ? ShiftLetter(text[i], delta) : text[i];
        }
        out[n] = '\0';
        return;
    }
    if (text[0] == '\'') {
        snprintf(out, (size_t)size, "'%c'", ShiftLetter(text[1], delta));
        return;
    }
    int digits = 0;
    while (isdigit((unsigned char)text[digits])) digits++;
    long long whole = strtoll(text, NULL, 10) + delta;
    snprintf(out, (size_t)size, "%lld%.*s", whole, length - digits, text + digits);
}

// Each given gets its own change, so two givens with the same value come apart.
static int DeltaFor(int index) { return index % 9 + 1; }

// Appends "name = value" to a summary of size STEP_CHECK_SUMMARY, comma-separated.
static void AddToSummary(char *summary, const char *name, const char *value)
{
    size_t used = strlen(summary);
    snprintf(summary + used, STEP_CHECK_SUMMARY - used, "%s%s = %s", used ? ", " : "", name, value);
}

// One replacement of a stretch of text.
typedef struct {
    int start, length;
    char text[40];
} Patch;

static int ByStart(const void *a, const void *b) { return ((const Patch *)a)->start - ((const Patch *)b)->start; }

// A copy of code with the patches applied (malloc'd).
static char *ApplyPatches(const char *code, Patch *patches, int count)
{
    qsort(patches, (size_t)count, sizeof *patches, ByStart);
    char *out = malloc(strlen(code) + (size_t)count * sizeof patches[0].text + 1);
    if (!out) return NULL;
    size_t used = 0;
    int at = 0;
    for (int i = 0; i < count; i++) {
        memcpy(out + used, code + at, (size_t)(patches[i].start - at));
        used += (size_t)(patches[i].start - at);
        memcpy(out + used, patches[i].text, strlen(patches[i].text));
        used += strlen(patches[i].text);
        at = patches[i].start + patches[i].length;
    }
    strcpy(out + used, code + at);
    return out;
}

// The length of the word at p if it is letters only, up to a space or the end, and short
// enough to patch; 0 otherwise.
static int WordLength(const char *p)
{
    int n = 0;
    while (isalpha((unsigned char)p[n])) n++;
    return n > 0 && n < MAX_STRING_SLOT && (p[n] == '\0' || isspace((unsigned char)p[n])) ? n : 0;
}

// The step's input with every number in it changed, and every word of letters moved along
// the alphabet (malloc'd); *changes counts them.
static char *VaryInput(const char *input, int *changes)
{
    static Patch patches[MAX_GIVENS];
    int count = 0;
    for (const char *p = input; *p && count < MAX_GIVENS;) {
        bool afterSpace = p == input || isspace((unsigned char)p[-1]);
        bool tokenStart = afterSpace || p[-1] == '-';
        int length = tokenStart && p[0] != '\'' ? LiteralLength(p) : 0;
        int word = afterSpace ? WordLength(p) : 0;
        if (length > 0 && (p[length] == '\0' || isspace((unsigned char)p[length]))) {
            patches[count].start = (int)(p - input);
            patches[count].length = length;
            ChangeLiteral(p, length, DeltaFor(count), patches[count].text, sizeof patches[count].text);
            count++;
            p += length;
        } else if (word > 0) {
            patches[count].start = (int)(p - input);
            patches[count].length = word;
            for (int i = 0; i < word; i++) patches[count].text[i] = ShiftLetter(p[i], DeltaFor(count));
            patches[count].text[word] = '\0';
            count++;
            p += word;
        } else {
            p++;
        }
    }
    *changes = count;
    return ApplyPatches(input, patches, count);
}

bool StepVariationMake(const Step *step, StepVariation *out)
{
    memset(out, 0, sizeof *out);
    if (step->fixedValues) return false;
    static Slot slots[MAX_SLOTS];
    static Patch patches[MAX_GIVENS];
    int givens[MAX_GIVENS];
    int slotCount = FindSlots(step->solution, slots);
    int givenCount = FindGivens(step, slots, slotCount, givens);
    for (int g = 0; g < givenCount; g++) {
        const Slot *slot = &slots[givens[g]];
        patches[g].start = slot->start;
        patches[g].length = slot->length;
        ChangeLiteral(step->solution + slot->start, slot->length, DeltaFor(g), patches[g].text, sizeof patches[g].text);
        AddToSummary(out->summary, slot->name, patches[g].text);
    }
    int inputChanges = 0;
    if (step->input) out->input = VaryInput(step->input, &inputChanges);
    out->changes = givenCount + inputChanges;
    if (out->changes == 0) {
        StepVariationFree(out);
        return false;
    }
    out->solution = ApplyPatches(step->solution, patches, givenCount);
    return out->solution != NULL;
}

void StepVariationFree(StepVariation *variation)
{
    free(variation->solution);
    free(variation->input);
    memset(variation, 0, sizeof *variation);
}

char *StepVariationApply(const Step *step, const char *code, int *missing, char *missingSummary)
{
    static Slot solutionSlots[MAX_SLOTS], codeSlots[MAX_SLOTS];
    static Patch patches[MAX_GIVENS];
    int givens[MAX_GIVENS];
    int solutionCount = FindSlots(step->solution, solutionSlots);
    int givenCount = FindGivens(step, solutionSlots, solutionCount, givens);
    int codeCount = FindSlots(code, codeSlots), patchCount = 0;
    *missing = 0;
    missingSummary[0] = '\0';
    for (int g = 0; g < givenCount; g++) {
        const Slot *given = &solutionSlots[givens[g]];
        Slot *match = MatchSlot(code, codeSlots, codeCount, step->solution, given);
        if (!match) {  // the learner's code doesn't set this given to its value any more
            char value[40];
            snprintf(value, sizeof value, "%.*s", given->length, step->solution + given->start);
            AddToSummary(missingSummary, given->name, value);
            (*missing)++;
            continue;
        }
        match->used = true;
        patches[patchCount].start = match->start;
        patches[patchCount].length = match->length;
        // The same change as in the solution: by the given's place, not the patch's.
        ChangeLiteral(code + match->start, match->length, DeltaFor(g), patches[patchCount].text,
                      sizeof patches[patchCount].text);
        patchCount++;
    }
    return ApplyPatches(code, patches, patchCount);
}

int StepMissingConstructs(const Step *step, const char *code, bool cpp, Skill *out, int max)
{
    // The skills that are a construct in the code (a loop is there or it isn't), unlike
    // output, types or operators, which nearly every line has.
    static const Skill constructs[] = {
        SKILL_INPUT, SKILL_CONDITIONS, SKILL_LOOPS, SKILL_ARRAYS, SKILL_FUNCTIONS, SKILL_POINTERS,
        SKILL_DYNAMIC_MEMORY, SKILL_FUNCTION_POINTERS, SKILL_REFERENCES, SKILL_CLASSES,
    };
    CodeSkills needed = CodeSkillsDetect(step->solution, cpp), has = CodeSkillsDetect(code, cpp);
    int count = 0;
    for (size_t i = 0; i < sizeof constructs / sizeof constructs[0] && count < max; i++)
        if (needed.present[constructs[i]] && !has.present[constructs[i]]) out[count++] = constructs[i];
    return count;
}
