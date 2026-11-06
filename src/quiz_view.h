// Shared rendering for normal quiz answers and read-only answer reviews.
#ifndef QUIZ_VIEW_H
#define QUIZ_VIEW_H
#include "quiz_attempt.h"

// Shared answer button; selected=-1 hides feedback until the student picks a choice.
float QuizViewChoice(const Question *q, int choice, int selected, float x, float y, float w, float scale, bool *chosen);
// Shared read-only review content. draw=false measures exactly the same layout.
float QuizViewReview(const QuizReviewItem *item, int number, float x, float y, float w, float scale, bool draw);
#endif
