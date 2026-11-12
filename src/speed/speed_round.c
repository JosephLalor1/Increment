// The clock and score of a speed round. See speed_round.h.
#include "speed/speed_round.h"

#include <string.h>

int SpeedRoundChoiceCount(const SpeedRound *round, int index)
{
    return index >= 0 && index < round->count ? SPEED_CHOICES : 0;
}

int SpeedRoundCorrect(const SpeedRound *round, int index)
{
    return index >= 0 && index < round->count ? round->generated[index].correct : -1;
}

double SpeedSecondsFor(const SpeedQuestion *question, SpeedFamiliarity familiarity)
{
    double seconds = SPEED_MIN_SECONDS;
    int lines = question->lineCount - question->firstBodyLine;
    if (lines > 4) seconds += (lines - 4) * 0.8;  // more code takes more tracing
    if (familiarity == SPEED_NEW) seconds += 4;
    if (familiarity == SPEED_STRUGGLING) seconds += 6;
    return seconds < SPEED_MIN_SECONDS ? SPEED_MIN_SECONDS : seconds > SPEED_MAX_SECONDS ? SPEED_MAX_SECONDS : seconds;
}

void SpeedRoundStart(SpeedRound *round, const SpeedPlanItem *plan, int count, bool endOnMiss,
                     SpeedFamiliarityLookup familiarity, uint32_t seed, double now)
{
    memset(round, 0, sizeof *round);
    if (count > SPEED_QUESTIONS) count = SPEED_QUESTIONS;
    if (count < 0) count = 0;
    round->count = count;
    round->endOnMiss = endOnMiss;
    if (seed == 0) seed = 1;
    for (int i = 0; i < count; i++) {
        round->plan[i] = plan[i];
        round->picked[i] = -1;
        // Questions grow with the run, so its end is the hardest part.
        SpeedGenerate(plan[i].skill, i, &seed, &round->generated[i]);
        SpeedFamiliarity known = familiarity ? familiarity(plan[i].reviewId) : SPEED_KNOWN;
        round->seconds[i] = SpeedSecondsFor(&round->generated[i], known);
    }
    round->phase = count > 0 ? SPEED_COUNTING_IN : SPEED_WON;
    round->phaseStart = now;
    round->lastAnswered = -1;
}

// After question number has been answered (or timed out): the next one, or the end.
static void MoveOn(SpeedRound *round, double now)
{
    if (round->number + 1 >= round->count) {
        round->phase = SPEED_WON;
        round->endedAt = now;
        return;
    }
    round->number++;
    round->phaseStart = now;  // no breather: the next question starts now
}

SpeedOutcome SpeedRoundUpdate(SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN && now - round->phaseStart >= SPEED_COUNT_IN) {
        round->phase = SPEED_PLAYING;
        round->phaseStart += SPEED_COUNT_IN;
    }
    if (round->phase != SPEED_PLAYING) return SPEED_IGNORED;
    double limit = round->seconds[round->number];
    if (now - round->phaseStart < limit) return SPEED_IGNORED;
    double timedOutAt = round->phaseStart + limit;
    round->lastAnswered = round->number;
    if (round->endOnMiss) {
        round->phase = SPEED_FAILED;
        round->endedAt = timedOutAt;
    } else {
        MoveOn(round, timedOutAt);
    }
    return SPEED_TIMED_OUT;
}

SpeedOutcome SpeedRoundAnswer(SpeedRound *round, int choice, double now)
{
    if (SpeedRoundUpdate(round, now) == SPEED_TIMED_OUT) return SPEED_IGNORED;  // too late: the timeout counts
    if (round->phase != SPEED_PLAYING || choice < 0 || choice >= SpeedRoundChoiceCount(round, round->number))
        return SPEED_IGNORED;
    round->picked[round->number] = choice;
    round->lastAnswered = round->number;
    if (choice != SpeedRoundCorrect(round, round->number)) {
        if (round->endOnMiss) {
            round->phase = SPEED_FAILED;
            round->endedAt = now;
        } else {
            MoveOn(round, now);
        }
        return SPEED_WRONG;
    }
    MoveOn(round, now);
    return SPEED_RIGHT;
}

int SpeedRoundRight(const SpeedRound *round)
{
    int right = 0;
    for (int i = 0; i < round->count; i++) right += round->picked[i] >= 0 && round->picked[i] == SpeedRoundCorrect(round, i);
    return right;
}

double SpeedRoundTimeLeft(const SpeedRound *round, double now)
{
    if (round->phase == SPEED_COUNTING_IN) return round->count ? round->seconds[0] : 0;
    if (round->phase != SPEED_PLAYING) return 0;
    double left = round->seconds[round->number] - (now - round->phaseStart);
    return left < 0 ? 0 : left;
}

float SpeedRoundTension(const SpeedRound *round, double now)
{
    if (round->phase != SPEED_PLAYING) return 0;
    float t = (float)((now - round->phaseStart) / round->seconds[round->number]);
    return t < 0 ? 0 : t > 1 ? 1 : t;
}
