#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"

// Learning journeys for C and C++, with detailed statistics available separately.
// Both views read existing progress and return lesson indices to main.c for navigation.

void DashboardOpen(void);  // entering from the menu: scroll to the top and reset the topic filters

// Draws the screen and handles input. Returns a lesson index to open, PAGE_STAY or
// PAGE_BACK (see layout.h). Scroll position and filters survive a visit to a lesson.
int DashboardScreen(const LessonSet *set);

// Dev aid for --shot: start scrolled down by `scroll` pixels.
void DashboardDemo(float scroll);
void DashboardDemoTrack(Track track, bool statistics); // dev preview selection

#endif
