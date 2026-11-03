#ifndef LESSON_H
#define LESSON_H

#include <stdbool.h>

#define MAX_BLOCKS    256
#define MAX_QUESTIONS 16
#define MAX_CHOICES   6

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

typedef struct {
    char id[32];
    char title[96];
    Track track;
    Block blocks[MAX_BLOCKS];
    int blockCount;
    Question questions[MAX_QUESTIONS];
    int questionCount;
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
