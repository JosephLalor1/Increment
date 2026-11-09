#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"
#include "search.h"

#include <stdbool.h>

// The home screen, shown after signing in: the C and C++ learning journeys, detailed
// statistics, saved lessons and lesson search, under a home bar with the code editor,
// Settings, Log out and Quit. It reads progress but never navigates: it returns what the
// user chose.

typedef enum {
    HOME_VIEW_JOURNEY,     // a track's winding path of lessons
    HOME_VIEW_STATISTICS,
    HOME_VIEW_SAVED,       // bookmarked lessons from both tracks
    HOME_VIEW_COUNT
} HomeView;

typedef enum {
    HOME_STAY,          // nothing to do
    HOME_OPEN_LESSON,   // open HomeAction.lessonIndex
    HOME_SPEED_ROUND,   // start HomeAction.lessonIndex's speed round (from the statistics)
    HOME_CODE_EDITOR,
    HOME_PRACTICE,
    HOME_COURSES,
    HOME_SETTINGS,
    HOME_LOG_OUT,
    HOME_QUIT,
} HomeChoice;

typedef struct {
    HomeChoice kind;
    int lessonIndex;  // HOME_OPEN_LESSON, HOME_SPEED_ROUND
} HomeAction;

// After signing in: the C journey, scrolled to the top, statistics filters reset and the
// search cleared. Coming back from a lesson, the code editor or Settings, don't call this:
// the screen is kept as it was left.
void DashboardOpen(void);

// Shows a view with the search cleared, e.g. for a lesson's "Return to Roadmap" button.
// track picks the journey tab (for any view, so it's ready when the journey is shown).
void DashboardShow(HomeView view, Track track);

// Draws the screen and handles input.
HomeAction DashboardScreen(const LessonSet *set);

// Dev aids for --shot: scroll the current view, and type into the search box (see
// SearchViewDemo).
void DashboardDemo(float scroll);
void DashboardDemoSearch(const char *query, SearchFilter filter);

#endif
