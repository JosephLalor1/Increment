// Local leaderboard screen; main owns navigation.
#ifndef RANKING_SCREEN_H
#define RANKING_SCREEN_H
#include "lesson.h"
// Refreshes the account roster and saved results; lesson data must outlive the screen.
void RankingScreenOpen(const LessonSet *lessons);
// Releases the roster on logout and shutdown.
void RankingScreenClear(void);
// Draws and handles input; true means Back to Home.
bool RankingScreen(void);
#endif
