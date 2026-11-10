// Local, deterministic checking for authored code predictions; never executes code.
#ifndef TRACING_H
#define TRACING_H
#include "lesson.h"
// Requires a complete task and a whitespace-free stable id; malformed tasks are disabled.
bool TracingValid(const TraceExercise *trace);
// Case-sensitive token comparison; leading, trailing and repeated whitespace is ignored.
bool TracingAnswer(const TraceExercise *trace, const char *answer);
#endif
