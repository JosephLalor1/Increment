#ifndef REVIEW_H
#define REVIEW_H

#include <stdbool.h>

// What the signed-in user tends to get right and wrong, question by question, with an
// Anki-style (SM-2) schedule for when each one should be practised again. Every answer the
// app checks (lesson quizzes, practice quizzes, coding questions, speed rounds) is recorded
// here under a stable id: LessonQuestionId for lesson questions, "skill:<name>" for a speed
// round skill's generated questions.
//
// Stored as one line per question in users/<name>.review.txt:
//   <id> <right> <wrong> <repetitions> <lapses> <ease> <interval days> <due day> <last day>
// Days count from 1 January 1970 (ReviewToday). Plain data, nothing draws, and every
// function that needs the date takes it, so it's unit-tested (tests/test_review.c).

#define REVIEW_ID_SIZE 64
#define REVIEW_MAX     2048

typedef struct {
    char id[REVIEW_ID_SIZE];
    int right, wrong;     // answers so far
    int repetitions;      // right answers in a row (SM-2's "n")
    int lapses;           // times it was got wrong after being learnt
    float ease;           // SM-2 ease factor: 2.5 to start, never below 1.3
    int interval;         // days until it's due again after the last right answer
    long due;             // the day it should next be practised
    long last;            // the day it was last answered
} ReviewItem;

// How well the learner knows a question, for giving more time to the ones that need it.
typedef enum { REVIEW_NEW, REVIEW_STRUGGLING, REVIEW_KNOWN } ReviewFamiliarity;

long ReviewToday(void);  // today, in days since 1970 (local time)

// Loads the user's history from path; "" clears it (signed out). A missing file is empty.
void ReviewLoad(const char *path);

// Records an answer and saves. Right answers push the next review further out (1 day, then
// 3, then the interval times the ease); wrong ones make it due again today and lower the
// ease. Returns false if the file couldn't be written (it still counts for this session).
// Does nothing while signed out.
bool ReviewRecord(const char *id, bool correct, long today);

const ReviewItem *ReviewFind(const char *id);  // NULL if never answered
int ReviewCount(void);
const ReviewItem *ReviewAt(int index);         // NULL when out of range

ReviewFamiliarity ReviewFamiliarityOf(const char *id);
int ReviewDueCount(long today);  // answered questions due for review today or earlier

// How much a question needs practice now, for choosing a short challenge: overdue questions
// first (the more overdue and the more often missed, the higher), then ones being missed,
// then new ones, then ones not yet due. item NULL means never answered.
float ReviewNeed(const ReviewItem *item, long today);

// How weak the learner is at it, from 0 (always right) to about 1 (always wrong), for
// listing the weakest questions. Few answers pull towards the middle.
float ReviewWeakness(const ReviewItem *item);

#endif
