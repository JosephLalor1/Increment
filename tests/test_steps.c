// Tests for coding steps' data modules: the line diff, code-only hints, the skills a pass
// practised, the checks against printed-in answers, the lesson parser and drafts.
//
// usage: test_steps (run from the repository folder)
#include "code/drafts.h"
#include "learning/lesson.h"
#include "level/line_diff.h"
#include "level/step_cheats.h"
#include "level/step_check.h"
#include "level/step_hints.h"
#include "level/step_skills.h"
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

static void TestReplayEarns(void)
{
    // A step that asks for two lines to be changed.
    const char *starter = "#include <stdio.h>\n\nint main() {\n    int a = 1;\n    int b = 2;\n"
                          "    printf(\"%d %d\\n\", a, b);\n    return 0;\n}\n";
    const char *solution = "#include <stdio.h>\n\nint main() {\n    int a = 1;\n    int b = 2;\n    int t = a;\n"
                           "    a = b;\n    b = t;\n    printf(\"%d %d\\n\", a, b);\n    return 0;\n}\n";
    Step step = { 0 };
    step.starter = (char *)starter;
    step.solution = (char *)solution;

    // Running the answer that was already there earns nothing, nor does re-spacing it.
    assert(!StepReplayEarns(&step, solution, solution));
    const char *respaced = "#include <stdio.h>\nint main() {\nint a = 1;\nint b = 2;\n\n  int t = a;\na = b;\n"
                           "b = t;\nprintf(\"%d %d\\n\", a, b);\nreturn 0;\n}";
    assert(!StepReplayEarns(&step, solution, respaced));
    // Nor does touching one line of a step that asks for three.
    const char *tweaked = "#include <stdio.h>\n\nint main() {\n    int a = 1;\n    int b = 2;\n    int tmp = a;\n"
                          "    a = b;\n    b = tmp;\n    printf(\"%d %d\\n\", a, b);\n    return 0;\n}\n";
    assert(!StepReplayEarns(&step, solution, tweaked));  // two lines rewritten, three asked for
    // Rewriting it all differently does.
    const char *rewritten = "#include <stdio.h>\n\nint main() {\n    int a = 1;\n    int b = 2;\n    a = a + b;\n"
                            "    b = a - b;\n    a = a - b;\n    printf(\"%d %d\\n\", a, b);\n    return 0;\n}\n";
    assert(StepReplayEarns(&step, solution, rewritten));

    // From the starter (a reset, or a step never edited), the pass is the work: even the
    // same answer, or a shorter one, earns.
    assert(StepReplayEarns(&step, starter, solution));
    const char *respacedStarter = "#include <stdio.h>\nint main() {\n  int a = 1;\n  int b = 2;\n"
                                  "  printf(\"%d %d\\n\", a, b);\n  return 0;\n}\n";
    assert(StepReplayEarns(&step, respacedStarter, solution));

    // A step asking for one line needs one rewritten line.
    Step one = { 0 };
    one.starter = (char *)hello;
    one.solution = "#include <stdio.h>\n\nint main() {\n    printf(\"Bye\\n\");\n    return 0;\n}\n";
    assert(!StepReplayEarns(&one, one.solution, one.solution));
    assert(StepReplayEarns(&one, one.solution, "#include <stdio.h>\n\nint main() {\n    puts(\"Bye\");\n"
                                               "    return 0;\n}\n"));
}

// A step from its parts; skill is the one it names at full weight (or SKILL_COUNT for none).
static Step MakeStep(const char *starter, const char *solution, const char *expected, const char *input, Skill skill)
{
    Step step = { 0 };
    step.starter = (char *)starter;
    step.solution = (char *)solution;
    step.expected = (char *)expected;
    step.input = (char *)input;
    if (skill < SKILL_COUNT) {
        step.skills[0] = (SkillWeight){ skill, 1.0f };
        step.skillCount = 1;
    }
    return step;
}

