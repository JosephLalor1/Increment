// Tests for speed_round.c: skill names, the round's clock and answers, and (the important
// part) that generated questions are right. Hundreds of questions are compiled with gcc and
// g++ and run, and each program's value must equal the question's answer.
//
// usage: test_speed_round <folder with gcc.exe and g++.exe>
#include "../src/speed_round.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PER_SKILL 60

// The definitions questions may show above their code. Every line shown there must be one
// of these lines, so the program below (which defines them all once) runs the same code.
static const char *cFunctions[] = {
    "void addTen(int *n) { *n += 10; }",
    "void addFive(int n) { n += 5; }",
    "void doubleIt(int *n) { *n *= 2; }",
    "void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }",
    "int square(int n) { return n * n; }",
    "int addOne(int n) { n++; return n; }",
    "int twice(int n) { return n + n; }",
    "int Add(int num1, int num2) { return num1 + num2; }",
    "int Subtract(int num1, int num2) { return num1 - num2; }",
    "int Multiply(int num1, int num2) { return num1 * num2; }",
};
static const char *cppFunctions[] = {
    "void addOne(int &n) { n++; }",
    "void addTwo(int n) { n += 2; }",
    "class Counter {",
    "public:",
    "    int n;",
    "    Counter(int start) { n = start; }",
    "    void add(int k) { n += k; }",
    "};",
    "struct Node { int data; Node *next; };",
};
#define C_FUNCTIONS   (int)(sizeof cFunctions / sizeof cFunctions[0])
#define CPP_FUNCTIONS (int)(sizeof cppFunctions / sizeof cppFunctions[0])

static void TestSkillNames(void)
{
    SpeedSkill skills[SPEED_SKILL_COUNT];
    assert(SpeedSkillsParse("pointers  pass_by_pointer,references nonsense", skills, SPEED_SKILL_COUNT) == 3);
    assert(skills[0] == SPEED_POINTERS && skills[1] == SPEED_PASS_BY_POINTER && skills[2] == SPEED_REFERENCES);
    assert(SpeedSkillsParse("", skills, SPEED_SKILL_COUNT) == 0);
    assert(SpeedSkillsParse(NULL, skills, SPEED_SKILL_COUNT) == 0);
    assert(SpeedSkillsParse("pointer", skills, SPEED_SKILL_COUNT) == 0);  // whole names only
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
        assert(SpeedSkillsParse(SpeedSkillName((SpeedSkill)s), skills, 1) == 1 && skills[0] == (SpeedSkill)s);
        assert(SpeedSkillTitle((SpeedSkill)s)[0]);
    }
}

// A plan of n generated arithmetic questions, as the tests' runs use.
static int ArithmeticPlan(SpeedPlanItem *plan, int n)
{
    for (int i = 0; i < n; i++) plan[i] = (SpeedPlanItem){ SPEED_ARITHMETIC, "skill:arithmetic" };
    return n;
}

static SpeedFamiliarity AlwaysNew(const char *id) { (void)id; return SPEED_NEW; }
static SpeedFamiliarity AlwaysStruggling(const char *id) { (void)id; return SPEED_STRUGGLING; }

