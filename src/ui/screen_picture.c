// A picture of the screen as drawn so far this frame. See screen_picture.h.
#include "ui/screen_picture.h"

#include "rlgl.h"

Texture2D ScreenPictureTake(Texture2D previous)
{
    if (previous.id) UnloadTexture(previous);
    rlDrawRenderBatchActive();  // what was drawn this frame isn't in the framebuffer until flushed
    Image screen = LoadImageFromScreen();
    Texture2D picture = LoadTextureFromImage(screen);
    UnloadImage(screen);
    if (picture.id) SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
    return picture;
}
