#include "quiz_attempt.h"

#include <string.h>

bool QuizChoiceValid(const Question *q, int choice)
{
    return q && choice >= 0 && choice < q->choiceCount && choice < MAX_CHOICES && q->choices[choice];
}

bool QuizAnswerCorrect(const Question *q, int choice) { return QuizChoiceValid(q, choice) && choice == q->correct; }

void QuizQuestionReview(const Question *q, int choice, QuizReviewItem *item)
{
    memset(item, 0, sizeof *item);
    if (!q) return;
    item->question = q;
    item->answered = QuizChoiceValid(q, choice);
    item->selectedAnswer = item->answered ? q->choices[choice] : NULL;
    item->correctAnswer = QuizChoiceValid(q, q->correct) ? q->choices[q->correct] : NULL;
    item->correct = QuizAnswerCorrect(q, choice);
    item->explanation = q->explain && q->explain[0] ? q->explain : NULL;
}

void QuizAttemptBegin(QuizAttempt *attempt, const Lesson *lesson)
{
    attempt->lesson = lesson;
    attempt->completed = false;
    for (int i = 0; i < MAX_QUESTIONS; i++) attempt->answers[i] = -1;
}

static bool ValidQuestion(const QuizAttempt *attempt, int question)
{
    return attempt->lesson && question >= 0 && question < attempt->lesson->questionCount && question < MAX_QUESTIONS;
}

bool QuizAttemptPick(QuizAttempt *attempt, int question, int choice)
{
    if (attempt->completed || !ValidQuestion(attempt, question) || attempt->answers[question] >= 0) return false;
    const Question *q = &attempt->lesson->questions[question];
    if (!QuizChoiceValid(q, choice)) return false;
    attempt->answers[question] = choice;
    return true;
}

int QuizAttemptCorrect(const QuizAttempt *attempt)
{
    int correct = 0;
    for (int i = 0; ValidQuestion(attempt, i); i++) {
        const Question *q = &attempt->lesson->questions[i];
        if (QuizAnswerCorrect(q, attempt->answers[i])) correct++;
    }
    return correct;
}

int QuizAttemptFinish(QuizAttempt *attempt)
{
    attempt->completed = attempt->lesson && attempt->lesson->questionCount > 0;
    return attempt->completed ? QuizAttemptCorrect(attempt) * 100 / attempt->lesson->questionCount : 0;
}

bool QuizAttemptReview(const QuizAttempt *attempt, int question, QuizReviewItem *item)
{
    memset(item, 0, sizeof *item);
    if (!attempt->completed || !ValidQuestion(attempt, question)) return false;
    const Question *q = &attempt->lesson->questions[question];
    QuizQuestionReview(q, attempt->answers[question], item);
    return true;
}
