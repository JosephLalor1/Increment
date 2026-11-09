#ifndef LESSON_H
#define LESSON_H

// Lessons as loaded from assets/lessons: blocks of text and code, quiz questions and
// coding exercises.

#include <stdbool.h>

#include "skills.h"

#define MAX_BLOCKS    256
#define MAX_QUESTIONS 32   // a lesson's whole question bank; a quiz asks some of them (quiz_attempt.h)
#define MAX_CHOICES   6
#define MAX_EXERCISES 8
#define MAX_EXERCISE_HINTS 8

typedef enum { TRACK_C, TRACK_CPP, TRACK_COUNT } Track;

typedef enum {
    BLOCK_HEADING, BLOCK_TEXT, BLOCK_CODE, BLOCK_NOTE, BLOCK_BULLET,
    BLOCK_MEMORY,  // a ```memory fence: a script for an interactive pointer diagram (memory_model.h)
} BlockType;

typedef struct {
    BlockType type;
    char *text;
} Block;

// A coding exercise, done in the code editor. The learner's program passes when its
// output matches `expected` for the given `input`.
typedef struct {
    char *title;
    char *description;  // paragraphs separated by blank lines
    char *starter;      // code the editor starts with
    char *input;        // stdin, may be NULL
    char *expected;     // expected stdout
    char *solution;     // reference solution, may be NULL
    char *hints[MAX_EXERCISE_HINTS]; // optional clues, in reveal order
    int hintCount;
} Exercise;

// A quiz question: either multiple choice, or (when task is set) a coding question that
// is solved in the code editor and counts as correct once the program's output matches.
typedef struct {
    char *prompt;
    char *code;      // optional snippet shown under the prompt, may be NULL
    char *choices[MAX_CHOICES];
    int choiceCount;
    int correct;     // index into choices (multiple choice only)
    char *explain;   // optional, may be NULL
    Exercise *task;  // a "?code" question's program to fix or finish; NULL for multiple choice
    int difficulty;  // 1 (easy) to 3 (hard), from its "~" line (2 if it has none)
    SkillWeight skills[MAX_QUESTION_SKILLS];  // what it exercises, from its "~" line
    int skillCount;
} Question;

typedef struct {
    char id[32];
    char title[96];
    Track track;
    bool practice;   // "@kind practice": exam/lab practice rather than new material
    char speedSkills[96];  // "@speed pointers ...": the skills its speed round tests (speed_round.h); "" for none
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

// A question's lasting id, for remembering how the learner does on it (review.h):
// "<lesson id>?<8 hex digits>", the digits hashed from its prompt and code. It doesn't depend
// on the question's position, so adding or reordering questions keeps everyone's history.
// size should be at least 48.
void LessonQuestionId(const Lesson *lesson, const Question *question, char *out, int size);

#endif
