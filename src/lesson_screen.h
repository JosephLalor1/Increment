#ifndef LESSON_SCREEN_H
#define LESSON_SCREEN_H

#include "lesson.h"

#include <stdbool.h>

// One lesson: reading it (text, examples, exercises, and a Save button that bookmarks it),
// its quiz, result and answer review.
// Moving between the pages, and on to the next lesson, happens here; leaving
// the lesson or opening the code editor is returned to main.c as an action.

typedef enum {
    LESSON_STAY,           // nothing to do
    LESSON_BACK,           // leave the lesson (to wherever it was opened from)
    LESSON_TRY_EXAMPLE,    // "Try it" on an example: open it in the code editor
    LESSON_OPEN_EXERCISE,  // open an exercise in the code editor
    LESSON_ROADMAP,
    LESSON_PROGRESS,
} LessonActionKind;

typedef struct {
    LessonActionKind kind;
    const char *exampleCode;  // LESSON_TRY_EXAMPLE: the example (owned by the lesson)
    int exerciseIndex;        // LESSON_OPEN_EXERCISE: zero-based
} LessonAction;

typedef enum { LESSON_PAGE_READING, LESSON_PAGE_QUIZ, LESSON_PAGE_RESULT, LESSON_PAGE_REVIEW, LESSON_PAGE_CELEBRATION } LessonPage;

// Opens lesson `lessonIndex` of set on its reading page. backLabel names where Back goes
// ("All lessons", "Dashboard"); the result page uses it on a button. set must outlive the screen.
void LessonScreenOpen(const LessonSet *set, int lessonIndex, const char *backLabel);
void LessonScreenClearAttempt(void); // drop answers on sign-out/account change

// Draws the current page and handles input. Coming back from the code editor, the reading
// page is where it was left.
LessonAction LessonScreen(void);

const Lesson *LessonScreenLesson(void);  // the open lesson

// Dev aid for --shot: show a page at a scroll position. answered (quiz page) picks the first
// choice of the first question; result/review use synthetic right/wrong/unanswered choices
// without recording a score.
void LessonScreenDemo(LessonPage page, float scroll, bool answered);

#endif
