// Frozen exam submissions, deadline handling and task buffers; no drawing or progress writes.
#ifndef EXAM_SESSION_H
#define EXAM_SESSION_H
#include "learning/exam_history.h"
typedef struct { ExamRecord record; double started,deadline; bool active,submitted; } ExamSession;
bool ExamSessionStart(ExamSession *session,const LessonSet *set,const Checkpoint *exam,double now);
// Updating code is allowed only before submission. Task switching never discards buffers.
bool ExamSessionCode(ExamSession *session,int task,const char *code);
// Absolute monotonic deadline, independent of frame rate; nonnegative rounded seconds.
int ExamSessionRemaining(const ExamSession *session,double now);
// Idempotent freeze; elapsed is clamped to the configured duration.
bool ExamSessionSubmit(ExamSession *session,double now);
// Each task has equal weight; its marks are the share of assessment tests passed.
void ExamSessionScore(ExamRecord *record);
void ExamSessionFree(ExamSession *session);
// A borrowed Step view of a saved reference, for existing checks and revision mode.
Step ExamTaskStep(const ExamTask *task);
#endif
