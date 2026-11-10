// Checks predicted values/output locally against the lesson author's answer.
#include "tracing.h"
#include <ctype.h>
#include <string.h>

bool TracingValid(const TraceExercise *trace)
{
    if (!trace || !trace->id[0] || !trace->code || !*trace->code || !trace->prompt || !*trace->prompt
        || !trace->answer || !*trace->answer || !trace->explain || !*trace->explain) return false;
    if (strlen(trace->answer) >= 512) return false;
    bool nonblank = false;
    for (const char *p = trace->answer; *p; p++) {
        if ((unsigned char)*p > 126 || ((unsigned char)*p < 32 && !isspace((unsigned char)*p))) return false;
        if (!isspace((unsigned char)*p)) nonblank = true;
    }
    if (!nonblank) return false;
    for (const char *p = trace->id; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return false;
    return true;
}

bool TracingAnswer(const TraceExercise *trace, const char *answer)
{
    if (!TracingValid(trace) || !answer) return false;
    const char *expected = trace->answer;
    while (true) {
        while (isspace((unsigned char)*answer)) answer++;
        while (isspace((unsigned char)*expected)) expected++;
        if (!*answer || !*expected) return *answer == *expected;
        const char *a = answer, *e = expected;
        while (*answer && !isspace((unsigned char)*answer)) answer++;
        while (*expected && !isspace((unsigned char)*expected)) expected++;
        size_t n = (size_t)(answer - a);
        if (n != (size_t)(expected - e) || strncmp(a, e, n)) return false;
    }
}
