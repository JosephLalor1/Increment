#ifndef MEMORY_VIEW_H
#define MEMORY_VIEW_H

#include "level/memory_probe.h"

#include "raylib.h"

// The level screen's picture of memory, for code that uses pointers: a box per variable
// with its name and value (arrays as a row of boxes), heap blocks set apart on the right,
// and an arrow from every pointer to what it points at. No words: the picture is the
// explanation.
//
// It moves with the learner. While they type, boxes appear for new declarations and each
// pointer's arrow swings to where the code now aims it, dashed, because nothing has run
// yet. After a run the arrows are drawn from the real addresses, solid, with light
// flowing along them, and values that changed flash. A null pointer ends in a ground
// mark, a freed block is crossed out in red, and a pointer to nowhere known ends in "?".
//
// It only draws: what the variables are comes from memory_probe.h.

void MemoryViewReset(void);  // forgets what it was showing (a new step)

// Draws the picture inside area (which it clips to and scrolls with the wheel).
// scan is the code as it stands; snapshot the last run's report (valid or not); fresh says
// the code hasn't changed since that run, so the report is the truth rather than a memory.
void MemoryViewDraw(Rectangle area, const ProbeScan *scan, const ProbeSnapshot *snapshot, bool fresh);

#endif
