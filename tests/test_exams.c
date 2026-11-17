// Checkpoint grouping/gates, frozen deadlines, immutable history, real C/C++ assessment.
#include "learning/checkpoints.h"
#include "learning/exam_session.h"
#include "learning/progress.h"
#include "learning/roadmap.h"
#include "level/exam_grader.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Grade(ExamRecord *record) {
    ExamGrader grader={0};ExamGraderStart(&grader,record);double start=GetTime();
    while(grader.busy && GetTime()-start<120) {ExamGraderTick(&grader);WaitTime(0.002);}
    assert(!grader.busy);ExamGraderFree(&grader);
}
int main(int argc,char **argv) {
    assert(argc==2);SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(900,640,"test_exams"); // Runner uses raylib's monotonic timer
    char userA[1200],userB[1200],progress[1200];
    snprintf(userA,sizeof userA,"%s/exam_a",argv[1]);snprintf(userB,sizeof userB,"%s/exam_b",argv[1]);
    MakeDirectory(userA);MakeDirectory(userB);snprintf(progress,sizeof progress,"%s/exam_progress.txt",argv[1]);
    ProgressLoad(progress);ExamHistoryInit(userA);
    LessonSet set;assert(LessonsLoad(&set,"assets/lessons"));
    assert(CheckpointCount(&set,TRACK_C)==4 && CheckpointCount(&set,TRACK_CPP)==3);
    for(int track=0;track<TRACK_COUNT;track++) {
        int previous=-1,levels=0;Checkpoint e;
        for(int s=0;CheckpointGet(&set,(Track)track,s,&e);s++) {
            assert(e.count>=4 && e.count<=5 && e.duration==1800 && e.passPercent==60);
            for(int j=0;j<e.count;j++) {
                assert(e.lessons[j]>previous && set.items[e.lessons[j]].track==(Track)track);
                assert(e.steps[j]==set.items[e.lessons[j]].stepCount-1);
                previous=e.lessons[j];levels++;
            }
            assert(CheckpointAfter(&set,e.lessons[e.count-1])==s);
        }
        assert(levels==(track==TRACK_C ? 17 : 13));
    }
    LessonSet empty={0};assert(CheckpointCount(&empty,TRACK_C)==0);
    LessonSet tiny={calloc(7,sizeof(Lesson)),7};assert(tiny.items);
    for(int i=0;i<7;i++) {snprintf(tiny.items[i].id,32,"tiny_%d",i);tiny.items[i].stepCount=1;}
    Checkpoint shortExam;assert(CheckpointCount(&tiny,TRACK_C)==2 && CheckpointGet(&tiny,TRACK_C,1,&shortExam) && shortExam.count==3);
    free(tiny.items);
    Checkpoint first,second,cpp;assert(CheckpointGet(&set,TRACK_C,0,&first) && CheckpointGet(&set,TRACK_C,1,&second) && CheckpointGet(&set,TRACK_CPP,0,&cpp));
    // Fresh accounts can select every real level, every step, and every exam in either track.
    for(int i=0;i<set.count;i++) {
        assert(CheckpointLessonOpen(&set,i));
        for(int s=0;s<set.items[i].stepCount;s++) assert(RoadmapStepOpen(&set.items[i],s));
    }
    ExamSession fresh={0};
    for(int track=0;track<TRACK_COUNT;track++) {
        Checkpoint exam;
        for(int section=0;CheckpointGet(&set,(Track)track,section,&exam);section++) {
            assert(CheckpointAvailable(&set,&exam) && !CheckpointPassed(&exam));
            assert(ExamSessionStart(&fresh,&set,&exam,0));ExamSessionFree(&fresh);
        }
    }
    assert(CheckpointAvailable(&set,&first) && CheckpointAvailable(&set,&second));
    assert(!CheckpointLessonOpen(&set,-1) && !CheckpointLessonOpen(&set,set.count));
    Checkpoint invalid=first;invalid.count=EXAM_MAX_TASKS+1;assert(!CheckpointAvailable(&set,&invalid));
    invalid=first;invalid.lessons[0]=-1;assert(!CheckpointAvailable(&set,&invalid));
    for(int i=0;i<first.count;i++) for(int j=0;j<set.items[first.lessons[i]].stepCount;j++) assert(ProgressRecordStep(set.items[first.lessons[i]].id,j));
    assert(CheckpointAvailable(&set,&first) && !CheckpointPassed(&first));
    char *before=LoadFileText(progress);assert(before);
    ExamSession session={0};Checkpoint fast=first;fast.duration=2;
    assert(ExamSessionStart(&session,&set,&fast,1000));
    assert(ExamSessionRemaining(&session,1000)==2 && ExamSessionRemaining(&session,1001.1)==1 && ExamSessionRemaining(&session,1005)==0);
    assert(ExamSessionCode(&session,0,"int main(void){return 0;}"));
    assert(ExamSessionCode(&session,1,"int main(void){return 1;}"));
    assert(!strcmp(session.record.tasks[0].code,"int main(void){return 0;}")); // switching buffers preserves work
    assert(ExamSessionSubmit(&session,1005) && session.record.elapsed==2 && !ExamSessionSubmit(&session,1006));
    assert(!ExamSessionCode(&session,0,"late edit"));ExamSessionScore(&session.record);
    assert(ExamHistorySave(&session.record) && CheckpointLessonOpen(&set,second.lessons[0]));
    assert(!CheckpointPassed(&first) && CheckpointAvailable(&set,&first) && CheckpointAvailable(&set,&second));
    int count;ExamHistoryItem *items=ExamHistoryList(first.id,&count);assert(count==1);
    ExamRecord loaded={0};assert(ExamHistoryRead(items[0].file,&loaded));
    assert(!strcmp(loaded.tasks[0].code,session.record.tasks[0].code) && loaded.track==TRACK_C && loaded.elapsed==2);
    assert(!strcmp(loaded.tasks[0].solution,set.items[first.lessons[0]].steps[first.steps[0]].solution));
    free(items);ExamRecordFree(&loaded);ExamSessionFree(&session);
    assert(ExamSessionStart(&session,&set,&first,2000));
    for(int i=0;i<session.record.count;i++) {session.record.tasks[i].passed=1;session.record.tasks[i].tests[0]=EXAM_TEST_PASS;}
    ExamSessionSubmit(&session,2010);ExamSessionScore(&session.record);assert(session.record.percent==100);
    assert(ExamHistorySave(&session.record) && CheckpointPassed(&first) && CheckpointLessonOpen(&set,second.lessons[0]));
    items=ExamHistoryList(first.id,&count);assert(count==2);free(items);
    assert(!CheckpointPassed(&cpp));ExamHistoryInit(userB);assert(!CheckpointPassed(&first));
    assert(CheckpointAvailable(&set,&second) && CheckpointLessonOpen(&set,second.lessons[0]));
    ExamHistoryInit(userA);assert(CheckpointPassed(&first)); // restart/account reload
    session.record.percent=0;for(int i=0;i<session.record.count;i++) session.record.tasks[i].passed=0;
    assert(ExamHistorySave(&session.record) && CheckpointPassed(&first)); // a failed retake preserves a pass
    items=ExamHistoryList(first.id,&count);assert(count==3);free(items);
    char damaged[1300];snprintf(damaged,sizeof damaged,"%s/exams/damaged.exam",userA);FILE *file=fopen(damaged,"wb");assert(file);fputs("INCEX01",file);fclose(file);
    ExamHistoryInit(userA);items=ExamHistoryList(first.id,&count);assert(count==3);free(items);
    assert(!ExamHistoryRead(damaged,&loaded));
    ExamSessionFree(&session);
    // Reference submissions for both tracks use actual last-step metadata and must pass real checks.
    for(int track=0;track<TRACK_COUNT;track++) {
        Checkpoint exam;assert(CheckpointGet(&set,(Track)track,0,&exam));assert(ExamSessionStart(&session,&set,&exam,0));
        for(int i=0;i<exam.count;i++) assert(ExamSessionCode(&session,i,session.record.tasks[i].solution));
        ExamSessionSubmit(&session,10);Grade(&session.record);
        if(session.record.percent!=100) for(int i=0;i<exam.count;i++) printf("task %d: %s / %s: %s\n",i,ExamTestName(session.record.tasks[i].tests[0]),ExamTestName(session.record.tasks[i].tests[1]),session.record.tasks[i].detail);
        assert(session.record.percent==100);ExamSessionFree(&session);
    }
    // Compilation, wrong output, runtime failures and timeouts are distinguishable.
    assert(ExamSessionStart(&session,&set,&first,0));
    assert(ExamSessionCode(&session,0,"this is not C"));
    assert(ExamSessionCode(&session,1,"#include <stdio.h>\nint main(void){puts(\"wrong\");return 0;}"));
    assert(ExamSessionCode(&session,2,"int main(void){return 1;}"));
    assert(ExamSessionCode(&session,3,"int main(void){for(;;){} return 0;}"));
    ExamSessionSubmit(&session,10);Grade(&session.record);
    assert(session.record.tasks[0].tests[0]==EXAM_COMPILE && session.record.tasks[1].tests[0]==EXAM_WRONG);
    assert(session.record.tasks[2].tests[0]==EXAM_RUNTIME && session.record.tasks[3].tests[0]==EXAM_TIMEOUT);
    assert(session.record.percent==0);
    char *after=LoadFileText(progress);assert(after && !strcmp(before,after));UnloadFileText(before);UnloadFileText(after);
    ExamHistoryInit(NULL);assert(!ExamHistorySave(&session.record) && ExamHistoryBest(first.id)==-1);
    ExamSessionFree(&session);LessonsUnload(&set);ProgressLoad("");
    CloseWindow();puts("All coding exam tests passed");return 0;
}
