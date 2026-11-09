// Answers to one lesson's quiz, and the rules for marking an answer, shared with practice
// sessions. No drawing and no saving.
#include "quiz_attempt.h"

#include <string.h>

bool QuizIsCoding(const Question *q) { return q && q->task; }

bool QuizChoiceValid(const Question *q, int choice)
{
    if (QuizIsCoding(q)) return choice == QUIZ_CODE_PASSED || choice == QUIZ_CODE_SKIPPED;
    return q && choice >= 0 && choice < q->choiceCount && choice < MAX_CHOICES && q->choices[choice];
}

bool QuizAnswerCorrect(const Question *q, int choice)
{
    if (QuizIsCoding(q)) return choice == QUIZ_CODE_PASSED;
    return QuizChoiceValid(q, choice) && choice == q->correct;
}

void QuizQuestionReview(const Question *q, int choice, QuizReviewItem *item)
{
    memset(item, 0, sizeof *item);
    if (!q) return;
    item->question = q;
    item->answered = QuizChoiceValid(q, choice);
    item->correct = QuizAnswerCorrect(q, choice);
    item->explanation = q->explain && q->explain[0] ? q->explain : NULL;
    if (QuizIsCoding(q)) {  // the "answer" is whether the program passed
        item->task = q->task;
        item->selectedAnswer = !item->answered ? NULL
                             : item->correct ? "Your program printed the expected output." : "Skipped";
        item->correctAnswer = "A program that prints the expected output";
        return;
    }
    item->selectedAnswer = item->answered ? q->choices[choice] : NULL;
    item->correctAnswer = QuizChoiceValid(q, q->correct) ? q->choices[q->correct] : NULL;
}

void QuizAttemptBegin(QuizAttempt *attempt, const Lesson *lesson)
{
    attempt->lesson = lesson;
    attempt->completed = false;
    attempt->count = lesson ? lesson->questionCount : 0;
    if (attempt->count > MAX_QUESTIONS) attempt->count = MAX_QUESTIONS;
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        attempt->order[i] = i;
        attempt->answers[i] = -1;
    }
}

void QuizAttemptBeginDrawn(QuizAttempt *attempt, const Lesson *lesson, unsigned seed)
{
    QuizAttemptBegin(attempt, lesson);
    if (!lesson) return;
    int choice[MAX_QUESTIONS], choices = 0, coding[MAX_QUESTIONS], codings = 0;
    for (int i = 0; i < attempt->count; i++) {
        if (QuizIsCoding(&lesson->questions[i])) coding[codings++] = i;
        else choice[choices++] = i;
    }
    // Fisher-Yates with a small xorshift generator, so a seed always gives the same quiz.
    if (seed == 0) seed = 0x9e3779b9u;
    for (int i = choices - 1; i > 0; i--) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        int j = (int)(seed % (unsigned)(i + 1));
        int t = choice[i]; choice[i] = choice[j]; choice[j] = t;
    }
    if (choices > QUIZ_CHOICE_QUESTIONS) choices = QUIZ_CHOICE_QUESTIONS;
    attempt->count = 0;
    for (int i = 0; i < choices; i++) attempt->order[attempt->count++] = choice[i];
    for (int i = 0; i < codings; i++) attempt->order[attempt->count++] = coding[i];
}

int QuizLength(const Lesson *lesson)
{
    int choices = 0, codings = 0;
    for (int i = 0; i < lesson->questionCount && i < MAX_QUESTIONS; i++) {
        if (QuizIsCoding(&lesson->questions[i])) codings++;
        else choices++;
    }
    return (choices < QUIZ_CHOICE_QUESTIONS ? choices : QUIZ_CHOICE_QUESTIONS) + codings;
}

static bool ValidQuestion(const QuizAttempt *attempt, int position)
{
    return attempt->lesson && position >= 0 && position < attempt->count && position < MAX_QUESTIONS;
}

const Question *QuizAttemptQuestion(const QuizAttempt *attempt, int position)
{
    return ValidQuestion(attempt, position) ? &attempt->lesson->questions[attempt->order[position]] : NULL;
}

bool QuizAttemptPick(QuizAttempt *attempt, int question, int choice)
{
    if (attempt->completed || !ValidQuestion(attempt, question) || attempt->answers[question] >= 0) return false;
    const Question *q = QuizAttemptQuestion(attempt, question);
    if (!QuizChoiceValid(q, choice)) return false;
    attempt->answers[question] = choice;
    return true;
}

int QuizAttemptCorrect(const QuizAttempt *attempt)
{
    int correct = 0;
    for (int i = 0; ValidQuestion(attempt, i); i++) {
        const Question *q = QuizAttemptQuestion(attempt, i);
        if (QuizAnswerCorrect(q, attempt->answers[i])) correct++;
    }
    return correct;
}

int QuizAttemptFinish(QuizAttempt *attempt)
{
    attempt->completed = attempt->lesson && attempt->count > 0;
    return attempt->completed ? QuizAttemptCorrect(attempt) * 100 / attempt->count : 0;
}

bool QuizAttemptReview(const QuizAttempt *attempt, int question, QuizReviewItem *item)
{
    memset(item, 0, sizeof *item);
    if (!attempt->completed || !ValidQuestion(attempt, question)) return false;
    QuizQuestionReview(QuizAttemptQuestion(attempt, question), attempt->answers[question], item);
    return true;
}
