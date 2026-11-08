#ifndef QUIZ_ATTEMPT_H
#define QUIZ_ATTEMPT_H

#include "lesson.h"

// A coding question's answer, stored where a multiple-choice question keeps its choice.
enum {
    QUIZ_CODE_PASSED = 0,   // the learner's program printed the expected output
    QUIZ_CODE_SKIPPED = 1,  // the learner moved on without solving it (counts as wrong)
};

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
    const Exercise *task;       // coding questions: the task, for its expected output and solution
    bool answered;
    bool correct;
} QuizReviewItem;

// Shared read-only answer rules for normal lesson quizzes and revision sessions.
// For a coding question the only valid answers are QUIZ_CODE_PASSED and QUIZ_CODE_SKIPPED.
bool QuizIsCoding(const Question *question);
bool QuizChoiceValid(const Question *question, int choice);
bool QuizAnswerCorrect(const Question *question, int choice);
void QuizQuestionReview(const Question *question, int choice, QuizReviewItem *item);

void QuizAttemptBegin(QuizAttempt *attempt, const Lesson *lesson);
bool QuizAttemptPick(QuizAttempt *attempt, int question, int choice);
int QuizAttemptCorrect(const QuizAttempt *attempt);
int QuizAttemptFinish(QuizAttempt *attempt); // percentage; does not save progress
bool QuizAttemptReview(const QuizAttempt *attempt, int question, QuizReviewItem *item);

#endif