static void TestRound(void)
{
    SpeedPlanItem plan[SPEED_QUESTIONS];
    ArithmeticPlan(plan, SPEED_QUESTIONS);
    SpeedRound round;
    SpeedRoundStart(&round, plan, SPEED_QUESTIONS, true, NULL, 42, 100.0);
    double limit = round.seconds[0];
    assert(round.count == SPEED_QUESTIONS && limit >= SPEED_MIN_SECONDS);
    assert(round.phase == SPEED_COUNTING_IN && SpeedRoundTension(&round, 100.5) == 0);
    assert(SpeedRoundAnswer(&round, SpeedRoundCorrect(&round, 0), 101.0) == SPEED_IGNORED);  // not yet
    SpeedRoundUpdate(&round, 103.0);
    assert(round.phase == SPEED_PLAYING && round.phaseStart == 103.0);
    assert(SpeedRoundTension(&round, 103.0 + limit / 2) == 0.5f);
    assert(SpeedRoundTimeLeft(&round, 104.0) == limit - 1);

    // Every right answer moves straight on, with a fresh clock; the tenth wins.
    double now = 104.0;
    for (int i = 0; i < SPEED_QUESTIONS; i++) {
        assert(round.number == i);
        assert(SpeedRoundAnswer(&round, -1, now) == SPEED_IGNORED);
        assert(SpeedRoundAnswer(&round, SpeedRoundCorrect(&round, i), now) == SPEED_RIGHT);
        assert(round.lastAnswered == i);
        if (i + 1 < SPEED_QUESTIONS) assert(round.phase == SPEED_PLAYING && round.phaseStart == now);
        now += SPEED_MIN_SECONDS - 0.1;  // just inside even the shortest limit
    }
    assert(round.phase == SPEED_WON && SpeedRoundRight(&round) == SPEED_QUESTIONS);
    assert(SpeedRoundAnswer(&round, 0, now) == SPEED_IGNORED);

    // In a lesson round a wrong answer ends the run and remembers what was picked.
    SpeedRoundStart(&round, plan, SPEED_QUESTIONS, true, NULL, 7, 0);
    SpeedRoundUpdate(&round, SPEED_COUNT_IN);
    int wrong = (SpeedRoundCorrect(&round, 0) + 1) % SPEED_CHOICES;
    assert(SpeedRoundAnswer(&round, wrong, SPEED_COUNT_IN + 1) == SPEED_WRONG);
    assert(round.phase == SPEED_FAILED && round.picked[0] == wrong && round.number == 0);

    // Running out of time ends it too, and a late answer doesn't count.
    SpeedRoundStart(&round, plan, SPEED_QUESTIONS, true, NULL, 7, 0);
    double late = SPEED_COUNT_IN + round.seconds[0] + 0.01;
    assert(SpeedRoundAnswer(&round, SpeedRoundCorrect(&round, 0), late) == SPEED_IGNORED);
    assert(round.phase == SPEED_FAILED && round.picked[0] == -1 && round.lastAnswered == 0);
    assert(round.endedAt == SPEED_COUNT_IN + round.seconds[0]);

    // A challenge carries on after misses and timeouts, and ends after the last question.
    SpeedRoundStart(&round, plan, 3, false, NULL, 9, 0);
    SpeedRoundUpdate(&round, SPEED_COUNT_IN);
    assert(SpeedRoundAnswer(&round, (SpeedRoundCorrect(&round, 0) + 1) % SPEED_CHOICES, 4) == SPEED_WRONG);
    assert(round.phase == SPEED_PLAYING && round.number == 1 && round.phaseStart == 4);
    assert(SpeedRoundUpdate(&round, 4 + round.seconds[1]) == SPEED_TIMED_OUT);
    assert(round.phase == SPEED_PLAYING && round.number == 2 && round.lastAnswered == 1);
    assert(SpeedRoundAnswer(&round, SpeedRoundCorrect(&round, 2), round.phaseStart + 1) == SPEED_RIGHT);
    assert(round.phase == SPEED_WON && SpeedRoundRight(&round) == 1);

    // Time: at least the minimum, more for new questions, most for struggling ones, capped.
    SpeedQuestion q;
    uint32_t seed = 5;
    SpeedGenerate(SPEED_POINTERS, 0, &seed, &q);
    double known = SpeedSecondsFor(&q, SPEED_KNOWN), fresh = SpeedSecondsFor(&q, SPEED_NEW);
    double struggling = SpeedSecondsFor(&q, SPEED_STRUGGLING);
    assert(known >= SPEED_MIN_SECONDS && fresh > known && struggling > fresh && struggling <= SPEED_MAX_SECONDS);
    SpeedQuestion longer = q;  // more lines to trace take longer
    longer.lineCount = longer.firstBodyLine + 12;
    assert(SpeedSecondsFor(&longer, SPEED_KNOWN) > known);
    assert(SpeedSecondsFor(&longer, SPEED_STRUGGLING) <= SPEED_MAX_SECONDS);
    SpeedRoundStart(&round, plan, 2, true, AlwaysNew, 3, 0);
    double newTime = round.seconds[0];
    SpeedRoundStart(&round, plan, 2, true, AlwaysStruggling, 3, 0);
    assert(round.seconds[0] > newTime);
    assert(SpeedRoundChoiceCount(&round, 0) == SPEED_CHOICES && SpeedRoundChoiceCount(&round, 5) == 0);
    assert(SpeedRoundCorrect(&round, -1) == -1);
}

static bool Listed(const char *line, const char **list, int count)
{
    for (int i = 0; i < count; i++) if (strcmp(line, list[i]) == 0) return true;
    return false;
}

