// Existing EditorWidget and Runner power exams; no hints/solutions are drawn while active.
#include "level/exam_screen.h"
#include "level/exam_grader.h"
#include "learning/roadmap.h"
#include "code/editor.h"
#include "core/text_copy.h"
#include "ui/ui.h"
#include "ui/layout.h"
#include "ui/mascot.h"
#include <stdio.h>
#include <string.h>
typedef enum { EXAM_INTRO,EXAM_ACTIVE,EXAM_GRADING,EXAM_RESULTS } ExamPage;
static struct {
    const LessonSet *set; Checkpoint definition; ExamSession session; ExamGrader grader;
    ExamHistoryItem *history; int historyCount,task;
    ExamPage page; Editor code,output; Runner preview; bool ready,saved,demo,showSolution,confirmFinish;
    float scroll,height; char error[180]; Lesson practice;
    unsigned snapshotVersion;
} screen;
static void Editors(void) {
    if(screen.ready) return;
    EditorInit(&screen.code,"",true);EditorInit(&screen.output,"",false);screen.output.readOnly=true;screen.ready=true;
}
static void History(void) { free(screen.history);screen.history=ExamHistoryList(screen.definition.id,&screen.historyCount); }
static void Task(int task) {
    RunnerFree(&screen.preview); // output/diagnostics belong to this task, never the previous one
    screen.task=task;screen.scroll=screen.height=0;screen.showSolution=false;
    EditorSetText(&screen.code,screen.session.record.tasks[task].code);screen.code.fontSize=18*UiTextScale();
    screen.snapshotVersion=screen.code.version;
    screen.code.readOnly=screen.page==EXAM_RESULTS;
    EditorSetText(&screen.output,"");UiFocus(&screen.code);
}
static bool Commit(void) {
    if(ExamSessionCode(&screen.session,screen.task,screen.code.text)) return true;
    snprintf(screen.error,sizeof screen.error,"Could not snapshot code. Maximum source size: 512 KiB per task.");return false;
}
static void Save(void) {
    screen.saved=screen.demo || ExamHistorySave(&screen.session.record);
    if(!screen.saved) snprintf(screen.error,sizeof screen.error,"Could not save this attempt. Check your data folder, then Retry Save.");
    else { screen.error[0]=0;History(); }
}
static void Submit(void) {
    if(!Commit()) return;
    if(!ExamSessionSubmit(&screen.session,GetTime())) return;
    RunnerCancel(&screen.preview);screen.page=EXAM_GRADING;screen.confirmFinish=false;
    ExamGraderStart(&screen.grader,&screen.session.record);UiFocus(NULL);
}
void ExamScreenShutdown(void) {
    if(screen.page==EXAM_ACTIVE || screen.page==EXAM_GRADING) {
        if(screen.page==EXAM_ACTIVE) { Commit();ExamSessionSubmit(&screen.session,GetTime()); }
        screen.session.record.interrupted=true;screen.session.record.percent=0;
        for(int i=0;i<screen.session.record.count;i++) {
            ExamTask *task=&screen.session.record.tasks[i];task->passed=0;task->tests[0]=task->tests[1]=EXAM_UNTESTED;
            free(task->detail);task->detail=CopyText("Interrupted before the final assessment completed. Submission retained for review.");
        }
        if(!screen.demo && !ExamHistorySave(&screen.session.record)) TraceLog(LOG_WARNING,"EXAM: interrupted attempt could not be saved");
    }
    if(screen.page==EXAM_RESULTS && !screen.saved && !screen.demo && screen.session.record.count)
        if(!ExamHistorySave(&screen.session.record)) TraceLog(LOG_WARNING,"EXAM: final attempt could not be saved on exit");
    ExamGraderFree(&screen.grader);RunnerFree(&screen.preview);ExamSessionFree(&screen.session);free(screen.history);
    if(screen.ready) { EditorFree(&screen.code);EditorFree(&screen.output); }
    memset(&screen,0,sizeof screen);
}
void ExamScreenOpen(const LessonSet *set,Track track,int section) {
    ExamScreenShutdown();screen.set=set;CheckpointGet(set,track,section,&screen.definition);Editors();History();
}
static float Text(const char *s,float x,float y,float w,float size,Color color) { return UiParagraph(s,x,y,w,size,color); }
static float Code(const char *s,float x,float y,float w) { return UiCodeBlock(s ? s : "",(Vector2){x,y},w,17*UiTextScale(),true); }
static ExamScreenAction Intro(void) {
    ExamScreenAction action={0};if(TopBar("Coding Exam Checkpoint")) {action.kind=EXAM_SCREEN_HOME;return action;}
    Rectangle view=ScrollView();UpdateScroll(&screen.scroll,screen.height,view.height);BeginView(view);
    float x=ContentX(),w=ContentWidth(),top=view.y+24-screen.scroll,y=top;Checkpoint *exam=&screen.definition;
    y+=Text(exam->title,x,y,w,28*UiTextScale(),COL_TEXT)+14;
    y+=Text(TextFormat("%d coding tasks | %d minutes | Pass: %d%%",exam->count,exam->duration/60,exam->passPercent),x,y,w,21,COL_ACCENT)+14;
    y+=Text("Closed book: no notes, hints or reference solutions. Finish or timeout freezes your code; assessment then runs in the background. Each task has equal weight.",x,y,w,18,COL_DIM)+18;
    for(int i=0;i<exam->count;i++) {
        const Lesson *lesson=&screen.set->items[exam->lessons[i]];
        y+=Text(TextFormat("%s  %s",RoadmapLevelComplete(lesson) ? "[completed]" : "[covered topic]",lesson->title),x,y,w,21,COL_TEXT)+10;
    }
    bool available=CheckpointAvailable(screen.set,exam);
    y+=Text("Optional milestone: take this exam at any time. Your score never restricts lessons or other exams.",x,y,w,18,COL_DIM)+14;
    if(UiButtonIcon((Rectangle){x,y,260,48},ICON_PLAY,CheckpointPassed(exam) ? "Retake Exam" : "Start Exam",available && exam->count>0)) {
        if(ExamSessionStart(&screen.session,screen.set,exam,GetTime())) {screen.page=EXAM_ACTIVE;screen.saved=false;Task(0);}
        else snprintf(screen.error,sizeof screen.error,"Could not initialise the exam tasks.");
    }
    y+=68;
    y+=MascotCompanionActor(FOX_JOURNEY,x,y,w,76,18,FOX_DETERMINED,available ? "Ready to show what you can code?" : "One level at a time. Your next milestone is waiting!")+18;
    y+=Text("Saved attempts",x,y,w,24,COL_TEXT)+12;
    if(!screen.historyCount) y+=Text("No attempts yet.",x,y,w,18,COL_DIM)+16;
    for(int i=0;i<screen.historyCount;i++) {
        ExamHistoryItem *item=&screen.history[i];
        if(UiButtonIconEx((Rectangle){x,y,w,44},ICON_BOOK,TextFormat("Attempt %d | %d%% | %d:%02d | Review",screen.historyCount-i,item->percent,item->elapsed/60,item->elapsed%60),18,COL_PANEL,COL_SECONDARY_HOVER,COL_TEXT,true)) {
            ExamRecordFree(&screen.session.record);
            if(ExamHistoryRead(item->file,&screen.session.record)) {screen.page=EXAM_RESULTS;screen.saved=true;screen.session.active=screen.session.submitted=false;Task(0);}
            else snprintf(screen.error,sizeof screen.error,"This saved attempt is unreadable.");
        }
        y+=56;
    }
    if(screen.error[0]) y+=Text(screen.error,x,y,w,18,COL_BAD)+12;
    screen.height=y-top+30;EndView();PageScrollBar(view,&screen.scroll,screen.height);return action;
}
static ExamScreenAction Active(void) {
    ExamScreenAction action={0};
    // Deadline is checked before drawing an editable widget, so a late frame cannot add code.
    if(!ExamSessionRemaining(&screen.session,GetTime())) {Submit();return action;}
    if(TopBar("Timed Coding Exam")) screen.confirmFinish=true;
    int remaining=ExamSessionRemaining(&screen.session,GetTime());
    UiText(gFonts.bold,TextFormat("%02d:%02d",remaining/60,remaining%60),GetScreenWidth()-128.0f,20,25,remaining<60 ? COL_BAD : COL_ACCENT);
    RunnerUpdate(&screen.preview);float W=GetScreenWidth(),H=GetScreenHeight(),x=16;
    if(UiButtonIcon((Rectangle){x,76,140,42},ICON_PLAY,"Run code",!RunnerBusy(&screen.preview))) RunnerStart(&screen.preview,screen.code.text,screen.definition.track==TRACK_CPP,screen.session.record.tasks[screen.task].input);
    if(UiButtonIcon((Rectangle){168,76,140,42},ICON_BACK,"Previous",screen.task>0) && Commit()) {RunnerCancel(&screen.preview);Task(screen.task-1);}
    if(UiButtonIcon((Rectangle){320,76,140,42},ICON_NEXT,"Next task",screen.task+1<screen.session.record.count) && Commit()) {RunnerCancel(&screen.preview);Task(screen.task+1);}
    if(UiButtonIcon((Rectangle){W-196,76,180,42},ICON_CHECK,"Finish Exam",true)) screen.confirmFinish=true;
    if(screen.confirmFinish) {
        UiRoundRect((Rectangle){16,130,W-32,90},10,COL_PANEL);
        UiText(gFonts.text,"Submit all tasks and finish?",30,140,20,COL_TEXT);
        if(UiButtonIcon((Rectangle){30,170,200,40},ICON_CHECK,"Submit now",true)) Submit();
        if(UiButtonIcon((Rectangle){242,170,180,40},ICON_BACK,"Keep coding",true)) screen.confirmFinish=false;
        return action;
    }
    float codeW=(W-48)*0.58f,rx=32+codeW,rw=W-rx-16;
    EditorWidget(&screen.code,(Rectangle){16,130,codeW,H-146});
    if(screen.code.version!=screen.snapshotVersion) {
        if(!Commit()) EditorSetText(&screen.code,screen.session.record.tasks[screen.task].code);
        screen.snapshotVersion=screen.code.version;
    }
    ExamTask *task=&screen.session.record.tasks[screen.task];
    Rectangle details={rx,130,rw,H-146};UpdateScroll(&screen.scroll,screen.height,details.height);BeginView(details);
    float top=details.y-screen.scroll,y=top;
    y+=Text(TextFormat("Task %d / %d: %s",screen.task+1,screen.session.record.count,task->title),rx,y,rw,22,COL_TEXT)+16;
    y+=Text("Input",rx,y,rw,19,COL_DIM)+6;y+=Code(task->input,rx,y,rw)+12;
    y+=Text("Target output",rx,y,rw,19,COL_DIM)+6;y+=Code(task->expected,rx,y,rw)+12;
    y+=Text(RunnerBusy(&screen.preview) ? "Compiling / running..." : screen.preview.summary,rx,y,rw,18,COL_ACCENT)+8;
    if(screen.preview.compileOutput) y+=Code(screen.preview.compileOutput,rx,y,rw)+10;
    if(screen.preview.programOutput) y+=Code(screen.preview.programOutput,rx,y,rw)+10;
    if(screen.error[0]) y+=Text(screen.error,rx,y,rw,18,COL_BAD)+10;
    screen.height=y-top+24;EndView();PageScrollBar(details,&screen.scroll,screen.height);return action;
}
static ExamScreenAction Results(void) {
    ExamScreenAction action={0};
    if(TopBar("Exam Results & Revision") && screen.saved) {action.kind=EXAM_SCREEN_HOME;return action;}
    Rectangle view=ScrollView();UpdateScroll(&screen.scroll,screen.height,view.height);BeginView(view);
    float x=ContentX(),w=ContentWidth(),top=view.y+24-screen.scroll,y=top;ExamRecord *record=&screen.session.record;
    int completed=0;for(int i=0;i<record->count;i++) completed+=record->tasks[i].passed==record->tasks[i].testCount;
    double marks=0;for(int i=0;i<record->count;i++) marks+=100.0*record->tasks[i].passed/record->tasks[i].testCount;
    y+=Text(TextFormat("%d%% | %s | Pass mark %d%%",record->percent,record->percent>=record->passPercent ? "PASSED" : "RETRY",record->passPercent),x,y,w,32,COL_ACCENT)+12;
    y+=Text(TextFormat("%d / %d tasks passed | Time %d:%02d%s",completed,record->count,record->elapsed/60,record->elapsed%60,record->interrupted ? " | Interrupted" : ""),x,y,w,20,COL_TEXT)+12;
    y+=Text(TextFormat("Total marks: %.0f / %d",marks,100*record->count),x,y,w,20,COL_TEXT)+12;
    if(screen.error[0]) y+=Text(screen.error,x,y,w,18,COL_BAD)+12;
    if(!screen.saved && UiButtonIcon((Rectangle){x,y,200,44},ICON_RETRY,"Retry Save",true)) Save();
    if(!screen.saved) y+=58;
    if(UiButtonIcon((Rectangle){x,y,240,44},ICON_BACK,"Return to Dashboard",screen.saved)) action.kind=EXAM_SCREEN_HOME;
    if(UiButtonIcon((Rectangle){x+252,y,200,44},ICON_RETRY,"Retake Exam",screen.saved)) {screen.page=EXAM_INTRO;screen.scroll=screen.height=0;}
    y+=64;
    for(int i=0;i<record->count;i++) {
        ExamTask *task=&record->tasks[i];
        y+=Text(TextFormat("Task %d: %s | %d / %d tests | %.0f / 100 marks",i+1,task->title,task->passed,task->testCount,100.0*task->passed/task->testCount),x,y,w,23,COL_TEXT)+10;
        for(int j=0;j<task->testCount;j++) y+=Text(TextFormat("Test %d: %s",j+1,ExamTestName(task->tests[j])),x,y,w,18,task->tests[j]==EXAM_TEST_PASS ? COL_GOOD : COL_BAD)+8;
        if(UiButtonIconEx((Rectangle){x,y,260,42},ICON_BOOK,screen.task==i && screen.showSolution ? "Hide code / solution" : "Review code / solution",18,COL_PANEL,COL_SECONDARY_HOVER,COL_TEXT,true)) {
            if(screen.task==i) screen.showSolution=!screen.showSolution;else {screen.task=i;screen.showSolution=true;}
        }
        if(UiButtonIconEx((Rectangle){x+272,y,220,42},ICON_CODE,"Retry in Learning Mode",17,COL_PANEL,COL_SECONDARY_HOVER,COL_TEXT,screen.saved)) {
            memset(&screen.practice,0,sizeof screen.practice);snprintf(screen.practice.title,sizeof screen.practice.title,"%.80s",task->title);
            screen.practice.track=record->track;screen.practice.stepCount=1;screen.practice.steps[0]=ExamTaskStep(task);
            action.kind=EXAM_SCREEN_PRACTICE;action.practice=&screen.practice;
        }
        y+=54;
        if(screen.task==i && screen.showSolution) {
            y+=Text("Submitted code",x,y,w,21,COL_TEXT)+8;y+=Code(task->code,x,y,w)+12;
            y+=Text("Target output",x,y,w,20,COL_DIM)+8;y+=Code(task->expected,x,y,w)+12;
            y+=Text("Actual output / compiler messages",x,y,w,20,COL_DIM)+8;y+=Code(task->output,x,y,w)+8;
            y+=Text(task->diagnostics ? task->diagnostics : "",x,y,w,18,COL_DIM)+12;
            y+=Text("Assessment explanation",x,y,w,20,COL_TEXT)+8;y+=Code(task->detail,x,y,w)+12;
            y+=Text("Reference solution (compare with your submission)",x,y,w,21,COL_TEXT)+8;y+=Code(task->solution,x,y,w)+18;
            y+=Text("Reference concepts",x,y,w,20,COL_TEXT)+8;
            for(int s=0;s<SKILL_COUNT;s++) if(task->skills[s]) y+=Text(SkillTitle((Skill)s),x+16,y,w-16,18,COL_DIM)+5;
            y+=Text("The reference produces the target output from the given input. Compare its code and concepts with the submitted code; the assessment report above identifies failed outputs, changed values, or missing required code.",x,y,w,18,COL_DIM)+18;
        }
        if(task->passed<task->testCount) {
            y+=Text("Recommended revision (task concepts)",x,y,w,18,COL_ACCENT)+8;
            for(int s=0;s<SKILL_COUNT;s++) if(task->skills[s]) y+=Text(SkillTitle((Skill)s),x+16,y,w-16,18,COL_DIM)+5;
        }
        y+=20;
    }
    screen.height=y-top+30;EndView();PageScrollBar(view,&screen.scroll,screen.height);return action;
}
ExamScreenAction ExamScreen(void) {
    if(screen.page==EXAM_INTRO) return Intro();
    if(screen.page==EXAM_ACTIVE) return Active();
    if(screen.page==EXAM_RESULTS) return Results();
    ExamGraderTick(&screen.grader);
    TopBar("Assessing frozen submission...");
    UiText(gFonts.text,TextFormat("Task %d / %d",screen.grader.task+1,screen.session.record.count),40,100,24,COL_ACCENT);
    if(!screen.grader.busy) {screen.page=EXAM_RESULTS;screen.scroll=screen.height=0;Save();}
    return (ExamScreenAction){0};
}
void ExamScreenDemo(int page) {
    screen.demo=true;
    if(page<=0) return;
    if(!ExamSessionStart(&screen.session,screen.set,&screen.definition,GetTime())) return;
    screen.page=EXAM_ACTIVE;Task(0);
    if(page>=2) {
        ExamSessionSubmit(&screen.session,GetTime());screen.page=EXAM_RESULTS;screen.saved=true;
        for(int i=0;i<screen.session.record.count;i++) {
            ExamTask *task=&screen.session.record.tasks[i];task->passed=i%2 ? 0 : 1;task->tests[0]=i%2 ? EXAM_WRONG : EXAM_TEST_PASS;
        }
        ExamSessionScore(&screen.session.record);
        if(page==3) {screen.showSolution=true;screen.scroll=280;screen.height=100000;}
    }
}
bool ExamScreenBusy(void) {return screen.page==EXAM_GRADING || RunnerBusy(&screen.preview);}
