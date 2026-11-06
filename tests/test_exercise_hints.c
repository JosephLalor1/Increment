// Optional hint parsing, ordered reveal, exercise isolation, and read-only progress.
#include "../src/exercise_hints.h"
#include "../src/progress.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetTraceLogLevel(LOG_ERROR);
    char folder[1024], lessonPath[1200], progressPath[1200];
    snprintf(folder, sizeof folder, "%s/hint_lessons", argv[1]);
    MakeDirectory(folder);
    snprintf(lessonPath, sizeof lessonPath, "%s/hints.txt", folder);
    FILE *file = fopen(lessonPath, "wb");
    assert(file);
    fputs("@id test_hints\n@track C\n@title Hints fixture\n"
          "@exercise Legacy\nOriginal description.\n```\nprintf(\"@hint literal\");\n```\n"
          "@expect\n```\nexpected output\n```\n@solution\n```\nsolution code\n```\n"
          "@exercise Single\nSingle description.\n@hint One clue.\n"
          "@exercise Multiple\nMultiple description.\n@hint First clue.\n@hint   \n"
          "@hint\tSecond clue.\n@expect\n```\noutput\n```\n@hint Third clue.\n"
          "@solution\n```\nuntouched solution\n```\n"
          "@exercise Limit\nLimit description.\n", file);
    for (int i = 0; i < MAX_EXERCISE_HINTS + 3; i++) fprintf(file, "@hint Clue %d\n", i + 1);
    fputs("@quiz\n? Existing quiz?\n* Yes\n- No\n! Existing explanation.\n", file);
    fclose(file);
    LessonSet set;
    assert(LessonsLoad(&set, folder) && set.count == 1);
    Lesson *lesson = &set.items[0];
    assert(lesson->exerciseCount == 4 && lesson->questionCount == 1);
    assert(lesson->exercises[0].hintCount == 0);
    assert(strstr(lesson->exercises[0].starter, "@hint literal"));
    assert(strcmp(lesson->exercises[0].expected, "expected output") == 0);
    assert(strcmp(lesson->exercises[0].solution, "solution code") == 0);
    assert(lesson->exercises[1].hintCount == 1);
    assert(lesson->exercises[2].hintCount == 3);
    assert(strcmp(lesson->exercises[2].description, "Multiple description.") == 0);
    assert(strcmp(lesson->exercises[2].expected, "output") == 0);
    assert(strcmp(lesson->exercises[2].solution, "untouched solution") == 0);
    assert(lesson->exercises[3].hintCount == MAX_EXERCISE_HINTS);
    assert(strcmp(lesson->exercises[3].hints[MAX_EXERCISE_HINTS - 1], "Clue 8") == 0);

    snprintf(progressPath, sizeof progressPath, "%s/hint_progress.txt", argv[1]);
    ProgressLoad(progressPath);
    ProgressRecord(lesson->id, 80);
    ProgressRecordExercise(lesson->id, 1);
    char *saved = LoadFileText(progressPath);
    assert(saved);
    ExerciseHints state;
    ExerciseHintsOpen(&state, NULL);
    assert(!ExerciseHintsAvailable(&state) && !ExerciseHintsReveal(&state));
    assert(ExerciseHintsGet(&state, 0) == NULL);
    ExerciseHintsOpen(&state, &lesson->exercises[0]);
    assert(!ExerciseHintsAvailable(&state) && !ExerciseHintsReveal(&state));
    ExerciseHintsOpen(&state, &lesson->exercises[1]);
    assert(ExerciseHintsAvailable(&state) && ExerciseHintsGet(&state, 0) == NULL);
    assert(ExerciseHintsReveal(&state) && state.revealed == 1);
    assert(strcmp(ExerciseHintsGet(&state, 0), "One clue.") == 0);
    assert(!ExerciseHintsAvailable(&state) && !ExerciseHintsReveal(&state));
    assert(state.revealed == 1);
    ExerciseHintsOpen(&state, &lesson->exercises[2]);
    assert(state.revealed == 0 && ExerciseHintsGet(&state, 0) == NULL);
    const char *expected[] = { "First clue.", "Second clue.", "Third clue." };
    for (int i = 0; i < 3; i++) {
        assert(ExerciseHintsReveal(&state) && state.revealed == i + 1);
        for (int j = 0; j <= i; j++) assert(strcmp(ExerciseHintsGet(&state, j), expected[j]) == 0);
        assert(ExerciseHintsGet(&state, i + 1) == NULL);
    }
    assert(!ExerciseHintsReveal(&state));
    assert(ExerciseHintsGet(&state, -1) == NULL && ExerciseHintsGet(&state, MAX_EXERCISE_HINTS) == NULL);
    ExerciseHintsOpen(&state, &lesson->exercises[3]);
    for (int i = 0; i < MAX_EXERCISE_HINTS; i++) assert(ExerciseHintsReveal(&state));
    for (int i = 0; i < 20; i++) assert(!ExerciseHintsReveal(&state));
    assert(state.revealed == MAX_EXERCISE_HINTS);
    assert(!ProgressExerciseDone(lesson->id, 0) && !ProgressExerciseDone(lesson->id, 2));
    assert(ProgressExerciseDone(lesson->id, 1) && ProgressBest(lesson->id) == 80);
    char *after = LoadFileText(progressPath);
    assert(after && strcmp(saved, after) == 0);
    assert(strcmp(lesson->exercises[2].solution, "untouched solution") == 0);
    UnloadFileText(saved);
    UnloadFileText(after);
    ExerciseHintsOpen(&state, NULL);
    assert(!ExerciseHintsGet(&state, 0));
    LessonsUnload(&set);

    assert(LessonsLoad(&set, "assets/lessons"));
    int withHints[TRACK_COUNT] = { 0 };
    int withoutHints = 0;
    for (int l = 0; l < set.count; l++) {
        for (int e = 0; e < set.items[l].exerciseCount; e++) {
            const Exercise *exercise = &set.items[l].exercises[e];
            if (exercise->hintCount) {
                withHints[set.items[l].track]++;
                ExerciseHintsOpen(&state, exercise);
                for (int i = 0; i < exercise->hintCount; i++) {
                    assert(ExerciseHintsReveal(&state));
                    assert(ExerciseHintsGet(&state, i) == exercise->hints[i]);
                }
                assert(!ExerciseHintsReveal(&state));
            } else withoutHints++;
        }
    }
    assert(withHints[TRACK_C] == 5 && withHints[TRACK_CPP] == 2 && withoutHints > 0);
    LessonsUnload(&set);
    ProgressLoad("");
    remove(lessonPath);
    remove(progressPath);
    puts("All gradual exercise hint tests passed");
    return 0;
}
