// Tests for coding steps' data modules: the line diff, code-only hints, the skills a pass
// practised, the checks against printed-in answers, the lesson parser and drafts.
//
// usage: test_steps (run from the repository folder)
#include "../src/drafts.h"
#include "../src/lesson.h"
#include "../src/line_diff.h"
#include "../src/step_check.h"
#include "../src/step_hints.h"
#include "../src/step_skills.h"
#include "raylib.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *hello =
    "#include <stdio.h>\n"
    "\n"
    "int main() {\n"
    "    printf(\"Hello\\n\");\n"
    "    return 0;\n"
    "}\n";

static void TestLineDiff(void)
{
    DiffOp ops[64];
    // Re-indenting and blank lines change nothing.
    int n = LineDiff(hello, "#include <stdio.h>\nint main() {\nprintf(\"Hello\\n\");\n\n\n  return 0;\n}", ops, 64);
    for (int i = 0; i < n; i++) assert(ops[i].kind == DIFF_SAME);
    assert(n == 5 && ops[1].oldLine == 2 && ops[1].newLine == 1);

    // A changed line is a removal then an addition, in place.
    const char *changed = "#include <stdio.h>\n\nint main() {\n    printf(\"Bye\\n\");\n    return 0;\n}\n";
    n = LineDiff(hello, changed, ops, 64);
    assert(n == 6);
    assert(ops[2].kind == DIFF_REMOVED && ops[2].oldLine == 3 && ops[2].newLine == -1);
    assert(ops[3].kind == DIFF_ADDED && ops[3].newLine == 3 && ops[3].oldLine == -1);

    // Empty texts and a cut-off op list.
    assert(LineDiff("", "", ops, 64) == 0);
    assert(LineDiff(NULL, "a\nb\n", ops, 64) == 2 && ops[0].kind == DIFF_ADDED);
    assert(LineDiff(hello, "", ops, 2) == 2);

    char line[64];
    assert(LineDiffGetLine(hello, 3, line, sizeof line) && strcmp(line, "printf(\"Hello\\n\");") == 0);
    assert(LineDiffGetLine(hello, 1, line, sizeof line) && line[0] == '\0');
    assert(!LineDiffGetLine(hello, 40, line, sizeof line) && line[0] == '\0');
    assert(LineDiffGetLine(hello, 3, line, 7) && strcmp(line, "printf") == 0);  // cut to fit
}

static void TestShapes(void)
{
    char shape[STEP_HINT_TEXT];
    StepHintShape("    printf(\"Total: %d\\n\", sum + 1);", shape, sizeof shape);
    assert(strcmp(shape, "printf(\"_ %d\\n\", _ + _);") == 0);
    StepHintShape("for (int i = 0; i < 10; i++) {", shape, sizeof shape);
    assert(strcmp(shape, "for (int _ = _; _ < _; _++) {") == 0);
    StepHintShape("scanf(\"%d %5.2lf\", &age, &x); // read them", shape, sizeof shape);
    assert(strcmp(shape, "scanf(\"%d %5.2lf\", &_, &_);") == 0);
    StepHintShape("#include <stdio.h>", shape, sizeof shape);
    assert(strcmp(shape, "#include <stdio.h>") == 0);
    StepHintShape("char c = 'A';", shape, sizeof shape);
    assert(strcmp(shape, "char _ = '_';") == 0);
    StepHintShape("cout << \"Hi\" << endl;", shape, sizeof shape);
    assert(strcmp(shape, "cout << \"_\" << endl;") == 0);
    StepHintShape("double rate = 3.75e2;", shape, sizeof shape);
    assert(strcmp(shape, "double _ = _;") == 0);
    StepHintShape("printf(\"100%%\\n\");", shape, sizeof shape);
    assert(strcmp(shape, "printf(\"_%%\\n\");") == 0);
    StepHintShape("printf(\"unterminated", shape, sizeof shape);  // no closing quote: no crash
    assert(strcmp(shape, "printf(\"_") == 0);
    StepHintShape("int numbers[3] = {1, 2, 3};", shape, 8);  // cut to fit
    assert(strlen(shape) == 7);
}

