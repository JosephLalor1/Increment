// Supported syntax hints and false-positive guards; no account/XP dependency.
#include "code/code_skills.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    CodeSkills s=CodeSkillsDetect("int main(void) { int a[3]; for(int i=0;i<3;i++) { if(i) printf(\"%d\",a[i]); } }",false);
    assert(s.complete && s.present[SKILL_PROGRAM_STRUCTURE] && s.present[SKILL_TYPES]);
    assert(s.present[SKILL_ARRAYS] && s.present[SKILL_LOOPS] && s.present[SKILL_CONDITIONS] && s.present[SKILL_OUTPUT]);
    s=CodeSkillsDetect("/* for while int *ptr malloc */\n// class if scanf\n#define FAKE while class\nconst char *s=\"for if malloc printf\"; char c='?';",false);
    assert(!s.present[SKILL_LOOPS] && !s.present[SKILL_CONDITIONS] && !s.present[SKILL_OUTPUT] && !s.present[SKILL_DYNAMIC_MEMORY]);
    s=CodeSkillsDetect("int before=1; int while_count=2; int printfCount=3; int a=before * while_count;",false);
    assert(!s.present[SKILL_LOOPS] && !s.present[SKILL_OUTPUT] && !s.present[SKILL_POINTERS]);
    s=CodeSkillsDetect("int *p; int **pp; int (*fn)(int); void helper(int x) { free(malloc(x)); }",false);
    assert(s.present[SKILL_POINTERS] && s.present[SKILL_INDIRECTION] && s.present[SKILL_FUNCTION_POINTERS]);
    assert(s.present[SKILL_FUNCTIONS] && s.present[SKILL_DYNAMIC_MEMORY] && !s.present[SKILL_MEMORY_SAFETY]);
    s=CodeSkillsDetect("class Box { private: int n; public: void set(int v) {n=v;} }; int &ref=value; std::vector<int> a; std::cout << ref;",true);
    assert(s.present[SKILL_CLASSES] && s.present[SKILL_ENCAPSULATION] && s.present[SKILL_REFERENCES]);
    assert(s.present[SKILL_CPP_BASICS] && s.present[SKILL_ARRAYS] && s.present[SKILL_OUTPUT]);
    s=CodeSkillsDetect("auto text=R\"tag(for if class printf malloc)tag\"; auto fn=[](){return 1;};",true);
    assert(!s.present[SKILL_LOOPS] && !s.present[SKILL_OUTPUT] && !s.present[SKILL_CLASSES] && !s.present[SKILL_ARRAYS]);
    s=CodeSkillsDetect("// for at EOF",false); assert(s.complete && !s.present[SKILL_LOOPS]);
    s=CodeSkillsDetect("/* unfinished while",false); assert(!s.present[SKILL_LOOPS]);
    s=CodeSkillsDetect("scanf(\"%d\",&n); fgets(buf,10,stdin); strlen(buf);",false);
    assert(s.present[SKILL_INPUT] && s.present[SKILL_STRINGS]);
    s=CodeSkillsDetect("",false); assert(s.complete);
    for(int i=0;i<SKILL_COUNT;i++) assert(!s.present[i]);
    s=CodeSkillsDetect(NULL,false); assert(!s.complete);
    puts("All code skill detection tests passed"); return 0;
}
