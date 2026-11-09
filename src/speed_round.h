#ifndef SPEED_ROUND_H
#define SPEED_ROUND_H

// Speed rounds: an optional, timed challenge for gold mastery of a lesson. Each question is
// a short generated snippet ("what is x after these lines?") with four answers, or one of
// the lesson's own multiple-choice questions; the learner
// has its own time per question (SpeedSecondsFor), the next one follows at once, and one
// wrong answer or timeout ends the run. Answering all SPEED_QUESTIONS earns gold. A short
// challenge runs the same way over the learner's most necessary questions, but carries on
// after a miss.
//
// This module generates the questions and runs the round's clock. It never draws and never
// reads the real clock (callers pass `now`), so it can be unit-tested; speed_screen.c draws it.

#include <stdbool.h>
#include <stdint.h>

#include "lesson.h"

#define SPEED_QUESTIONS  10
#define SPEED_MIN_SECONDS 10.0  // every question gets at least this long
#define SPEED_MAX_SECONDS 25.0  // and at most this
#define SPEED_COUNT_IN   3.0   // "3, 2, 1" before the first question
#define SPEED_CHOICES    4     // generated questions; lesson questions have their own (up to MAX_CHOICES)
#define SPEED_REVIEW_ID  64
#define SPEED_MAX_LINES  18
#define SPEED_LINE_SIZE  72

// What a question practises. A lesson names its skills on an "@speed" line, by these names:
// variables arithmetic conditions loops arrays strings functions pointers pointer_to_pointer
// pass_by_pointer pointer_arithmetic malloc function_pointers references new_delete objects
// linked_list
typedef enum {
    SPEED_VARIABLES, SPEED_ARITHMETIC, SPEED_CONDITIONS, SPEED_LOOPS, SPEED_ARRAYS, SPEED_STRINGS,
    SPEED_FUNCTIONS, SPEED_POINTERS, SPEED_POINTER_TO_POINTER, SPEED_PASS_BY_POINTER,
    SPEED_POINTER_ARITHMETIC, SPEED_MALLOC, SPEED_FUNCTION_POINTERS, SPEED_REFERENCES, SPEED_NEW_DELETE,
    SPEED_OBJECTS, SPEED_LINKED_LIST, SPEED_SKILL_COUNT
} SpeedSkill;

typedef struct {
    SpeedSkill skill;
    bool cpp;                                      // C++ (references, new, classes); C otherwise
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
typedef enum { SPEED_IGNORED, SPEED_RIGHT, SPEED_WRONG, SPEED_TIMED_OUT } SpeedOutcome;

// How well the learner knows a question, which buys more time (see SpeedSecondsFor).
typedef enum { SPEED_NEW, SPEED_STRUGGLING, SPEED_KNOWN } SpeedFamiliarity;
// Looks up the learner's familiarity with a question by its review id. The caller supplies
// it (from review.h), so this module needn't know where the history is kept.
typedef SpeedFamiliarity (*SpeedFamiliarityLookup)(const char *reviewId);

// One question of a run, as planned before it starts (speed_plan.h makes plans): one of a
// lesson's multiple-choice questions, or a generated one for a skill.
typedef struct {
    const Question *bank;              // a lesson question; NULL for a generated one
    SpeedSkill skill;                  // generated questions: what they practise
    char reviewId[SPEED_REVIEW_ID];    // what the answer is recorded under (review.h)
} SpeedPlanItem;

typedef struct {
    SpeedPlanItem plan[SPEED_QUESTIONS];
    int count;                          // questions in this run
    SpeedQuestion generated[SPEED_QUESTIONS];  // the generated questions, made at the start
    double seconds[SPEED_QUESTIONS];    // time allowed for each question
    bool endOnMiss;                     // lesson rounds: one miss ends the run; challenges carry on
    SpeedPhase phase;
    int number;               // zero-based number of the current (or failed) question
    double phaseStart;        // when the count-in or the current question began
    double endedAt;           // when the run failed or finished
    int picked[SPEED_QUESTIONS];        // the choice picked for each question, -1 for none
    int lastAnswered;         // the question the last outcome was for, -1 before any
} SpeedRound;

// Starts a run of count planned questions (at most SPEED_QUESTIONS), beginning with the
// count-in. Generated questions get harder along the run; familiarity (may be NULL: all
// known) sets each question's time.
void SpeedRoundStart(SpeedRound *round, const SpeedPlanItem *plan, int count, bool endOnMiss,
                     SpeedFamiliarityLookup familiarity, uint32_t seed, double now);
// Moves from the count-in to the first question, and handles running out of time: that
// ends a lesson round, and moves a challenge on. Returns SPEED_TIMED_OUT when it happened.
SpeedOutcome SpeedRoundUpdate(SpeedRound *round, double now);
// The learner picked choice. A right answer moves straight on to the next question; a wrong
// one ends a lesson round, and moves a challenge on. After the last question the run is
// SPEED_WON (for a challenge, whatever the score). Ignored outside SPEED_PLAYING.
SpeedOutcome SpeedRoundAnswer(SpeedRound *round, int choice, double now);
int SpeedRoundRight(const SpeedRound *round);  // questions answered right so far

// The current question's details, whichever kind it is.
bool SpeedRoundIsBank(const SpeedRound *round, int index);
int SpeedRoundChoiceCount(const SpeedRound *round, int index);
int SpeedRoundCorrect(const SpeedRound *round, int index);

double SpeedRoundTimeLeft(const SpeedRound *round, double now);  // seconds left on this question
// 0 when a question appears, rising to 1 at its time limit; 0 outside SPEED_PLAYING.
float SpeedRoundTension(const SpeedRound *round, double now);

// The time a question gets: at least SPEED_MIN_SECONDS, more for longer code or text, more
// again for a new question and most for one the learner has been struggling with, up to
// SPEED_MAX_SECONDS. generated is used when bank is NULL.
double SpeedSecondsFor(const Question *bank, const SpeedQuestion *generated, SpeedFamiliarity familiarity);

#endif
