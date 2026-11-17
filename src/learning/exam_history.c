// Length-prefixed snapshots keep submitted code, reference and feedback together per attempt.
#include "learning/exam_history.h"
#include "core/platform.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
static char folder[1100];
static unsigned serial;
static void Forget(void);
static bool Bytes(FILE *f,void *p,size_t n,bool write) { return (write ? fwrite(p,1,n,f) : fread(p,1,n,f))==n; }
static bool Number(FILE *f,int *v,bool write) { int32_t n=*v; if(!Bytes(f,&n,sizeof n,write)) return false; if(!write) *v=n; return true; }
static bool String(FILE *f,char **s,bool write) {
    int n=write ? (int)strlen(*s ? *s : "") : 0;
    if(!Number(f,&n,write) || n<0 || n>EXAM_STRING_LIMIT) return false;
    if(write) return n==0 || Bytes(f,*s,(size_t)n,true);
    *s=calloc((size_t)n+1,1); return *s && Bytes(f,*s,(size_t)n,false) && !memchr(*s,0,(size_t)n);
}
// Version 2 adds each task's extra test cases and a result per test (up to EXAM_MAX_TESTS);
// version 1 attempts (one case, two results) still read. *version says which was read.
static bool Header(FILE *f,ExamRecord *r,bool write,int *version) {
    char magic[8]="INCEX02";
    if(!Bytes(f,magic,8,write)) return false;
    if(!memcmp(magic,"INCEX02",8)) *version=2;
    else if(!memcmp(magic,"INCEX01",8)) *version=1;
    else return false;
    int track=r->track, interrupted=r->interrupted;
    bool ok=Bytes(f,r->id,sizeof r->id,write) && Bytes(f,r->title,sizeof r->title,write)
        && Number(f,&track,write) && Number(f,&r->count,write) && Number(f,&r->passPercent,write)
        && Number(f,&r->duration,write) && Number(f,&r->elapsed,write) && Number(f,&r->percent,write)
        && Bytes(f,&r->timestamp,sizeof r->timestamp,write) && Number(f,&interrupted,write);
    r->track=(Track)track; r->interrupted=interrupted!=0;
    return ok && memchr(r->id,0,sizeof r->id) && memchr(r->title,0,sizeof r->title) && r->id[0]
        && track>=0 && track<TRACK_COUNT && r->count>0 && r->count<=EXAM_MAX_TASKS && r->percent>=0 && r->percent<=100
        && r->passPercent>0 && r->passPercent<=100 && r->duration>0 && r->elapsed>=0 && r->elapsed<=r->duration;
}
static bool Tasks(FILE *f,ExamRecord *r,bool write,int version) {
    int maxTests=version>=2 ? EXAM_MAX_TESTS : 2;
    for(int i=0;i<r->count;i++) {
        ExamTask *t=&r->tasks[i];
        if(!Bytes(f,t->lessonId,sizeof t->lessonId,write) || !Bytes(f,t->title,sizeof t->title,write) || !Number(f,&t->step,write)
           || !memchr(t->lessonId,0,sizeof t->lessonId) || !memchr(t->title,0,sizeof t->title)) return false;
        for(int s=0;s<SKILL_COUNT;s++) { int present=t->skills[s]; if(!Number(f,&present,write)) return false; t->skills[s]=present!=0; }
        if(!Bytes(f,t->weights,sizeof t->weights,write) || !Number(f,&t->difficulty,write)) return false;
        for(int s=0;s<SKILL_COUNT;s++) if(!isfinite(t->weights[s]) || t->weights[s]<0 || t->weights[s]>1) return false;
        int fixed=t->fixedValues; if(!Number(f,&fixed,write)) return false; t->fixedValues=fixed!=0;
        if(!Number(f,&t->testCount,write) || !Number(f,&t->passed,write) || t->testCount<1 || t->testCount>maxTests || t->passed<0 || t->passed>t->testCount) return false;
        for(int j=0;j<maxTests;j++) { int status=t->tests[j]; if(!Number(f,&status,write) || status<EXAM_UNTESTED || status>EXAM_INFRASTRUCTURE) return false; t->tests[j]=(ExamTest)status; }
        char **strings[]={&t->code,&t->starter,&t->input,&t->expected,&t->solution,&t->diagnostics,&t->output,&t->detail};
        for(unsigned j=0;j<sizeof strings/sizeof *strings;j++) if(!String(f,strings[j],write)) return false;
        if(version<2) continue;
        if(!Number(f,&t->extraCount,write) || t->extraCount<0 || t->extraCount>EXAM_EXTRA_CASES) return false;
        for(int j=0;j<t->extraCount;j++) if(!String(f,&t->extraInputs[j],write) || !String(f,&t->extraExpected[j],write)) return false;
    }
    return true;
}
void ExamRecordFree(ExamRecord *r) {
    for(int i=0;i<EXAM_MAX_TASKS;i++) { ExamTask *t=&r->tasks[i];
        free(t->code);free(t->starter);free(t->input);free(t->expected);free(t->solution);free(t->diagnostics);free(t->output);free(t->detail);
        for(int j=0;j<EXAM_EXTRA_CASES;j++) { free(t->extraInputs[j]);free(t->extraExpected[j]); }
    } memset(r,0,sizeof *r);
}
bool ExamHistoryRead(const char *path,ExamRecord *r) {
    memset(r,0,sizeof *r); FILE *f=fopen(path,"rb"); if(!f) return false;
    int version=0;
    bool ok=Header(f,r,false,&version) && Tasks(f,r,false,version) && fgetc(f)==EOF && !ferror(f); fclose(f);
    if(!ok) ExamRecordFree(r);
    return ok;
}
void ExamHistoryInit(const char *codeFolder) {
    Forget(); folder[0]=0; if(codeFolder && *codeFolder) snprintf(folder,sizeof folder,"%s/exams",codeFolder);
}
static int Recent(const void *a,const void *b) { const ExamHistoryItem *x=a,*y=b;
    if(x->timestamp!=y->timestamp) return x->timestamp>y->timestamp ? -1 : 1;
    return strcmp(y->file,x->file); }
