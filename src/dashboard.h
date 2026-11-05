#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"
#include "search.h"

#include <stdbool.h>

// The home screen, shown after signing in: the C and C++ learning journeys, detailed
// statistics, and lesson search, under a home bar with the code editor, Settings, Log out
// and Quit. It reads progress but never navigates: it returns what the user chose.

typedef enum {
    HOME_STAY,          // nothing to do
    HOME_OPEN_LESSON,   // open HomeAction.lessonIndex
    HOME_CODE_EDITOR,
    HOME_SETTINGS,
    HOME_LOG_OUT,
    HOME_QUIT,
} HomeChoice;

typedef struct {
    HomeChoice kind;
    int lessonIndex;  // HOME_OPEN_LESSON
} HomeAction;

// After signing in: the C journey, scrolled to the top, statistics filters reset and the
// search cleared. Coming back from a lesson, the code editor or Settings, don't call this:
// the screen is kept as it was left.
void DashboardOpen(void);

// Shows a track's journey (or the statistics) with the search cleared, e.g. for a lesson's
// "Return to Roadmap" button.
void DashboardShowJourney(Track track, bool statistics);

// Draws the screen and handles input.
HomeAction DashboardScreen(const LessonSet *set);

// Dev aids for --shot: scroll the current view, and type into the search box (see
// SearchViewDemo).
void DashboardDemo(float scroll);
void DashboardDemoSearch(const char *query, SearchFilter filter);

#endif
