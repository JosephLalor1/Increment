#ifndef REVIEW_VIEW_H
#define REVIEW_VIEW_H

#include <stdbool.h>

// The statistics page's "Your speed round questions" section: how the learner does on each
// skill's generated questions (from review.h), weakest first, with a button for a short
// challenge.

typedef struct {
    bool showAll;  // every answered skill, not just the weakest few
} ReviewView;

// Draws the section at y and returns its height. Sets *challenge when "Start short
// challenge" was pressed.
float ReviewViewDraw(ReviewView *view, float x, float y, float w, float scale, bool *challenge);

#endif
