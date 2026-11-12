#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>
#include <stddef.h>

// The signed-in learner's progress, stored as "id value" lines in a text file:
//   <lesson id>/<n>     100 once step n (1-based) of the level has been passed
//   <lesson id>!gold    100 once the level's speed round has been won (gold mastery)
// Only this module builds those ids; everyone else asks with the functions below. How far a
// learner is through a level or a track is worked out from these by roadmap.h. Lines from
// older versions (quiz scores, exercises) are kept in the file but mean nothing now.

void ProgressLoad(const char *path);  // "" clears it (signed out)

bool ProgressStepDone(const char *lessonId, int stepIndex);   // zero-based index
// Marks the step passed and saves. Returns false if it couldn't be saved (or signed out).
bool ProgressRecordStep(const char *lessonId, int stepIndex);
// Reads a stored id back: true for a step's id, giving its lesson id and zero-based step
// index. For tools that read another learner's file (rankings).
bool ProgressParseStepId(const char *id, char *lessonId, size_t size, int *stepIndex);

bool ProgressGold(const char *lessonId);        // the level's speed round has been won
void ProgressRecordGold(const char *lessonId);  // marks gold mastery and saves

#endif
