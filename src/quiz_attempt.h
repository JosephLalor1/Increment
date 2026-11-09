#ifndef QUIZ_ATTEMPT_H
#define QUIZ_ATTEMPT_H

#include "lesson.h"

// A coding question's answer, stored where a multiple-choice question keeps its choice.
enum {
    QUIZ_CODE_PASSED = 0,   // the learner's program printed the expected output
    QUIZ_CODE_SKIPPED = 1,  // the learner moved on without solving it (counts as wrong)
};

// How many multiple-choice questions a lesson quiz asks, drawn at random from the lesson's
// bank each time (its coding questions are always asked, last).
#define QUIZ_CHOICE_QUESTIONS 5

// One in-memory attempt. Lesson owns the questions; the attempt keeps which of them it asks,
// in what order, and the selections. Positions (0 .. count-1) index both.
typedef struct {
    const Lesson *lesson;
    int order[MAX_QUESTIONS];   // the lesson question asked at each position
    int count;                  // questions in this attempt
    int answers[MAX_QUESTIONS]; // by position; -1 means not answered
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

// Every question of the lesson, in file order (lesson NULL: an empty attempt).
void QuizAttemptBegin(QuizAttempt *attempt, const Lesson *lesson);
// A lesson quiz: up to QUIZ_CHOICE_QUESTIONS multiple-choice questions picked at random from
// the bank (seed picks which, and their order), then the coding questions.
void QuizAttemptBeginDrawn(QuizAttempt *attempt, const Lesson *lesson, unsigned seed);
// How many questions a drawn quiz of this lesson asks.
int QuizLength(const Lesson *lesson);
// The question at a position, NULL when out of range.
const Question *QuizAttemptQuestion(const QuizAttempt *attempt, int position);
bool QuizAttemptPick(QuizAttempt *attempt, int question, int choice);
int QuizAttemptCorrect(const QuizAttempt *attempt);
int QuizAttemptFinish(QuizAttempt *attempt); // percentage; does not save progress
bool QuizAttemptReview(const QuizAttempt *attempt, int question, QuizReviewItem *item);

#endif
