#ifndef STEP_NOVELTY_H
#define STEP_NOVELTY_H

#include "learning/lesson.h"

#include <stdbool.h>

// What a step shows the learner for the first time: the code words and syntax in its starter
// or solution that no earlier step used (earlier steps of its level, and every step of the
// levels before it in course order, so the C++ track counts the C track as learned). The
// level screen animates them as the step opens. Data only, unit-tested (tests/test_steps.c).
//
// The pieces that count are what a learner has to learn, not what they name themselves:
//  - the language's and its libraries' words: for, scanf, malloc, cout, class, nullptr...
//  - operators and punctuation with a meaning of their own: -> :: << >> ++ -- += == != <=
//    && || ! % ? [ ] (as "[]") and & (address or reference);
//  - pointer syntax: a type with stars ("int*", "char**") and the dereferencing star ("*",
//    told apart from multiplication by what comes before it);
//  - preprocessor lines: "#include" and the header, "<stdio.h>";
//  - inside strings: format conversions ("%d", "%.2f", "%c") and escapes ("\n", "\t").

#define CODE_PIECE_KEY 24   // the longest piece, with its terminator
#define NOVELTY_MAX 8       // the most new pieces a step reports

// One piece found in code: where it is, and what it is (its key, which is also how it's
// shown: "for", "->", "int*", "%d"). The dereferencing star's key is "*"; multiplication
// isn't a piece.
typedef struct {
    int start, length;
    char key[CODE_PIECE_KEY];
} CodePiece;

// The pieces in code, in order; returns how many (at most max).
int CodePiecesFind(const char *code, CodePiece *out, int max);

typedef struct {
    char key[CODE_PIECE_KEY];
    bool inStarter;  // it's in the code the step opens with; otherwise only in its solution
} NovelPiece;

typedef struct {
    NovelPiece pieces[NOVELTY_MAX];  // in the order they first appear (starter, then solution)
    int count;
} StepNovelty;

// The pieces step (0-based) of set's level lessonIndex shows for the first time.
void StepNoveltyFind(const LessonSet *set, int lessonIndex, int step, StepNovelty *out);

#endif
