// Parses lesson files (assets/lessons/*.txt) into Lesson structs. The format is described
// in CLAUDE.md.
#include "lesson.h"
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
    bool codeIsMemory;   // the open fence is ```memory: a pointer diagram script, not code
    bool inQuiz;
    bool inExercise;
    bool inTrace;
    bool ignoreTrace; // extra tasks must not overwrite the last accepted task
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

// The "@hint", "@input", "@expect" and "@solution" lines shared by exercises and quiz
// coding questions. Returns false if line is none of them.
static bool ParseTaskLine(Parser *ps, Exercise *ex, const char *line)
{
    if (strcmp(line, "@hint") == 0 || StartsWith(line, "@hint ") || StartsWith(line, "@hint\t")) {
        const char *text = line + 5;
        while (*text == ' ' || *text == '\t') text++;
        if (!*text) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: empty exercise hint ignored", ps->file, ps->lineNo);
        } else if (ex->hintCount >= MAX_EXERCISE_HINTS) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many exercise hints, ignoring extra hint", ps->file, ps->lineNo);
        } else ex->hints[ex->hintCount++] = CopyStr(text);
        return true;
    }
    if (StartsWith(line, "@input"))    { ps->fenceTarget = &ex->input; return true; }
    if (StartsWith(line, "@expect"))   { ps->fenceTarget = &ex->expected; return true; }
    if (StartsWith(line, "@solution")) { ps->fenceTarget = &ex->solution; return true; }
    return false;
}

// The coding task of the quiz question being parsed, or NULL for multiple choice.
static Exercise *CurrentQuizTask(Parser *ps)
{
    Lesson *L = ps->lesson;
    return ps->inQuiz && L->questionCount > 0 ? L->questions[L->questionCount - 1].task : NULL;
}

static void ParseExerciseLine(Parser *ps, const char *line)
{
    Exercise *ex = &ps->lesson->exercises[ps->lesson->exerciseCount - 1];
    if (ParseTaskLine(ps, ex, line)) return;
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
    if (ps->inTrace) {
        if (ps->ignoreTrace) { free(code); return; }
        TraceExercise *trace = &L->traces[L->traceCount - 1];
        free(trace->code);
        trace->code = code;
    } else if (ps->inExercise) {
        char **target = ps->fenceTarget ? ps->fenceTarget : &L->exercises[L->exerciseCount - 1].starter;
        free(*target);
        *target = code;
        ps->fenceTarget = NULL;  // an unlabelled block is the starter code
    } else if (CurrentQuizTask(ps)) {  // a coding question: like an exercise's code blocks
        Exercise *task = CurrentQuizTask(ps);
        char **target = ps->fenceTarget ? ps->fenceTarget : &task->starter;
        free(*target);
        *target = code;
        ps->fenceTarget = NULL;
    } else if (ps->inQuiz && L->questionCount > 0) {
        Question *q = &L->questions[L->questionCount - 1];
        free(q->code);
        q->code = code;
    } else {
        AddBlock(ps, ps->codeIsMemory ? BLOCK_MEMORY : BLOCK_CODE, code);
    }
}

static void AppendParaLine(Parser *ps, BlockType type, const char *text)
{
    if (ps->para.len && ps->paraType != type) FlushPara(ps);
    ps->paraType = type;
    if (ps->para.len) SbAppend(&ps->para, " ", 1);
    SbAppend(&ps->para, text, strlen(text));
}

// A question's "~" line: its difficulty (1 to 3), then up to MAX_QUESTION_SKILLS skills,
// each name:weight with a weight from 0 to 1.
static void ParseSkillLine(Parser *ps, Question *q, const char *text)
{
    char *end;
    long difficulty = strtol(text, &end, 10);
    if (end == text || difficulty < 1 || difficulty > SKILL_MAX_DIFFICULTY) {
        TraceLog(LOG_WARNING, "LESSON: %s:%d: a skills line starts with a difficulty from 1 to 3", ps->file, ps->lineNo);
        return;
    }
    q->difficulty = (int)difficulty;
    q->skillCount = 0;
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
        } else if (q->skillCount < MAX_QUESTION_SKILLS) {
            q->skills[q->skillCount++] = (SkillWeight){ skill, weight };
        }
        p += tokenLength;
    }
}

