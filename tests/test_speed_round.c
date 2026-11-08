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

// The functions questions may show above their code. The generated text must be exactly
// one of these, so the program below (which defines each once) runs the same code.
static const char *cFunctions[] = {
    "void addTen(int *n) { *n += 10; }",
    "void addFive(int n) { n += 5; }",
    "void doubleIt(int *n) { *n *= 2; }",
    "void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }",
};
static const char *cppFunctions[] = {
    "void addOne(int &n) { n++; }",
    "void addTwo(int n) { n += 2; }",
};

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

static void TestRound(void)
{
    SpeedSkill skill = SPEED_ARITHMETIC;
    SpeedRound round;
    SpeedRoundStart(&round, &skill, 1, 42, 100.0);
    assert(round.phase == SPEED_COUNTING_IN && SpeedRoundTension(&round, 100.5) == 0);
    assert(SpeedRoundAnswer(&round, round.question.correct, 101.0) == SPEED_IGNORED);  // not yet
    SpeedRoundUpdate(&round, 103.0);
    assert(round.phase == SPEED_PLAYING && round.phaseStart == 103.0);
    assert(SpeedRoundTension(&round, 105.5) == 0.5f);
    assert(SpeedRoundTimeLeft(&round, 104.0) == SPEED_SECONDS - 1);

    // Every right answer moves straight on, with a fresh clock; the tenth wins.
    double now = 104.0;
    for (int i = 0; i < SPEED_QUESTIONS; i++) {
        assert(round.number == i);
        assert(SpeedRoundAnswer(&round, -1, now) == SPEED_IGNORED);
        assert(SpeedRoundAnswer(&round, round.question.correct, now) == SPEED_RIGHT);
        if (i + 1 < SPEED_QUESTIONS) assert(round.phase == SPEED_PLAYING && round.phaseStart == now);
        now += 4.9;  // just inside the limit every time
    }
    assert(round.phase == SPEED_WON);
    assert(SpeedRoundAnswer(&round, 0, now) == SPEED_IGNORED);

    // A wrong answer ends the run and remembers what was picked.
    SpeedRoundStart(&round, &skill, 1, 7, 0);
    SpeedRoundUpdate(&round, SPEED_COUNT_IN);
    int wrong = (round.question.correct + 1) % SPEED_CHOICES;
    assert(SpeedRoundAnswer(&round, wrong, SPEED_COUNT_IN + 1) == SPEED_WRONG);
    assert(round.phase == SPEED_FAILED && round.picked == wrong && round.number == 0);

    // Running out of time ends it too, and a late answer doesn't count.
    SpeedRoundStart(&round, &skill, 1, 7, 0);
    assert(SpeedRoundAnswer(&round, round.question.correct, SPEED_COUNT_IN + SPEED_SECONDS + 0.01) == SPEED_IGNORED);
    assert(round.phase == SPEED_FAILED && round.picked == -1);
    assert(round.endedAt == SPEED_COUNT_IN + SPEED_SECONDS);
}

static bool Listed(const char *line, const char **list, int count)
{
    for (int i = 0; i < count; i++) if (strcmp(line, list[i]) == 0) return true;
    return false;
}

// Checks a question's shape: choices, the answer's place, line widths, function lines.
static void CheckShape(const SpeedQuestion *q)
{
    assert(q->lineCount > 0 && q->lineCount <= SPEED_MAX_LINES);
    assert(q->correct >= 0 && q->correct < SPEED_CHOICES);
    for (int i = 0; i < SPEED_CHOICES; i++)
        for (int j = i + 1; j < SPEED_CHOICES; j++) assert(q->choices[i] != q->choices[j]);
    assert(strcmp(q->asks, "x") == 0 || strcmp(q->asks, "y") == 0);
    for (int i = 0; i < q->lineCount; i++) {
        assert(strlen(q->lines[i]) <= 66);  // fits a code box at the largest text size
        assert(strlen(q->lines[i]) < SPEED_LINE_SIZE - 1);  // never cut off
        if (i < q->firstBodyLine && q->lines[i][0]) {
            assert(q->cpp ? Listed(q->lines[i], cppFunctions, 2) : Listed(q->lines[i], cFunctions, 4));
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
    fprintf(f, "#include <stdio.h>\n");
    const char **functions = cpp ? cppFunctions : cFunctions;
    for (int i = 0; i < (cpp ? 2 : 4); i++) fprintf(f, "%s\n", functions[i]);
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
    static SpeedQuestion cQuestions[SPEED_SKILL_COUNT * PER_SKILL], cppQuestions[PER_SKILL];
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
