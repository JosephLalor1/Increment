// Parses lesson files (assets/lessons/*.txt) into Lesson structs: a header, then the
// level's coding steps. The format is described in CLAUDE.md.
#include "learning/lesson.h"
#include "core/text_copy.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void SbAppend(StrBuf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        while (cap < b->len + n + 1) cap *= 2;
        b->data = realloc(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

// Returns the buffer contents as a new heap string and empties the buffer.
static char *SbTake(StrBuf *b)
{
    char *s = malloc(b->len + 1);
    memcpy(s, b->data ? b->data : "", b->len);
    s[b->len] = '\0';
    b->len = 0;
    return s;
}

static bool StartsWith(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void CopyField(char *dst, size_t size, const char *src)
{
    while (*src == ' ') src++;
    size_t n = strlen(src);
    if (n > size - 1) n = size - 1;  // truncate over-long fields
    memcpy(dst, src, n);
    dst[n] = '\0';
}

typedef struct {
    Lesson *lesson;
    StrBuf code;
    bool inCode;
    bool ignoringStep;   // past MAX_STEPS: the rest of the file's steps are skipped
    char **fenceTarget;  // which field of the current step the next code block fills
    const char *file;
    int lineNo;
} Parser;

static Step *CurrentStep(Parser *ps)
{
    Lesson *L = ps->lesson;
    return L->stepCount > 0 && !ps->ignoringStep ? &L->steps[L->stepCount - 1] : NULL;
}

static void FlushCode(Parser *ps)
{
    // Drop the trailing newline of the last code line.
    if (ps->code.len && ps->code.data[ps->code.len - 1] == '\n') ps->code.data[--ps->code.len] = '\0';
    char *code = SbTake(&ps->code);
    Step *step = CurrentStep(ps);
    if (!step) {
        if (!ps->ignoringStep) TraceLog(LOG_WARNING, "LESSON: %s:%d: code outside a step ignored", ps->file, ps->lineNo);
        free(code);
        return;
    }
    char **target = ps->fenceTarget ? ps->fenceTarget : &step->starter;  // an unlabelled block is the starter
    free(*target);
    *target = code;
    ps->fenceTarget = NULL;
}

// A step's "~" line: its difficulty (1 to 3), then up to MAX_QUESTION_SKILLS skills, each
// name:weight with a weight from 0 to 1.
static void ParseSkillLine(Parser *ps, Step *step, const char *text)
{
    char *end;
    long difficulty = strtol(text, &end, 10);
    if (end == text || difficulty < 1 || difficulty > SKILL_MAX_DIFFICULTY) {
        TraceLog(LOG_WARNING, "LESSON: %s:%d: a skills line starts with a difficulty from 1 to 3", ps->file, ps->lineNo);
        return;
    }
    step->difficulty = (int)difficulty;
    step->skillCount = 0;
    for (const char *p = end; *p;) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char *colon = strchr(p, ':');
        size_t tokenLength = strcspn(p, " \t");
        Skill skill;
        float weight = 0;
        if (!colon || (size_t)(colon - p) > tokenLength || !SkillFromName(p, (int)(colon - p), &skill)
            || (weight = strtof(colon + 1, NULL)) <= 0 || weight > 1) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: bad skill \"%.*s\"", ps->file, ps->lineNo, (int)tokenLength, p);
        } else if (step->skillCount < MAX_QUESTION_SKILLS) {
            step->skills[step->skillCount++] = (SkillWeight){ skill, weight };
        }
        p += tokenLength;
    }
}

// options is what follows "@step": "broken" (the starter deliberately doesn't compile, which
// only the checker cares about) and/or "fixed".
static void StartStep(Parser *ps, const char *options)
{
    Lesson *L = ps->lesson;
    ps->fenceTarget = NULL;
    if (L->stepCount >= MAX_STEPS) {
        if (!ps->ignoringStep) TraceLog(LOG_WARNING, "LESSON: %s:%d: too many steps, ignoring the rest", ps->file, ps->lineNo);
        ps->ignoringStep = true;
        return;
    }
    Step *step = &L->steps[L->stepCount++];
    step->difficulty = 1;  // until its "~" line says otherwise
    step->fixedValues = strstr(options, "fixed") != NULL;
}

