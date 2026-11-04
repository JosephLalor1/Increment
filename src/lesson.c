// Parses lesson files (assets/lessons/*.txt) into Lesson structs. The format is described
// in CLAUDE.md.
#include "lesson.h"
#include "raylib.h"

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

static char *CopyStr(const char *s)
{
    size_t n = strlen(s);
    char *c = malloc(n + 1);
    memcpy(c, s, n + 1);
    return c;
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
    StrBuf para;
    StrBuf code;
    BlockType paraType;
    bool inCode;
    bool inQuiz;
    bool inExercise;
    char **fenceTarget;  // in an exercise: which field the next code block fills
    StrBuf exDesc;       // description of the current exercise
    bool exBlankLine;    // a paragraph break is pending in exDesc
    const char *file;
    int lineNo;
} Parser;

// Moves the collected description into the current exercise.
static void FinishExercise(Parser *ps)
{
    Lesson *L = ps->lesson;
    if (ps->inExercise && L->exerciseCount > 0) {
        Exercise *ex = &L->exercises[L->exerciseCount - 1];
        free(ex->description);
        ex->description = SbTake(&ps->exDesc);
    }
    ps->inExercise = false;
    ps->exBlankLine = false;
}

static void ParseExerciseLine(Parser *ps, const char *line)
{
    Exercise *ex = &ps->lesson->exercises[ps->lesson->exerciseCount - 1];
    if (StartsWith(line, "@input"))    { ps->fenceTarget = &ex->input; return; }
    if (StartsWith(line, "@expect"))   { ps->fenceTarget = &ex->expected; return; }
    if (StartsWith(line, "@solution")) { ps->fenceTarget = &ex->solution; return; }
    if (line[0] == '\0') {
        if (ps->exDesc.len) ps->exBlankLine = true;
        return;
    }
    if (ps->exDesc.len) SbAppend(&ps->exDesc, ps->exBlankLine ? "\n\n" : " ", ps->exBlankLine ? 2 : 1);
    ps->exBlankLine = false;
    SbAppend(&ps->exDesc, line, strlen(line));
}

static void AddBlock(Parser *ps, BlockType type, char *text)
{
    Lesson *L = ps->lesson;
    if (L->blockCount >= MAX_BLOCKS) {
        TraceLog(LOG_WARNING, "LESSON: %s: too many blocks, ignoring the rest", ps->file);
        free(text);
        return;
    }
    L->blocks[L->blockCount].type = type;
    L->blocks[L->blockCount].text = text;
    L->blockCount++;
}

static void FlushPara(Parser *ps)
{
    if (ps->para.len == 0) return;
    AddBlock(ps, ps->paraType, SbTake(&ps->para));
}

static void FlushCode(Parser *ps)
{
    // Drop the trailing newline of the last code line.
    if (ps->code.len && ps->code.data[ps->code.len - 1] == '\n') ps->code.data[--ps->code.len] = '\0';
    char *code = SbTake(&ps->code);

    Lesson *L = ps->lesson;
    if (ps->inExercise) {
        char **target = ps->fenceTarget ? ps->fenceTarget : &L->exercises[L->exerciseCount - 1].starter;
        free(*target);
        *target = code;
        ps->fenceTarget = NULL;  // an unlabelled block is the starter code
    } else if (ps->inQuiz && L->questionCount > 0) {
        Question *q = &L->questions[L->questionCount - 1];
        free(q->code);
        q->code = code;
    } else {
        AddBlock(ps, BLOCK_CODE, code);
    }
}

static void AppendParaLine(Parser *ps, BlockType type, const char *text)
{
    if (ps->para.len && ps->paraType != type) FlushPara(ps);
    ps->paraType = type;
    if (ps->para.len) SbAppend(&ps->para, " ", 1);
    SbAppend(&ps->para, text, strlen(text));
}

static void ParseQuizLine(Parser *ps, const char *line)
{
    Lesson *L = ps->lesson;
    Question *q = L->questionCount ? &L->questions[L->questionCount - 1] : NULL;

    if (StartsWith(line, "? ")) {
        if (L->questionCount >= MAX_QUESTIONS) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many questions", ps->file, ps->lineNo);
            return;
        }
        q = &L->questions[L->questionCount++];
        q->prompt = CopyStr(line + 2);
        q->correct = -1;
    } else if (StartsWith(line, "- ") || StartsWith(line, "* ")) {
        if (!q) return;
        if (q->choiceCount >= MAX_CHOICES) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many choices", ps->file, ps->lineNo);
            return;
        }
        if (line[0] == '*') q->correct = q->choiceCount;
        q->choices[q->choiceCount++] = CopyStr(line + 2);
    } else if (StartsWith(line, "! ")) {
        if (!q) return;
        free(q->explain);
        q->explain = CopyStr(line + 2);
    } else if (line[0] != '\0') {
        TraceLog(LOG_WARNING, "LESSON: %s:%d: unrecognised quiz line", ps->file, ps->lineNo);
    }
}

