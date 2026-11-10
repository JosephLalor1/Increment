// Local leaderboard screen; main owns navigation.
#ifndef RANKING_SCREEN_H
#define RANKING_SCREEN_H
#include "ranking.h"
// Refreshes the account roster and saved results; lesson data must outlive the screen.
void RankingScreenOpen(const LessonSet *lessons);
// Selects mode/track and refreshes real saved data, including for developer previews.
void RankingScreenSelect(RankingMode mode, Track track);
// Developer preview: expand a row (-1 collapses all), at a settled scroll position.
void RankingScreenDemo(int row, float scroll);
// Releases the roster on logout and shutdown.
void RankingScreenClear(void);
// Draws and handles input; true means Back to Home.
bool RankingScreen(void);
#endif
