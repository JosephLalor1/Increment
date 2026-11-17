// Copies reference tasks at start so attempts remain reviewable after curriculum edits.
#include "learning/exam_session.h"
#include "core/text_copy.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void ExamSessionFree(ExamSession *s) { ExamRecordFree(&s->record); memset(s,0,sizeof *s); }
bool ExamSessionCode(ExamSession *s,int task,const char *code) {
    if(!s->active || s->submitted || task<0 || task>=s->record.count || !code || strlen(code)>EXAM_STRING_LIMIT) return false;
    char *copy=CopyText(code); if(!copy) return false; free(s->record.tasks[task].code); s->record.tasks[task].code=copy; return true;
}
bool ExamSessionStart(ExamSession *s,const LessonSet *set,const Checkpoint *exam,double now) {
    ExamSessionFree(s);
    if(!set || !exam || exam->count<1 || exam->count>EXAM_MAX_TASKS || exam->duration<=0) return false;
    ExamRecord *r=&s->record; snprintf(r->id,sizeof r->id,"%s",exam->id); snprintf(r->title,sizeof r->title,"%s",exam->title);
    r->track=exam->track; r->count=exam->count; r->passPercent=exam->passPercent; r->duration=exam->duration;
    for(int i=0;i<r->count;i++) {
        if(exam->lessons[i]<0 || exam->lessons[i]>=set->count) goto failed;
        const Lesson *lesson=&set->items[exam->lessons[i]];
        if(exam->steps[i]<0 || exam->steps[i]>=lesson->stepCount || lesson->track!=exam->track) goto failed;
        const Step *step=&lesson->steps[exam->steps[i]]; ExamTask *t=&r->tasks[i];
        if(!step->starter || !step->expected || !step->solution) goto failed;
        snprintf(t->lessonId,sizeof t->lessonId,"%s",lesson->id); snprintf(t->title,sizeof t->title,"%s",lesson->title); t->step=exam->steps[i];
        t->code=CopyText(step->starter); t->starter=CopyText(step->starter); t->input=CopyText(step->input ? step->input : "");
        t->expected=CopyText(step->expected); t->solution=CopyText(step->solution); t->testCount=1;
        // Fixed-value metadata is needed by the existing second-test generator.
        t->fixedValues=step->fixedValues;
        t->difficulty=step->difficulty;
        for(int j=0;j<step->skillCount;j++) if(step->skills[j].skill>=0 && step->skills[j].skill<SKILL_COUNT) {
            t->skills[step->skills[j].skill]=true;t->weights[step->skills[j].skill]=step->skills[j].weight;
        }
        if(!t->code || !t->starter || !t->input || !t->expected || !t->solution) goto failed;
    }
    s->started=now; s->deadline=now+r->duration; s->active=true; return true;
failed: ExamSessionFree(s); return false;
}
int ExamSessionRemaining(const ExamSession *s,double now) { double left=s->deadline-now; return left>0 ? (int)ceil(left) : 0; }
bool ExamSessionSubmit(ExamSession *s,double now) {
    if(!s->active || s->submitted) return false;
    double elapsed=now-s->started; if(elapsed<0) elapsed=0; if(elapsed>s->record.duration) elapsed=s->record.duration;
    s->record.elapsed=(int)elapsed; s->submitted=true; s->active=false; return true;
}
void ExamSessionScore(ExamRecord *r) {
    double marks=0; for(int i=0;i<r->count;i++) if(r->tasks[i].testCount>0) marks+=(double)r->tasks[i].passed/r->tasks[i].testCount;
    r->percent=r->count ? (int)floor(marks*100/r->count+0.00001) : 0;
}
Step ExamTaskStep(const ExamTask *t) {
    Step step={0}; step.starter=t->starter;step.input=t->input;step.expected=t->expected;step.solution=t->solution;step.fixedValues=t->fixedValues;step.difficulty=t->difficulty;
    for(int s=0;s<SKILL_COUNT && step.skillCount<MAX_QUESTION_SKILLS;s++) if(t->skills[s]) step.skills[step.skillCount++]=(SkillWeight){(Skill)s,t->weights[s]};
    return step;
}
