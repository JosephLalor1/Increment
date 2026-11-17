// Used by tools/check_lessons.py: builds a step's second run (src/step_check.h) with the
// app's own code, so the checker tests exactly what learners will meet.
//
// usage: step_variant <c|cpp> <starter file> <solution file> <out prefix> <case count>
//        (<input file or -> <expected file>) per test case, then [<skill:weight> ...]
//
// Writes <out prefix>.code (the solution with its given values changed) and, if the main
// (first) test case has input, <out prefix>.in. Prints one line: the number of changes, then
// the changed values. Also checks the solution against its own step: prints "missing ..."
// or "cheat ..." lines if it is refused.
#include "level/step_cheats.h"
#include "level/step_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ReadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    text[fread(text, 1, (size_t)size, f)] = '\0';
    fclose(f);
    return text;
}

static void WriteFile(const char *prefix, const char *extension, const char *text)
{
    char path[1024];
    snprintf(path, sizeof path, "%s%s", prefix, extension);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 6) return 2;
    bool cpp = strcmp(argv[1], "cpp") == 0;
    const char *prefix = argv[4];
    Step step = { 0 };
    step.starter = ReadFile(argv[2]);
    step.solution = ReadFile(argv[3]);
    step.caseCount = atoi(argv[5]);
    if (!step.starter || !step.solution || step.caseCount < 1 || step.caseCount > MAX_STEP_CASES
        || argc < 6 + 2 * step.caseCount)
        return 2;
    for (int i = 0; i < step.caseCount; i++) {
        const char *input = argv[6 + 2 * i], *expected = argv[7 + 2 * i];
        step.cases[i].input = strcmp(input, "-") == 0 ? NULL : ReadFile(input);
        step.cases[i].expected = ReadFile(expected);
        if (!step.cases[i].expected) return 2;
    }

    StepVariation variation;
    if (StepVariationMake(&step, &variation)) {
        WriteFile(prefix, ".code", variation.solution);
        if (variation.input) WriteFile(prefix, ".in", variation.input);
    }
    printf("%d %s\n", variation.changes, variation.summary);

    // The solution must pass its own checks.
    int missing;
    char summary[STEP_CHECK_SUMMARY];
    free(StepVariationApply(&step, step.solution, &missing, summary));
    if (missing) printf("missing givens: %s\n", summary);
    Skill construct;
    if (StepMissingConstructs(&step, step.solution, cpp, &construct, 1)) printf("missing construct\n");

    // Nor may it look like a cheat (step_cheats.h).
    for (int i = 6 + 2 * step.caseCount; i < argc && step.skillCount < MAX_QUESTION_SKILLS; i++) {
        char name[64];
        float weight;
        Skill skill;
        if (sscanf(argv[i], "%63[^:]:%f", name, &weight) == 2 && SkillFromName(name, -1, &skill))
            step.skills[step.skillCount++] = (SkillWeight){ skill, weight };
    }
    StepCheat cheat = StepCheatFind(&step, step.solution);
    if (cheat.kind != CHEAT_NONE) printf("cheat in solution: %s\n", cheat.note);
    return 0;
}
