// The window's mode and where it's saved. See window_mode.h.
#include "window_mode.h"
#include "raylib.h"

#include <stdio.h>

#define NORMAL_WIDTH  1280
#define NORMAL_HEIGHT 800

static WindowMode mode = WINDOW_MAXIMIZED;
static WindowMode windowedMode = WINDOW_MAXIMIZED;  // where F11 returns to from full screen
static bool enabled;
static char savePath[1100];

static bool FullScreen(void) { return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE); }

// A 1280x800 window centred on its monitor (smaller if the monitor is).
static void CentredWindow(void)
{
    int monitor = GetCurrentMonitor();
    int monitorW = GetMonitorWidth(monitor), monitorH = GetMonitorHeight(monitor);
    int w = NORMAL_WIDTH < monitorW ? NORMAL_WIDTH : monitorW;
    int h = NORMAL_HEIGHT < monitorH - 80 ? NORMAL_HEIGHT : monitorH - 80;  // leave room for the taskbar
    Vector2 origin = GetMonitorPosition(monitor);
    SetWindowSize(w, h);
    SetWindowPosition((int)origin.x + (monitorW - w) / 2, (int)origin.y + (monitorH - h) / 2);
}

static void Apply(void)
{
    if (!enabled) return;
    switch (mode) {
    case WINDOW_FULL_SCREEN:
        // Borderless rather than exclusive full screen: it covers the monitor without
        // changing its resolution, so Alt+Tab stays instant.
        if (!FullScreen()) ToggleBorderlessWindowed();
        break;
    case WINDOW_NORMAL:
        if (FullScreen()) ToggleBorderlessWindowed();
        if (IsWindowMaximized()) RestoreWindow();
        CentredWindow();
        break;
    case WINDOW_MAXIMIZED:
    default:
        if (FullScreen()) ToggleBorderlessWindowed();
        MaximizeWindow();
        break;
    }
}

static bool Save(void)
{
    if (!savePath[0]) return false;
    FILE *f = fopen(savePath, "w");
    if (!f) {
        TraceLog(LOG_WARNING, "WINDOW: could not save %s", savePath);
        return false;
    }
    bool ok = fprintf(f, "window %d\n", (int)mode) > 0;
    return (fclose(f) == 0) && ok;
}

void WindowModeInit(const char *dataDir)
{
    snprintf(savePath, sizeof savePath, "%swindow.txt", dataDir ? dataDir : "");
    mode = WINDOW_MAXIMIZED;
    FILE *f = fopen(savePath, "r");
    if (f) {
        int value;
        if (fscanf(f, "window %d", &value) == 1 && value >= 0 && value < WINDOW_MODE_COUNT) mode = (WindowMode)value;
        fclose(f);
    }
    windowedMode = mode == WINDOW_FULL_SCREEN ? WINDOW_MAXIMIZED : mode;
}

void WindowModeEnable(void)
{
    enabled = true;
    Apply();
}

WindowMode WindowModeGet(void) { return mode; }

bool WindowModeSet(WindowMode newMode)
{
    if (newMode < 0 || newMode >= WINDOW_MODE_COUNT) return false;
    mode = newMode;
    if (mode != WINDOW_FULL_SCREEN) windowedMode = mode;
    Apply();
    return Save();
}

bool WindowModeToggleFullScreen(void)
{
    return WindowModeSet(mode == WINDOW_FULL_SCREEN ? windowedMode : WINDOW_FULL_SCREEN);
}