static void ParseLine(Parser *ps, const char *line)
{
    Lesson *L = ps->lesson;

    if (ps->inCode) {
        if (StartsWith(line, "```")) {
            FlushCode(ps);
            ps->inCode = false;
            return;
        }
        for (const char *c = line; *c; c++) {
            if (*c == '\t') SbAppend(&ps->code, "    ", 4);
            else SbAppend(&ps->code, c, 1);
        }
        SbAppend(&ps->code, "\n", 1);
        return;
    }

    if (StartsWith(line, "```")) {
        ps->inCode = true;
        ps->code.len = 0;
        return;
    }
    if (line[0] == '\0') return;
    if (StartsWith(line, "@title ")) { CopyField(L->title, sizeof L->title, line + 7); return; }
    if (StartsWith(line, "@id "))    { CopyField(L->id, sizeof L->id, line + 4); return; }
    if (StartsWith(line, "@track ")) {
        const char *t = line + 7;
        while (*t == ' ') t++;
        L->track = (strcmp(t, "C++") == 0 || strcmp(t, "cpp") == 0) ? TRACK_CPP : TRACK_C;
        return;
    }
    if (StartsWith(line, "@speed ")) { CopyField(L->speedSkills, sizeof L->speedSkills, line + 7); return; }
    if (strcmp(line, "@step") == 0 || StartsWith(line, "@step ")) { StartStep(ps, line + 5); return; }

    Step *step = CurrentStep(ps);
    if (ps->ignoringStep) return;
    if (step && StartsWith(line, "~ "))       { ParseSkillLine(ps, step, line + 2); return; }
    if (step && StartsWith(line, "@input"))    { ps->fenceTarget = &step->input; return; }
    if (step && StartsWith(line, "@expect"))   { ps->fenceTarget = &step->expected; return; }
    if (step && StartsWith(line, "@solution")) { ps->fenceTarget = &step->solution; return; }
    // Levels have nothing to read: any other text is a mistake in the file.
    TraceLog(LOG_WARNING, "LESSON: %s:%d: unrecognised line", ps->file, ps->lineNo);
}

// Fills in what a step left out, so the screens never see NULL code or output. A step
// without a solution uses its starter, so hints have something to compare with.
static void FinishStep(const char *path, int number, Step *step)
{
    if (!step->starter) step->starter = CopyText("");
    if (!step->expected) {
        TraceLog(LOG_WARNING, "LESSON: %s: step %d has no @expect output", path, number);
        step->expected = CopyText("");
    }
    if (!step->solution) {
        TraceLog(LOG_WARNING, "LESSON: %s: step %d has no @solution", path, number);
        step->solution = CopyText(step->starter);
    }
}

static bool ParseLessonFile(const char *path, Lesson *L)
{
    char *text = LoadFileText(path);
    if (!text) return false;

    memset(L, 0, sizeof *L);
    CopyField(L->id, sizeof L->id, GetFileNameWithoutExt(path));

    Parser ps = { 0 };
    ps.lesson = L;
    ps.file = path;

    char line[1024];
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        size_t copy = n < sizeof line - 1 ? n : sizeof line - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        if (copy && line[copy - 1] == '\r') line[copy - 1] = '\0';
        ps.lineNo++;
        ParseLine(&ps, line);
        p = end ? end + 1 : p + n;
    }
    if (ps.inCode) FlushCode(&ps);
    free(ps.code.data);
    UnloadFileText(text);

    for (int i = 0; i < L->stepCount; i++) FinishStep(path, i + 1, &L->steps[i]);
    if (L->title[0] == '\0') CopyField(L->title, sizeof L->title, L->id);
    return true;
}

static void FreeLesson(Lesson *L)
{
    for (int i = 0; i < L->stepCount; i++) {
        Step *step = &L->steps[i];
        free(step->starter);
        free(step->input);
        free(step->expected);
        free(step->solution);
    }
}

static int ComparePaths(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

bool LessonsLoad(LessonSet *set, const char *dir)
{
    memset(set, 0, sizeof *set);
    if (!DirectoryExists(dir)) return false;

    FilePathList files = LoadDirectoryFilesEx(dir, ".txt", false);
    qsort(files.paths, files.count, sizeof(char *), ComparePaths);

    set->items = calloc(files.count ? files.count : 1, sizeof(Lesson));
    for (unsigned int i = 0; i < files.count; i++) {
        if (ParseLessonFile(files.paths[i], &set->items[set->count])) set->count++;
    }
    UnloadDirectoryFiles(files);
    TraceLog(LOG_INFO, "LESSON: loaded %d lessons from %s", set->count, dir);
    return set->count > 0;
}

void LessonsUnload(LessonSet *set)
{
    for (int i = 0; i < set->count; i++) FreeLesson(&set->items[i]);
    free(set->items);
    set->items = NULL;
    set->count = 0;
}

const char *TrackName(Track t)
{
    return t == TRACK_CPP ? "C++" : "C";
}
