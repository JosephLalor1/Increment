// Immutable exam submissions, separate from level progress and earned skill XP.
#ifndef EXAM_HISTORY_H
#define EXAM_HISTORY_H
#include "learning/checkpoints.h"
#include <stdint.h>
#define EXAM_STRING_LIMIT (512*1024)
typedef enum { EXAM_UNTESTED, EXAM_TEST_PASS, EXAM_WRONG, EXAM_COMPILE, EXAM_RUNTIME, EXAM_TIMEOUT, EXAM_CONSTRUCT, EXAM_INFRASTRUCTURE } ExamTest;
typedef struct {
    char lessonId[32], title[96]; int step;
    bool skills[SKILL_COUNT];
    float weights[SKILL_COUNT]; int difficulty;
    bool fixedValues;
    char *code,*starter,*input,*expected,*solution,*diagnostics,*output,*detail;
    ExamTest tests[2]; int testCount, passed;
} ExamTask;
typedef struct {
    char id[64],title[256]; Track track;
    int count,passPercent,duration,elapsed,percent; int64_t timestamp;
    bool interrupted;
    ExamTask tasks[EXAM_MAX_TASKS];
} ExamRecord;
typedef struct { char file[1200]; int percent,elapsed; int64_t timestamp; } ExamHistoryItem;
// Folder is the account's code folder; exams live in its exams/ child. NULL signs out.
void ExamHistoryInit(const char *codeFolder);
int ExamHistoryBest(const char *id); // -1 if no valid recorded attempts
// Writes a fresh file atomically, never overwriting an earlier attempt.
bool ExamHistorySave(ExamRecord *record);
// Returned array belongs to caller. Most recent first. Reads only this account's folder.
ExamHistoryItem *ExamHistoryList(const char *id,int *count);
bool ExamHistoryRead(const char *file,ExamRecord *record);
void ExamRecordFree(ExamRecord *record);
#endif