static void ParseQuizLine(Parser *ps, const char *line)
{
    Lesson *L = ps->lesson;
    Question *q = L->questionCount ? &L->questions[L->questionCount - 1] : NULL;
    bool coding = StartsWith(line, "?code ");

    if (StartsWith(line, "? ") || coding) {
        if (L->questionCount >= MAX_QUESTIONS) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many questions", ps->file, ps->lineNo);
            return;
        }
        q = &L->questions[L->questionCount++];
        const char *prompt = line + (coding ? 6 : 2);
        while (*prompt == ' ') prompt++;
        q->prompt = CopyStr(prompt);
        q->correct = -1;
        q->difficulty = 2;  // until its "~" line says otherwise
        ps->fenceTarget = NULL;
        if (coding) {  // the prompt doubles as the task description in the code editor
            q->task = calloc(1, sizeof *q->task);
            q->task->title = CopyStr("Coding question");
            q->task->description = CopyStr(prompt);
        }
    } else if (StartsWith(line, "~ ")) {  // "~ 2 tracing:1 pointers:0.4": difficulty, then weighted skills
        if (q) ParseSkillLine(ps, q, line + 2);
    } else if (q && q->task && ParseTaskLine(ps, q->task, line)) {
        return;
    } else if (StartsWith(line, "- ") || StartsWith(line, "* ")) {
        if (!q) return;
        if (q->task) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: a coding question has no choices", ps->file, ps->lineNo);
            return;
        }
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
        ps->codeIsMemory = StartsWith(line, "```memory") && !ps->inExercise && !ps->inQuiz && !ps->inTrace;
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
    if (StartsWith(line, "@speed ")) { CopyField(L->speedSkills, sizeof L->speedSkills, line + 7); return; }
    if (StartsWith(line, "@kind ")) {
        L->practice = strstr(line + 6, "practice") != NULL;
        return;
    }
    if (StartsWith(line, "@quiz")) {
        FlushPara(ps);
        FinishExercise(ps);
        ps->inQuiz = true;
        ps->inTrace = false;
        ps->ignoreTrace = false;
        return;
    }
    if (StartsWith(line, "@exercise")) {
        FlushPara(ps);
        FinishExercise(ps);
        ps->inQuiz = false;
        ps->inTrace = false;
        ps->ignoreTrace = false;
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

    if (StartsWith(line, "@trace ")) {
        FlushPara(ps);
        FinishExercise(ps);
        ps->inQuiz = false;
        ps->inTrace = false;
        ps->ignoreTrace = false;
        if (L->traceCount >= MAX_TRACES) {
            TraceLog(LOG_WARNING, "LESSON: %s:%d: too many tracing exercises", ps->file, ps->lineNo);
            ps->inTrace = ps->ignoreTrace = true;
            return;
        }
        TraceExercise *trace = &L->traces[L->traceCount++];
        const char *id = line + 7;
        while (*id == ' ') id++;
        const char *title = strchr(id, ' ');
        size_t n = title ? (size_t)(title - id) : strlen(id);
        if (n >= sizeof trace->id) n = 0; // disable overlong ids rather than aliasing another task
        memcpy(trace->id, id, n);
        CopyField(trace->title, sizeof trace->title, title ? title + 1 : id);
        ps->inTrace = true;
        return;
    }
    if (ps->inTrace) {
        if (ps->ignoreTrace) return;
        TraceExercise *trace = &L->traces[L->traceCount - 1];
        char **field = NULL;
        const char *value = NULL;
        if (StartsWith(line, "@predict ")) { field = &trace->prompt; value = line + 9; }
        if (StartsWith(line, "@answer ")) { field = &trace->answer; value = line + 8; }
        if (StartsWith(line, "@explain ")) { field = &trace->explain; value = line + 9; }
        if (field) { free(*field); *field = CopyStr(value); }
        else if (*line) TraceLog(LOG_WARNING, "LESSON: %s:%d: unrecognised tracing line", ps->file, ps->lineNo);
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

// Fills in what an exercise or coding question left out, so the editor never sees NULL
// starter code or expected output.
static void FinishTask(const char *path, Exercise *ex)
{
    if (!ex->starter) ex->starter = CopyStr("");
    if (!ex->expected) {
        TraceLog(LOG_WARNING, "LESSON: %s: exercise \"%s\" has no @expect output", path, ex->title);
        ex->expected = CopyStr("");
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
    FlushPara(&ps);
    FinishExercise(&ps);

    for (int i = 0; i < L->traceCount; i++) {
        for (int j = 0; j < i; j++) {
            if (strcmp(L->traces[i].id, L->traces[j].id) == 0) {
                TraceLog(LOG_WARNING, "LESSON: %s: duplicate tracing id disabled", path);
                L->traces[i].id[0] = '\0';
                break;
            }
        }
    }

    free(ps.para.data);
    free(ps.code.data);
    free(ps.exDesc.data);
    for (int i = 0; i < L->exerciseCount; i++) FinishTask(path, &L->exercises[i]);
    UnloadFileText(text);

    if (L->title[0] == '\0') CopyField(L->title, sizeof L->title, L->id);
    for (int i = 0; i < L->questionCount; i++) {
        Question *q = &L->questions[i];
        if (q->task) {
            FinishTask(path, q->task);
        } else if (q->correct < 0) {
            TraceLog(LOG_WARNING, "LESSON: %s: question %d has no correct answer (mark one with '* ')", path, i + 1);
            q->correct = 0;
        }
    }
    return true;
}

static void FreeExercise(Exercise *ex)
{
    free(ex->title);
    free(ex->description);
    free(ex->starter);
    free(ex->input);
    free(ex->expected);
    free(ex->solution);
    for (int h = 0; h < ex->hintCount; h++) free(ex->hints[h]);
}

static void FreeLesson(Lesson *L)
{
    for (int i = 0; i < L->traceCount; i++) {
        free(L->traces[i].code); free(L->traces[i].prompt);
        free(L->traces[i].answer); free(L->traces[i].explain);
    }
    for (int i = 0; i < L->blockCount; i++) free(L->blocks[i].text);
    for (int i = 0; i < L->exerciseCount; i++) FreeExercise(&L->exercises[i]);
    for (int i = 0; i < L->questionCount; i++) {
        Question *q = &L->questions[i];
        free(q->prompt);
        free(q->code);
        free(q->explain);
        for (int c = 0; c < q->choiceCount; c++) free(q->choices[c]);
        if (q->task) FreeExercise(q->task);
        free(q->task);
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

// FNV-1a: a small, stable hash, so a question's id is the same on every run and machine.
static unsigned HashText(unsigned hash, const char *text)
{
    for (const unsigned char *c = (const unsigned char *)(text ? text : ""); *c; c++) hash = (hash ^ *c) * 16777619u;
    return hash;
}

void LessonQuestionId(const Lesson *lesson, const Question *question, char *out, int size)
{
    unsigned hash = HashText(2166136261u, question->prompt);
    hash = HashText(hash ^ 0x1fu, question->code);  // the same prompt over different code is a different question
    snprintf(out, (size_t)size, "%s?%08x", lesson->id, hash);
}

const char *TrackName(Track t)
{
    return t == TRACK_CPP ? "C++" : "C";
}
