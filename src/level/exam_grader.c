// Base output plus a changed-input/givens assessment; solutions stay outside the exam UI.
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
void ExamGraderFree(ExamGrader *g) { RunnerFree(&g->runner);ClearTest(g);memset(g,0,sizeof *g); }
void ExamGraderStart(ExamGrader *g,ExamRecord *r) { ExamGraderFree(g);g->record=r;g->busy=r && r->count>0; }
static ExamTest Outcome(Runner *r,const char *expected) {
    int line;
    if(r->result==RESULT_COMPILE_ERROR) return EXAM_COMPILE;
    if(r->result==RESULT_TIMEOUT) return EXAM_TIMEOUT;
    if(r->result==RESULT_CRASH || (r->result==RESULT_OK && r->exitCode)) return EXAM_RUNTIME;
    if(r->result!=RESULT_OK) return EXAM_INFRASTRUCTURE;
    return !r->outputTruncated && RunnerOutputMatches(r->programOutput ? r->programOutput : "",expected,&line) ? EXAM_TEST_PASS : EXAM_WRONG;
}
static void Next(ExamGrader *g) {
    ClearTest(g);g->stage=0;g->task++;
    if(g->task>=g->record->count) { g->busy=false;ExamSessionScore(g->record); }
}
void ExamGraderTick(ExamGrader *g) {
    if(!g->busy) return;
    ExamTask *t=&g->record->tasks[g->task]; Step step=ExamTaskStep(t);
    if(g->stage==0) {
        t->passed=0;t->tests[0]=t->tests[1]=EXAM_UNTESTED;
        t->testCount=StepVariationMake(&step,&g->variation) ? 2 : 1;
        RunnerStart(&g->runner,t->code,g->record->track==TRACK_CPP,t->input);g->stage=1;return;
    }
    RunnerUpdate(&g->runner); if(RunnerBusy(&g->runner)) return;
    if(g->stage==1) {
        Replace(&t->diagnostics,g->runner.compileOutput);Replace(&t->output,g->runner.programOutput);
        t->tests[0]=Outcome(&g->runner,t->expected);Replace(&t->detail,g->runner.summary);
        if(t->tests[0]==EXAM_TEST_PASS) {
            Skill missing[SKILL_COUNT]; StepCheat cheat=StepCheatFind(&step,t->code);
            int n=StepMissingConstructs(&step,t->code,g->record->track==TRACK_CPP,missing,SKILL_COUNT);
            if(n || cheat.kind!=CHEAT_NONE) { t->tests[0]=EXAM_CONSTRUCT;Replace(&t->detail,n ? SkillTitle(missing[0]) : cheat.note); }
        }
        if(t->tests[0]!=EXAM_TEST_PASS) { Next(g);return; }
        t->passed=1;
        if(t->testCount==1) { Next(g);return; }
        int missing=0;char summary[STEP_CHECK_SUMMARY]="";
        g->varied=StepVariationApply(&step,t->code,&missing,summary);
        if(!g->varied || missing) { t->tests[1]=EXAM_CONSTRUCT;Replace(&t->detail,summary);Next(g);return; }
        RunnerStart(&g->runner,g->variation.solution,g->record->track==TRACK_CPP,g->variation.input);g->stage=2;return;
    }
    if(g->stage==2) {
        if(g->runner.result!=RESULT_OK || g->runner.exitCode || g->runner.outputTruncated) { t->tests[1]=EXAM_INFRASTRUCTURE;Next(g);return; }
        g->expected=CopyText(g->runner.programOutput ? g->runner.programOutput : "");
        if(!g->expected) { t->tests[1]=EXAM_INFRASTRUCTURE;Next(g);return; }
        RunnerStart(&g->runner,g->varied,g->record->track==TRACK_CPP,g->variation.input);g->stage=3;return;
    }
    t->tests[1]=Outcome(&g->runner,g->expected);if(t->tests[1]==EXAM_TEST_PASS) t->passed++;
    char report[12000];snprintf(report,sizeof report,"Changed values: %s\nExpected:\n%.4000s\nActual:\n%.4000s\n%s",g->variation.summary,g->expected,
                               g->runner.programOutput ? g->runner.programOutput : "",g->runner.summary);
    Replace(&t->detail,report); Next(g);
}
