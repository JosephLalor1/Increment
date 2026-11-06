// Practice randomisation, review, track isolation and unchanged saved lesson progress.
#include "../src/practice.h"
#include "../src/progress.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Question QuestionData(char *prompt, char *code)
{
    return (Question){ .prompt = prompt, .code = code, .choices = { "Yes", "No" }, .choiceCount = 2, .correct = 0 };
}

static void Finish(PracticeSession *s, bool right)
{
    while (!s->completed) {
        const Question *q = PracticeQuestionAt(s, s->current);
        assert(PracticePick(s, right ? q->correct : (q->correct + 1) % q->choiceCount));
        assert(!PracticePick(s, q->correct));
        assert(PracticeAdvance(s));
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetTraceLogLevel(LOG_ERROR);
    char path[1024];
    snprintf(path, sizeof path, "%s/practice_progress.txt", argv[1]);
    ProgressLoad(path);
    ProgressRecord("c_alpha", 60);
    ProgressRecord("cpp_beta", 80);
    char *before = LoadFileText(path);
    assert(before);
    LessonSet set = { calloc(6, sizeof(Lesson)), 6 };
    assert(set.items);
    char *ids[] = { "c_alpha", "cpp_beta", "c_empty", "c_gamma", "cpp_one", "c_alias" };
    int counts[] = { 3, 2, 0, 3, 1, 1 };
    for (int i = 0; i < set.count; i++) {
        snprintf(set.items[i].id, sizeof set.items[i].id, "%s", ids[i]);
        snprintf(set.items[i].title, sizeof set.items[i].title, "Lesson %d", i);
        set.items[i].track = i == 1 || i == 4 ? TRACK_CPP : TRACK_C;
        set.items[i].questionCount = counts[i];
    }
    set.items[0].questions[0] = QuestionData("Shared question", NULL);
    set.items[0].questions[1] = QuestionData("Variables", NULL);
    set.items[0].questions[1].explain = "An existing explanation.";
    set.items[0].questions[2] = QuestionData("Loops", NULL);
    set.items[1].questions[0] = QuestionData("C++ question 1", NULL);
    set.items[1].questions[1] = QuestionData("C++ question 2", NULL);
    set.items[3].questions[0] = QuestionData("Shared question", NULL);
    set.items[3].questions[0].choices[0] = "No";
    set.items[3].questions[0].choices[1] = "Yes";
    set.items[3].questions[0].correct = 1; // reordered choices still represent the same question
    set.items[3].questions[1] = QuestionData("Functions", NULL);
    set.items[3].questions[2] = QuestionData("Shared question", "int x = 2;"); // code makes this distinct
    set.items[4].questions[0] = QuestionData("Only question", NULL);
    set.items[5].questions[0] = set.items[0].questions[0]; // aliased content must not duplicate

    PracticeSession s;
    assert(PracticeInit(&s, &set));
    assert(!PracticeStart(&s, 1) && s.available == 0);
    PracticeSelect(&s, 1, true); // other track
    PracticeSelect(&s, 2, true); // zero questions
    assert(s.available == 0);
    PracticeSelect(&s, 0, true);
    assert(s.available == 3 && s.selectedLessons == 1);
    PracticeCount(&s, 99);
    assert(s.requested == 3);
    PracticeSelect(&s, 3, true);
    PracticeSelect(&s, 5, true);
    assert(s.available == 5 && s.selectedLessons == 3);
    PracticeCount(&s, s.available);
    assert(PracticeStart(&s, 77) && s.count == 5);
    assert(!PracticeAdvance(&s) && !PracticePick(&s, -1) && !PracticePick(&s, MAX_CHOICES));
    PracticeQuestion old[5];
    memcpy(old, s.questions, sizeof old);
    for (int i = 0; i < s.count; i++) {
        const Lesson *source = PracticeSource(&s, i);
        assert(source && source->track == TRACK_C && s.selected[s.questions[i].lesson]);
        assert(PracticeQuestionAt(&s, i) == &source->questions[s.questions[i].question]);
        for (int j = 0; j < i; j++)
            assert(s.questions[i].lesson != s.questions[j].lesson || s.questions[i].question != s.questions[j].question);
    }
    QuizReviewItem item;
    assert(!PracticeReview(&s, 0, &item));
    Finish(&s, true);
    assert(PracticeCorrect(&s) == 5 && PracticePercent(&s) == 100);
    assert(!PracticeAdvance(&s) && !PracticePick(&s, 0));
    for (int i = 0; i < s.count; i++) {
        assert(PracticeReview(&s, i, &item) && item.correct && item.answered);
        assert(item.selectedAnswer == item.correctAnswer);
        assert(item.explanation == PracticeQuestionAt(&s, i)->explain);
    }
    assert(!PracticeReview(&s, -1, &item) && !PracticeReview(&s, 99, &item));
    assert(PracticeStart(&s, 77)); // same seed cannot force an identical consecutive order
    bool different = false;
    for (int i = 0; i < s.count; i++) {
        different |= old[i].lesson != s.questions[i].lesson || old[i].question != s.questions[i].question;
        assert(s.questions[i].answer == -1);
    }
    assert(different && !s.completed && s.current == 0);
    Finish(&s, false);
    assert(PracticePercent(&s) == 0);
    for (uint32_t seed = 1; seed <= 25; seed++) {
        PracticeCount(&s, 2);
        assert(PracticeStart(&s, seed) && s.count == 2);
        assert(s.questions[0].lesson != s.questions[1].lesson || s.questions[0].question != s.questions[1].question);
    }
    PracticeCount(&s, 5);
    PracticeSelect(&s, 3, false);
    assert(s.available == 3 && s.requested == 3);
    PracticeTrack(&s, TRACK_CPP);
    assert(s.available == 0 && s.selectedLessons == 0 && !PracticeStart(&s, 1));
    PracticeSelect(&s, 4, true);
    assert(s.available == 1);
    PracticeCount(&s, -2);
    assert(PracticeStart(&s, 1) && s.count == 1);
    assert(PracticeSource(&s, 0) == &set.items[4]);
    Finish(&s, true);
    assert(PracticePercent(&s) == 100 && PracticeReview(&s, 0, &item) && !item.explanation);
    snprintf(set.items[4].title, sizeof set.items[4].title, "Renamed source lesson");
    assert(strcmp(PracticeSource(&s, 0)->title, "Renamed source lesson") == 0);
    PracticeSelectAll(&s, true);
    assert(s.available == 3 && s.selectedLessons == 2);
    PracticeCount(&s, 2);
    assert(PracticeStart(&s, 1));
    assert(PracticePick(&s, PracticeQuestionAt(&s, 0)->correct) && PracticeAdvance(&s));
    assert(PracticePick(&s, (PracticeQuestionAt(&s, 1)->correct + 1) % 2) && PracticeAdvance(&s));
    assert(s.completed && PracticePercent(&s) == 50 && PracticeCorrect(&s) == 1);
    PracticeTrack(&s, TRACK_COUNT); // no selected track is a safe disabled setup
    PracticeSelectAll(&s, true);
    assert(!PracticeStart(&s, 0));
    char *after = LoadFileText(path);
    assert(after && strcmp(before, after) == 0);
    assert(ProgressBest("c_alpha") == 60 && !ProgressLessonPassed("c_alpha"));
    assert(ProgressBest("cpp_beta") == 80 && ProgressBest("c_gamma") == -1);
    UnloadFileText(before); UnloadFileText(after);
    PracticeFree(&s);
    assert(!s.selected && !s.questions && !s.lessons);
    LessonSet reordered = { calloc(2, sizeof(Lesson)), 2 };
    assert(reordered.items);
    reordered.items[0] = set.items[4];
    reordered.items[1] = set.items[0];
    assert(PracticeInit(&s, &reordered));
    PracticeTrack(&s, TRACK_CPP);
    PracticeSelectAll(&s, true);
    assert(PracticeStart(&s, 2) && s.count == 1 && PracticeSource(&s, 0) == &reordered.items[0]);
    PracticeFree(&s);
    free(reordered.items);
    LessonSet empty = { 0 };
    assert(PracticeInit(&s, &empty));
    PracticeSelectAll(&s, true);
    assert(!PracticeStart(&s, 1));
    PracticeFree(&s);
    LessonSet real;
    assert(LessonsLoad(&real, "assets/lessons"));
    assert(PracticeInit(&s, &real));
    PracticeSelectAll(&s, true);
    PracticeCount(&s, s.available);
    assert(PracticeStart(&s, 3) && s.count > MAX_QUESTIONS); // multi-lesson sessions have no 16-question cap
    Finish(&s, true);
    assert(PracticePercent(&s) == 100 && PracticeReview(&s, s.count - 1, &item));
    PracticeFree(&s);
    LessonsUnload(&real);
    free(set.items);
    ProgressLoad("");
    remove(path);
    puts("All custom practice quiz tests passed");
    return 0;
}
