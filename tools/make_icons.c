// Generates the app icon files in assets/icon from the logo in src/brand.c.
// Run via: build.bat icons
//
// Writes icon_<size>.png for each icon size, logo_512.png, and increment.ico
// (a multi-size icon with PNG entries, used for the .exe and window icon).
#include "raylib.h"
#include "ui/brand.h"

#include <stdio.h>
#include <string.h>

static const int iconSizes[] = { 16, 24, 32, 48, 64, 128, 256 };
#define ICON_COUNT (int)(sizeof iconSizes / sizeof iconSizes[0])

// Render at 4x and downscale: render textures have no MSAA, so this gives smooth edges.
static Image RenderLogo(int size)
{
    int big = size * 4;
    RenderTexture2D rt = LoadRenderTexture(big, big);
    BeginTextureMode(rt);
    // Transparent, but with the tile's RGB so downscaled edges don't pick up a dark fringe.
    ClearBackground((Color){ BRAND_BLUE.r, BRAND_BLUE.g, BRAND_BLUE.b, 0 });
    BrandDrawLogo((Vector2){ 0, 0 }, (float)big, BRAND_BLUE, WHITE, LOGO_STATIC, false);
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);  // render textures are stored upside down
    ImageResize(&img, size, size);
    UnloadRenderTexture(rt);
    return img;
}

static void Put16(FILE *f, int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void Put32(FILE *f, int v) { Put16(f, v & 0xFFFF); Put16(f, (v >> 16) & 0xFFFF); }

static bool WriteIco(const char *path, Image *images, int count)
{
    unsigned char *png[ICON_COUNT];
    int pngSize[ICON_COUNT];
    for (int i = 0; i < count; i++) png[i] = ExportImageToMemory(images[i], ".png", &pngSize[i]);

    FILE *f = fopen(path, "wb");
    if (!f) return false;
    Put16(f, 0);      // reserved
    Put16(f, 1);      // type: icon
    Put16(f, count);
    int offset = 6 + 16 * count;
    for (int i = 0; i < count; i++) {
        int s = images[i].width;
        fputc(s >= 256 ? 0 : s, f);  // width (0 means 256)
        fputc(s >= 256 ? 0 : s, f);  // height
        fputc(0, f);                 // palette colours
        fputc(0, f);                 // reserved
        Put16(f, 1);                 // colour planes
        Put16(f, 32);                // bits per pixel
        Put32(f, pngSize[i]);
        Put32(f, offset);
        offset += pngSize[i];
    }
    for (int i = 0; i < count; i++) {
        fwrite(png[i], 1, (size_t)pngSize[i], f);
        MemFree(png[i]);
    }
    fclose(f);
    return true;
}

int main(int argc, char **argv)
{
    const char *outDir = argc > 1 ? argv[1] : "assets/icon";
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "make_icons");
    MakeDirectory(outDir);

    Image images[ICON_COUNT];
    for (int i = 0; i < ICON_COUNT; i++) {
        images[i] = RenderLogo(iconSizes[i]);
        ExportImage(images[i], TextFormat("%s/icon_%d.png", outDir, iconSizes[i]));
    }
    Image logo = RenderLogo(512);
    ExportImage(logo, TextFormat("%s/logo_512.png", outDir));
    UnloadImage(logo);

    bool ok = WriteIco(TextFormat("%s/increment.ico", outDir), images, ICON_COUNT);
    for (int i = 0; i < ICON_COUNT; i++) UnloadImage(images[i]);
    CloseWindow();

    printf(ok ? "Wrote icons to %s\n" : "Failed to write %s/increment.ico\n", outDir);
    return ok ? 0 : 1;
}
