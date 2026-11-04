// The learning roadmap's data: track progress and preparation suggestions. See roadmap.h.
#include "roadmap.h"
#include "progress.h"

#include <stdio.h>
#include <string.h>

// Optional preparation for each lesson, by lesson id (ids never change once shipped, so the
// table survives lessons being reordered). C++ lessons may suggest C foundations. Ids that
// don't match a loaded lesson are skipped.
static const struct { const char *id, *before[3]; } prerequisites[] = {
    { "c_03_input", { "c_02_variables" } },
    { "c_04_arithmetic", { "c_02_variables" } },
    { "c_05_decisions", { "c_04_arithmetic" } },
    { "c_06_loops", { "c_05_decisions" } },
    { "c_07_arrays", { "c_02_variables", "c_06_loops" } },
    { "c_08_strings", { "c_07_arrays" } },
    { "c_09_functions", { "c_02_variables", "c_06_loops" } },
    { "c_10_exam1_practice", { "c_07_arrays", "c_09_functions" } },
    { "c_11_pointers", { "c_02_variables", "c_07_arrays" } },
    { "c_12_pass_by_pointer", { "c_09_functions", "c_11_pointers" } },
    { "c_13_pointer_to_pointer", { "c_11_pointers", "c_12_pass_by_pointer" } },
    { "c_14_pointer_arithmetic", { "c_07_arrays", "c_11_pointers" } },
    { "c_15_exam2_practice", { "c_12_pass_by_pointer", "c_14_pointer_arithmetic" } },
    { "c_16_malloc_free", { "c_11_pointers", "c_14_pointer_arithmetic" } },
    { "c_17_scope_globals_fnptrs", { "c_09_functions", "c_11_pointers" } },
    { "cpp_01_hello", { "c_09_functions" } },
    { "cpp_02_references", { "cpp_01_hello", "c_12_pass_by_pointer" } },
    { "cpp_03_new_delete", { "c_11_pointers", "cpp_01_hello" } },
    { "cpp_04_memory_mistakes", { "cpp_03_new_delete" } },
    { "cpp_05_dynamic_arrays", { "c_07_arrays", "cpp_03_new_delete" } },
    { "cpp_06_classes", { "cpp_02_references" } },
    { "cpp_07_objects_methods", { "cpp_06_classes" } },
    { "cpp_08_constructors", { "cpp_07_objects_methods", "cpp_03_new_delete" } },
    { "cpp_09_access_static", { "cpp_06_classes", "cpp_07_objects_methods" } },
    { "cpp_10_copy_constructors", { "cpp_08_constructors", "cpp_02_references" } },
    { "cpp_11_arrays_destructors", { "cpp_05_dynamic_arrays", "cpp_08_constructors" } },
    { "cpp_12_linked_lists", { "c_13_pointer_to_pointer", "cpp_03_new_delete" } },
    { "cpp_13_linked_list_class", { "cpp_12_linked_lists", "cpp_11_arrays_destructors" } },
};

int RoadmapNext(const LessonSet *set, Track track, int *done, int *total)
{
    int next = -1;
    *done = *total = 0;
    for (int i = 0; i < set->count; i++) {
        const Lesson *lesson = &set->items[i];
        if (lesson->track != track) continue;
        (*total)++;
        if (ProgressLessonPassed(lesson->id)) (*done)++;
        else if (next < 0) next = i;
    }
    return next;
}

static const Lesson *FindLesson(const LessonSet *set, const char *id)
{
    for (int i = 0; i < set->count; i++)
        if (strcmp(set->items[i].id, id) == 0) return &set->items[i];
    return NULL;
}

bool RoadmapPreparation(const LessonSet *set, const Lesson *lesson, char *out, size_t size)
{
    snprintf(out, size, "Suggested preparation (optional): ");
    bool found = false;
    for (size_t p = 0; p < sizeof prerequisites / sizeof prerequisites[0]; p++) {
        if (strcmp(prerequisites[p].id, lesson->id) != 0) continue;
        for (int j = 0; j < 3 && prerequisites[p].before[j]; j++) {
            const Lesson *before = FindLesson(set, prerequisites[p].before[j]);
            if (!before) continue;
            size_t used = strlen(out);
            snprintf(out + used, size - used, "%s%s: %s", found ? "; " : "", TrackName(before->track), before->title);
            found = true;
        }
        break;
    }
    if (!found) out[0] = '\0';
    return found;
}
