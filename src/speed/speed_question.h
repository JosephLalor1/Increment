#ifndef SPEED_QUESTION_H
#define SPEED_QUESTION_H

// Speed round questions: short generated snippets ("what is x after these lines?") with
// four answers, one of them a tempting mistake, and a note per line of what the variables
// hold, for explaining a miss. Data only, unit-tested (tests/test_speed_round.c compiles
// hundreds of them to check their answers); speed_round.h runs a round of them.

#include <stdbool.h>
#include <stdint.h>

#define SPEED_CHOICES    4     // answers to choose from
#define SPEED_MAX_LINES  18
#define SPEED_LINE_SIZE  72

// What a question practises. A lesson names its skills on an "@speed" line, by these names:
// variables arithmetic conditions loops arrays strings functions pointers pointer_to_pointer
// pass_by_pointer pointer_arithmetic malloc function_pointers references new_delete objects
// linked_list
typedef enum {
    SPEED_VARIABLES, SPEED_ARITHMETIC, SPEED_CONDITIONS, SPEED_LOOPS, SPEED_ARRAYS, SPEED_STRINGS,
    SPEED_FUNCTIONS, SPEED_POINTERS, SPEED_POINTER_TO_POINTER, SPEED_PASS_BY_POINTER,
    SPEED_POINTER_ARITHMETIC, SPEED_MALLOC, SPEED_FUNCTION_POINTERS, SPEED_REFERENCES, SPEED_NEW_DELETE,
    SPEED_OBJECTS, SPEED_LINKED_LIST, SPEED_SKILL_COUNT
} SpeedSkill;

typedef struct {
    SpeedSkill skill;
    bool cpp;                                      // C++ (references, new, classes); C otherwise
    int lineCount;
    int firstBodyLine;                             // lines before this are function definitions
    char lines[SPEED_MAX_LINES][SPEED_LINE_SIZE];  // the snippet, one line each
    char notes[SPEED_MAX_LINES][SPEED_LINE_SIZE];  // the variables' values after each line ("" for none)
    char asks[8];                                  // the variable asked about: "x" or "y"
    int choices[SPEED_CHOICES];
    int correct;                                   // index into choices
} SpeedQuestion;

const char *SpeedSkillName(SpeedSkill skill);      // as written on "@speed" lines
const char *SpeedSkillTitle(SpeedSkill skill);     // for people: "Pointers to pointers"
// Reads skill names separated by spaces into out (at most max). Unknown names are skipped.
// Returns how many were read.
int SpeedSkillsParse(const char *names, SpeedSkill *out, int max);

// Makes a question. difficulty (0 = easiest) adds more lines; seed advances.
void SpeedGenerate(SpeedSkill skill, int difficulty, uint32_t *seed, SpeedQuestion *out);
// How hard a generated question is, 1 to 3, from how many lines there are to trace.
int SpeedQuestionDifficulty(const SpeedQuestion *q);
// The snippet as one string, lines joined with '\n' (from..lineCount-1); returns out.
char *SpeedQuestionCode(const SpeedQuestion *q, int from, char *out, int size);

#endif
