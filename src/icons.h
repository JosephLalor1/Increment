#ifndef ICONS_H
#define ICONS_H

#include "raylib.h"

// Line icons drawn from shapes, so they scale and take any colour (and follow the theme).
typedef enum {
    ICON_BACK,
    ICON_NEXT,
    ICON_CHECK,
    ICON_CROSS,
    ICON_GEAR,
    ICON_LOGOUT,
    ICON_POWER,
    ICON_USER,
    ICON_KEY,
    ICON_LOCK,
    ICON_THEME,
    ICON_TRASH,
    ICON_INFO,
    ICON_BOOK,
    ICON_RETRY,
    ICON_PLAY,
    ICON_TROPHY,
    ICON_COPY,
    ICON_TARGET,
    ICON_CODE,
    ICON_MAIL,
    ICON_BOOKMARK,         // outline: not saved
    ICON_BOOKMARK_FILLED,  // saved
    ICON_COUNT
} Icon;

// Draws icon centred on c, fitting a size x size box.
void IconDraw(Icon icon, Vector2 c, float size, Color col);

#endif
