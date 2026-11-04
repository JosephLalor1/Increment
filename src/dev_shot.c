// The --shot dev aid and the icon gallery screen. See dev_shot.h.
#include "dev_shot.h"
#include "account.h"
#include "account_screens.h"
#include "brand.h"
#include "code_screen.h"
#include "dashboard.h"
#include "layout.h"
#include "lesson_screen.h"
#include "progress.h"
#include "track_screen.h"
#include "ui.h"

#include "raylib.h"
#include "rlgl.h"

#include <stdlib.h>
#include <string.h>

static bool Is(const char *name, const char *wanted) { return strcmp(name, wanted) == 0; }

// Opens the code editor for the editor, exercise and tryit shots. Returns false when that
// isn't possible (signed out, or no such exercise or example).
static bool PrepareCodeShot(const char *name, const LessonSet *lessons, int lessonIndex, int exerciseNumber)
{
    if (!AccountCurrent()) return false;  // drafts and progress need a user
    CodeSetDraftFolder(AccountCodeDir());
    if (Is(name, "editor")) {
        CodeOpenPlayground();
        return true;
    }
    if (lessons->count == 0) return false;
    const Lesson *lesson = &lessons->items[lessonIndex];
    if (Is(name, "exercise")) {
        int exercise = exerciseNumber - 1;
        if (exercise < 0 || exercise >= lesson->exerciseCount) return false;
        CodeOpenExercise(lesson, exercise);
        return true;
    }
    for (int b = 0; b < lesson->blockCount; b++) {  // tryit: the lesson's first runnable example
        if (lesson->blocks[b].type == BLOCK_CODE && strstr(lesson->blocks[b].text, "main(")) {
            CodeOpenSnippet(TextFormat("Try it: %s", lesson->title), lesson->blocks[b].text, lesson->track == TRACK_CPP);
            return true;
        }
    }
    return false;
}

// The screen a shot name shows, setting up that screen's state on the way.
// extra is argv[5]: a scroll position, a splash time in ms, or an exercise number.
static AppScreen PrepareScreen(const char *name, const LessonSet *lessons, int lessonIndex, const char *extra,
                               DevShot *shot)
{
    float scroll = extra ? (float)atof(extra) : 0;

    if (Is(name, "splash")) {
        shot->splashTime = (extra ? (float)atof(extra) : 1500) / 1000.0f;
        return SCREEN_SPLASH;
    }
    if (Is(name, "gallery")) return SCREEN_GALLERY;
    if (Is(name, "login") || Is(name, "register") || Is(name, "reset") || Is(name, "resetemail")
        || Is(name, "resetcode")) {
        return SCREEN_LOGIN;
    }
    if (Is(name, "recovery")) return SCREEN_RECOVERY;
    if (Is(name, "verify") || Is(name, "addemail")) return AccountCurrent() ? SCREEN_VERIFY : SCREEN_LOGIN;
    if (Is(name, "settings") || Is(name, "delete")) {
        SettingsOpen();
        return SCREEN_SETTINGS;
    }
    if (Is(name, "editor") || Is(name, "exercise") || Is(name, "tryit")) {
        if (!PrepareCodeShot(name, lessons, lessonIndex, extra ? atoi(extra) : 1)) return SCREEN_MENU;
        const char *runFirst = getenv("INCREMENT_SHOT_RUN");
        if (runFirst && runFirst[0] == '1') CodeRunNow();
        return SCREEN_CODE;
    }
    if (Is(name, "dashboard")) {
        DashboardOpen();
        DashboardDemo(scroll);
        return SCREEN_DASHBOARD;
    }

    // The rest show lesson content, so they need lessons.
    if (lessons->count == 0) return SCREEN_MENU;
    if (Is(name, "list")) {
        // INCREMENT_SHOT_SEARCH types into the search box (even " " focuses it, for the
        // empty-search hint); INCREMENT_SHOT_FILTER is c or cpp.
        const char *filterName = getenv("INCREMENT_SHOT_FILTER");
        SearchFilter filter = !filterName           ? SEARCH_BOTH
                            : Is(filterName, "cpp") ? SEARCH_CPP
                            : Is(filterName, "c")   ? SEARCH_C
                                                    : SEARCH_BOTH;
        TrackScreenOpen(lessons->items[lessonIndex].track);
        TrackScreenDemo(getenv("INCREMENT_SHOT_SEARCH"), filter, scroll);
        return SCREEN_TRACK;
    }
    LessonPage page = Is(name, "lesson")                         ? LESSON_PAGE_READING
                    : Is(name, "quiz") || Is(name, "answered")   ? LESSON_PAGE_QUIZ
                    : Is(name, "result")                         ? LESSON_PAGE_RESULT
                                                                 : (LessonPage)-1;
    if ((int)page < 0) return SCREEN_MENU;  // "menu", or a name we don't know
    LessonScreenOpen(lessons, lessonIndex, "All lessons");
    LessonScreenDemo(page, scroll, Is(name, "answered"));
    return SCREEN_LESSON;
}

