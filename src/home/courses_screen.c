// Plain ordered lesson rows and optional exam milestones; no progression restrictions.
#include "home/courses_screen.h"
#include "learning/checkpoints.h"
#include "learning/roadmap.h"
#include "learning/progress.h"
#include "learning/exam_history.h"
#include "ui/ui.h"
#include "ui/layout.h"
static Track track;static float scroll,height;
void CoursesScreenOpen(void) {track=TRACK_C;scroll=height=0;}
CoursesAction CoursesScreen(const LessonSet *set) {
    CoursesAction action={-1,-1,track,false};if(TopBar("Courses")) {action.back=true;return action;}
    float x=ContentX(),w=ContentWidth();
    for(int i=0;i<2;i++) if(UiButtonEx((Rectangle){x+i*(w+12)/2,78,(w-12)/2,42},i ? "C++ Course" : "C Course",21,
        track==(Track)i ? COL_ACCENT : COL_PANEL,COL_SECONDARY_HOVER,track==(Track)i ? COL_ON_ACCENT : COL_TEXT,true)) {track=(Track)i;scroll=height=0;}
    action.track=track;Rectangle view={0,136,(float)GetScreenWidth(),GetScreenHeight()-136.0f};
    UpdateScroll(&scroll,height,view.height);BeginView(view);float top=view.y+16-scroll,y=top;int number=0;
    for(int i=0;i<set->count;i++) if(set->items[i].track==track) {
        const Lesson *lesson=&set->items[i];number++;bool open=CheckpointLessonOpen(set,i),complete=RoadmapLevelComplete(lesson);
        float titleH=UiWrappedText(gFonts.text,lesson->title,(Vector2){0,0},w-175,22*UiTextScale(),COL_TEXT,false);
        float h=titleH+60;UiRoundRect((Rectangle){x,y,w,h},10,COL_PANEL);
        UiParagraph(TextFormat("%d. %s",number,lesson->title),x+16,y+12,w-185,22*UiTextScale(),open ? COL_TEXT : COL_DIM);
        UiText(gFonts.text,!RoadmapLevelReady(lesson) ? "Coming soon" : complete ? "Completed" : ProgressLevelStarted(lesson->id) || RoadmapStepsDone(lesson)>0 ? "In progress" : "Not started",x+16,y+h-28,17,COL_DIM);
        if(UiButtonIcon((Rectangle){x+w-146,y+12,130,42},ICON_PLAY,"Open",open)) action.lesson=i;
        y+=h+12;int section=CheckpointAfter(set,i);Checkpoint exam;
        if(section>=0 && CheckpointGet(set,track,section,&exam)) {
            if(UiButtonIconEx((Rectangle){x+16,y,w-32,52},CheckpointPassed(&exam) ? ICON_TROPHY : ICON_CODE,
                TextFormat("Exam %d | %s",section+1,CheckpointPassed(&exam) ? "Passed - Review / Retake" : ExamHistoryBest(exam.id)>=0 ? "Attempted - Retry anytime" : "Not started - Start anytime"),20,
                COL_PANEL_HI,COL_SECONDARY_HOVER,COL_ACCENT,true)) action.exam=section;
            y+=68;
        }
    }
    height=y-top+24;EndView();PageScrollBar(view,&scroll,height);return action;
}
