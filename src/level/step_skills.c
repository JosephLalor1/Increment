// The skills a passed coding step practised. See step_skills.h.
#include "level/step_skills.h"
#include "code/code_skills.h"
#include "level/line_diff.h"

#include <stdlib.h>
#include <string.h>

// The lines of code that aren't in the starter, joined with newlines, and how many there are.
// Returns a malloc'd string ("" when nothing was written).
static char *WrittenLines(const char *starter, const char *code, int *lineCount)
{
    static DiffOp ops[LINE_DIFF_MAX_LINES * 2];
    int count = LineDiff(starter, code, ops, LINE_DIFF_MAX_LINES * 2);
    size_t size = 1;
    for (int i = 0; i < count; i++) size += ops[i].kind == DIFF_ADDED ? 1024 : 0;
    char *written = malloc(size);
    if (!written) return NULL;
    written[0] = '\0';
    *lineCount = 0;
    char line[1024];
    for (int i = 0; i < count; i++) {
        if (ops[i].kind != DIFF_ADDED) continue;
        LineDiffGetLine(code, ops[i].newLine, line, sizeof line);
        strcat(written, line);
        strcat(written, "\n");
        (*lineCount)++;
    }
    return written;
}

// How many lines of `after` aren't in `before` (re-spaced lines are the same line).
static int LinesAdded(const char *before, const char *after)
{
    static DiffOp ops[LINE_DIFF_MAX_LINES * 2];
    int count = LineDiff(before, after, ops, LINE_DIFF_MAX_LINES * 2), added = 0;
    for (int i = 0; i < count; i++) added += ops[i].kind == DIFF_ADDED;
    return added;
}

bool StepReplayEarns(const Step *step, const char *before, const char *code)
{
    bool fromStarter = LinesAdded(step->starter, before) == 0 && LinesAdded(before, step->starter) == 0;
    if (fromStarter) return true;
    int asked = LinesAdded(step->starter, step->solution);
    return LinesAdded(before, code) >= (asked > 1 ? asked : 1);
}

int StepSkills(const Step *step, const char *code, bool cpp, SkillWeight *out)
{
    float weight[SKILL_COUNT] = { 0 };
    for (int i = 0; i < step->skillCount; i++)
        if (step->skills[i].skill < SKILL_COUNT) weight[step->skills[i].skill] = step->skills[i].weight;

    int writtenCount = 0;
    char *written = WrittenLines(step->starter, code, &writtenCount);
    if (written) {
        CodeSkills found = CodeSkillsDetect(written, cpp);
        for (int s = 0; s < SKILL_COUNT; s++)
            if (found.present[s] && weight[s] < STEP_WRITTEN_WEIGHT) weight[s] = STEP_WRITTEN_WEIGHT;
        free(written);
    }
    if (writtenCount >= STEP_SOLVING_LINES && weight[SKILL_PROBLEM_SOLVING] < STEP_SOLVING_WEIGHT)
        weight[SKILL_PROBLEM_SOLVING] = STEP_SOLVING_WEIGHT;

    // The strongest few. On equal weights the skill later in the catalogue wins: within each
    // group the catalogue goes from basic to advanced, so a loop that prints its total counts
    // for loops before output.
    int count = 0;
    while (count < MAX_QUESTION_SKILLS) {
        int best = -1;
        for (int s = 0; s < SKILL_COUNT; s++)
            if (weight[s] > 0 && (best < 0 || weight[s] >= weight[best])) best = s;
        if (best < 0) break;
        out[count++] = (SkillWeight){ (Skill)best, weight[best] };
        weight[best] = 0;
    }
    return count;
}
