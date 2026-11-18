#ifndef SCREEN_PICTURE_H
#define SCREEN_PICTURE_H

#include "raylib.h"

// Keeps what has been drawn this frame as a texture, so a transition can go on showing a
// screen (zooming into it, closing an iris on it) after the screen itself has moved on.
// Unloads previous (if any) and returns the new picture, or an empty texture (id 0) if the
// framebuffer couldn't be read.
Texture2D ScreenPictureTake(Texture2D previous);

#endif
