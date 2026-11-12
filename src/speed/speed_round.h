#ifndef SPEED_ROUND_H
#define SPEED_ROUND_H

// Speed rounds: an optional, timed challenge for gold mastery of a lesson. Each question is
// a short generated snippet ("what is x after these lines?") with four answers; the learner
// has its own time per question (SpeedSecondsFor), the next one follows at once, and one
// wrong answer or timeout ends the run. Answering all SPEED_QUESTIONS earns gold. A short
// challenge runs the same way over the learner's most necessary questions, but carries on
// after a miss.
//
// This module runs the round: its clock, the answers picked and the outcome. The questions
// come from speed_question.h. It never draws and never reads the real clock (callers pass
// `now`), so it can be unit-tested; speed_screen.c draws it.

#include <stdbool.h>
#include <stdint.h>

#include "speed/speed_question.h"

#define SPEED_QUESTIONS  10
#define SPEED_MIN_SECONDS 10.0  // every question gets at least this long
#define SPEED_MAX_SECONDS 25.0  // and at most this
#define SPEED_COUNT_IN   3.0   // "3, 2, 1" before the first question
#define SPEED_REVIEW_ID  64

typedef enum { SPEED_COUNTING_IN, SPEED_PLAYING, SPEED_FAILED, SPEED_WON } SpeedPhase;
typedef enum { SPEED_IGNORED, SPEED_RIGHT, SPEED_WRONG, SPEED_TIMED_OUT } SpeedOutcome;

// How well the learner knows a question, which buys more time (see SpeedSecondsFor).
typedef enum { SPEED_NEW, SPEED_STRUGGLING, SPEED_KNOWN } SpeedFamiliarity;
// Looks up the learner's familiarity with a question by its review id. The caller supplies
// it (from review.h), so this module needn't know where the history is kept.
typedef SpeedFamiliarity (*SpeedFamiliarityLookup)(const char *reviewId);

// One question of a run, as planned before it starts (speed_plan.h makes plans): the skill
// its generated question practises.
typedef struct {
    SpeedSkill skill;
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

// A question's choices and which is right.
int SpeedRoundChoiceCount(const SpeedRound *round, int index);
int SpeedRoundCorrect(const SpeedRound *round, int index);

double SpeedRoundTimeLeft(const SpeedRound *round, double now);  // seconds left on this question
// 0 when a question appears, rising to 1 at its time limit; 0 outside SPEED_PLAYING.
float SpeedRoundTension(const SpeedRound *round, double now);

// The time a question gets: at least SPEED_MIN_SECONDS, more for longer code, more
// again for a new question and most for one the learner has been struggling with, up to
// SPEED_MAX_SECONDS.
double SpeedSecondsFor(const SpeedQuestion *question, SpeedFamiliarity familiarity);

#endif
