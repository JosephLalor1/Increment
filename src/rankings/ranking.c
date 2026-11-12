// Derives local leaderboard results without loading another account into global state.
#include "rankings/ranking.h"
#include "learning/progress.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Marks the step a stored progress line stands for as passed, if it's a step of one of the
// loaded levels and its value says done (100).
static void MarkStep(const LessonSet *lessons, bool *passed, const char *id, long value)
{
    char lessonId[96];
    int step;
    if (value < 100 || !ProgressParseStepId(id, lessonId, sizeof lessonId, &step)) return;
    for (int i = 0; i < lessons->count; i++) {
        const Lesson *lesson = &lessons->items[i];
        if (!strcmp(lessonId, lesson->id) && step < lesson->stepCount) passed[i * MAX_STEPS + step] = true;
    }
}

void RankingRead(RankingEntry *entry, const LessonSet *lessons, Track track, const char *path)
{
    entry->completed = entry->stepsPassed = entry->rank = 0;
    entry->warning = false;
    if (!lessons || !path || track < 0 || track > TRACK_COUNT) {
        entry->warning = true;
        return;
    }
    bool *passed = calloc((size_t)(lessons->count ? lessons->count : 1) * MAX_STEPS, sizeof *passed);
    if (!passed) {
        entry->warning = true;
        return;
    }
    FILE *file = fopen(path, "r");
    if (!file)
        entry->warning = errno != ENOENT;
    else {
        char line[256], id[96], value[96], extra;
        while (fgets(line, sizeof line, file)) {
            if (!strchr(line, '\n') && !feof(file)) {
                int c;
                while ((c = fgetc(file)) != '\n' && c != EOF) {}
                entry->warning = true;
                continue;
            }
            const char *p = line;
            while (isspace((unsigned char)*p)) p++;
            if (!*p) continue;
            if (sscanf(line, "%95s %95s %c", id, value, &extra) != 2) {
                entry->warning = true;
                continue;
            }
            errno = 0;
            char *end;
            long score = strtol(value, &end, 10);
            if (errno || *end || end == value || score < 0 || score > 100) {
                entry->warning = true;
                continue;
            }
            MarkStep(lessons, passed, id, score);
        }
        if (ferror(file)) entry->warning = true;
        fclose(file);
    }
    for (int i = 0; i < lessons->count; i++) {
        const Lesson *lesson = &lessons->items[i];
        if (track != TRACK_COUNT && lesson->track != track) continue;
        int done = 0;
        for (int s = 0; s < lesson->stepCount; s++) done += passed[i * MAX_STEPS + s];
        entry->stepsPassed += done;
        if (lesson->stepCount > 0 && done == lesson->stepCount) entry->completed++;
    }
    free(passed);
}

static int Compare(const void *left, const void *right)
{
    const RankingEntry *a = left, *b = right;
    if (a->completed != b->completed) return a->completed > b->completed ? -1 : 1;
    if (a->stepsPassed != b->stepsPassed) return a->stepsPassed > b->stepsPassed ? -1 : 1;
    const unsigned char *an = (const unsigned char *)a->name, *bn = (const unsigned char *)b->name;
    while (*an && *bn && tolower(*an) == tolower(*bn)) {
        an++;
        bn++;
    }
    return tolower(*an) - tolower(*bn);
}

void RankingSort(RankingEntry *entries, int count)
{
    if (!entries || count <= 0) return;
    qsort(entries, count, sizeof *entries, Compare);
    for (int i = 0; i < count; i++)
        entries[i].rank = i && entries[i].completed == entries[i - 1].completed &&
                entries[i].stepsPassed == entries[i - 1].stepsPassed
            ? entries[i - 1].rank
            : i + 1;
}
