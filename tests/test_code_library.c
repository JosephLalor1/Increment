// Named copies, atomic replacement, filename boundaries and user-folder isolation.
#include "../src/code_library.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==2); SetTraceLogLevel(LOG_ERROR);
    const char *bad[]={"","../escape","a/b","a\\b","CON","nul","COM1","LPT9","a.c"," a","a ","a:b","a?b"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) assert(!CodeLibraryNameValid(bad[i]));
    assert(CodeLibraryNameValid("My loops-2_test") && !CodeLibraryNameValid(NULL));
    char a[1200],b[1200],path[1300];
    snprintf(a,sizeof a,"%s/library_a",argv[1]); snprintf(b,sizeof b,"%s/library_b",argv[1]);
    assert(MakeDirectory(a)==0 && MakeDirectory(b)==0);
    SavedProgram *list=NULL; int count=0;
    assert(CodeLibraryList(a,&list,&count) && count==0); free(list);
    const char *code="int main(void) { for(int i=0;i<3;i++) {} }\n";
    assert(CodeLibrarySave(a,"My loops",false,code,false)==CODE_SAVE_OK);
    assert(CodeLibrarySave(a,"My loops",true,"int *ptr;",false)==CODE_SAVE_OK);
    char *loaded=CodeLibraryLoad(a,"My loops",false); assert(loaded && !strcmp(loaded,code)); free(loaded);
    assert(!CodeLibraryLoad(b,"My loops",false));
    assert(CodeLibrarySave(a,"My loops",false,"replace",false)==CODE_SAVE_EXISTS);
    loaded=CodeLibraryLoad(a,"My loops",false); assert(loaded && !strcmp(loaded,code)); free(loaded);
    assert(CodeLibrarySave(a,"My loops",false,"int a[5];",true)==CODE_SAVE_OK);
    loaded=CodeLibraryLoad(a,"My loops",false); assert(loaded && !strcmp(loaded,"int a[5];")); free(loaded);
    assert(CodeLibraryList(a,&list,&count) && count==2);
    assert(!list[0].cpp && list[0].skills.present[SKILL_ARRAYS]);
    assert(list[1].cpp && list[1].skills.present[SKILL_POINTERS]); free(list);
    snprintf(path,sizeof path,"%s/saved/My loops.c.tmp",a);
    assert(MakeDirectory(path)==0); // an unwritable temporary destination must preserve the saved copy
    assert(CodeLibrarySave(a,"My loops",false,"replacement",true)==CODE_SAVE_ERROR);
    loaded=CodeLibraryLoad(a,"My loops",false); assert(loaded && !strcmp(loaded,"int a[5];")); free(loaded);
    assert(CodeLibrarySave(b,"My loops",false,"int other;",false)==CODE_SAVE_OK);
    loaded=CodeLibraryLoad(a,"My loops",false); assert(loaded && !strcmp(loaded,"int a[5];")); free(loaded);
    assert(CodeLibrarySave(a,"../escape",false,code,true)==CODE_SAVE_ERROR);
    assert(CodeLibrarySave("","safe",false,code,false)==CODE_SAVE_ERROR);
    assert(!CodeLibraryLoad(a,"../escape",false));
    char *large=malloc(CODE_SAVE_MAX+2); assert(large);
    memset(large,'x',CODE_SAVE_MAX+1); large[CODE_SAVE_MAX+1]=0;
    assert(CodeLibrarySave(a,"Too large",false,large,false)==CODE_SAVE_ERROR); free(large);
    snprintf(path,sizeof path,"%s/saved/Binary.c",a);
    FILE *file=fopen(path,"wb"); assert(file); fputc(0,file); fclose(file);
    assert(!CodeLibraryLoad(a,"Binary",false));
    assert(!CodeLibraryList(a,&list,&count) && count==2); free(list);
    assert(CodeLibrarySave(a,"Empty",false,"",false)==CODE_SAVE_OK);
    loaded=CodeLibraryLoad(a,"Empty",false); assert(loaded && !*loaded); free(loaded);
    puts("All sandbox code library tests passed"); return 0;
}
