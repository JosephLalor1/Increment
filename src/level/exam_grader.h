// Background assessment using the existing runner and honest-solution checks.
#ifndef EXAM_GRADER_H
#define EXAM_GRADER_H
#include "learning/exam_session.h"
#include "level/step_check.h"
#include "code/runner.h"
typedef struct {
    Runner runner; ExamRecord *record; int task,stage; bool busy;
    StepVariation variation; char *varied,*expected;
} ExamGrader;
void ExamGraderStart(ExamGrader *grader,ExamRecord *record);
// Tick until busy is false. Only the frozen record is updated; never writes progress/XP.
void ExamGraderTick(ExamGrader *grader);
void ExamGraderFree(ExamGrader *grader);
const char *ExamTestName(ExamTest result);
#endif
