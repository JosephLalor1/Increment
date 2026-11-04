#include "../src/quiz_attempt.h"
#include "../src/progress.h"
#include "raylib.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void CheckReview(const Lesson *lesson)
{
    QuizAttempt attempt;
    QuizAttemptBegin(&attempt, lesson);
    QuizReviewItem item;
    assert(!QuizAttemptReview(&attempt, 0, &item)); // unfinished attempt is not a review
    assert(!QuizAttemptPick(&attempt, -1, 0));
    assert(!QuizAttemptPick(&attempt, MAX_QUESTIONS, 0));
    assert(!QuizAttemptPick(&attempt, 0, MAX_CHOICES));
    assert(!QuizAttemptPick(&attempt, 0, -1));
    assert(QuizAttemptPick(&attempt, 0, lesson->questions[0].correct));
    assert(!QuizAttemptPick(&attempt, 0, (lesson->questions[0].correct + 1) % lesson->questions[0].choiceCount));
    int wrong = (lesson->questions[1].correct + 1) % lesson->questions[1].choiceCount;
    assert(QuizAttemptPick(&attempt, 1, wrong));
    assert(QuizAttemptCorrect(&attempt) == 1);
    assert(QuizAttemptFinish(&attempt) == 100 / lesson->questionCount);
    assert(!QuizAttemptPick(&attempt, 2, 0)); // completed selections cannot be overwritten

    assert(QuizAttemptReview(&attempt, 0, &item));
    assert(item.question == &lesson->questions[0]);
    assert(item.answered && item.correct);
    assert(item.selectedAnswer == lesson->questions[0].choices[lesson->questions[0].correct]);
    assert(item.correctAnswer == item.selectedAnswer);
    assert(item.explanation == lesson->questions[0].explain);
    assert(QuizAttemptReview(&attempt, 1, &item));
    assert(item.answered && !item.correct);
    assert(item.selectedAnswer == lesson->questions[1].choices[wrong]);
    assert(item.correctAnswer == lesson->questions[1].choices[lesson->questions[1].correct]);
    assert(QuizAttemptReview(&attempt, 2, &item));
    assert(!item.answered && !item.correct && item.selectedAnswer == NULL);
    assert(!QuizAttemptReview(&attempt, lesson->questionCount, &item));

    ProgressRecord(lesson->id, 100);
    // Opening/reading/leaving review has no progress side effects, on disk or in memory.
    char *before = LoadFileText("build/test_quiz_attempt_progress.txt");
    assert(before);
    for (int pass = 0; pass < 3; pass++)
        for (int i = 0; i < lesson->questionCount; i++) assert(QuizAttemptReview(&attempt, i, &item));
    assert(ProgressBest(lesson->id) == 100);
    char *after = LoadFileText("build/test_quiz_attempt_progress.txt");
    assert(after && strcmp(before, after) == 0);
    UnloadFileText(before);
    UnloadFileText(after);

    QuizAttemptBegin(&attempt, lesson); // retry clears current answers, retains best score
    assert(!attempt.completed && QuizAttemptCorrect(&attempt) == 0);
    for (int i = 0; i < MAX_QUESTIONS; i++) assert(attempt.answers[i] == -1);
    assert(ProgressBest(lesson->id) == 100);
    for (int i = 0; i < lesson->questionCount; i++)
        assert(QuizAttemptPick(&attempt, i, (lesson->questions[i].correct + 1) % lesson->questions[i].choiceCount));
    ProgressRecord(lesson->id, QuizAttemptFinish(&attempt)); // lower retry uses normal best-score logic
    assert(ProgressBest(lesson->id) == 100);
    QuizAttemptBegin(&attempt, NULL); // account change / clear
    assert(!QuizAttemptReview(&attempt, 0, &item));
    assert(QuizAttemptFinish(&attempt) == 0);
}

int main(void)
{
    SetTraceLogLevel(LOG_ERROR);
    const char *path = "build/test_quiz_attempt_progress.txt";
    remove(path);
    ProgressLoad(path);
    LessonSet lessons;
    assert(LessonsLoad(&lessons, "assets/lessons"));
    bool checked[TRACK_COUNT] = { false };
    for (int i = 0; i < lessons.count; i++) {
        Lesson *lesson = &lessons.items[i];
        if (checked[lesson->track] || lesson->questionCount < 3) continue;
        CheckReview(lesson);
        checked[lesson->track] = true;
    }
    assert(checked[TRACK_C] && checked[TRACK_CPP]);

    Lesson *fixture = calloc(1, sizeof *fixture);
    assert(fixture);
    fixture->questionCount = 1;
    fixture->questions[0] = (Question){ .prompt = "Example", .choices = { "A", "B" }, .choiceCount = 2, .correct = 1 };
    QuizAttempt attempt;
    QuizReviewItem item;
    QuizAttemptBegin(&attempt, fixture);
    QuizAttemptFinish(&attempt);
    assert(QuizAttemptReview(&attempt, 0, &item) && !item.explanation && !item.answered);
    fixture->questions[0].explain = "";
    assert(QuizAttemptReview(&attempt, 0, &item) && !item.explanation);
    fixture->questions[0].explain = "Existing explanation";
    assert(QuizAttemptReview(&attempt, 0, &item) && strcmp(item.explanation, "Existing explanation") == 0);
    fixture->questions[0].correct = -1;
    assert(QuizAttemptReview(&attempt, 0, &item) && !item.correctAnswer && !item.correct);

    // Every slot, including the last question, must retain its own selection.
    fixture->questionCount = MAX_QUESTIONS;
    for (int i = 0; i < MAX_QUESTIONS; i++)
        fixture->questions[i] = (Question){ .prompt = "Example", .choices = { "A", "B" }, .choiceCount = 2, .correct = 1 };
    QuizAttemptBegin(&attempt, fixture);
    for (int i = 0; i < MAX_QUESTIONS; i++) assert(QuizAttemptPick(&attempt, i, i % 2));
    assert(QuizAttemptFinish(&attempt) == 50);
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        assert(QuizAttemptReview(&attempt, i, &item));
        assert(item.answered && item.correct == (i % 2 == 1));
        assert(strcmp(item.selectedAnswer, i % 2 ? "B" : "A") == 0);
        assert(strcmp(item.correctAnswer, "B") == 0);
    }
    QuizAttemptBegin(&attempt, fixture);
    for (int i = 0; i < MAX_QUESTIONS; i++) assert(QuizAttemptPick(&attempt, i, 1));
    assert(QuizAttemptFinish(&attempt) == 100);
    fixture->questionCount = 0;
    QuizAttemptBegin(&attempt, fixture);
    assert(QuizAttemptFinish(&attempt) == 0 && !attempt.completed);
    assert(!QuizAttemptReview(&attempt, 0, &item));
    free(fixture);
    LessonsUnload(&lessons);
    ProgressLoad("");
    remove(path);
    puts("All quiz attempt/review tests passed");
    return 0;
}