// Checks a question's shape: choices, the answer's place, line widths, function lines.
static void CheckShape(const SpeedQuestion *q)
{
    assert(q->lineCount > 0 && q->lineCount < SPEED_MAX_LINES);  // below the limit: no line was dropped
    assert(q->correct >= 0 && q->correct < SPEED_CHOICES);
    for (int i = 0; i < SPEED_CHOICES; i++)
        for (int j = i + 1; j < SPEED_CHOICES; j++) assert(q->choices[i] != q->choices[j]);
    assert(strcmp(q->asks, "x") == 0 || strcmp(q->asks, "y") == 0);
    for (int i = 0; i < q->lineCount; i++) {
        assert(strlen(q->lines[i]) <= 66);  // fits a code box at the largest text size
        assert(strlen(q->lines[i]) < SPEED_LINE_SIZE - 1);  // never cut off
        if (i < q->firstBodyLine && q->lines[i][0]) {
            assert(q->cpp ? Listed(q->lines[i], cppFunctions, CPP_FUNCTIONS) : Listed(q->lines[i], cFunctions, C_FUNCTIONS));
        }
    }
    assert(q->notes[q->lineCount - 1][0]);  // the last line always explains the result
    for (int i = 0; i < q->lineCount; i++) {  // each variable appears once per note
        const char *first = strstr(q->notes[i], "x = ");
        assert(!first || !strstr(first + 1, "x = "));
    }
}

// Writes every question as a function returning the asked variable, and a main printing
// them all; returns how many.
static int WriteProgram(const char *path, bool cpp, SpeedQuestion *questions, int count)
{
    FILE *f = fopen(path, "w");
    assert(f);
    fprintf(f, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n");  // strlen, malloc
    const char **functions = cpp ? cppFunctions : cFunctions;
    for (int i = 0; i < (cpp ? CPP_FUNCTIONS : C_FUNCTIONS); i++) fprintf(f, "%s\n", functions[i]);
    for (int i = 0; i < count; i++) {
        fprintf(f, "static int question%d(void) {\n", i);
        for (int line = questions[i].firstBodyLine; line < questions[i].lineCount; line++)
            fprintf(f, "    %s\n", questions[i].lines[line]);
        fprintf(f, "    return %s;\n}\n", questions[i].asks);
    }
    fprintf(f, "int main(void) {\n");
    for (int i = 0; i < count; i++) fprintf(f, "    printf(\"%%d\\n\", question%d());\n", i);
    fprintf(f, "    return 0;\n}\n");
    fclose(f);
    return count;
}

// Compiles and runs a program, checking it prints each question's answer in turn.
static void CompileAndCompare(const char *compilerDir, bool cpp, SpeedQuestion *questions, int count)
{
    const char *source = cpp ? "build/speed_check.cpp" : "build/speed_check.c";
    WriteProgram(source, cpp, questions, count);
    char command[1024];
    // The outer quotes keep cmd.exe from stripping the quotes around the compiler's path.
    snprintf(command, sizeof command, "\"\"%s/%s\" -w %s -o build/speed_check.exe\"", compilerDir,
             cpp ? "g++.exe" : "gcc.exe", source);
    if (system(command) != 0) {
        fprintf(stderr, "speed round: generated %s code did not compile (see %s)\n", cpp ? "C++" : "C", source);
        exit(1);
    }
    assert(system("build\\speed_check.exe > build\\speed_check.txt") == 0);
    FILE *f = fopen("build/speed_check.txt", "r");
    assert(f);
    for (int i = 0; i < count; i++) {
        int value;
        assert(fscanf(f, "%d", &value) == 1);
        const SpeedQuestion *q = &questions[i];
        if (value != q->choices[q->correct]) {
            char code[1200];
            fprintf(stderr, "speed round: wrong answer %d (the code gives %d) for %s:\n%s\n",
                    q->choices[q->correct], value, SpeedSkillName(q->skill), SpeedQuestionCode(q, 0, code, sizeof code));
            exit(1);
        }
    }
    fclose(f);
}

static void TestGeneratedAnswers(const char *compilerDir)
{
    static SpeedQuestion cQuestions[SPEED_SKILL_COUNT * PER_SKILL], cppQuestions[SPEED_SKILL_COUNT * PER_SKILL];
    int cCount = 0, cppCount = 0;
    uint32_t seed = 12345;
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
        for (int i = 0; i < PER_SKILL; i++) {
            SpeedQuestion q;
            SpeedGenerate((SpeedSkill)s, i % SPEED_QUESTIONS, &seed, &q);
            CheckShape(&q);
            if (q.cpp) cppQuestions[cppCount++] = q;
            else cQuestions[cCount++] = q;
        }
    }
    CompileAndCompare(compilerDir, false, cQuestions, cCount);
    CompileAndCompare(compilerDir, true, cppQuestions, cppCount);
    printf("%d generated speed round questions give their stated answers\n", cCount + cppCount);
}

int main(int argc, char **argv)
{
    TestSkillNames();
    TestRound();
    TestGeneratedAnswers(argc > 1 ? argv[1] : "C:/raylib/w64devkit/bin");
    puts("speed round: all tests passed");
    return 0;
}
