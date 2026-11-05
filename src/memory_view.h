#ifndef MEMORY_VIEW_H
#define MEMORY_VIEW_H

// The interactive pointer diagram drawn for a ```memory block in a lesson: the example's
// code, memory as boxes (stack frames and the heap) with arrows from each pointer to what
// it points at, and an explanation. Clicking a line of code runs the example up to that
// line, with the arrows and values animating to their new state; clicking a box explains
// it (and for a pointer, sends a dot along its arrow). Colours come from the theme and
// accent. The script format and the explanations are memory_model.h's job.

// Draws the diagram for script at (x, y), w wide, with lesson text scaled by scale, and
// handles its clicks. Returns the height used. Each script keeps its own step and
// animation state while the lesson is open.
float MemoryViewDraw(const char *script, float x, float y, float w, float scale);

// Frees the parsed scripts, e.g. at exit.
void MemoryViewShutdown(void);

// Dev aid for --shot: diagrams drawn from now on start at this step (0-based, clamped) with
// this box selected (a cell index, or -1 for none), already settled.
void MemoryViewDemo(int step, int selectedCell);

#endif