static void TestHints(void)
{
    // Nothing to do.
    StepHint hint = StepHintFind(hello, hello);
    assert(hint.kind == STEP_HINT_NONE);

    // A line to change: the learner's line, and the solution's line with its shape.
    const char *solution = "#include <stdio.h>\n\nint main() {\n    printf(\"Hi, %s\\n\", name);\n    return 0;\n}\n";
    hint = StepHintFind(hello, solution);
    assert(hint.kind == STEP_HINT_CHANGE && hint.line == 3);
    assert(strcmp(hint.target, "printf(\"Hi, %s\\n\", name);") == 0);
    assert(strcmp(hint.shape, "printf(\"_ %s\\n\", _);") == 0);

    // A missing line goes before the next line the code still has.
    const char *missing = "#include <stdio.h>\n\nint main() {\n    return 0;\n}\n";
    hint = StepHintFind(missing, hello);
    assert(hint.kind == STEP_HINT_ADD && hint.line == 3 && strcmp(hint.target, "printf(\"Hello\\n\");") == 0);

    // A gap marker line is replaced by the line that belongs there.
    const char *gap = "#include <stdio.h>\n\nint main() {\n    // ?\n    return 0;\n}\n";
    hint = StepHintFind(gap, hello);
    assert(hint.kind == STEP_HINT_CHANGE && hint.line == 3);

    // An extra line should go.
    const char *extra = "#include <stdio.h>\n\nint main() {\n    printf(\"Hello\\n\");\n    printf(\"Oops\\n\");\n"
                        "    return 0;\n}\n";
    hint = StepHintFind(extra, hello);
    assert(hint.kind == STEP_HINT_REMOVE && hint.line == 4 && hint.target[0] == '\0');

    // Only the first difference: fixing it moves the hint on to the next.
    const char *two = "#include <stdio.h>\n\nint main() {\n    int a = 1;\n    int b = 2;\n    return 0;\n}\n";
    const char *twoFixed = "#include <stdio.h>\n\nint main() {\n    int a = 5;\n    int b = 7;\n    return 0;\n}\n";
    hint = StepHintFind(two, twoFixed);
    assert(hint.kind == STEP_HINT_CHANGE && hint.line == 3 && strcmp(hint.target, "int a = 5;") == 0);
    const char *oneFixed = "#include <stdio.h>\n\nint main() {\n    int a = 5;\n    int b = 2;\n    return 0;\n}\n";
    hint = StepHintFind(oneFixed, twoFixed);
    assert(hint.kind == STEP_HINT_CHANGE && hint.line == 4 && strcmp(hint.target, "int b = 7;") == 0);

    // Missing lines at the very end of the code.
    hint = StepHintFind("int main() {", "int main() {\n}\n");
    assert(hint.kind == STEP_HINT_ADD && hint.line == 1 && strcmp(hint.target, "}") == 0);
    hint = StepHintFind("", "int x;\n");
    assert(hint.kind == STEP_HINT_ADD && hint.line == 0);
}

static bool Has(const SkillWeight *skills, int count, Skill skill, float weight)
{
    for (int i = 0; i < count; i++)
        if (skills[i].skill == skill) return weight < 0 || skills[i].weight == weight;
    return false;
}

