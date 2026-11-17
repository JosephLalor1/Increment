// Curriculum grouping is data-driven, never a second hardcoded list of lesson ids.
#include "learning/checkpoints.h"
#include "learning/exam_history.h"
#include "learning/roadmap.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int Group(int remaining) { return remaining<=5 ? remaining : remaining==10 ? 5 : 4; }
static int Ready(const LessonSet *set,Track track) {
    int n=0; for(int i=0;i<set->count;i++) if(set->items[i].track==track && RoadmapLevelReady(&set->items[i])) n++; return n;
}
int CheckpointCount(const LessonSet *set,Track track) {
    if(!set || track<0 || track>=TRACK_COUNT) return 0;
    int n=Ready(set,track),groups=0; while(n) { n-=Group(n); groups++; } return groups;
}
bool CheckpointGet(const LessonSet *set,Track track,int section,Checkpoint *out) {
    if(!out || section<0 || section>=CheckpointCount(set,track)) return false;
    memset(out,0,sizeof *out); int remaining=Ready(set,track),offset=0;
    for(int s=0;s<section;s++) { int n=Group(remaining); offset+=n; remaining-=n; }
    out->count=Group(remaining); out->track=track; out->section=section;
    out->duration=EXAM_DURATION_SECONDS; out->passPercent=EXAM_PASS_PERCENT;
    uint32_t hash=2166136261u; int ordinal=0,found=0;
    for(int i=0;i<set->count;i++) if(set->items[i].track==track && RoadmapLevelReady(&set->items[i])) {
        if(ordinal>=offset && found<out->count) {
            const Lesson *lesson=&set->items[i]; out->lessons[found]=i; out->steps[found++]=lesson->stepCount-1;
            for(const unsigned char *p=(const unsigned char *)lesson->id;*p;p++) hash=(hash^*p)*16777619u;
            hash=(hash^0xffu)*16777619u;
        }
        ordinal++;
    }
    snprintf(out->id,sizeof out->id,"%s_%08x",track==TRACK_C ? "c" : "cpp",(unsigned)hash);
    snprintf(out->title,sizeof out->title,"%s Exam %d: %.90s / %.90s",TrackName(track),section+1,
             set->items[out->lessons[0]].title,set->items[out->lessons[out->count-1]].title);
    return true;
}
bool CheckpointPassed(const Checkpoint *exam) { return ExamHistoryBest(exam->id)>=exam->passPercent; }
int CheckpointPending(const LessonSet *set,Track track) {
    Checkpoint exam;for(int s=0;CheckpointGet(set,track,s,&exam);s++) if(!CheckpointPassed(&exam)) return s;
    return -1;
}
bool CheckpointAvailable(const LessonSet *set,const Checkpoint *exam) {
    // Availability validates content only; completion and scores never restrict access.
    if(!set || !exam || exam->track<0 || exam->track>=TRACK_COUNT || exam->count<1 || exam->count>EXAM_MAX_TASKS) return false;
    for(int i=0;i<exam->count;i++) {
        int index=exam->lessons[i];
        if(index<0 || index>=set->count) return false;
        const Lesson *lesson=&set->items[index];
        if(lesson->track!=exam->track || exam->steps[i]<0 || exam->steps[i]>=lesson->stepCount) return false;
    }
    return true;
}
int CheckpointAfter(const LessonSet *set,int lesson) {
    if(!set || lesson<0 || lesson>=set->count) return -1;
    Checkpoint exam; Track track=set->items[lesson].track;
    for(int s=0;CheckpointGet(set,track,s,&exam);s++) if(exam.lessons[exam.count-1]==lesson) return s;
    return -1;
}
bool CheckpointLessonOpen(const LessonSet *set,int lesson) {
    return set && lesson>=0 && lesson<set->count && RoadmapLevelReady(&set->items[lesson]);
}
