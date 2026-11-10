// Tracing parsing, local checking, retries and isolated per-account completion.
#include "../src/tracing.h"
#include "../src/progress.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetTraceLogLevel(LOG_ERROR);
    char folder[1024], path[1200], userA[1200], userB[1200];
    snprintf(folder, sizeof folder, "%s/tracing", argv[1]);
    MakeDirectory(folder);
    snprintf(path, sizeof path, "%s/fixture.txt", folder);
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs("@id trace_fixture\n@title Tracing\n@track C\nOriginal reading.\n"
          "@exercise Original\nDescription\n```\nint x;\n```\n@expect\n```\n2\n```\n"
          "@trace updates Variable updates\n```\nint x = 2;\nx += 3;\n```\n"
          "@predict What is x?\n@answer 5\n@explain 2 plus 3 is 5.\n"
          "@trace output Output order\n```\nprintf(\"1 3 6\");\n```\n"
          "@predict What prints?\n@answer 1 3 6\n@explain Three numbers.\n"
          "@trace broken Incomplete task\n@predict Missing code and answer\n", file);
    for (int i = 3; i < MAX_TRACES + 2; i++)
        fprintf(file, "@trace task%d Task %d\n```\nint n = %d;\n```\n@predict n?\n@answer %d\n@explain A value.\n", i, i, i, i);
    fputs("@quiz\n? Original quiz?\n* Yes\n- No\n! Same explanation.\n", file);
    fclose(file);
    LessonSet set;
    assert(LessonsLoad(&set, folder) && set.count == 1);
    Lesson *lesson = &set.items[0];
    assert(lesson->blockCount == 1 && !strcmp(lesson->blocks[0].text, "Original reading."));
    assert(lesson->exerciseCount == 1 && !strcmp(lesson->exercises[0].expected, "2"));
    assert(lesson->questionCount == 1 && !strcmp(lesson->questions[0].prompt, "Original quiz?"));
    assert(lesson->traceCount == MAX_TRACES);
    assert(!strcmp(lesson->traces[7].id, "task7") && !strcmp(lesson->traces[7].answer, "7"));
    assert(TracingValid(&lesson->traces[0]) && !TracingValid(&lesson->traces[2]));
    assert(!TracingAnswer(NULL, "5") && !TracingAnswer(&lesson->traces[0], NULL));
    assert(!TracingAnswer(&lesson->traces[0], "") && !TracingAnswer(&lesson->traces[0], "4"));
    assert(TracingAnswer(&lesson->traces[0], " 5 \r\n")); // wrong answers can be retried
    assert(TracingAnswer(&lesson->traces[1], "  1\t3\n6 "));
    assert(!TracingAnswer(&lesson->traces[1], "136") && !TracingAnswer(&lesson->traces[1], "1 3 6 7"));
    TraceExercise words = {"case", "Case", "code", "prompt", "Hello", "explanation"};
    assert(!TracingAnswer(&words, "hello") && TracingAnswer(&words, "Hello"));
    snprintf(userA, sizeof userA, "%s/trace_a.txt", argv[1]);
    snprintf(userB, sizeof userB, "%s/trace_b.txt", argv[1]);
    ProgressLoad(userA);
    ProgressRecord(lesson->id, 60);
    ProgressRecordExercise(lesson->id, 0);
    ProgressRecordGold(lesson->id);
    assert(!ProgressTraceDone(lesson->id, "updates"));
    assert(ProgressRecordTrace(lesson->id, "updates"));
    assert(ProgressBest(lesson->id) == 60 && !ProgressLessonPassed(lesson->id));
    assert(ProgressExerciseDone(lesson->id, 0) && !ProgressTraceDone(lesson->id, "output"));
    assert(ProgressGold(lesson->id));
    ProgressLoad(userB);
    assert(!ProgressTraceDone(lesson->id, "updates") && ProgressBest(lesson->id) == -1);
    ProgressLoad(userA);
    assert(ProgressTraceDone(lesson->id, "updates") && ProgressBest(lesson->id) == 60);
    assert(ProgressGold(lesson->id) && ProgressExerciseDone(lesson->id, 0));
    // Stable keys survive exercise reordering and support the full lesson/id lengths.
    TraceExercise swap = lesson->traces[0]; lesson->traces[0] = lesson->traces[1]; lesson->traces[1] = swap;
    assert(ProgressTraceDone(lesson->id, lesson->traces[1].id));
    const char *longId = "1234567890123456789012345678901";
    assert(ProgressRecordTrace(longId, longId));
    ProgressLoad(userA);
    assert(ProgressTraceDone(longId, longId));
    ProgressLoad("");
    assert(!ProgressRecordTrace(lesson->id, "updates"));
    assert(!ProgressTraceDone(lesson->id, "updates"));
    ProgressLoad(folder); // a directory is not a writable progress file
    assert(!ProgressRecordTrace(lesson->id, "updates"));
    assert(!ProgressTraceDone(lesson->id, "updates"));
    ProgressLoad("");
    LessonsUnload(&set);
    remove(path); remove(userA); remove(userB);
    assert(LessonsLoad(&set, "assets/lessons"));
    int count = 0;
    for (int i = 0; i < set.count; i++) for (int t = 0; t < set.items[i].traceCount; t++) {
        TraceExercise *trace = &set.items[i].traces[t];
        assert(TracingValid(trace) && TracingAnswer(trace, trace->answer));
        count++;
    }
    assert(count >= 3);
    LessonsUnload(&set);
    puts("All code tracing tests passed");
    return 0;
}
