// Reads current saved sources through the sandbox library; no separate skill database.
#include "rankings/ranking_skills.h"
#include "sandbox/code_library.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void RankingReadSkills(RankingEntry *entry, Track track, const char *codeFolder)
{
    memset(entry->skills, 0, sizeof entry->skills);
    entry->uniqueSkills = entry->skillPrograms = entry->rank = 0;
    entry->warning = false;
    if (track < 0 || track > TRACK_COUNT) {
        entry->warning = true;
        return;
    }
    SavedProgram *programs = NULL;
    int count = 0;
    entry->warning = !CodeLibraryList(codeFolder, &programs, &count);
    for (int i = 0; i < count; i++) {
        const SavedProgram *program = &programs[i];
        Track language = program->cpp ? TRACK_CPP : TRACK_C;
        if (track != TRACK_COUNT && track != language) continue;
        if (!program->skills.complete) {
            entry->warning = true;
            continue;
        }
        bool contributes = false;
        for (int s = 0; s < SKILL_COUNT; s++)
            if (program->skills.present[s]) {
                contributes = true;
                if (!entry->skills[s]) {
                    entry->skills[s] = true;
                    entry->uniqueSkills++;
                }
            }
        if (contributes) entry->skillPrograms++;
    }
    free(programs);
}

static int CompareSkills(const void *left, const void *right)
{
    const RankingEntry *a = left, *b = right;
    if (a->uniqueSkills != b->uniqueSkills) return a->uniqueSkills > b->uniqueSkills ? -1 : 1;
    if (a->skillPrograms != b->skillPrograms) return a->skillPrograms > b->skillPrograms ? -1 : 1;
    const unsigned char *an = (const unsigned char *)a->name, *bn = (const unsigned char *)b->name;
    while (*an && *bn && tolower(*an) == tolower(*bn)) {
        an++;
        bn++;
    }
    return tolower(*an) - tolower(*bn);
}

void RankingSortMode(RankingEntry *entries, int count, RankingMode mode)
{
    if (mode != RANKING_SKILLS) {
        RankingSort(entries, count);
        return;
    }
    if (!entries || count <= 0) return;
    qsort(entries, count, sizeof *entries, CompareSkills);
    for (int i = 0; i < count; i++)
        entries[i].rank = i && entries[i].uniqueSkills == entries[i - 1].uniqueSkills &&
                entries[i].skillPrograms == entries[i - 1].skillPrograms
            ? entries[i - 1].rank
            : i + 1;
}
