// Increment: a desktop app for learning C and C++.
//
// This file starts the app and owns navigation. Each screen lives in its own module and
// reports what the user asked for (an action, a choice, or a lesson to open); the functions
// below decide which screen comes next. No screen switches screens itself.

#include "raylib.h"

#include "account.h"
#include "account_screens.h"
#include "app_screen.h"
#include "code_screen.h"
#include "dashboard.h"
#include "dev_shot.h"
#include "email.h"
#include "layout.h"
#include "lesson.h"
#include "lesson_screen.h"
#include "mascot.h"
#include "progress.h"
#include "splash.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LessonSet lessons;

static struct {
    AppScreen screen;
    AppScreen codeReturn;     // where the code editor goes back to
    AppScreen afterRecovery;  // where to go once a new recovery code has been saved
    double splashStart;       // GetTime() when the splash began
    float splashTime;         // --shot splash: a fixed moment of the animation, or < 0 to run live
    bool quit;
} app;

// ---------------------------------------------------------------------------
// Opening screens

// Lessons are always opened from home, so Back from a lesson goes home.
static void OpenLesson(int lessonIndex)
{
    LessonScreenOpen(&lessons, lessonIndex, "Home");
    app.screen = SCREEN_LESSON;
}

// Switches to the code editor. Call before one of the CodeOpen* functions, which load the
// signed-in user's draft from the folder set here.
static void OpenCodeEditor(AppScreen returnTo)
{
    CodeSetDraftFolder(AccountCodeDir());
    app.codeReturn = returnTo;
    app.screen = SCREEN_CODE;
}

static void OpenSettings(void)
{
    SettingsOpen();
    app.screen = SCREEN_SETTINGS;
}

static void ShowLogin(void)
{
    LessonScreenClearAttempt();
    MascotResetInteractions();
    ProgressLoad("");  // drop the previous user's scores from memory
    LoginReset();
    ApplyUserAppearance();
    app.screen = SCREEN_LOGIN;
}

static void LogOut(void)
{
    AccountLogout();
    ShowLogin();
}

// After signing in (or verifying): show a new recovery code if there is one, then home.
static void EnterApp(void)
{
    DashboardOpen();  // a fresh home for this user
    if (RecoveryPending()) {
        app.afterRecovery = SCREEN_HOME;
        app.screen = SCREEN_RECOVERY;
    } else {
        app.screen = SCREEN_HOME;
    }
}

static void SignedIn(void)
{
    ProgressLoad(AccountProgressPath());
    if (VerifyNeeded()) {  // the account isn't usable until its email is verified
        VerifyOpen(false);
        app.screen = SCREEN_VERIFY;
    } else {
        EnterApp();
    }
}

// ---------------------------------------------------------------------------
// Acting on each screen's result

static void RunSplash(void)
{
    float t = app.splashTime >= 0 ? app.splashTime : (float)(GetTime() - app.splashStart);
    bool finished = SplashScreen(t);
    if (finished && app.splashTime < 0) app.screen = SCREEN_LOGIN;
}

static void RunLogin(void)
{
    AccountAction action = LoginScreen();
    if (action == ACCOUNT_SIGNED_IN) SignedIn();
    else if (action == ACCOUNT_QUIT) app.quit = true;
}

static void RunRecovery(void)
{
    if (RecoveryScreen() == ACCOUNT_BACK) app.screen = app.afterRecovery;
}

static void RunVerify(void)
{
    AccountAction action = VerifyScreen();
    if (action == ACCOUNT_VERIFIED) {
        if (VerifyChanging()) app.screen = SCREEN_SETTINGS;
        else EnterApp();
    } else if (action == ACCOUNT_BACK) {
        app.screen = SCREEN_SETTINGS;
    } else if (action == ACCOUNT_SIGNED_OUT) {
        LogOut();
    }
}

static void RunSettings(void)
{
    AccountAction action = SettingsScreen();
    if (action == ACCOUNT_BACK) {
        app.screen = SCREEN_HOME;
    } else if (action == ACCOUNT_SIGNED_OUT) {  // the account was deleted
        ShowLogin();
    } else if (action == ACCOUNT_SHOW_CODE) {
        app.afterRecovery = SCREEN_SETTINGS;
        app.screen = SCREEN_RECOVERY;
    } else if (action == ACCOUNT_CHANGE_EMAIL) {
        SettingsOpen();
        VerifyOpen(true);
        app.screen = SCREEN_VERIFY;
    }
}

static void RunHome(void)
{
    HomeAction action = DashboardScreen(&lessons);
    switch (action.kind) {
    case HOME_OPEN_LESSON: OpenLesson(action.lessonIndex); break;
    case HOME_CODE_EDITOR:
        OpenCodeEditor(SCREEN_HOME);
        CodeOpenPlayground();
        break;
    case HOME_SETTINGS: OpenSettings(); break;
    case HOME_LOG_OUT:  LogOut(); break;
    case HOME_QUIT:     app.quit = true; break;
    case HOME_STAY:     break;
    }
}

