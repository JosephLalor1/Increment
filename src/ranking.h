// Read-only rankings from saved lesson progress; no account switching or score writes.
#ifndef RANKING_H
#define RANKING_H
#include "lesson.h"
#include "account.h"

typedef enum { RANKING_PROGRESS, RANKING_SKILLS, RANKING_MODE_COUNT } RankingMode;

typedef struct {
    char name[USERNAME_MAX + 1];
    int completed, quizPoints, rank;
    bool warning; // missing files are new users; unreadable or malformed records warn
    bool skills[SKILL_COUNT]; // union of concepts in the selected saved-program track(s)
    int uniqueSkills, skillPrograms;
} RankingEntry;

// Reads best scores for loaded lessons only. TRACK_COUNT includes both tracks.
// Does not change name or the signed-in learner's in-memory progress. Missing file = zero.
void RankingRead(RankingEntry *entry, const LessonSet *lessons, Track track, const char *path);
// Sorts by lessons completed, then sum of best quiz scores; equal results share a rank
// (1,1,3). Names break display-order ties only, so file order never decides a winner.
void RankingSort(RankingEntry *entries, int count);
#endif
