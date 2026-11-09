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
    for (int i = 0; i < lesson->questionCount; i++) {
        const Question *q = &lesson->questions[i];
        assert(QuizAttemptPick(&attempt, i, QuizIsCoding(q) ? QUIZ_CODE_SKIPPED : (q->correct + 1) % q->choiceCount));
    }
    ProgressRecord(lesson->id, QuizAttemptFinish(&attempt)); // lower retry uses normal best-score logic
    assert(ProgressBest(lesson->id) == 100);
    QuizAttemptBegin(&attempt, NULL); // account change / clear
    assert(!QuizAttemptReview(&attempt, 0, &item));
    assert(QuizAttemptFinish(&attempt) == 0);
}

// Coding questions: passing the program is the right answer, skipping is wrong, and the
// review hands over the task so its expected output and solution can be shown.
static void CheckCodingQuestions(void)
{
    Exercise task = { .title = "Coding question", .description = "Print hi", .starter = "", .expected = "hi",
                      .solution = "int main() {}" };
    Lesson *fixture = calloc(1, sizeof *fixture);
    assert(fixture);
    fixture->questionCount = 2;
    fixture->questions[0] = (Question){ .prompt = "Print hi", .correct = -1, .task = &task };
    fixture->questions[1] = (Question){ .prompt = "Print hi again", .correct = -1, .task = &task };
    const Question *q = &fixture->questions[0];
    assert(QuizIsCoding(q) && !QuizIsCoding(NULL));
    assert(QuizChoiceValid(q, QUIZ_CODE_PASSED) && QuizChoiceValid(q, QUIZ_CODE_SKIPPED));
    assert(!QuizChoiceValid(q, -1) && !QuizChoiceValid(q, 2));
    assert(QuizAnswerCorrect(q, QUIZ_CODE_PASSED) && !QuizAnswerCorrect(q, QUIZ_CODE_SKIPPED));

    QuizAttempt attempt;
    QuizAttemptBegin(&attempt, fixture);
    assert(!QuizAttemptPick(&attempt, 0, 5));
    assert(QuizAttemptPick(&attempt, 0, QUIZ_CODE_PASSED));
    assert(!QuizAttemptPick(&attempt, 0, QUIZ_CODE_SKIPPED));  // a pass can't be undone by skipping
    assert(QuizAttemptPick(&attempt, 1, QUIZ_CODE_SKIPPED));
    assert(QuizAttemptFinish(&attempt) == 50);

    QuizReviewItem item;
    assert(QuizAttemptReview(&attempt, 0, &item));
    assert(item.task == &task && item.answered && item.correct && item.selectedAnswer && item.correctAnswer);
    assert(QuizAttemptReview(&attempt, 1, &item));
    assert(item.task == &task && item.answered && !item.correct && strcmp(item.selectedAnswer, "Skipped") == 0);
    free(fixture);
}

// The lessons' coding questions arrive complete: a task with starter code, expected output
// and a solution, and no multiple-choice answers.
static void CheckLessonCodingQuestions(const LessonSet *lessons)
{
    int found = 0;
    for (int i = 0; i < lessons->count; i++) {
        const Lesson *lesson = &lessons->items[i];
        for (int k = 0; k < lesson->questionCount; k++) {
            const Question *q = &lesson->questions[k];
            if (!QuizIsCoding(q)) continue;
            found++;
            assert(q->choiceCount == 0 && q->prompt && q->prompt[0]);
            assert(q->task->starter && q->task->starter[0] && q->task->expected && q->task->solution);
            assert(strcmp(q->task->description, q->prompt) == 0);
        }
    }
    assert(found > 0);
    printf("%d lesson coding questions parse\n", found);
}

// Every lesson question arrives with a difficulty and the skills it exercises, from its "~"
// line, and the parser reads such a line exactly.
static void CheckSkillTags(const LessonSet *lessons)
{
    int tagged = 0;
    for (int l = 0; l < lessons->count; l++) {
        const Lesson *lesson = &lessons->items[l];
        for (int q = 0; q < lesson->questionCount; q++) {
            const Question *question = &lesson->questions[q];
            assert(question->difficulty >= 1 && question->difficulty <= SKILL_MAX_DIFFICULTY);
            assert(question->skillCount >= 1 && question->skillCount <= MAX_QUESTION_SKILLS);
            for (int s = 0; s < question->skillCount; s++) {
                assert(question->skills[s].skill < SKILL_COUNT);
                assert(question->skills[s].weight > 0 && question->skills[s].weight <= 1);
            }
            tagged++;
        }
    }
    assert(tagged > 300);

    // A lesson written here: the "~" line's difficulty, skills and weights, and bad parts skipped.
    const char *path = "build/test_skill_tags/a_lesson.txt";
    MakeDirectory("build/test_skill_tags");
    SaveFileText(path, "@quiz\n? Q?\n~ 3 tracing:1 pointers:0.3 nonsense:1 loops:2\n- a\n* b\n! e\n");
    LessonSet one;
    assert(LessonsLoad(&one, "build/test_skill_tags"));
    const Question *q = &one.items[0].questions[0];
    assert(q->difficulty == 3 && q->skillCount == 2);
    assert(q->skills[0].skill == SKILL_TRACING && q->skills[0].weight == 1);
    assert(q->skills[1].skill == SKILL_POINTERS && q->skills[1].weight > 0.29f && q->skills[1].weight < 0.31f);
    LessonsUnload(&one);
    remove(path);
}

// Drawn quizzes: up to QUIZ_CHOICE_QUESTIONS different multiple-choice questions, then
// every coding question; the same seed gives the same quiz, and different seeds vary it.
static void CheckDrawn(const LessonSet *lessons)
{
    bool varied = false;
    for (int l = 0; l < lessons->count; l++) {
        const Lesson *lesson = &lessons->items[l];
        int choices = 0, codings = 0;
        for (int i = 0; i < lesson->questionCount; i++) {
            if (QuizIsCoding(&lesson->questions[i])) codings++;
            else choices++;
        }
        QuizAttempt a, b, c;
        QuizAttemptBeginDrawn(&a, lesson, 11);
        QuizAttemptBeginDrawn(&b, lesson, 11);
        QuizAttemptBeginDrawn(&c, lesson, 12345);
        int expected = (choices < QUIZ_CHOICE_QUESTIONS ? choices : QUIZ_CHOICE_QUESTIONS) + codings;
        assert(a.count == expected && c.count == expected);
        for (int i = 0; i < a.count; i++) {
            assert(a.order[i] == b.order[i]);
            if (a.order[i] != c.order[i]) varied = true;
            for (int j = i + 1; j < a.count; j++) assert(a.order[i] != a.order[j]);
            bool coding = QuizIsCoding(QuizAttemptQuestion(&a, i));
            assert(coding == (i >= a.count - codings));  // coding questions come last
            assert(a.answers[i] == -1);
        }
        assert(QuizAttemptQuestion(&a, a.count) == NULL);
    }
    assert(varied);
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
    CheckCodingQuestions();
    CheckDrawn(&lessons);
    CheckSkillTags(&lessons);
    CheckLessonCodingQuestions(&lessons);

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
