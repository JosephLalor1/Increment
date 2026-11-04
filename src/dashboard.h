#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"

// The learning progress dashboard screen: overall and per-track completion, milestones,
// quiz performance, recommendations and a filterable topic list. Statistics come from
// dashboard_stats.h; this module only lays them out.

void DashboardOpen(void);  // entering from the menu: scroll to the top and reset the topic filters

// Draws the screen and handles input. Returns a lesson index to open, PAGE_STAY or
// PAGE_BACK (see layout.h). Scroll position and filters survive a visit to a lesson.
int DashboardScreen(const LessonSet *set);

// Dev aid for --shot: start scrolled down by `scroll` pixels.
void DashboardDemo(float scroll);

#endif
