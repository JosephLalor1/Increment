// Read-only revision session data, random selection and answer tracking.
#ifndef PRACTICE_H
#define PRACTICE_H

#include "quiz_attempt.h"
#include <stdint.h>

// References into the immutable LessonSet, never copies of question banks.
typedef struct { int lesson, question, answer; } PracticeQuestion;
typedef struct {
    const LessonSet *lessons;
    Track track;
    bool *selected;
    int requested, available, selectedLessons;
    PracticeQuestion *questions;
    int count, current;
    bool completed;
} PracticeSession;

// How many of a lesson's questions a practice session can ask: all but coding questions,
// which need the code editor and so only appear in the lesson's own quiz.
int PracticeLessonQuestions(const Lesson *lesson);

// LessonSet must outlive the session. Init/free also clear account-specific selections.
bool PracticeInit(PracticeSession *session, const LessonSet *lessons);
void PracticeFree(PracticeSession *session);
// Track changes clear lesson selections and keep counts valid. TRACK_COUNT means none.
void PracticeTrack(PracticeSession *session, Track track);
void PracticeSelect(PracticeSession *session, int lesson, bool selected);
void PracticeSelectAll(PracticeSession *session, bool selected);
void PracticeCount(PracticeSession *session, int requested);
// Fisher-Yates shuffle of unique eligible references. Does not touch progress or content.
bool PracticeStart(PracticeSession *session, uint32_t seed);
const Lesson *PracticeSource(const PracticeSession *session, int index);
const Question *PracticeQuestionAt(const PracticeSession *session, int index);
bool PracticePick(PracticeSession *session, int choice);
bool PracticeAdvance(PracticeSession *session); // finish after the final answered question
int PracticeCorrect(const PracticeSession *session);
int PracticePercent(const PracticeSession *session);
bool PracticeReview(const PracticeSession *session, int index, QuizReviewItem *item);

#endif
