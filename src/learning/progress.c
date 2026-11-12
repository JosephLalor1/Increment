// The signed-in learner's passed steps and gold mastery. See progress.h.
#include "learning/progress.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define MAX_ENTRIES 1024  // every step of every level, plus gold; far more than the course has

// One stored mark: a step or a gold id (see progress.h), and 100 once done.
typedef struct {
    char id[96];
    int best;
} Entry;

static Entry entries[MAX_ENTRIES];
static int entryCount;
static char savePath[1024];  // where Save writes; "" when signed out

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
    char id[96];
    int best;
    while (fscanf(f, "%95s %d", id, &best) == 2) {
        Entry *e = FindOrAdd(id);
        if (e && best > e->best) e->best = best;
    }
    fclose(f);
}

// Writes every entry to the signed-in learner's file.
static bool Save(void)
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

static int Best(const char *id)
{
    Entry *e = Find(id);
    return e ? e->best : -1;
}

// Marks an id done (100) and saves. Returns false if it couldn't be stored or saved.
static bool RecordDone(const char *id)
{
    Entry *e = FindOrAdd(id);
    if (!e) return false;
    e->best = 100;
    return Save();
}

// "<lesson id>/<n>" with n 1-based, the id a passed step is stored under.
static void StepId(const char *lessonId, int stepIndex, char *out, size_t size)
{
    snprintf(out, size, "%s/%d", lessonId, stepIndex + 1);
}

bool ProgressStepDone(const char *lessonId, int stepIndex)
{
    char id[64];
    StepId(lessonId, stepIndex, id, sizeof id);
    return Best(id) >= 100;
}

bool ProgressRecordStep(const char *lessonId, int stepIndex)
{
    char id[64];
    StepId(lessonId, stepIndex, id, sizeof id);
    return RecordDone(id);
}

bool ProgressParseStepId(const char *id, char *lessonId, size_t size, int *stepIndex)
{
    const char *slash = strrchr(id, '/');
    if (!slash || slash == id || !slash[1]) return false;
    int number = 0;
    for (const char *c = slash + 1; *c; c++) {
        if (*c < '0' || *c > '9' || number > 1000) return false;
        number = number * 10 + (*c - '0');
    }
    size_t length = (size_t)(slash - id);
    if (number < 1 || length >= size) return false;
    memcpy(lessonId, id, length);
    lessonId[length] = '\0';
    *stepIndex = number - 1;
    return true;
}

// "<lesson id>!gold", the id a won speed round is stored under.
static void GoldId(const char *lessonId, char *out, size_t size)
{
    snprintf(out, size, "%s!gold", lessonId);
}

bool ProgressGold(const char *lessonId)
{
    char id[64];
    GoldId(lessonId, id, sizeof id);
    return Best(id) >= 100;
}

void ProgressRecordGold(const char *lessonId)
{
    char id[64];
    GoldId(lessonId, id, sizeof id);
    RecordDone(id);
}
