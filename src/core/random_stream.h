#ifndef RANDOM_STREAM_H
#define RANDOM_STREAM_H

#include <stdint.h>

// A small repeatable random number stream (xorshift32). The caller owns the state, so
// each user has its own sequence: effects keep one each and never disturb one another,
// and question generators can be replayed from a seed in tests. No drawing, no raylib.

typedef uint32_t RandomStream;  // the state; any value (0 is replaced by a fixed seed)

uint32_t RandomNext(RandomStream *stream);                           // the next 32 bits
float RandomUnit(RandomStream *stream);                              // from 0 up to, not including, 1
float RandomBetween(RandomStream *stream, float low, float high);    // from low up to, not including, high
int RandomBelow(RandomStream *stream, int count);                    // 0 to count - 1 (0 if count < 2)
char RandomChar(RandomStream *stream, const char *set);              // one character of a non-empty string

#endif
