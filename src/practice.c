// Custom revision sessions. This module has no dependency on progress or accounts.
#include "practice.h"
#include <stdlib.h>
#include <string.h>

static bool SameText(const char *a, const char *b) { return strcmp(a ? a : "", b ? b : "") == 0; }

static bool SameQuestion(const Question *a, const Question *b)
{
    if (a == b) return true;
    if (!SameText(a->prompt, b->prompt) || !SameText(a->code, b->code) || a->choiceCount != b->choiceCount
        || !SameText(a->choices[a->correct], b->choices[b->correct])) return false;
    bool used[MAX_CHOICES] = { false };
    for (int i = 0; i < a->choiceCount; i++) {
        bool found = false;
        for (int j = 0; j < b->choiceCount; j++)
            if (!used[j] && SameText(a->choices[i], b->choices[j])) { used[j] = true; found = true; break; }
        if (!found) return false;
    }
    return true;
}

int PracticeLessonQuestions(const Lesson *lesson)
{
    int count = 0;
    for (int q = 0; q < lesson->questionCount && q < MAX_QUESTIONS; q++)
        if (!QuizIsCoding(&lesson->questions[q])) count++;
    return count;
}

static bool Eligible(const PracticeSession *s, int lesson)
{
    return s->selected && lesson >= 0 && lesson < s->lessons->count
        && s->lessons->items[lesson].track == s->track && s->selected[lesson];
}

// Pool construction also removes identical questions occurring in multiple lessons.
static PracticeQuestion *BuildPool(const PracticeSession *s, int *count)
{
    *count = 0;
    int capacity = 0;
    for (int i = 0; i < s->lessons->count; i++) if (Eligible(s, i)) capacity += s->lessons->items[i].questionCount;
    if (!capacity) return NULL;
    PracticeQuestion *pool = malloc((size_t)capacity * sizeof *pool);
    if (!pool) return NULL;
    for (int i = 0; i < s->lessons->count; i++) {
        if (!Eligible(s, i)) continue;
        const Lesson *lesson = &s->lessons->items[i];
        for (int q = 0; q < lesson->questionCount && q < MAX_QUESTIONS; q++) {
            const Question *question = &lesson->questions[q];
            // Coding questions need the code editor, which only the lesson quiz opens.
            if (QuizIsCoding(question)) continue;
            if (!question->prompt || question->choiceCount > MAX_CHOICES || !QuizChoiceValid(question, question->correct)) continue;
            bool duplicate = false;
            for (int j = 0; j < *count; j++) {
                const Question *other = &s->lessons->items[pool[j].lesson].questions[pool[j].question];
                if (SameQuestion(question, other)) { duplicate = true; break; }
            }
            if (!duplicate) pool[(*count)++] = (PracticeQuestion){ i, q, -1 };
        }
    }
    return pool;
}

static void Refresh(PracticeSession *s)
{
    s->selectedLessons = 0;
    for (int i = 0; i < s->lessons->count; i++) if (Eligible(s, i)) s->selectedLessons++;
    int count;
    PracticeQuestion *pool = BuildPool(s, &count);
    free(pool);
    s->available = count;
    PracticeCount(s, s->requested);
}

bool PracticeInit(PracticeSession *s, const LessonSet *lessons)
{
    *s = (PracticeSession){ .lessons = lessons, .track = TRACK_C, .requested = 1 };
    s->selected = calloc((size_t)(lessons->count ? lessons->count : 1), sizeof *s->selected);
    return s->selected != NULL;
}

void PracticeFree(PracticeSession *s)
{
    free(s->selected);
    free(s->questions);
    *s = (PracticeSession){ 0 };
}

void PracticeTrack(PracticeSession *s, Track track)
{
    if (s->track == track) return;
    s->track = track;
    if (s->selected) memset(s->selected, 0, (size_t)s->lessons->count * sizeof *s->selected);
    Refresh(s);
}

void PracticeSelect(PracticeSession *s, int lesson, bool selected)
{
    if (!s->selected || lesson < 0 || lesson >= s->lessons->count) return;
    const Lesson *item = &s->lessons->items[lesson];
    if (item->track != s->track || !PracticeLessonQuestions(item)) return;
    s->selected[lesson] = selected;
    Refresh(s);
}

void PracticeSelectAll(PracticeSession *s, bool selected)
{
    for (int i = 0; i < s->lessons->count; i++)
        if (s->selected) s->selected[i] = selected && s->lessons->items[i].track == s->track && PracticeLessonQuestions(&s->lessons->items[i]) > 0;
    Refresh(s);
}

void PracticeCount(PracticeSession *s, int requested)
{
    s->requested = requested < 1 ? 1 : requested;
    if (s->available && s->requested > s->available) s->requested = s->available;
    if (!s->available) s->requested = 1; // disabled until a nonempty pool exists
}

static uint32_t Random(uint32_t *seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}

bool PracticeStart(PracticeSession *s, uint32_t seed)
{
    int count;
    PracticeQuestion *pool = BuildPool(s, &count);
    if (!pool || !count) { free(pool); return false; }
    s->available = count;
    PracticeCount(s, s->requested);
    if (!seed) seed = 0x9e3779b9u;
    for (int i = count - 1; i > 0; i--) {
        int j = (int)(Random(&seed) % (uint32_t)(i + 1));
        PracticeQuestion temp = pool[i]; pool[i] = pool[j]; pool[j] = temp;
    }
    bool same = s->count == s->requested && s->questions;
    for (int i = 0; same && i < s->requested; i++)
        same = pool[i].lesson == s->questions[i].lesson && pool[i].question == s->questions[i].question;
    if (same && count > 1) {
        PracticeQuestion temp = pool[0]; pool[0] = pool[1]; pool[1] = temp;
    }
    free(s->questions);
    s->questions = pool;
    s->count = s->requested;
    s->current = 0;
    s->completed = false;
    return true;
}

const Lesson *PracticeSource(const PracticeSession *s, int index)
{
    if (!s->questions || index < 0 || index >= s->count) return NULL;
    return &s->lessons->items[s->questions[index].lesson];
}

const Question *PracticeQuestionAt(const PracticeSession *s, int index)
{
    const Lesson *lesson = PracticeSource(s, index);
    return lesson ? &lesson->questions[s->questions[index].question] : NULL;
}

bool PracticePick(PracticeSession *s, int choice)
{
    const Question *q = PracticeQuestionAt(s, s->current);
    if (s->completed || !q || s->questions[s->current].answer >= 0 || !QuizChoiceValid(q, choice)) return false;
    s->questions[s->current].answer = choice;
    return true;
}

bool PracticeAdvance(PracticeSession *s)
{
    if (s->completed || !s->questions || s->current >= s->count || s->questions[s->current].answer < 0) return false;
    if (s->current + 1 == s->count) s->completed = true;
    else s->current++;
    return true;
}

int PracticeCorrect(const PracticeSession *s)
{
    int correct = 0;
    for (int i = 0; i < s->count; i++) if (QuizAnswerCorrect(PracticeQuestionAt(s, i), s->questions[i].answer)) correct++;
    return correct;
}

int PracticePercent(const PracticeSession *s) { return s->count ? PracticeCorrect(s) * 100 / s->count : 0; }

bool PracticeReview(const PracticeSession *s, int index, QuizReviewItem *item)
{
    memset(item, 0, sizeof *item);
    if (!s->completed || !PracticeQuestionAt(s, index)) return false;
    QuizQuestionReview(PracticeQuestionAt(s, index), s->questions[index].answer, item);
    return true;
}