static void TestStepSkills(void)
{
    Step step = { 0 };
    step.starter = (char *)hello;
    step.difficulty = 1;

    // Unchanged code practised nothing (beyond what the author names).
    SkillWeight skills[MAX_QUESTION_SKILLS];
    assert(StepSkills(&step, hello, false, skills) == 0);

    // A changed printf: output, from the learner's own line.
    const char *changed = "#include <stdio.h>\n\nint main() {\n    printf(\"Bye\\n\");\n    return 0;\n}\n";
    int n = StepSkills(&step, changed, false, skills);
    assert(n == 1 && Has(skills, n, SKILL_OUTPUT, STEP_WRITTEN_WEIGHT));

    // A loop written from scratch: loops, types and operators, and problem solving for the
    // number of lines; the author's skill comes first at its own weight.
    step.skills[0] = (SkillWeight){ SKILL_DEBUGGING, 1.0f };
    step.skillCount = 1;
    const char *loop = "#include <stdio.h>\n\nint main() {\n    int total = 0;\n    for (int i = 1; i <= 5; i++) {\n"
                       "        total += i;\n    }\n    printf(\"%d\\n\", total);\n    return 0;\n}\n";
    n = StepSkills(&step, loop, false, skills);
    assert(n == MAX_QUESTION_SKILLS);
    assert(skills[0].skill == SKILL_DEBUGGING && skills[0].weight == 1.0f);
    for (int i = 1; i < n; i++) assert(skills[i].weight <= skills[i - 1].weight);  // strongest first
    assert(Has(skills, n, SKILL_LOOPS, STEP_WRITTEN_WEIGHT));
    // Five lines written: problem solving is in the running, but four skills at 0.8 outrank it.
    assert(!Has(skills, n, SKILL_PROBLEM_SOLVING, -1));

    // The author's weight wins over a lower detected one, and a higher detected one wins over
    // a low author weight.
    step.skills[0] = (SkillWeight){ SKILL_LOOPS, 0.3f };
    n = StepSkills(&step, loop, false, skills);
    assert(Has(skills, n, SKILL_LOOPS, STEP_WRITTEN_WEIGHT));

    // C++: cout counts as output.
    step.skillCount = 0;
    step.starter = "#include <iostream>\nusing namespace std;\nint main() {\n}\n";
    n = StepSkills(&step, "#include <iostream>\nusing namespace std;\nint main() {\n    cout << 1;\n}\n", true, skills);
    assert(Has(skills, n, SKILL_OUTPUT, STEP_WRITTEN_WEIGHT));
}

static void WriteText(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs(text, f);
    assert(fclose(f) == 0);
}

static void TestParser(void)
{
    MakeDirectory("build/test_steps_lessons");
    WriteText("build/test_steps_lessons/b_level.txt",
              "@id c_99_test\n@track C\n@title Test level\n@speed loops arrays\n\n"
              "@step\n~ 2 loops:1 output:0.5\n```\nint main() {\n\tint x;\n}\n```\n@input\n```\n3 4\n```\n"
              "@expect\n```\n7\n```\n@solution\n```\nint main() {}\n```\n\n"
              "@step broken fixed\n```\nstarter two\n```\n@expect\n```\nout\n```\n\n"
              "Some words to read.\n");
    WriteText("build/test_steps_lessons/a_unwritten.txt", "@id c_98_soon\n@track C++\n@title Coming soon\n");
    LessonSet set;
    assert(LessonsLoad(&set, "build/test_steps_lessons") && set.count == 2);
    const Lesson *soon = &set.items[0], *level = &set.items[1];  // in filename order
    assert(!strcmp(soon->id, "c_98_soon") && soon->track == TRACK_CPP && soon->stepCount == 0);
    assert(!strcmp(level->id, "c_99_test") && !strcmp(level->title, "Test level") && level->track == TRACK_C);
    assert(!strcmp(level->speedSkills, "loops arrays") && level->stepCount == 2);
    const Step *one = &level->steps[0], *two = &level->steps[1];
    assert(one->difficulty == 2 && one->skillCount == 2 && one->skills[0].skill == SKILL_LOOPS);
    assert(one->skills[1].skill == SKILL_OUTPUT && one->skills[1].weight == 0.5f);
    assert(!strcmp(one->starter, "int main() {\n    int x;\n}"));  // tabs become four spaces
    assert(!strcmp(one->input, "3 4") && !strcmp(one->expected, "7") && !strcmp(one->solution, "int main() {}"));
    // A step without a "~" line is difficulty 1; without a solution, its starter stands in.
    assert(two->difficulty == 1 && two->skillCount == 0 && two->input == NULL);
    assert(two->fixedValues && !one->fixedValues);
    assert(!strcmp(two->starter, "starter two") && !strcmp(two->solution, "starter two"));
    LessonsUnload(&set);
    remove("build/test_steps_lessons/a_unwritten.txt");
    remove("build/test_steps_lessons/b_level.txt");
}

