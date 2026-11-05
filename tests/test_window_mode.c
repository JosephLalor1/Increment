// The window mode setting: default, saving and reloading, bad files, and F11's toggle.
// WindowModeEnable is never called, so no window is opened or moved.
// Usage: test_window_mode <empty temp dir>
#include "../src/window_mode.h"
#include "raylib.h"  // SetTraceLogLevel
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char dir[1024];  // ends in a slash, as main.c passes it

static void WriteSetting(const char *text)
{
    char path[1100];
    snprintf(path, sizeof path, "%swindow.txt", dir);
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_window_mode <empty temp dir>\n");
        return 2;
    }
    snprintf(dir, sizeof dir, "%s/", argv[1]);
    SetTraceLogLevel(LOG_WARNING);

    // No file yet: a maximised window, not full screen.
    WindowModeInit(dir);
    assert(WindowModeGet() == WINDOW_MAXIMIZED);

    // A choice is saved and comes back next launch.
    assert(WindowModeSet(WINDOW_NORMAL));
    WindowModeInit(dir);
    assert(WindowModeGet() == WINDOW_NORMAL);

    // F11 goes to full screen and back to the windowed mode used before, saving each time.
    assert(WindowModeToggleFullScreen() && WindowModeGet() == WINDOW_FULL_SCREEN);
    WindowModeInit(dir);
    assert(WindowModeGet() == WINDOW_FULL_SCREEN);
    assert(WindowModeToggleFullScreen() && WindowModeGet() == WINDOW_MAXIMIZED);  // after a restart: maximised
    assert(WindowModeSet(WINDOW_NORMAL));
    assert(WindowModeToggleFullScreen() && WindowModeGet() == WINDOW_FULL_SCREEN);
    assert(WindowModeToggleFullScreen() && WindowModeGet() == WINDOW_NORMAL);     // same session: as it was

    // Unknown or broken files fall back to the default.
    WriteSetting("window 9\n");
    WindowModeInit(dir);
    assert(WindowModeGet() == WINDOW_MAXIMIZED);
    WriteSetting("rubbish");
    WindowModeInit(dir);
    assert(WindowModeGet() == WINDOW_MAXIMIZED);
    assert(!WindowModeSet((WindowMode)7) && WindowModeGet() == WINDOW_MAXIMIZED);

    // A folder that can't be written: the mode still changes for this session.
    WindowModeInit("Z:/no/such/folder/");
    assert(!WindowModeSet(WINDOW_FULL_SCREEN) && WindowModeGet() == WINDOW_FULL_SCREEN);

    printf("window mode: all tests passed\n");
    return 0;
}
