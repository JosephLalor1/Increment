// Derives local leaderboard results without loading another account into global state.
#include "ranking.h"
#include "progress.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void RankingRead(RankingEntry *entry, const LessonSet *lessons, Track track, const char *path)
{
    entry->completed = entry->quizPoints = entry->rank = 0;
    entry->warning = false;
    if (!lessons || !path || track < 0 || track > TRACK_COUNT) { entry->warning = true; return; }
    int *best = calloc(lessons->count ? lessons->count : 1, sizeof *best);
    if (!best) { entry->warning = true; return; }
    for (int i = 0; i < lessons->count; i++) best[i] = -1;
    FILE *file = fopen(path, "r");
    if (!file) entry->warning = errno != ENOENT;
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
            if (errno || *end || end == value || score < 0 || score > 100) { entry->warning = true; continue; }
            for (int i = 0; i < lessons->count; i++) {
                if (!strcmp(id, lessons->items[i].id) && score > best[i]) best[i] = score;
            }
        }
        if (ferror(file)) entry->warning = true;
        fclose(file);
    }
    for (int i = 0; i < lessons->count; i++) {
        const Lesson *lesson = &lessons->items[i];
        if (track != TRACK_COUNT && lesson->track != track) continue;
        if (best[i] >= PASS_PERCENT) entry->completed++;
        if (lesson->questionCount && best[i] >= 0) entry->quizPoints += best[i];
    }
    free(best);
}

static int Compare(const void *left, const void *right)
{
    const RankingEntry *a = left, *b = right;
    if (a->completed != b->completed) return a->completed > b->completed ? -1 : 1;
    if (a->quizPoints != b->quizPoints) return a->quizPoints > b->quizPoints ? -1 : 1;
    const unsigned char *an = (const unsigned char *)a->name, *bn = (const unsigned char *)b->name;
    while (*an && *bn && tolower(*an) == tolower(*bn)) { an++; bn++; }
    return tolower(*an) - tolower(*bn);
}

void RankingSort(RankingEntry *entries, int count)
{
    if (!entries || count <= 0) return;
    qsort(entries, count, sizeof *entries, Compare);
    for (int i = 0; i < count; i++)
        entries[i].rank = i && entries[i].completed == entries[i-1].completed && entries[i].quizPoints == entries[i-1].quizPoints
            ? entries[i-1].rank : i + 1;
}