// The swap step: a and b are given, and the learner is meant to swap them through temp.
static const char *swapStarter =
    "#include <stdio.h>\n\nint main() {\n    int a = 3;\n    int b = 8;\n    int temp;\n    // ?\n"
    "    printf(\"a = %d, b = %d\\n\", a, b);\n    return 0;\n}\n";
static const char *swapSolution =
    "#include <stdio.h>\n\nint main() {\n    int a = 3;\n    int b = 8;\n    int temp;\n    temp = a;\n    a = b;\n"
    "    b = temp;\n    printf(\"a = %d, b = %d\\n\", a, b);\n    return 0;\n}\n";

static void TestSecondRun(void)
{
    Step swap = { 0 };
    swap.starter = (char *)swapStarter;
    swap.solution = (char *)swapSolution;
    StepVariation variation;
    assert(StepVariationMake(&swap, &variation));
    assert(variation.changes == 2 && variation.input == NULL);
    assert(strcmp(variation.summary, "a = 4, b = 10") == 0);  // each given changes by its own amount
    assert(strstr(variation.solution, "int a = 4;") && strstr(variation.solution, "int b = 10;"));
    assert(strstr(variation.solution, "temp = a;"));  // nothing else is touched
    StepVariationFree(&variation);

    // The cheat: assigning the answer. The givens are still there, so they change, and the
    // literals the learner wrote in don't, which the second run will show up.
    const char *cheat =
        "#include <stdio.h>\n\nint main() {\n    int a = 3;\n    int b = 8;\n    int temp;\n    a = 8;\n    b = 3;\n"
        "    printf(\"a = %d, b = %d\\n\", a, b);\n    return 0;\n}\n";
    int missing;
    char note[STEP_CHECK_SUMMARY];
    char *varied = StepVariationApply(&swap, cheat, &missing, note);
    assert(missing == 0 && strstr(varied, "int a = 4;") && strstr(varied, "int b = 10;"));
    assert(strstr(varied, "    a = 8;\n    b = 3;"));
    free(varied);

    // Editing the givens instead is caught without a run, and says which values must stay.
    const char *edited =
        "#include <stdio.h>\n\nint main() {\n    int a = 8;\n    int b = 3;\n    int temp;\n"
        "    printf(\"a = %d, b = %d\\n\", a, b);\n    return 0;\n}\n";
    varied = StepVariationApply(&swap, edited, &missing, note);
    assert(missing == 2 && strcmp(note, "a = 3, b = 8") == 0);
    free(varied);

    // A different honest layout still has its givens found: one declaration, another type.
    const char *relaid =
        "#include <stdio.h>\nint main() {\n    long a = 3, b = 8, temp = a;\n    a = b; b = temp;\n"
        "    printf(\"a = %ld, b = %ld\\n\", a, b);\n}\n";
    varied = StepVariationApply(&swap, relaid, &missing, note);
    assert(missing == 0 && strstr(varied, "long a = 4, b = 10, temp = a;"));
    free(varied);

    // What counts as a given: a value the starter and the solution both set. A value the
    // learner is meant to change isn't one, nor are literals in calls, conditions, sizes,
    // strings and comments; fractions, suffixes, letters and initialiser lists are.
    Step mixed = { 0 };
    mixed.starter = "int n[3] = {1, 2, 3};\ndouble rate = 0.08;\nfloat f = 2.5f;\nchar grade = 'A';\nint age = 25;\n"
                    "int x = 0x10;\nfor (int i = 0; i < 3; i++) total += 2;\nif (age == 25) puts(\"age = 25\");\n";
    mixed.solution = "int n[3] = {1, 2, 3};\ndouble rate = 0.08;\nfloat f = 2.5f;\nchar grade = 'A';\nint age = 30;\n"
                     "int x = 0x10;\nfor (int i = 0; i < 3; i++) total += 2;\nif (age == 25) puts(\"age = 25\");\n";
    assert(StepVariationMake(&mixed, &variation));
    assert(strstr(variation.solution, "int n[3] = {2, 4, 6};"));
    assert(strstr(variation.solution, "double rate = 4.08;") && strstr(variation.solution, "float f = 7.5f;"));
    assert(strstr(variation.solution, "char grade = 'G';"));
    assert(strstr(variation.solution, "int age = 30;") && strstr(variation.solution, "int x = 0x10;"));
    assert(strstr(variation.solution, "for (int i = 0; i < 3; i++) total += 9;"));  // "+=" a literal counts
    assert(strstr(variation.solution, "if (age == 25) puts(\"age = 25\");"));
    assert(variation.changes == 7);
    StepVariationFree(&variation);

    // Input numbers change too; words don't. A step with nothing to vary has no second run,
    // and "@step fixed" switches it off.
    Step reads = { 0 };
    reads.starter = reads.solution = "int main() { }";
    reads.input = "25 J\n1500.50 -3";
    assert(StepVariationMake(&reads, &variation));
    assert(variation.changes == 3 && strcmp(variation.input, "26 J\n1502.50 -6") == 0);
    assert(strcmp(variation.solution, reads.solution) == 0);
    StepVariationFree(&variation);
    reads.input = NULL;
    assert(!StepVariationMake(&reads, &variation) && variation.solution == NULL && variation.changes == 0);
    swap.fixedValues = true;
    assert(!StepVariationMake(&swap, &variation));
}

