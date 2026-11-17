// Timed coding checkpoint, attempt history and frozen submission review.
#ifndef EXAM_SCREEN_H
#define EXAM_SCREEN_H
#include "learning/checkpoints.h"
typedef enum { EXAM_SCREEN_STAY,EXAM_SCREEN_HOME,EXAM_SCREEN_PRACTICE } ExamScreenChoice;
typedef struct { ExamScreenChoice kind; const Lesson *practice; } ExamScreenAction;
void ExamScreenOpen(const LessonSet *set,Track track,int section);
ExamScreenAction ExamScreen(void);
// On normal application exit, active submissions are retained as interrupted attempts.
void ExamScreenShutdown(void);
// Developer previews never save or grant passes. page: 0 intro, 1 active, 2 results.
void ExamScreenDemo(int page);
bool ExamScreenBusy(void);
#endif
