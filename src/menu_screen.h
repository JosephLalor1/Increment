#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

#include "lesson.h"

// The main menu: a card per track with its progress, the code editor, the dashboard and
// Quit, plus the signed-in user's corner (Settings, Log out). Returns what was chosen;
// main.c does the navigating.

typedef enum {
    MENU_STAY,
    MENU_OPEN_TRACK,    // *trackOut says which
    MENU_CODE_EDITOR,
    MENU_DASHBOARD,
    MENU_SETTINGS,
    MENU_LOG_OUT,
    MENU_QUIT,
} MenuChoice;

MenuChoice MenuScreen(const LessonSet *set, Track *trackOut);

#endif
