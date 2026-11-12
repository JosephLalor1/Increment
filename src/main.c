// Increment: a desktop app for learning C and C++.
//
// This file starts the app and owns navigation. Each screen lives in its own module and
// reports what the user asked for (an action, a choice, or a level to play); the functions
// below decide which screen comes next. No screen switches screens itself.

#include "raylib.h"

#include "account.h"
#include "account_screens.h"
#include "app_screen.h"
#include "review.h"
#include "skill_progress.h"
#include "skill_reward.h"
#include "code_screen.h"
#include "ranking_screen.h"
#include "dashboard.h"
#include "dev_shot.h"
#include "email.h"
#include "layout.h"
#include "lesson.h"
#include "level_entry.h"
#include "level_screen.h"
#include "mascot.h"
#include "progress.h"
#include "speed_screen.h"
#include "splash.h"
#include "window_mode.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LessonSet lessons;

static struct {
    AppScreen screen;
    AppScreen speedReturn;    // where a speed round goes back to: the journey or the statistics
    int level;                // the level being played (an index into lessons)
    AppScreen afterRecovery;  // where to go once a new recovery code has been saved
    double splashStart;       // GetTime() when the splash began
    float splashTime;         // --shot splash: a fixed moment of the animation, or < 0 to run live
    bool rememberedSignIn;    // signed in at start-up with "remember me": skip the login form
    bool quit;
} app;

// ---------------------------------------------------------------------------
// Opening screens

// Plays a level. The draft folder is set here so the level screen doesn't depend on accounts.
static void OpenLevel(int lessonIndex)
{
    app.level = lessonIndex;
    LevelScreenOpen(&lessons, lessonIndex, AccountCodeDir());
    app.screen = SCREEN_LEVEL;
}

static void OpenSpeedRound(const Lesson *lesson, AppScreen returnTo)
{
    SpeedScreenOpen(lesson);
    app.speedReturn = returnTo;
    app.screen = SCREEN_SPEED;
}

static void OpenSettings(void)
{
    SettingsOpen();
    app.screen = SCREEN_SETTINGS;
}

static void ShowLogin(void)
{
    LevelScreenClose();  // saves the draft into the previous user's folder before it's forgotten
    RankingScreenClear();
    MascotResetInteractions();
    ProgressLoad("");  // drop the previous user's progress from memory
    ReviewLoad("");  // and their answer history and skill XP
    SkillsLoad("");
    SkillRewardClear();  // no badges of theirs left on the sign-in page
    LoginReset();
    ApplyUserAppearance();
    app.screen = SCREEN_LOGIN;
}

static void LogOut(void)
{
    AccountForgetMe();  // logging out on purpose means typing the password next time
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
    ReviewLoad(AccountReviewPath());
    SkillsLoad(AccountSkillsPath());
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
    if (!finished || app.splashTime >= 0) return;
    if (app.rememberedSignIn) SignedIn();  // straight in, as on the last visit
    else app.screen = SCREEN_LOGIN;
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
    } else if (action == ACCOUNT_DATA_RESTORED) {  // a backup replaced the user's files: read them again
        ProgressLoad(AccountProgressPath());
        ReviewLoad(AccountReviewPath());
        SkillsLoad(AccountSkillsPath());
        DashboardOpen();  // and home starts afresh on the restored progress
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
    case HOME_OPEN_LESSON:  // zoom into the level first; it opens once the zoom covers the screen
        app.level = action.lessonIndex;
        LevelEntryStart(action.clickedAt);
        break;
    case HOME_SPEED_ROUND: OpenSpeedRound(&lessons.items[action.lessonIndex], SCREEN_HOME); break;
    case HOME_CHALLENGE:
        SpeedScreenOpenChallenge(&lessons);
        app.speedReturn = SCREEN_HOME;
        app.screen = SCREEN_SPEED;
        break;
    case HOME_RANKINGS:
        RankingScreenOpen(&lessons);
        app.screen = SCREEN_RANKINGS;
        break;
    case HOME_CODE_EDITOR:
        CodeSetDraftFolder(AccountCodeDir());
        CodeOpenPlayground();
        app.screen = SCREEN_CODE;
        break;
    case HOME_SETTINGS: OpenSettings(); break;
    case HOME_LOG_OUT:  LogOut(); break;
    case HOME_QUIT:     app.quit = true; break;
    case HOME_STAY:     break;
    }
}

