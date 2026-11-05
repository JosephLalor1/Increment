#ifndef SAVED_VIEW_H
#define SAVED_VIEW_H

#include "lesson.h"
#include "raylib.h"

#include <stdbool.h>

// The "Saved lessons" list: every bookmarked lesson from both tracks, in course order, each
// with a button to remove its bookmark; or, with none saved, how to bookmark one. Drawn by
// the home screen, which keeps a SavedView. The bookmarks themselves are in bookmarks.h.

typedef struct {
    float scroll;
    float height;  // measured last frame, for clamping the scroll
} SavedView;

typedef struct {
    int openLesson;  // lesson index to open, or -1
    bool browse;     // the empty state's "Browse the journey" link was clicked
} SavedChoice;

// Draws the list scrolling inside view and handles its clicks.
SavedChoice SavedViewDraw(SavedView *s, const LessonSet *set, Rectangle view);

#endif
