#ifndef DEV_SHOT_H
#define DEV_SHOT_H

#include "app_screen.h"
#include "lesson.h"

#include <stdbool.h>

// Dev aid: `increment.exe --shot out.png <screen> [lessonIndex] [extra]` renders one screen
// to a PNG and exits, so screens can be checked without clicking through the app. See
// CLAUDE.md for the screen names and the INCREMENT_SHOT_* environment variables.

typedef struct {
    const char *outputPath;  // NULL when not taking a shot
    AppScreen screen;        // the screen to show
    float splashTime;        // splash: seconds into the animation to show; < 0 runs it live
    int framesLeft;          // frames to draw before capturing (layout settles over a couple)
    int framesWaited;        // frames spent waiting for a program to finish running
    int delayFrames;         // extra frames to wait after that (INCREMENT_SHOT_DELAY)
} DevShot;

// Reads --shot from the command line and prepares the requested screen's state (signing in
// with INCREMENT_SHOT_USER / INCREMENT_SHOT_PASS when set). Call after accounts, email and
// lessons are loaded. Returns a shot with outputPath NULL for a normal run.
DevShot DevShotParse(int argc, char **argv, const LessonSet *lessons);

// Call each frame after drawing the screen, before EndDrawing. Saves the PNG when the
// screen is ready and returns true then (the app should quit).
bool DevShotCapture(DevShot *shot);

// The SCREEN_GALLERY screen: the logo at several sizes, every icon, and sample widgets.
void DevGalleryScreen(void);

#endif
