// Background assessment using the case runner and the honest-solution checks: a test per
// test case of each task, then one with changed values.
#ifndef EXAM_GRADER_H
#define EXAM_GRADER_H
#include "learning/exam_session.h"
#include "level/step_check.h"
#include "code/case_runner.h"
#include "code/runner.h"
typedef struct {
    CaseRunner cases;  // the task's test cases, built once
    Runner runner;     // the changed-values run
    ExamRecord *record; int task,stage; bool busy;
    StepVariation variation; char *varied,*expected;
} ExamGrader;
void ExamGraderStart(ExamGrader *grader,ExamRecord *record);
// Tick until busy is false. Only the frozen record is updated; never writes progress/XP.
void ExamGraderTick(ExamGrader *grader);
void ExamGraderFree(ExamGrader *grader);
const char *ExamTestName(ExamTest result);
#endif
