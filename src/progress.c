#include "progress.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define MAX_ENTRIES 512  // lessons plus exercises; far more than the course has

// One stored score: a lesson id or an exercise id (see progress.h).
typedef struct {
    char id[32];
    int best;
} Entry;

static Entry entries[MAX_ENTRIES];
static int entryCount;
static char savePath[1024];  // where ProgressSave writes; "" when signed out

static Entry *Find(const char *id)
{
    for (int i = 0; i < entryCount; i++) {
        if (strcmp(entries[i].id, id) == 0) return &entries[i];
    }
    return NULL;
}

static Entry *FindOrAdd(const char *id)
{
    Entry *e = Find(id);
    if (e || entryCount >= MAX_ENTRIES) return e;
    e = &entries[entryCount++];
    strncpy(e->id, id, sizeof e->id - 1);
    e->id[sizeof e->id - 1] = '\0';
    e->best = -1;
    return e;
}

void ProgressLoad(const char *path)
{
    strncpy(savePath, path, sizeof savePath - 1);
    savePath[sizeof savePath - 1] = '\0';
    entryCount = 0;

    FILE *f = fopen(path, "r");
    if (!f) return;
    char id[32];
    int best;
    while (fscanf(f, "%31s %d", id, &best) == 2) {
        Entry *e = FindOrAdd(id);
        if (e && best > e->best) e->best = best;
    }
    fclose(f);
}

bool ProgressSave(void)
{
    FILE *f = fopen(savePath, "w");
    if (!f) {
        TraceLog(LOG_WARNING, "PROGRESS: could not write %s", savePath);
        return false;
    }
    for (int i = 0; i < entryCount; i++) fprintf(f, "%s %d\n", entries[i].id, entries[i].best);
    fclose(f);
    return true;
}

int ProgressBest(const char *id)
{
    Entry *e = Find(id);
    return e ? e->best : -1;
}

const char *ProgressScoreNote(const char *lessonId)
{
    int best = ProgressBest(lessonId);
    return best < 0 ? "New" : TextFormat("Best: %d%%", best);
}

void ProgressRecord(const char *id, int percent)
{
    Entry *e = FindOrAdd(id);
    if (!e) return;
    if (percent > e->best) e->best = percent;
    ProgressSave();
}

bool ProgressLessonPassed(const char *lessonId)
{
    return ProgressBest(lessonId) >= PASS_PERCENT;
}

// "<lesson id>#<n>" with n 1-based, the id an exercise's pass is stored under.
static void ExerciseId(const char *lessonId, int exerciseIndex, char *out, size_t size)
{
    snprintf(out, size, "%s#%d", lessonId, exerciseIndex + 1);
}

bool ProgressExerciseDone(const char *lessonId, int exerciseIndex)
{
    char id[64];
    ExerciseId(lessonId, exerciseIndex, id, sizeof id);
    return ProgressBest(id) >= 100;
}

void ProgressRecordExercise(const char *lessonId, int exerciseIndex)
{
    char id[64];
    ExerciseId(lessonId, exerciseIndex, id, sizeof id);
    ProgressRecord(id, 100);
}
