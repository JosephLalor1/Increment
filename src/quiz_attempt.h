#ifndef QUIZ_ATTEMPT_H
#define QUIZ_ATTEMPT_H

#include "lesson.h"

// One in-memory attempt. Lesson owns the questions; only selections are retained here.
typedef struct {
    const Lesson *lesson;
    int answers[MAX_QUESTIONS]; // -1 means not answered
    bool completed;
} QuizAttempt;

typedef struct {
    const Question *question;
    const char *selectedAnswer; // NULL when unanswered
    const char *correctAnswer;  // NULL if content has no valid correct choice
    const char *explanation;    // NULL when missing or empty
    bool answered;
    bool correct;
} QuizReviewItem;

void QuizAttemptBegin(QuizAttempt *attempt, const Lesson *lesson);
bool QuizAttemptPick(QuizAttempt *attempt, int question, int choice);
int QuizAttemptCorrect(const QuizAttempt *attempt);
int QuizAttemptFinish(QuizAttempt *attempt); // percentage; does not save progress
bool QuizAttemptReview(const QuizAttempt *attempt, int question, QuizReviewItem *item);

#endif
