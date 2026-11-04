#ifndef LESSON_H
#define LESSON_H

#include <stdbool.h>

#define MAX_BLOCKS    256
#define MAX_QUESTIONS 16
#define MAX_CHOICES   6
#define MAX_EXERCISES 8

typedef enum { TRACK_C, TRACK_CPP, TRACK_COUNT } Track;

typedef enum { BLOCK_HEADING, BLOCK_TEXT, BLOCK_CODE, BLOCK_NOTE, BLOCK_BULLET } BlockType;

typedef struct {
    BlockType type;
    char *text;
} Block;

typedef struct {
    char *prompt;
    char *code;      // optional snippet shown under the prompt, may be NULL
    char *choices[MAX_CHOICES];
    int choiceCount;
    int correct;     // index into choices
    char *explain;   // optional, may be NULL
} Question;

// A coding exercise, done in the code editor. The learner's program passes when its
// output matches `expected` for the given `input`.
typedef struct {
    char *title;
    char *description;  // paragraphs separated by blank lines
    char *starter;      // code the editor starts with
    char *input;        // stdin, may be NULL
    char *expected;     // expected stdout
    char *solution;     // reference solution, may be NULL
} Exercise;

typedef struct {
    char id[32];
    char title[96];
    Track track;
    bool practice;   // "@kind practice": exam/lab practice rather than new material
    Block blocks[MAX_BLOCKS];
    int blockCount;
    Question questions[MAX_QUESTIONS];
    int questionCount;
    Exercise exercises[MAX_EXERCISES];
    int exerciseCount;
} Lesson;

typedef struct {
    Lesson *items;
    int count;
} LessonSet;

// Loads every *.txt in dir, sorted by filename. See CLAUDE.md for the format.
bool LessonsLoad(LessonSet *set, const char *dir);
void LessonsUnload(LessonSet *set);

const char *TrackName(Track t);

#endif