static void TestConstructs(void)
{
    Step count = { 0 };
    count.starter = "#include <stdio.h>\nint main() {\n    return 0;\n}\n";
    count.solution = "#include <stdio.h>\nint main() {\n    for (int i = 1; i <= 3; i++) {\n"
                     "        if (i != 2) printf(\"%d\\n\", i);\n    }\n    return 0;\n}\n";
    Skill missing[4];
    // Printing the lines one by one: no loop and no condition.
    const char *printed = "#include <stdio.h>\nint main() {\n    printf(\"1\\n\");\n    printf(\"3\\n\");\n}\n";
    assert(StepMissingConstructs(&count, printed, false, missing, 4) == 2);
    assert(missing[0] == SKILL_CONDITIONS && missing[1] == SKILL_LOOPS);
    assert(StepMissingConstructs(&count, printed, false, missing, 1) == 1);
    // Another kind of loop and condition is as good; words in strings and comments aren't.
    const char *other = "#include <stdio.h>\nint main() {\n    int i = 0;\n    while (++i <= 3) i == 2 ? 0 : printf(\"%d\\n\", i);\n}\n";
    assert(StepMissingConstructs(&count, other, false, missing, 4) == 0);
    const char *fake = "#include <stdio.h>\nint main() {\n    // for if\n    printf(\"1 for 3 if\\n\");\n}\n";
    assert(StepMissingConstructs(&count, fake, false, missing, 4) == 2);
    assert(StepMissingConstructs(&count, count.solution, false, missing, 4) == 0);
}

static void TestDrafts(void)
{
    char path[256];
    DraftPath("build", "c_01_step2", true, path, sizeof path);
    assert(!strcmp(path, "build/c_01_step2.cpp"));
    DraftPath(NULL, "x", false, path, sizeof path);
    assert(path[0] == '\0');
    remove("build/test_draft.c");
    assert(DraftLoad("build", "test_draft", false) == NULL);
    assert(DraftSave("build", "test_draft", false, "int x;\nabc", 6));
    char *text = DraftLoad("build", "test_draft", false);
    assert(text && !strcmp(text, "int x;"));
    free(text);
    assert(!DraftSave("", "test_draft", false, "x", 1));  // drafts off
    assert(DraftLoad(NULL, "test_draft", false) == NULL);
    remove("build/test_draft.c");
}

int main(void)
{
    SetTraceLogLevel(LOG_ERROR);
    TestLineDiff();
    TestShapes();
    TestHints();
    TestStepSkills();
    TestParser();
    TestSecondRun();
    TestConstructs();
    TestDrafts();
    puts("Step tests passed.");
    return 0;
}
