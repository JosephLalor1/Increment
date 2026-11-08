#ifndef SPEED_ROUND_H
#define SPEED_ROUND_H

// Speed rounds: an optional, timed challenge for gold mastery of a lesson. Each question is
// a short generated snippet ("what is x after these lines?") with four answers; the learner
// has SPEED_SECONDS per question, the next one follows at once, and one wrong answer or
// timeout ends the run. Answering all SPEED_QUESTIONS earns gold.
//
// This module generates the questions and runs the round's clock. It never draws and never
// reads the real clock (callers pass `now`), so it can be unit-tested; speed_screen.c draws it.

#include <stdbool.h>
#include <stdint.h>

#define SPEED_QUESTIONS  10
#define SPEED_SECONDS    5.0   // per question
#define SPEED_COUNT_IN   3.0   // "3, 2, 1" before the first question
#define SPEED_CHOICES    4
#define SPEED_MAX_LINES  14
#define SPEED_LINE_SIZE  72

// What a question practises. A lesson names its skills on an "@speed" line, by these names:
// arithmetic loops arrays pointers pointer_to_pointer pass_by_pointer pointer_arithmetic references
typedef enum {
    SPEED_ARITHMETIC, SPEED_LOOPS, SPEED_ARRAYS, SPEED_POINTERS, SPEED_POINTER_TO_POINTER,
    SPEED_PASS_BY_POINTER, SPEED_POINTER_ARITHMETIC, SPEED_REFERENCES, SPEED_SKILL_COUNT
} SpeedSkill;

typedef struct {
    SpeedSkill skill;
    bool cpp;                                      // C++ (references); C otherwise
    int lineCount;
    int firstBodyLine;                             // lines before this are function definitions
    char lines[SPEED_MAX_LINES][SPEED_LINE_SIZE];  // the snippet, one line each
    char notes[SPEED_MAX_LINES][SPEED_LINE_SIZE];  // the variables' values after each line ("" for none)
    char asks[8];                                  // the variable asked about: "x" or "y"
    int choices[SPEED_CHOICES];
    int correct;                                   // index into choices
} SpeedQuestion;

const char *SpeedSkillName(SpeedSkill skill);      // as written on "@speed" lines
const char *SpeedSkillTitle(SpeedSkill skill);     // for people: "Pointers to pointers"
// Reads skill names separated by spaces into out (at most max). Unknown names are skipped.
// Returns how many were read.
int SpeedSkillsParse(const char *names, SpeedSkill *out, int max);

// Makes a question. difficulty (0 = easiest) adds more lines; seed advances.
void SpeedGenerate(SpeedSkill skill, int difficulty, uint32_t *seed, SpeedQuestion *out);
// The snippet as one string, lines joined with '\n' (from..lineCount-1); returns out.
char *SpeedQuestionCode(const SpeedQuestion *q, int from, char *out, int size);

typedef enum { SPEED_COUNTING_IN, SPEED_PLAYING, SPEED_FAILED, SPEED_WON } SpeedPhase;
typedef enum { SPEED_IGNORED, SPEED_RIGHT, SPEED_WRONG } SpeedOutcome;

typedef struct {
    SpeedSkill skills[SPEED_SKILL_COUNT];
    int skillCount;
    uint32_t seed;
    SpeedPhase phase;
    int number;               // zero-based number of the current (or failed) question
    SpeedQuestion question;
    double phaseStart;        // when the count-in or the current question began
    double endedAt;           // when the run failed or was won
    int picked;               // after failing: the choice picked, or -1 if time ran out
} SpeedRound;

// Starts a run with questions drawn from the given skills (at least one), beginning with
// the count-in.
void SpeedRoundStart(SpeedRound *round, const SpeedSkill *skills, int skillCount, uint32_t seed, double now);
// Moves from the count-in to the first question, and fails the run when time runs out.
void SpeedRoundUpdate(SpeedRound *round, double now);
// The learner picked choice. A right answer moves straight to the next question (or wins
// after the last); a wrong one fails the run. Ignored outside SPEED_PLAYING.
SpeedOutcome SpeedRoundAnswer(SpeedRound *round, int choice, double now);
double SpeedRoundTimeLeft(const SpeedRound *round, double now);  // seconds left on this question
// 0 when a question appears, rising to 1 at its time limit; 0 outside SPEED_PLAYING.
float SpeedRoundTension(const SpeedRound *round, double now);

#endif