static void ParseLine(Parser *ps, char *line)
{
    Lesson *L = ps->lesson;

    if (ps->inCode) {
        if (StartsWith(line, "```")) {
            FlushCode(ps);
            ps->inCode = false;
            return;
        }
        for (char *c = line; *c; c++) {
            if (*c == '\t') SbAppend(&ps->code, "    ", 4);
            else SbAppend(&ps->code, c, 1);
        }
        SbAppend(&ps->code, "\n", 1);
        return;
    }

    if (StartsWith(line, "```")) {
        FlushPara(ps);
        ps->inCode = true;
        ps->code.len = 0;
        return;
    }
    if (StartsWith(line, "@title ")) { CopyField(L->title, sizeof L->title, line + 7); return; }
    if (StartsWith(line, "@id "))    { CopyField(L->id, sizeof L->id, line + 4); return; }
    if (StartsWith(line, "@track ")) {
        const char *t = line + 7;
        while (*t == ' ') t++;
        L->track = (strcmp(t, "C++") == 0 || strcmp(t, "cpp") == 0) ? TRACK_CPP : TRACK_C;
        return;
    }
    if (StartsWith(line, "@kind ")) {
        L->practice = strstr(line + 6, "practice") != NULL;
        return;
    }
    if (StartsWith(line, "@quiz")) {
        FlushPara(ps);
        FinishExercise(ps);
        ps->inQuiz = true;
        return;
    }
    if (StartsWith(line, "@exercise")) {
        FlushPara(ps);
        FinishExercise(ps);
        ps->inQuiz = false;
        if (L->exerciseCount >= MAX_EXERCISES) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many exercises", ps->file, ps->lineNo);
            return;
        }
        const char *title = line + 9;
        while (*title == ' ') title++;
        L->exercises[L->exerciseCount++].title = CopyStr(*title ? title : "Exercise");
        ps->inExercise = true;
        ps->fenceTarget = NULL;
        return;
    }

    if (ps->inExercise) { ParseExerciseLine(ps, line); return; }
    if (ps->inQuiz) { ParseQuizLine(ps, line); return; }

    if (line[0] == '\0') { FlushPara(ps); return; }
    if (StartsWith(line, "# ")) {
        FlushPara(ps);
        AddBlock(ps, BLOCK_HEADING, CopyStr(line + 2));
        return;
    }
    if (StartsWith(line, "> ")) { AppendParaLine(ps, BLOCK_NOTE, line + 2); return; }
    if (StartsWith(line, "- ")) {
        // Each bullet is its own block; an indented line continues the previous bullet.
        FlushPara(ps);
        AddBlock(ps, BLOCK_BULLET, CopyStr(line + 2));
        return;
    }
    if (StartsWith(line, "  ") && L->blockCount && L->blocks[L->blockCount - 1].type == BLOCK_BULLET
        && ps->para.len == 0) {
        Block *b = &L->blocks[L->blockCount - 1];
        const char *more = line;
        while (*more == ' ') more++;
        size_t a = strlen(b->text), m = strlen(more);
        b->text = realloc(b->text, a + m + 2);
        b->text[a] = ' ';
        memcpy(b->text + a + 1, more, m + 1);
        return;
    }
    AppendParaLine(ps, BLOCK_TEXT, line);
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
    FlushPara(&ps);
    FinishExercise(&ps);

    free(ps.para.data);
    free(ps.code.data);
    free(ps.exDesc.data);
    for (int i = 0; i < L->exerciseCount; i++) {
        Exercise *ex = &L->exercises[i];
        if (!ex->starter) ex->starter = CopyStr("");
        if (!ex->expected) {
            TraceLog(LOG_WARNING, "LESSON: %s: exercise \"%s\" has no @expect output", path, ex->title);
            ex->expected = CopyStr("");
        }
    }
    UnloadFileText(text);

    if (L->title[0] == '\0') CopyField(L->title, sizeof L->title, L->id);
    for (int i = 0; i < L->questionCount; i++) {
        Question *q = &L->questions[i];
        if (q->correct < 0) {
            TraceLog(LOG_WARNING, "LESSON: %s: question %d has no correct answer (mark one with '* ')", path, i + 1);
            q->correct = 0;
        }
    }
    return true;
}

static void FreeLesson(Lesson *L)
{
    for (int i = 0; i < L->blockCount; i++) free(L->blocks[i].text);
    for (int i = 0; i < L->exerciseCount; i++) {
        Exercise *ex = &L->exercises[i];
        free(ex->title);
        free(ex->description);
        free(ex->starter);
        free(ex->input);
        free(ex->expected);
        free(ex->solution);
    }
    for (int i = 0; i < L->questionCount; i++) {
        Question *q = &L->questions[i];
        free(q->prompt);
        free(q->code);
        free(q->explain);
        for (int c = 0; c < q->choiceCount; c++) free(q->choices[c]);
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