ExamHistoryItem *ExamHistoryList(const char *id,int *count) {
    *count=0; if(!folder[0] || !DirectoryExists(folder)) return NULL;
    FilePathList files=LoadDirectoryFilesEx(folder,".exam",false);
    ExamHistoryItem *items=calloc(files.count ? files.count : 1,sizeof *items);
    if(items) for(unsigned i=0;i<files.count;i++) {
        ExamRecord r={0}; bool ok=ExamHistoryRead(files.paths[i],&r);
        if(ok && !strcmp(r.id,id)) { ExamHistoryItem *item=&items[(*count)++];
            snprintf(item->file,sizeof item->file,"%s",files.paths[i]); item->percent=r.percent; item->elapsed=r.elapsed;item->timestamp=r.timestamp;
        }
        ExamRecordFree(&r);
    }
    UnloadDirectoryFiles(files); if(items) qsort(items,*count,sizeof *items,Recent); return items;
}
// A tiny cache makes drawing all checkpoint nodes independent of disk size.
typedef struct { char id[64]; int best; } Best;
static Best cache[128]; static int cacheCount;
static void Forget(void) { cacheCount=0; }
int ExamHistoryBest(const char *id) {
    for(int i=0;i<cacheCount;i++) if(!strcmp(cache[i].id,id)) return cache[i].best;
    int count,best=-1; ExamHistoryItem *items=ExamHistoryList(id,&count);
    for(int i=0;i<count;i++) if(items[i].percent>best) best=items[i].percent;
    free(items);
    if(cacheCount<128) { snprintf(cache[cacheCount].id,64,"%s",id); cache[cacheCount++].best=best; } return best;
}
bool ExamHistorySave(ExamRecord *r) {
    if(!folder[0] || (!DirectoryExists(folder) && MakeDirectory(folder)!=0)) return false;
    char path[1200],temp[1220]; r->timestamp=(int64_t)time(NULL);
    uint64_t nonce;
    if(!PlatformRandom((unsigned char *)&nonce,sizeof nonce)) return false;
    do { snprintf(path,sizeof path,"%s/%lld_%08u_%016llx.exam",folder,(long long)r->timestamp,++serial,(unsigned long long)nonce); } while(FileExists(path));
    snprintf(temp,sizeof temp,"%s.tmp",path); FILE *f=fopen(temp,"wb"); if(!f) return false;
    int version=2;
    bool ok=Header(f,r,true,&version) && Tasks(f,r,true,version); if(fclose(f)!=0) ok=false;
    if(ok) ok=PlatformReplaceFile(temp,path);
    if(!ok) remove(temp); else Forget();
    return ok;
}
