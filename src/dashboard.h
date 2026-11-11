#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "lesson.h"

#include "raylib.h"

#include <stdbool.h>

// The home screen, shown after signing in: the C and C++ learning journeys and detailed
// statistics, under a home bar with the code editor,
// Settings, Log out and Quit. It reads progress but never navigates: it returns what the
// user chose.

typedef enum {
    HOME_VIEW_JOURNEY,     // a track's winding path of lessons
    HOME_VIEW_STATISTICS,
    HOME_VIEW_COUNT
} HomeView;

typedef enum {
    HOME_STAY,          // nothing to do
    HOME_OPEN_LESSON,   // play level HomeAction.lessonIndex
    HOME_SPEED_ROUND,   // start HomeAction.lessonIndex's speed round (from the statistics)
    HOME_CHALLENGE,     // a short challenge on the learner's most necessary skills
    HOME_CODE_EDITOR,
    HOME_RANKINGS,
    HOME_SETTINGS,
    HOME_LOG_OUT,
    HOME_QUIT,
} HomeChoice;

typedef struct {
    HomeChoice kind;
    int lessonIndex;  // HOME_OPEN_LESSON, HOME_SPEED_ROUND
    Vector2 clickedAt;  // HOME_OPEN_LESSON: where on screen the level was picked (its station,
                        // or the mouse), for the zoom into it
} HomeAction;

// After signing in: the C journey, scrolled to the top, and statistics filters reset. Coming back from a lesson, the code editor or Settings, don't call this:
// the screen is kept as it was left.
void DashboardOpen(void);

// Shows a view, e.g. the journey after finishing a level.
// track picks the journey tab (for any view, so it's ready when the journey is shown).
void DashboardShow(HomeView view, Track track);

// Draws the screen and handles input.
HomeAction DashboardScreen(const LessonSet *set);

// Where a level is on the screen as last drawn: the middle of its station on the journey,
// or the middle of the screen if the journey wasn't showing it. For the zoom out of a level.
Vector2 DashboardLevelPoint(int lessonIndex);

// Dev aid for --shot: scroll the current view.
void DashboardDemo(float scroll);

#endif
