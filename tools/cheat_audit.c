// Audit: for every step of every level, builds the plainest cheat (the starter, with main
// reduced to its declarations and a printf of each target line) and says which check, if
// any, refuses it. Static checks only: a step with a second run is counted as caught by it.
#include "../src/lesson.h"
#include "../src/step_cheats.h"
#include "../src/step_check.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool StartsWithType(const char *line)
{
    static const char *const types[] = { "int", "char", "float", "double", "long", "bool", "unsigned", NULL };
    while (*line == ' ') line++;
    for (int i = 0; types[i]; i++) {
        size_t n = strlen(types[i]);
        if (strncmp(line, types[i], n) == 0 && !isalnum((unsigned char)line[n])) return true;
    }
    return false;
}

static char *Cheat(const Step *step)
{
    const char *mainAt = strstr(step->starter, "main(");
    if (!mainAt) return NULL;
    const char *open = strchr(mainAt, '{');
    if (!open) return NULL;
    char *out = malloc(strlen(step->starter) + strlen(step->expected) * 2 + 4096);
    size_t used = (size_t)(open + 1 - step->starter);
    memcpy(out, step->starter, used);
    out[used++] = '\n';
    // Keep main's top-level declarations that set a value (the givens stay in place).
    int depth = 1;
    for (const char *line = open + 1; *line && depth > 0;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        int before = depth;
        for (const char *p = line; p < end; p++) depth += (*p == '{') - (*p == '}');
        if (before == 1 && depth == 1 && StartsWithType(line) && memchr(line, '=', (size_t)(end - line))
            && !memchr(line, '(', (size_t)(end - line))) {
            memcpy(out + used, line, (size_t)(end - line));
            used += (size_t)(end - line);
            out[used++] = '\n';
        }
        line = *end ? end + 1 : end;
    }
    for (const char *line = step->expected; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        used += (size_t)sprintf(out + used, "    printf(\"");
        for (const char *p = line; p < end; p++) {
            if (*p == '"' || *p == '\\') out[used++] = '\\';
            if (*p == '%') out[used++] = '%';
            out[used++] = *p;
        }
        used += (size_t)sprintf(out + used, "\\n\");\n");
        line = *end ? end + 1 : end;
    }
    sprintf(out + used, "    return 0;\n}\n");
    return out;
}

int main(int argc, char **argv)
{
    LessonSet set = { 0 };
    LessonsLoad(&set, argc > 1 ? argv[1] : "assets/lessons");
    int steps = 0, open = 0, by[5] = { 0 };
    for (int l = 0; l < set.count; l++) {
        const Lesson *lesson = &set.items[l];
        bool cpp = lesson->track == TRACK_CPP;
        for (int s = 0; s < lesson->stepCount; s++) {
            const Step *step = &lesson->steps[s];
            char *cheat = Cheat(step);
            if (!cheat) continue;
            steps++;
            Skill construct;
            int missing = 0;
            char note[STEP_CHECK_SUMMARY];
            StepVariation variation;
            const char *caught = NULL;
            if (StepMissingConstructs(step, cheat, cpp, &construct, 1)) { caught = "construct"; by[0]++; }
            else if (StepCheatFind(step, cheat).kind != CHEAT_NONE) { caught = "cheat"; by[1]++; }
            else {
                free(StepVariationApply(step, cheat, &missing, note));
                if (missing) { caught = "given"; by[2]++; }
                else if (StepVariationMake(step, &variation)) { caught = "second run"; by[3]++; StepVariationFree(&variation); }
            }
            if (!caught) {
                open++;
                printf("OPEN %s step %d\n", lesson->id, s + 1);
            }
            free(cheat);
        }
    }
    printf("%d steps: %d by construct, %d by code checks, %d by givens, %d left to the second run, %d open\n", steps, by[0],
           by[1], by[2], by[3], open);
    return 0;
}
