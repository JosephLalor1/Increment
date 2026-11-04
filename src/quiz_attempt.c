#include "quiz_attempt.h"

#include <string.h>

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
    if (choice < 0 || choice >= q->choiceCount || choice >= MAX_CHOICES) return false;
    attempt->answers[question] = choice;
    return true;
}

int QuizAttemptCorrect(const QuizAttempt *attempt)
{
    int correct = 0;
    for (int i = 0; ValidQuestion(attempt, i); i++) {
        const Question *q = &attempt->lesson->questions[i];
        if (attempt->answers[i] >= 0 && attempt->answers[i] == q->correct) correct++;
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
    item->question = q;
    int selected = attempt->answers[question];
    item->answered = selected >= 0 && selected < q->choiceCount && selected < MAX_CHOICES;
    item->selectedAnswer = item->answered ? q->choices[selected] : NULL;
    bool validCorrect = q->correct >= 0 && q->correct < q->choiceCount && q->correct < MAX_CHOICES;
    item->correctAnswer = validCorrect ? q->choices[q->correct] : NULL;
    item->correct = item->answered && validCorrect && selected == q->correct;
    item->explanation = q->explain && q->explain[0] ? q->explain : NULL;
    return true;
}
