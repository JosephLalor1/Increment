#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>

// The signed-in user's best scores, stored as "id percent" lines in a text file.
//
// Two kinds of id are stored:
//   <lesson id>               best quiz score for the lesson (0-100)
//   <lesson id>#<n>           100 once exercise n (1-based) has been passed
//   <lesson id>!gold          100 once the lesson's speed round has been won (gold mastery)
// Only this module builds those ids; everyone else asks with the functions below.

#define PASS_PERCENT 70  // a lesson counts as complete at this quiz score or above

void ProgressLoad(const char *path);  // "" clears the scores (signed out)
bool ProgressSave(void);

int ProgressBest(const char *lessonId);           // best quiz score, -1 if never attempted
bool ProgressLessonPassed(const char *lessonId);  // best score >= PASS_PERCENT
const char *ProgressScoreNote(const char *lessonId);  // "New" or "Best: 80%", for lesson rows (temporary string)
void ProgressRecord(const char *lessonId, int percent);  // keeps the best score and saves

bool ProgressExerciseDone(const char *lessonId, int exerciseIndex);  // zero-based index
void ProgressRecordExercise(const char *lessonId, int exerciseIndex); // marks it passed and saves

bool ProgressGold(const char *lessonId);        // the lesson's speed round has been won
void ProgressRecordGold(const char *lessonId);  // marks gold mastery and saves

#endif
