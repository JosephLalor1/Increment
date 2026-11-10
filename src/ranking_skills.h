// Aggregation of existing sandbox concept hints for local comparison, never earned XP.
#ifndef RANKING_SKILLS_H
#define RANKING_SKILLS_H
#include "ranking.h"
// Replaces skill summary/warning/rank, preserving name and progress fields. Uses the
// saved-program listing's language flag. TRACK_COUNT unions both tracks, once per concept.
void RankingReadSkills(RankingEntry *entry, Track track, const char *codeFolder);
// Progress delegates to the unchanged existing sorter. Skills use unique concepts, then
// contributing programs, then alphabetical display order; equal numeric results share rank.
void RankingSortMode(RankingEntry *entries, int count, RankingMode mode);
#endif