// Straight on to the next level, or back to the journey: the way out closes on the level
// first, and the journey is shown once it has (see RunLevelEntry).
static void RunLevel(void)
{
    LevelAction action = LevelScreen();
    if (action.kind == LEVEL_OPEN_LEVEL) OpenLevel(action.lessonIndex);
    else if (action.kind == LEVEL_BACK) LevelExitStart();
}

// The zoom into a level, or back out of one, standing in for the screen. The screens change
// while it has everything covered.
static void RunLevelEntry(void)
{
    LevelEntryCue cue = LevelEntryDraw();
    if (cue == ENTRY_CUE_OPEN_LEVEL) {
        OpenLevel(app.level);
    } else if (cue == ENTRY_CUE_SHOW_JOURNEY) {
        // The journey of the level's track, drawn once for the picture that gets zoomed out
        // of; whatever it reports this frame is ignored.
        DashboardShow(HOME_VIEW_JOURNEY, lessons.items[app.level].track);
        app.screen = SCREEN_HOME;
        ClearBackground(COL_BG);
        DashboardScreen(&lessons);
        LevelExitLand(DashboardLevelPoint(app.level));
    }
}

// A speed round goes back to where it was opened from: its lesson or the statistics.
static void RunSpeed(void)
{
    if (SpeedScreen() == SPEED_SCREEN_BACK) app.screen = app.speedReturn;
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
    WindowModeInit(dataDir);  // the saved window mode, applied once we know this isn't a --shot run
    const char *emailConfigDirs[] = { GetApplicationDirectory(), dataDir };  // where email_config.txt may live
    EmailInit(emailConfigDirs, 2);

    ShowLogin();                   // prepares the login form...
    app.screen = SCREEN_SPLASH;    // ...which shows once the splash finishes
    app.splashStart = GetTime();
    app.splashTime = -1;

    DevShot shot = DevShotParse(argc, argv, &lessons);
    if (shot.outputPath) {
        app.screen = shot.screen;
        app.level = shot.lessonIndex;
        app.splashTime = shot.splashTime;
    } else {
        // Open the window in the saved mode (maximised by default). Screenshots
        // keep the fixed 1280x800 window so they stay comparable.
        WindowModeEnable();
        // "Remember me": sign that user in behind the splash. (Never for screenshots, which
        // sign in as they're told.)
        app.rememberedSignIn = AccountSignInRemembered();
        if (app.rememberedSignIn) ApplyUserAppearance();
    }

    while (!WindowShouldClose() && !app.quit) {
        if (IsKeyPressed(KEY_F11)) WindowModeToggleFullScreen();  // does nothing in --shot runs
        UiBeginFrame();
        BeginDrawing();
        ClearBackground(COL_BG);

        LevelEntryUpdate(GetFrameTime());
        if (LevelEntryStandsIn()) RunLevelEntry();
        else switch (app.screen) {
        case SCREEN_SPLASH:    RunSplash();        break;
        case SCREEN_GALLERY:   DevGalleryScreen(); break;
        case SCREEN_LOGIN:     RunLogin();         break;
        case SCREEN_RECOVERY:  RunRecovery();      break;
        case SCREEN_VERIFY:    RunVerify();        break;
        case SCREEN_SETTINGS:  RunSettings();      break;
        case SCREEN_CODE:      if (CodeScreen()) app.screen = SCREEN_HOME; break;
        case SCREEN_HOME:      RunHome();          break;
        case SCREEN_LEVEL:     RunLevel();         break;
        case SCREEN_SPEED:     RunSpeed();         break;
        case SCREEN_RANKINGS:  if (RankingScreen()) app.screen = SCREEN_HOME; break;
        }
        LevelEntryDrawReveal();  // a level just zoomed into shows through an opening iris

        // The skill badges after an answer float over whichever screen is showing.
        SkillRewardUpdate(GetFrameTime());
        SkillRewardDraw();

        if (DevShotCapture(&shot)) app.quit = true;
        EndDrawing();
        UiEndFrame();
    }

    CodeShutdown();
    LevelScreenShutdown();  // saves the draft of a step left open when the window was closed
    LevelEntryShutdown();
    RankingScreenClear();
    EmailShutdown();
    LessonsUnload(&lessons);
    MascotUnload();
    UiUnloadFonts();
    CloseWindow();
    return 0;
}
