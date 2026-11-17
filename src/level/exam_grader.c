// Assesses an exam's frozen code: every test case of each task's step (built once, run on each
// case's input), then a second run with changed input/givens. Solutions stay outside the exam UI.
#include "level/exam_grader.h"
#include "level/step_cheats.h"
#include "core/text_copy.h"
#include <stdio.h>
#include <string.h>
const char *ExamTestName(ExamTest r) {
    const char *names[]={"Not assessed","Passed","Incorrect output","Compilation error","Runtime failure","Timed out","Required code missing / answer hardcoded","Compiler or assessment unavailable"};
    return r>=EXAM_UNTESTED && r<=EXAM_INFRASTRUCTURE ? names[r] : "Unknown";
}
static void Replace(char **field,const char *text) { free(*field); *field=CopyText(text ? text : ""); }
static void ClearTest(ExamGrader *g) { StepVariationFree(&g->variation);free(g->varied);free(g->expected);g->varied=g->expected=NULL; }
void ExamGraderFree(ExamGrader *g) { RunnerFree(&g->runner);CaseRunnerFree(&g->cases);ClearTest(g);memset(g,0,sizeof *g); }
void ExamGraderStart(ExamGrader *g,ExamRecord *r) { ExamGraderFree(g);g->record=r;g->busy=r && r->count>0; }
// What one run did, judged against the output it should give.
static ExamTest Judge(RunResult result,unsigned long exitCode,bool truncated,const char *output,const char *expected) {
    if(result==RESULT_COMPILE_ERROR) return EXAM_COMPILE;
    if(result==RESULT_TIMEOUT) return EXAM_TIMEOUT;
    if(result==RESULT_CRASH || (result==RESULT_OK && exitCode)) return EXAM_RUNTIME;
    if(result!=RESULT_OK) return EXAM_INFRASTRUCTURE;
    return !truncated && RunnerOutputMatches(output ? output : "",expected,NULL) ? EXAM_TEST_PASS : EXAM_WRONG;
}
static ExamTest Outcome(Runner *r,const char *expected) {
    return Judge(r->result,r->exitCode,r->outputTruncated,r->programOutput,expected);
}
static void Next(ExamGrader *g) {
    ClearTest(g);g->stage=0;g->task++;
    if(g->task>=g->record->count) { g->busy=false;ExamSessionScore(g->record); }
}
// Stage 1 is over: each test case's result, the main case's code checks, and the first
// failing extra case's input, target and output as the detail to review.
static void JudgeCases(ExamGrader *g,ExamTask *t,const Step *step) {
    const CaseRunner *c=&g->cases;
    Replace(&t->diagnostics,c->runner.compileOutput);Replace(&t->output,c->outcomes[0].output);
    Replace(&t->detail,c->outcomes[0].summary);
    for(int i=0;i<=t->extraCount;i++) {
        const CaseOutcome *o=&c->outcomes[i];
        t->tests[i]=Judge(o->result,o->exitCode,o->truncated,o->output,i ? t->extraExpected[i-1] : t->expected);
    }
    if(t->tests[0]==EXAM_TEST_PASS) {
        Skill missing[SKILL_COUNT]; StepCheat cheat=StepCheatFind(step,t->code);
        int n=StepMissingConstructs(step,t->code,g->record->track==TRACK_CPP,missing,SKILL_COUNT);
        if(n || cheat.kind!=CHEAT_NONE) { t->tests[0]=EXAM_CONSTRUCT;Replace(&t->detail,n ? SkillTitle(missing[0]) : cheat.note); }
    }
    t->passed=0;
    for(int i=0;i<=t->extraCount;i++) t->passed+=t->tests[i]==EXAM_TEST_PASS;
    for(int i=1;i<=t->extraCount && t->tests[0]==EXAM_TEST_PASS;i++) {
        if(t->tests[i]==EXAM_TEST_PASS) continue;
        char report[12000];
        snprintf(report,sizeof report,"Test %d input:\n%.2000s\nExpected:\n%.4000s\nActual:\n%.4000s\n%s",i+1,t->extraInputs[i-1],
                 t->extraExpected[i-1],c->outcomes[i].output,c->outcomes[i].summary);
        Replace(&t->detail,report);
        break;
    }
}
void ExamGraderTick(ExamGrader *g) {
    if(!g->busy) return;
    ExamTask *t=&g->record->tasks[g->task]; Step step=ExamTaskStep(t);
    int changedTest=1+t->extraCount;  // the changed-values run comes after the test cases
    if(g->stage==0) {
        t->passed=0;for(int i=0;i<EXAM_MAX_TESTS;i++) t->tests[i]=EXAM_UNTESTED;
        t->testCount=changedTest+(StepVariationMake(&step,&g->variation) ? 1 : 0);
        const char *inputs[MAX_STEP_CASES]={t->input};
        for(int i=0;i<t->extraCount;i++) inputs[i+1]=t->extraInputs[i];
        CaseRunnerStart(&g->cases,t->code,g->record->track==TRACK_CPP,inputs,changedTest);g->stage=1;return;
    }
    if(g->stage==1) {
        CaseRunnerUpdate(&g->cases); if(CaseRunnerBusy(&g->cases)) return;
        JudgeCases(g,t,&step);
        if(t->tests[0]!=EXAM_TEST_PASS || t->testCount==changedTest) { Next(g);return; }
        int missing=0;char summary[STEP_CHECK_SUMMARY]="";
        g->varied=StepVariationApply(&step,t->code,&missing,summary);
        if(!g->varied || missing) { t->tests[changedTest]=EXAM_CONSTRUCT;Replace(&t->detail,summary);Next(g);return; }
        RunnerStart(&g->runner,g->variation.solution,g->record->track==TRACK_CPP,g->variation.input);g->stage=2;return;
    }
    RunnerUpdate(&g->runner); if(RunnerBusy(&g->runner)) return;
    if(g->stage==2) {
        if(g->runner.result!=RESULT_OK || g->runner.exitCode || g->runner.outputTruncated) { t->tests[changedTest]=EXAM_INFRASTRUCTURE;Next(g);return; }
        g->expected=CopyText(g->runner.programOutput ? g->runner.programOutput : "");
        if(!g->expected) { t->tests[changedTest]=EXAM_INFRASTRUCTURE;Next(g);return; }
        RunnerStart(&g->runner,g->varied,g->record->track==TRACK_CPP,g->variation.input);g->stage=3;return;
    }
    t->tests[changedTest]=Outcome(&g->runner,g->expected);if(t->tests[changedTest]==EXAM_TEST_PASS) t->passed++;
    char report[12000];snprintf(report,sizeof report,"Changed values: %s\nExpected:\n%.4000s\nActual:\n%.4000s\n%s",g->variation.summary,g->expected,
                               g->runner.programOutput ? g->runner.programOutput : "",g->runner.summary);
    Replace(&t->detail,report); Next(g);
}
