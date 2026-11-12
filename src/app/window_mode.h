#ifndef WINDOW_MODE_H
#define WINDOW_MODE_H

#include <stdbool.h>

// How the app's window appears: maximised (the default), full screen, or a normal
// 1280x800 window. It's a setting of the computer rather than of a student, so it lives
// in <data>/window.txt ("window <n>"), shared by every account and used before anyone
// signs in. Only this file moves or resizes the window.

typedef enum {
    WINDOW_MAXIMIZED,    // a normal window filling the screen, title bar and taskbar showing
    WINDOW_FULL_SCREEN,  // borderless, covering the whole monitor
    WINDOW_NORMAL,       // a 1280x800 window in the middle of the screen
    WINDOW_MODE_COUNT
} WindowMode;

// Loads the saved mode from dataDir (which ends in a slash). A missing or unreadable file
// means the default, WINDOW_MAXIMIZED.
void WindowModeInit(const char *dataDir);

// Turns window changes on and applies the saved mode. Until it's called, nothing moves the
// window, so --shot runs keep their fixed 1280x800 one. main.c calls it for normal runs.
void WindowModeEnable(void);

WindowMode WindowModeGet(void);

// Switches to mode and saves it. Returns false if it couldn't be saved (it still applies
// for this session).
bool WindowModeSet(WindowMode mode);

// F11: full screen, or back to the windowed mode used before it. Saved like WindowModeSet.
bool WindowModeToggleFullScreen(void);

#endif
