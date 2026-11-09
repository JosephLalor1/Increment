#ifndef REVIEW_VIEW_H
#define REVIEW_VIEW_H

#include "lesson.h"

#include <stdbool.h>

// The statistics page's "Your questions" section: how the learner does on each question
// they've answered (from review.h), weakest first, with a button for a short challenge.

typedef struct {
    bool showAll;  // every answered question, not just the weakest few
} ReviewView;

typedef struct {
    int openLesson;  // a question's lesson was clicked: its index in the set, else -1
    bool challenge;  // "Start short challenge" was pressed
} ReviewChoice;

// Draws the section at y and returns its height.
float ReviewViewDraw(ReviewView *view, const LessonSet *set, float x, float y, float w, float scale,
                     ReviewChoice *choice);

#endif