static void TestCheats(void)
{
    // The answer written in: a sum the solution computes, typed as a literal.
    const char *sumStarter = "#include <stdio.h>\nint main() {\n    int n = 5;\n    int sum = 0;\n"
                             "    printf(\"Sum: %d\\n\", sum);\n    return 0;\n}\n";
    const char *sumSolution = "#include <stdio.h>\nint main() {\n    int n = 5;\n    int sum = 0;\n"
                              "    for (int i = 1; i <= n; i++) {\n        sum += i;\n    }\n"
                              "    printf(\"Sum: %d\\n\", sum);\n    return 0;\n}\n";
    Step sum = MakeStep(sumStarter, sumSolution, "Sum: 15\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&sum, sumSolution).kind == CHEAT_NONE);
    StepCheat cheat = StepCheatFind(&sum, "#include <stdio.h>\nint main() {\n    int n = 5;\n    int sum = 0;\n"
                                          "    for (int i = 0; i < n; i++) { }\n    printf(\"Sum: 15\\n\");\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_ANSWER_WRITTEN && strcmp(cheat.note, "15") == 0);
    cheat = StepCheatFind(&sum, "#include <stdio.h>\nint main() {\n    int n = 5;\n    int sum = 0;\n"
                                "    for (int i = 0; i < n; i++) { sum = 15; }\n    printf(\"Sum: %d\\n\", sum);\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_ANSWER_WRITTEN);
    // Another honest way, with literals of its own, is fine; so are a comment and a name with the number in.
    assert(StepCheatFind(&sum, "#include <stdio.h>\nint main() {\n    int n = 5;\n    int sum = 0;\n    // 15?\n"
                               "    int x15 = n * (n + 1) / 2;\n    sum = x15;\n    printf(\"Sum: %2d\\n\", sum);\n"
                               "    return 0;\n}\n").kind == CHEAT_NONE);

    // Several computed numbers: one that happens to be a loop bound proves nothing, most of them do.
    const char *oddStarter = "#include <stdio.h>\nint main() {\n    for (int i = 1; i <= 10; i++) {\n"
                             "        printf(\"%d \", i);\n    }\n    return 0;\n}\n";
    const char *oddSolution = "#include <stdio.h>\nint main() {\n    for (int i = 1; i <= 10; i++) {\n"
                              "        if (i % 2 == 0) continue;\n        printf(\"%d \", i);\n    }\n    return 0;\n}\n";
    Step odd = MakeStep(oddStarter, oddSolution, "1 3 5 7 9\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&odd, "#include <stdio.h>\nint main() {\n    for (int i = 1; i <= 9; i += 2) {\n"
                               "        printf(\"%d \", i);\n    }\n    return 0;\n}\n").kind == CHEAT_NONE);
    cheat = StepCheatFind(&odd, "#include <stdio.h>\nint main() {\n    for (int i = 1; i <= 1; i++) {\n"
                                "        printf(\"1 3 5 7 9\");\n    }\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_ANSWER_WRITTEN && strcmp(cheat.note, "3, 5, 7, 9") == 0);
    // Numbers that come in as input were never computed.
    Step echo = MakeStep("int main() { return 0; }", "int main() { int a; scanf(\"%d\", &a); printf(\"%d\\n\", a); return 0; }",
                         "42\n", "42\n", SKILL_COUNT);
    assert(StepCheatFind(&echo, "int main() { printf(\"42\\n\"); return 0; }").kind == CHEAT_NONE);

    // A word the solution has to build (a name reversed) typed in as a string.
    const char *nameStarter = "#include <stdio.h>\nint main() {\n    char name[20] = \"Alice\";\n    printf(\"%s\\n\", name);\n"
                              "    return 0;\n}\n";
    const char *nameSolution = "#include <stdio.h>\nint main() {\n    char name[20] = \"Alice\";\n"
                               "    for (int i = 4; i >= 0; i--) printf(\"%c\", name[i]);\n    printf(\"\\n\");\n    return 0;\n}\n";
    Step reversed = MakeStep(nameStarter, nameSolution, "ecilA\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&reversed, nameSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&reversed, "#include <stdio.h>\nint main() {\n    char name[20] = \"Alice\";\n"
                                     "    for (int i = 0; i < 1; i++) printf(\"ecilA\\n\");\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_ANSWER_WRITTEN && strcmp(cheat.note, "\"ecilA\"") == 0);
    // Words the step's own strings hold ("Alice" here) are no evidence.
    Step greeting = MakeStep(nameStarter, nameSolution, "Alice says ecilA\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&greeting, nameStarter).kind == CHEAT_NONE);

    // A function left as it came: the stub is still a stub, and the work is in main.
    const char *maxStarter = "#include <stdio.h>\nint maxOf(int arr[], int size) {\n    return 0;\n}\n"
                             "int main() {\n    int values[] = {45, 12, 89};\n    printf(\"Max: %d\\n\", maxOf(values, 3));\n"
                             "    return 0;\n}\n";
    const char *maxSolution = "#include <stdio.h>\nint maxOf(int arr[], int size) {\n    int max = arr[0];\n"
                              "    for (int i = 1; i < size; i++) {\n        if (arr[i] > max) max = arr[i];\n    }\n"
                              "    return max;\n}\n"
                              "int main() {\n    int values[] = {45, 12, 89};\n    printf(\"Max: %d\\n\", maxOf(values, 3));\n"
                              "    return 0;\n}\n";
    Step max = MakeStep(maxStarter, maxSolution, "Max: 89\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&max, maxSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&max, "#include <stdio.h>\nint maxOf(int arr[], int size)\n{\n    return 0;   // later\n}\n"
                                "int main() {\n    int values[] = {45, 12, 89};\n    int m = values[0];\n"
                                "    for (int i = 1; i < 3; i++) if (values[i] > m) m = values[i];\n"
                                "    printf(\"Max: %d\\n\", m);\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_FUNCTION_UNTOUCHED && strcmp(cheat.note, "maxOf()") == 0);
    // A function whose body the solution doesn't change (only its parameter) may stay as it is.
    const char *refStarter = "void addOne(int x) {\n    x = x + 1;\n}\nint main() { int a = 5; addOne(a); return 0; }\n";
    const char *refSolution = "void addOne(int& x) {\n    x = x + 1;\n}\nint main() { int a = 5; addOne(a); return 0; }\n";
    Step reference = MakeStep(refStarter, refSolution, "a = 6\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&reference, refSolution).kind == CHEAT_NONE);
    // Methods count, each by its own class's turn: the second print() is the one to write.
    const char *classStarter = "class A { public: void print() { cout << 1; } };\n"
                               "class B { public: void print() { } };\nint main() { B b; b.print(); }\n";
    const char *classSolution = "class A { public: void print() { cout << 1; } };\n"
                                "class B { public: void print() { cout << 22; } };\nint main() { B b; b.print(); }\n";
    Step method = MakeStep(classStarter, classSolution, "22\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&method, classSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&method, "class A { public: void print() { cout << 1; } };\n"
                                   "class B { public: void print() { } };\nint main() { B b; b.print(); cout << 2 << 2; }\n");
    assert(cheat.kind == CHEAT_FUNCTION_UNTOUCHED && strcmp(cheat.note, "print()") == 0);

    // A function the step is about must be used: called where the solution adds a call...
    const char *greetStarter = "#include <stdio.h>\nvoid printGreeting() {\n    printf(\"Hello!\\n\");\n}\n"
                               "int main() {\n    return 0;\n}\n";
    const char *greetSolution = "#include <stdio.h>\nvoid printGreeting() {\n    printf(\"Hello!\\n\");\n}\n"
                                "int main() {\n    printGreeting();\n    return 0;\n}\n";
    Step greet = MakeStep(greetStarter, greetSolution, "Hello!\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&greet, greetSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&greet, "#include <stdio.h>\nvoid printGreeting() {\n    printf(\"Hello!\\n\");\n}\n"
                                  "int main() {\n    printf(\"Hello!\\n\");\n    return 0;\n}\n");
    assert(cheat.kind == CHEAT_FUNCTION_UNTOUCHED && strcmp(cheat.note, "printGreeting()") == 0);
    // ...not stripped of the calls it came with (a method here)...
    const char *setStarter = "class P { double x; public: void setX(double v) { x = v; } double getX() { return x; } };\n"
                             "int main() { P p; p.setX(2); cout << p.getX(); }\n";
    const char *setSolution = "class P { double x; public: void setX(double v) { x = v; } double getX() { return x; } };\n"
                              "int main() { P p; p.setX(5); cout << p.getX(); }\n";
    Step setter = MakeStep(setStarter, setSolution, "5\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&setter, setSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&setter, "class P { double x; public: void setX(double v) { x = v; } double getX() { return x; } };\n"
                                   "int main() { P p; p.setX(5); cout << 5; }\n");
    assert(cheat.kind == CHEAT_FUNCTION_UNTOUCHED && strcmp(cheat.note, "getX()") == 0);
    // ...and written, when the starter already settles its name: a destructor.
    const char *goneStarter = "class N { public: N() { cout << 1; } };\nint main() { { N n; } cout << 3; }\n";
    const char *goneSolution = "class N { public: N() { cout << 1; } ~N() { cout << 2; } };\nint main() { { N n; } cout << 3; }\n";
    Step gone = MakeStep(goneStarter, goneSolution, "123\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&gone, goneSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&gone, "class N { public: N() { cout << 1; } };\nint main() { { N n; } cout << 2; cout << 3; }\n");
    assert(cheat.kind == CHEAT_FUNCTION_UNTOUCHED && strcmp(cheat.note, "~N()") == 0);
    // A function written from scratch may have any name, and one loop may make the calls.
    const char *freeStarter = "int main() { return 0; }\n";
    const char *freeSolution = "int power(int b, int e) { return e ? b * power(b, e - 1) : 1; }\n"
                               "int main() { printf(\"%d\", power(3, 4)); return 0; }\n";
    Step fromScratch = MakeStep(freeStarter, freeSolution, "81\n", NULL, SKILL_COUNT);
    assert(StepCheatFind(&fromScratch, "int raise(int b, int e) { int r = 1; while (e--) r *= b; return r; }\n"
                                       "int main() { printf(\"%d\", raise(3, 4)); return 0; }\n").kind == CHEAT_NONE);

    // Indexing in a step about pointer arithmetic.
    const char *walkStarter = "#include <stdio.h>\nint sum(int* ptr, int numValues) {\n    return 0;\n}\n"
                              "int main() {\n    int a[] = {4, 6, 7};\n    printf(\"%d\\n\", sum(a, 3));\n    return 0;\n}\n";
    const char *walkSolution = "#include <stdio.h>\nint sum(int* ptr, int numValues) {\n    int total = 0;\n"
                               "    for (int i = 0; i < numValues; i++) {\n        total += *(ptr + i);\n    }\n"
                               "    return total;\n}\n"
                               "int main() {\n    int a[] = {4, 6, 7};\n    printf(\"%d\\n\", sum(a, 3));\n    return 0;\n}\n";
    const char *walkIndexed = "#include <stdio.h>\nint sum(int* ptr, int numValues) {\n    int total = 0;\n"
                              "    for (int i = 0; i < numValues; i++) {\n        total += ptr[i];\n    }\n"
                              "    return total;\n}\n"
                              "int main() {\n    int a[] = {4, 6, 7};\n    printf(\"%d\\n\", sum(a, 3));\n    return 0;\n}\n";
    Step walk = MakeStep(walkStarter, walkSolution, "17\n", NULL, SKILL_POINTER_ARITHMETIC);
    assert(StepCheatFind(&walk, walkSolution).kind == CHEAT_NONE);
    cheat = StepCheatFind(&walk, walkIndexed);
    assert(cheat.kind == CHEAT_INDEXED && strcmp(cheat.note, "ptr[i]") == 0);
    // The same code in a step about something else is fine, and declaring an array always is.
    Step plain = MakeStep(walkStarter, walkSolution, "17\n", NULL, SKILL_LOOPS);
    assert(StepCheatFind(&plain, walkIndexed).kind == CHEAT_NONE);
    assert(StepCheatFind(&walk, "#include <stdio.h>\nint sum(int* ptr, int numValues) {\n    int seen[8], total = 0;\n"
                                "    char label[] = \"t\", other[4];\n"
                                "    for (int* p = ptr; p < ptr + numValues; p++) total += *p;\n    return total;\n}\n"
                                "int main() {\n    int a[] = {4, 6, 7};\n    printf(\"%d\\n\", sum(a, 3));\n    return 0;\n}\n")
               .kind == CHEAT_NONE);
}

// The second run changes strings and words too.
static void TestSecondRunStrings(void)
{
    const char *starter = "#include <stdio.h>\nint main() {\n    char name[20] = \"Alice\";\n    char sep = 'x';\n"
                          "    printf(\"Hi %s\\n\", name);\n    return 0;\n}\n";
    const char *solution = "#include <stdio.h>\nint main() {\n    char name[20] = \"Alice\";\n    char sep = 'x';\n"
                           "    printf(\"Hi %s%c\\n\", name, sep);\n    return 0;\n}\n";
    Step step = MakeStep(starter, solution, "Hi Alicex\n", "b 7 Zed 3x\n", SKILL_COUNT);
    StepVariation variation;
    assert(StepVariationMake(&step, &variation));
    // Each given by its own amount: the string's letters move one place, the letter two.
    assert(strstr(variation.solution, "char name[20] = \"Bmjdf\";") && strstr(variation.solution, "char sep = 'z';"));
    assert(strstr(variation.solution, "printf(\"Hi %s%c\\n\", name, sep);"));  // a format string is never touched
    assert(strcmp(variation.summary, "name = \"Bmjdf\", sep = 'z'") == 0);
    // In the input, numbers grow and whole words of letters move along; mixed tokens stay.
    assert(strcmp(variation.input, "c 9 Chg 3x\n") == 0);
    StepVariationFree(&variation);

    // The learner's code gets the same change; a learner who edited the string is caught.
    int missing;
    char note[STEP_CHECK_SUMMARY];
    char *varied = StepVariationApply(&step, solution, &missing, note);
    assert(missing == 0 && strstr(varied, "\"Bmjdf\""));
    free(varied);
    varied = StepVariationApply(&step, "int main() {\n    char name[20] = \"Alicex\";\n    char sep = 'x';\n}\n", &missing, note);
    assert(missing == 1 && strcmp(note, "name = \"Alice\"") == 0);
    free(varied);

    // Escapes and conversions inside a string value stay as they are.
    Step odd = MakeStep("int main() { char f[] = \"a\\n%d b\"; }", "int main() { char f[] = \"a\\n%d b\"; return 0; }", "", NULL,
                        SKILL_COUNT);
    assert(StepVariationMake(&odd, &variation) && strstr(variation.solution, "\"b\\n%d c\""));
    StepVariationFree(&variation);
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

    // Input numbers change too, and words of letters move along the alphabet, each by its own
    // amount. A step with nothing to vary has no second run, and "@step fixed" switches it off.
    Step reads = { 0 };
    reads.starter = reads.solution = "int main() { }";
    reads.input = "25 J\n1500.50 -3";
    assert(StepVariationMake(&reads, &variation));
    assert(variation.changes == 4 && strcmp(variation.input, "26 L\n1503.50 -7") == 0);
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
    TestReplayEarns();
    TestParser();
    TestSecondRun();
    TestSecondRunStrings();
    TestCheats();
    TestConstructs();
    TestDrafts();
    puts("Step tests passed.");
    return 0;
}
