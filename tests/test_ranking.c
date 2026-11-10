// Local ranking rules, defensive reads, public roster and signed-in state isolation.
#include "../src/ranking.h"
#include "../src/progress.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Write(const char *path, const char *text)
{
    FILE *file = fopen(path,"w"); assert(file);
    fputs(text,file); assert(fclose(file)==0);
}

int main(int argc, char **argv)
{
    assert(argc==2);
    SetTraceLogLevel(LOG_ERROR);
    char directory[1200];
    snprintf(directory,sizeof directory,"%s/ranking_accounts/",argv[1]);
    assert(MakeDirectory(directory)==0);
    AccountsInit(directory);
    const char *error = NULL;
    char recovery[RECOVERY_CODE_SIZE];
    assert(AccountCreate("Alice","password",recovery,&error));
    assert(AccountCreate("Bob","password",recovery,&error));
    assert(AccountCreate("Cara","password",recovery,&error));
    assert(AccountCount()==3);
    char paths[3][1100];
    RankingEntry rows[3] = {0};
    for (int i=0;i<3;i++) assert(AccountRankingProfile(i,rows[i].name,sizeof rows[i].name,paths[i],sizeof paths[i]));
    assert(!strcmp(rows[0].name,"Alice") && strstr(paths[0],"/alice.txt"));
    char name[21], path[1100];
    assert(!AccountRankingProfile(-1,name,sizeof name,path,sizeof path));
    assert(!AccountRankingProfile(3,name,sizeof name,path,sizeof path));
    assert(!AccountRankingProfile(0,name,1,path,sizeof path));
    assert(!AccountRankingProfile(0,NULL,0,path,sizeof path));
    assert(!AccountRankingProfile(0,name,sizeof name,path,1));
    LessonSet set = {calloc(3,sizeof(Lesson)),3}; assert(set.items);
    snprintf(set.items[0].id,sizeof set.items[0].id,"c_a");
    set.items[0].track=TRACK_C; set.items[0].questionCount=1;
    snprintf(set.items[1].id,sizeof set.items[1].id,"c_b");
    set.items[1].track=TRACK_C; // a reading-only lesson counts for completion but has no quiz points
    snprintf(set.items[2].id,sizeof set.items[2].id,"cpp_a");
    set.items[2].track=TRACK_CPP; set.items[2].questionCount=1;
    Write(paths[0],"c_a 80\nc_a 90\ncpp_a 70\nc_b 100\nremoved_lesson 100\nc_a#1 100\nc_a@trace:x 100\nc_a!gold 100\n");
    Write(paths[1],"c_a 100\ncpp_a 100\n");
    ProgressLoad(paths[2]);
    ProgressRecord("sentinel",42);
    char *before=LoadFileText(paths[0]), *currentBefore=LoadFileText(paths[2]); assert(before && currentBefore);
    for(int i=0;i<3;i++) RankingRead(&rows[i],&set,TRACK_COUNT,paths[i]);
    assert(rows[0].completed==3 && rows[0].quizPoints==160 && !rows[0].warning);
    assert(rows[1].completed==2 && rows[1].quizPoints==200);
    assert(rows[2].completed==0 && rows[2].quizPoints==0);
    RankingSort(rows,3);
    assert(!strcmp(rows[0].name,"Alice") && rows[0].rank==1 && rows[1].rank==2 && rows[2].rank==3);
    RankingRead(&rows[0],&set,TRACK_C,paths[0]);
    assert(rows[0].completed==2 && rows[0].quizPoints==90);
    RankingRead(&rows[0],&set,TRACK_CPP,paths[0]);
    assert(rows[0].completed==1 && rows[0].quizPoints==70);
    RankingRead(&rows[0],&set,TRACK_COUNT,"no-such-ranking-file.txt");
    assert(!rows[0].warning && rows[0].completed==0 && rows[0].quizPoints==0);
    RankingEntry ties[] = {{"zoe",1,100},{"Amy",1,100},{"Ben",1,99},{"Dan",0,200}};
    RankingSort(ties,4);
    assert(!strcmp(ties[0].name,"Amy") && ties[0].rank==1 && ties[1].rank==1 && ties[2].rank==3 && ties[3].rank==4);
    RankingSort(ties,1); assert(ties[0].rank==1);
    RankingSort(NULL,0);
    char damaged[1300]; snprintf(damaged,sizeof damaged,"%sdamaged.txt",directory);
    Write(damaged,"c_a 1000\nc_a -1\nc_a 70bad\nc_a 9999999999999999999999999999999\nc_a 100 extra\n\nc_a 69\ncpp_a 70\n");
    RankingRead(&rows[0],&set,TRACK_COUNT,damaged);
    assert(rows[0].warning && rows[0].completed==1 && rows[0].quizPoints==139);
    FILE *file=fopen(damaged,"w"); assert(file);
    for(int i=0;i<300;i++) fputc('x',file);
    fputs("\nc_a 100\n",file); fclose(file);
    RankingRead(&rows[0],&set,TRACK_COUNT,damaged);
    assert(rows[0].warning && rows[0].completed==1 && rows[0].quizPoints==100);
    LessonSet empty={0}; RankingRead(&rows[0],&empty,TRACK_COUNT,paths[0]);
    assert(!rows[0].warning && rows[0].completed==0 && rows[0].quizPoints==0);
    assert(!strcmp(AccountCurrent(),"Cara") && !strcmp(AccountProgressPath(),paths[2]));
    assert(ProgressBest("sentinel")==42 && ProgressBest("c_a")==-1);
    char *after=LoadFileText(paths[0]), *currentAfter=LoadFileText(paths[2]);
    assert(after && currentAfter && !strcmp(before,after) && !strcmp(currentBefore,currentAfter));
    UnloadFileText(before); UnloadFileText(after); UnloadFileText(currentBefore); UnloadFileText(currentAfter);
    assert(AccountDelete("password",&error) && AccountCount()==2);
    assert(!AccountRankingProfile(2,name,sizeof name,path,sizeof path));
    ProgressLoad(""); free(set.items);
    puts("All local user ranking tests passed");
    return 0;
}