static void RunLesson(void)
{
    LessonAction action = LessonScreen();
    const Lesson *lesson = LessonScreenLesson();
    switch (action.kind) {
    case LESSON_BACK:
        app.screen = SCREEN_HOME;
        break;
    case LESSON_ROADMAP:  // "Return to Roadmap": that track's journey
        DashboardShowJourney(lesson->track, false);
        app.screen = SCREEN_HOME;
        break;
    case LESSON_PROGRESS:  // "View Progress": the statistics
        DashboardShowJourney(lesson->track, true);
        app.screen = SCREEN_HOME;
        break;
    case LESSON_TRY_EXAMPLE:
        OpenCodeEditor(SCREEN_LESSON);
        CodeOpenSnippet(TextFormat("Try it: %s", lesson->title), action.exampleCode, lesson->track == TRACK_CPP);
        break;
    case LESSON_OPEN_EXERCISE:
        OpenCodeEditor(SCREEN_LESSON);
        CodeOpenExercise(lesson, action.exerciseIndex);
        break;
    case LESSON_STAY:
        break;
    }
}

static void RunCode(void)
{
    // Back to the menu or the lesson; the lesson screen keeps its own scroll position.
    if (CodeScreen()) app.screen = app.codeReturn;
}

// ---------------------------------------------------------------------------
// Startup

// Window and taskbar icons from the generated PNGs. (The .exe's own icon is embedded by
// increment.rc, and the window falls back to it if these are missing.)
static void LoadWindowIcons(const char *assetDir)
{
    static const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    Image images[7];
    int count = 0;
    for (int i = 0; i < 7; i++) {
        const char *path = TextFormat("%s/icon/icon_%d.png", assetDir, sizes[i]);
        if (!FileExists(path)) continue;
        images[count] = LoadImage(path);
        ImageFormat(&images[count], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        count++;
    }
    if (count) SetWindowIcons(images, count);
    for (int i = 0; i < count; i++) UnloadImage(images[i]);
}

// The assets folder: next to the exe, one level up (running from build\), or the current folder.
static void FindAssetDir(char *out, size_t size)
{
    const char *exeDir = GetApplicationDirectory();
    const char *candidates[] = { "%sassets", "%s../assets", "assets" };
    for (int i = 0; i < 3; i++) {
        const char *path = TextFormat(candidates[i], exeDir);
        if (DirectoryExists(path)) {
            snprintf(out, size, "%s", path);
            return;
        }
    }
    snprintf(out, size, "assets");
}

// Accounts and progress live next to the exe unless INCREMENT_DATA_DIR says otherwise.
// The result always ends in a slash.
static void FindDataDir(char *out, size_t size)
{
    const char *override = getenv("INCREMENT_DATA_DIR");
    if (override && override[0]) {
        size_t n = strlen(override);
        bool endsWithSlash = override[n - 1] == '/' || override[n - 1] == '\\';
        snprintf(out, size, "%s%s", override, endsWithSlash ? "" : "/");
    } else {
        snprintf(out, size, "%s", GetApplicationDirectory());
    }
}

int main(int argc, char **argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Increment - learn C and C++");
    SetWindowMinSize(900, 640);
    SetExitKey(KEY_NULL);  // Esc means "back", not "quit"
    SetTargetFPS(60);

    char assetDir[1024];
    FindAssetDir(assetDir, sizeof assetDir);
    UiLoadFonts(assetDir);
    MascotLoad(assetDir);
    LoadWindowIcons(assetDir);
    LessonsLoad(&lessons, TextFormat("%s/lessons", assetDir));

    char dataDir[1024];
    FindDataDir(dataDir, sizeof dataDir);
    AccountsInit(dataDir);
    const char *emailConfigDirs[] = { GetApplicationDirectory(), dataDir };  // where email_config.txt may live
    EmailInit(emailConfigDirs, 2);

    ShowLogin();                   // prepares the login form...
    app.screen = SCREEN_SPLASH;    // ...which shows once the splash finishes
    app.codeReturn = SCREEN_HOME;
    app.splashStart = GetTime();
    app.splashTime = -1;

    DevShot shot = DevShotParse(argc, argv, &lessons);
    if (shot.outputPath) {
        app.screen = shot.screen;
        app.splashTime = shot.splashTime;
    } else {
        // Open full screen. Borderless rather than exclusive full screen: it covers the
        // monitor without changing its resolution, so Alt+Tab stays instant. Screenshots
        // keep the fixed 1280x800 window so they stay comparable.
        ToggleBorderlessWindowed();
    }

    while (!WindowShouldClose() && !app.quit) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();  // F11 switches to a window and back
        UiBeginFrame();
        BeginDrawing();
        ClearBackground(COL_BG);

        switch (app.screen) {
        case SCREEN_SPLASH:    RunSplash();        break;
        case SCREEN_GALLERY:   DevGalleryScreen(); break;
        case SCREEN_LOGIN:     RunLogin();         break;
        case SCREEN_RECOVERY:  RunRecovery();      break;
        case SCREEN_VERIFY:    RunVerify();        break;
        case SCREEN_SETTINGS:  RunSettings();      break;
        case SCREEN_CODE:      RunCode();          break;
        case SCREEN_HOME:      RunHome();          break;
        case SCREEN_LESSON:    RunLesson();        break;
        }

        if (DevShotCapture(&shot)) app.quit = true;
        EndDrawing();
        UiEndFrame();
    }

    CodeShutdown();
    EmailShutdown();
    LessonsUnload(&lessons);
    MascotUnload();
    UiUnloadFonts();
    CloseWindow();
    return 0;
}
