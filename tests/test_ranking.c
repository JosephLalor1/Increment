// Local ranking rules, defensive reads, public roster and signed-in state isolation.
#include "../src/ranking.h"
#include "../src/ranking_skills.h"
#include "../src/code_library.h"
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
    char folders[3][1100];
    for (int i=0;i<3;i++) {
        assert(AccountRankingCodeFolder(i,folders[i],sizeof folders[i]));
        assert(!DirectoryExists(folders[i])); // reading another account's path must not create a library
    }
    assert(!AccountRankingCodeFolder(-1,path,sizeof path));
    assert(!AccountRankingCodeFolder(3,path,sizeof path));
    assert(!AccountRankingCodeFolder(0,path,1));
    LessonSet set = {calloc(3,sizeof(Lesson)),3}; assert(set.items);
    snprintf(set.items[0].id,sizeof set.items[0].id,"c_a");
    set.items[0].track=TRACK_C; set.items[0].stepCount=2;
    snprintf(set.items[1].id,sizeof set.items[1].id,"c_b");
    set.items[1].track=TRACK_C; set.items[1].stepCount=1;
    snprintf(set.items[2].id,sizeof set.items[2].id,"cpp_a");
    set.items[2].track=TRACK_CPP; set.items[2].stepCount=1;
    // Alice: everything, with a repeated step, a step beyond the level's last, a removed level,
    // gold and lines from older versions (quiz scores, exercises), none of which count.
    Write(paths[0],"c_a/1 100\nc_a/2 100\nc_a/2 100\ncpp_a/1 100\nc_b/1 100\nc_a/3 100\nremoved_lesson/1 100\n"
                   "c_a!gold 100\nc_a 90\nc_a#1 100\n");
    Write(paths[1],"c_a/1 100\ncpp_a/1 100\nc_b/1 100\n");  // Bob: half of c_a
    ProgressLoad(paths[2]);
    ProgressRecordStep("sentinel",0);
    char *before=LoadFileText(paths[0]), *currentBefore=LoadFileText(paths[2]); assert(before && currentBefore);
    for(int i=0;i<3;i++) RankingRead(&rows[i],&set,TRACK_COUNT,paths[i]);
    assert(rows[0].completed==3 && rows[0].stepsPassed==4 && !rows[0].warning);
    assert(rows[1].completed==2 && rows[1].stepsPassed==3);
    assert(rows[2].completed==0 && rows[2].stepsPassed==0);
    RankingSort(rows,3);
    assert(!strcmp(rows[0].name,"Alice") && rows[0].rank==1 && rows[1].rank==2 && rows[2].rank==3);
    RankingRead(&rows[0],&set,TRACK_C,paths[0]);
    assert(rows[0].completed==2 && rows[0].stepsPassed==3);
    RankingRead(&rows[0],&set,TRACK_CPP,paths[0]);
    assert(rows[0].completed==1 && rows[0].stepsPassed==1);
    RankingRead(&rows[0],&set,TRACK_COUNT,"no-such-ranking-file.txt");
    assert(!rows[0].warning && rows[0].completed==0 && rows[0].stepsPassed==0);
    // Real sandbox files: repeated evidence is counted once per concept, per selected track.
    assert(MakeDirectory(folders[0])==0 && MakeDirectory(folders[1])==0);
    assert(CodeLibrarySave(folders[0],"Loop one",false,"for (;;) {}",false)==CODE_SAVE_OK);
    assert(CodeLibrarySave(folders[0],"Loop two",false,"while (1) {}",false)==CODE_SAVE_OK);
    assert(CodeLibrarySave(folders[0],"Nothing",false,"// for class int x[3]",false)==CODE_SAVE_OK);
    assert(CodeLibrarySave(folders[0],"Objects",true,"class Box {public: int x;};",false)==CODE_SAVE_OK);
    assert(CodeLibrarySave(folders[1],"Arrays",false,"int values[3];",false)==CODE_SAVE_OK);
    RankingEntry skillRows[3] = {{"Alice"},{"Bob"},{"Cara"}};
    for (int i=0;i<3;i++) RankingReadSkills(&skillRows[i],TRACK_C,folders[i]);
    assert(skillRows[0].uniqueSkills==1 && skillRows[0].skillPrograms==2 && skillRows[0].skills[SKILL_LOOPS]);
    assert(skillRows[1].uniqueSkills==2 && skillRows[1].skillPrograms==1);
    assert(skillRows[2].uniqueSkills==0 && skillRows[2].skillPrograms==0 && !skillRows[2].warning);
    RankingSortMode(skillRows,3,RANKING_SKILLS);
    assert(!strcmp(skillRows[0].name,"Bob") && skillRows[0].rank==1);
    RankingReadSkills(&skillRows[0],TRACK_CPP,folders[0]);
    assert(skillRows[0].uniqueSkills==4 && skillRows[0].skillPrograms==1 && !skillRows[0].skills[SKILL_LOOPS]);
    RankingReadSkills(&skillRows[0],TRACK_COUNT,folders[0]);
    assert(skillRows[0].uniqueSkills==5 && skillRows[0].skillPrograms==3);
    // A concept present in both languages remains one concept in Overall.
    assert(CodeLibrarySave(folders[0],"Cpp loop",true,"while (1) {}",false)==CODE_SAVE_OK);
    RankingReadSkills(&skillRows[0],TRACK_COUNT,folders[0]);
    assert(skillRows[0].uniqueSkills==5 && skillRows[0].skillPrograms==4);
    assert(CodeLibrarySave(folders[0],"Loop one",false,"int *ptr;",true)==CODE_SAVE_OK);
    RankingReadSkills(&skillRows[0],TRACK_C,folders[0]);
    assert(skillRows[0].uniqueSkills==3 && skillRows[0].skills[SKILL_POINTERS] && skillRows[0].skillPrograms==2);
    char programPath[1300]; snprintf(programPath,sizeof programPath,"%s/saved/Loop two.c",folders[0]);
    assert(remove(programPath)==0);
    RankingReadSkills(&skillRows[0],TRACK_C,folders[0]);
    assert(skillRows[0].uniqueSkills==2 && !skillRows[0].skills[SKILL_LOOPS] && skillRows[0].skillPrograms==1);
    RankingReadSkills(&skillRows[0],TRACK_CPP,folders[0]);
    assert(skillRows[0].skills[SKILL_LOOPS] && skillRows[0].skillPrograms==2);
    snprintf(programPath,sizeof programPath,"%s/saved/Loop one.c",folders[0]);
    assert(remove(programPath)==0);
    RankingReadSkills(&skillRows[0],TRACK_C,folders[0]);
    assert(skillRows[0].uniqueSkills==0 && skillRows[0].skillPrograms==0);
    RankingReadSkills(&skillRows[0],TRACK_C,folders[1]);
    assert(skillRows[0].uniqueSkills==2 && skillRows[0].skills[SKILL_ARRAYS]); // no cross-account loss
    RankingReadSkills(&skillRows[0],TRACK_CPP,folders[1]);
    assert(skillRows[0].uniqueSkills==0 && skillRows[0].skillPrograms==0);
    RankingEntry modeRows[4] = {{"Zoe",5,400},{"Amy",1,80},{"Ben",2,150},{"Dan",0,0}};
    modeRows[0].uniqueSkills=1; modeRows[0].skillPrograms=10;
    modeRows[1].uniqueSkills=3; modeRows[1].skillPrograms=1;
    modeRows[2].uniqueSkills=3; modeRows[2].skillPrograms=2;
    modeRows[3].uniqueSkills=3; modeRows[3].skillPrograms=2;
    RankingSortMode(modeRows,4,RANKING_PROGRESS);
    assert(!strcmp(modeRows[0].name,"Zoe") && modeRows[0].rank==1);
    RankingSortMode(modeRows,4,RANKING_SKILLS);
    assert(!strcmp(modeRows[0].name,"Ben") && !strcmp(modeRows[1].name,"Dan"));
    assert(modeRows[0].rank==1 && modeRows[1].rank==1 && modeRows[2].rank==3 && modeRows[3].rank==4);
    RankingSortMode(modeRows,4,RANKING_PROGRESS);
    assert(!strcmp(modeRows[0].name,"Zoe") && modeRows[0].completed==5 && modeRows[0].stepsPassed==400);
    RankingSortMode(NULL,0,RANKING_SKILLS);
    RankingReadSkills(&skillRows[0],TRACK_COUNT,""); assert(skillRows[0].warning);
    char *saved=CodeLibraryLoad(folders[1],"Arrays",false); assert(saved && !strcmp(saved,"int values[3];")); free(saved);
    RankingEntry ties[] = {{"zoe",1,100},{"Amy",1,100},{"Ben",1,99},{"Dan",0,200}};
    RankingSort(ties,4);
    assert(!strcmp(ties[0].name,"Amy") && ties[0].rank==1 && ties[1].rank==1 && ties[2].rank==3 && ties[3].rank==4);
    RankingSort(ties,1); assert(ties[0].rank==1);
    RankingSort(NULL,0);
    char damaged[1300]; snprintf(damaged,sizeof damaged,"%sdamaged.txt",directory);
    Write(damaged,"c_a/1 1000\nc_a/1 -1\nc_a/1 70bad\nc_a/1 9999999999999999999999999999999\nc_a/1 100 extra\n\n"
                  "c_a/1 69\ncpp_a/1 100\n");
    RankingRead(&rows[0],&set,TRACK_COUNT,damaged);
    assert(rows[0].warning && rows[0].completed==1 && rows[0].stepsPassed==1);
    FILE *file=fopen(damaged,"w"); assert(file);
    for(int i=0;i<300;i++) fputc('x',file);
    fputs("\nc_a/1 100\nc_a/2 100\n",file); fclose(file);
    RankingRead(&rows[0],&set,TRACK_COUNT,damaged);
    assert(rows[0].warning && rows[0].completed==1 && rows[0].stepsPassed==2);
    LessonSet empty={0}; RankingRead(&rows[0],&empty,TRACK_COUNT,paths[0]);
    assert(!rows[0].warning && rows[0].completed==0 && rows[0].stepsPassed==0);
    assert(!strcmp(AccountCurrent(),"Cara") && !strcmp(AccountProgressPath(),paths[2]));
    assert(ProgressStepDone("sentinel",0) && !ProgressStepDone("c_a",0));
    char *after=LoadFileText(paths[0]), *currentAfter=LoadFileText(paths[2]);
    assert(after && currentAfter && !strcmp(before,after) && !strcmp(currentBefore,currentAfter));
    UnloadFileText(before); UnloadFileText(after); UnloadFileText(currentBefore); UnloadFileText(currentAfter);
    assert(AccountDelete("password",&error) && AccountCount()==2);
    assert(!AccountRankingProfile(2,name,sizeof name,path,sizeof path));
    ProgressLoad(""); free(set.items);
    puts("All local user ranking tests passed");
    return 0;
}