DevShot DevShotParse(int argc, char **argv, const LessonSet *lessons)
{
    DevShot shot = { .outputPath = NULL, .screen = SCREEN_SPLASH, .splashTime = -1 };
    if (argc < 3 || strcmp(argv[1], "--shot") != 0) return shot;
    shot.outputPath = argv[2];
    shot.framesLeft = 3;

    const char *height = getenv("INCREMENT_SHOT_HEIGHT");
    if (height) SetWindowSize(1280, atoi(height));

    const char *name = argc > 3 ? argv[3] : "menu";
    int lessonIndex = argc > 4 ? atoi(argv[4]) : 0;
    if (lessonIndex < 0 || lessonIndex >= lessons->count) lessonIndex = 0;
    const char *extra = argc > 5 ? argv[5] : NULL;

    // Content screens normally require a login; shots may sign in to show a user's state.
    const char *user = getenv("INCREMENT_SHOT_USER"), *password = getenv("INCREMENT_SHOT_PASS");
    const char *err;
    if (user && password && AccountLogin(user, password, &err)) ProgressLoad(AccountProgressPath());
    ApplyUserAppearance();

    shot.screen = PrepareScreen(name, lessons, lessonIndex, extra, &shot);
    AccountScreensDemo(name);
    return shot;
}

bool DevShotCapture(DevShot *shot)
{
    if (!shot->outputPath) return false;
    // A shot of the code editor waits for the compile and run to finish (up to ~30 s).
    if (CodeBusy() && shot->framesLeft == 1 && shot->framesWaited++ < 1800) return false;
    if (--shot->framesLeft > 0) return false;

    rlDrawRenderBatchActive();  // pending batched draws aren't in the framebuffer yet
    Image image = LoadImageFromScreen();
    ExportImage(image, shot->outputPath);
    UnloadImage(image);
    return true;
}

void DevGalleryScreen(void)
{
    static const char *names[ICON_COUNT] = {
        "back", "next", "check", "cross", "gear", "logout", "power", "user", "key", "lock",
        "theme", "trash", "info", "book", "retry", "play", "trophy", "copy", "target", "code", "mail",
    };
    // The logo at several sizes along the top, and the wordmark lockup at the right.
    float x = 40, y = 40;
    float sizes[] = { 128, 64, 48, 32, 16 };
    for (int i = 0; i < 5; i++) {
        BrandDrawLogo((Vector2){ x, y + (128 - sizes[i]) }, sizes[i], COL_BRAND, WHITE, LOGO_STATIC, i == 0);
        x += sizes[i] + 28;
    }
    DrawLogoLockup(GetScreenWidth() - 300.0f, y + 30, 70);

    // Every icon, large in a tile and small in the accent colour beside its name.
    y = 220;
    for (int i = 0; i < ICON_COUNT; i++) {
        float cx = 70 + (i % 10) * 118.0f, cy = y + (i / 10) * 150.0f;
        UiRoundRect((Rectangle){ cx - 44, cy - 44, 88, 88 }, 12, COL_PANEL);
        IconDraw((Icon)i, (Vector2){ cx, cy }, 48, COL_TEXT);
        IconDraw((Icon)i, (Vector2){ cx + 30, cy + 62 }, 18, COL_ACCENT);
        UiTextCentered(gFonts.text, names[i], cx - 10, cy + 52, 16, COL_DIM);
    }

    // Icons in context.
    UiButtonIcon((Rectangle){ 40, 690, 240, 50 }, ICON_PLAY, "Start quiz", true);
    UiButtonIconEx((Rectangle){ 300, 690, 160, 50 }, ICON_GEAR, "Settings", 20, COL_PANEL, COL_SECONDARY_HOVER,
                   COL_TEXT, true);
    UiIconText(ICON_CHECK, gFonts.bold, "Correct!", 490, 702, 26, COL_GOOD);
}
