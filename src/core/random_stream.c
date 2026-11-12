// A small repeatable random number stream. See random_stream.h.
#include "core/random_stream.h"

#include <string.h>

uint32_t RandomNext(RandomStream *stream)
{
    if (*stream == 0) *stream = 0x9e3779b9u;  // xorshift would stay at 0 for ever
    *stream ^= *stream << 13;
    *stream ^= *stream >> 17;
    *stream ^= *stream << 5;
    return *stream;
}

float RandomUnit(RandomStream *stream) { return (float)(RandomNext(stream) % 10000) / 10000.0f; }

float RandomBetween(RandomStream *stream, float low, float high)
{
    return low + (high - low) * RandomUnit(stream);
}

int RandomBelow(RandomStream *stream, int count)
{
    return count <= 1 ? 0 : (int)(RandomNext(stream) % (uint32_t)count);
}

char RandomChar(RandomStream *stream, const char *set)
{
    return set[RandomNext(stream) % (uint32_t)strlen(set)];
}
