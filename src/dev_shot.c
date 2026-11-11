// The --shot dev aid and the icon gallery screen. See dev_shot.h.
#include "dev_shot.h"
#include "account.h"
#include "account_screens.h"
#include "ambience.h"
#include "brand.h"
#include "mouse_trail.h"
#include "code_screen.h"
#include "ranking_screen.h"
#include "dashboard.h"
#include "journey_view.h"
#include "layout.h"
#include "level_entry.h"
#include "level_screen.h"
#include "progress.h"
#include "review.h"
#include "roadmap.h"
#include "skill_icons.h"
#include "skill_progress.h"
#include "skill_reward.h"
#include "speed_screen.h"
#include "ui.h"

#include "raylib.h"
#include "rlgl.h"

#include <stdlib.h>
#include <string.h>

static bool showSkillLogos;  // the gallery shows the skill logos instead of the app icons

static bool Is(const char *name, const char *wanted) { return strcmp(name, wanted) == 0; }

// The fallback for shots that can't show what was asked: home, freshly opened.
static AppScreen Home(void)
{
    DashboardOpen();
    return SCREEN_HOME;
}

// The screen a shot name shows, setting up that screen's state on the way.
// extra is argv[5]: a scroll position, a splash time in ms, or a step number.
static AppScreen PrepareScreen(const char *name, const LessonSet *lessons, int lessonIndex, const char *extra,
                               DevShot *shot)
{
    float scroll = extra ? (float)atof(extra) : 0;
    if (Is(name, "rankings") || Is(name, "rankings-skills")) {
        RankingScreenOpen(lessons);
        if (Is(name, "rankings-skills")) RankingScreenSelect(RANKING_SKILLS, lessons->count ? lessons->items[lessonIndex].track : TRACK_COUNT);
        return SCREEN_RANKINGS;
    }
    // speed: a lesson's speed round intro; speedready: the count-in; speedplay <index> <ms>:
    // question 7, ms into its 10 seconds; speedfail: the explained failure; speedgold: gold.
    if (Is(name, "speed") || Is(name, "speedready") || Is(name, "speedplay") || Is(name, "speedfail")
        || Is(name, "speedgold")) {
        if (lessons->count == 0 || !lessons->items[lessonIndex].speedSkills[0]) return Home();
        SpeedScreenOpen(&lessons->items[lessonIndex]);
        int stage = Is(name, "speedready") ? 1 : Is(name, "speedplay") ? 2 : Is(name, "speedfail") ? 3
                  : Is(name, "speedgold") ? 4 : 0;
        SpeedScreenDemo(stage, extra ? (float)atof(extra) : 0);
        return SCREEN_SPEED;
    }
    // challenge: a short challenge's intro; challengeplay <index> <ms>: its seventh question,
    // ms in; challengeresults: its results, with a few misses. From the signed-in user's history.
    if (Is(name, "challenge") || Is(name, "challengeplay") || Is(name, "challengeresults")) {
        if (lessons->count == 0) return Home();
        SpeedScreenOpenChallenge(lessons);
        SpeedScreenDemo(Is(name, "challengeplay") ? 2 : Is(name, "challengeresults") ? 5 : 0, extra ? (float)atof(extra) : 0);
        return SCREEN_SPEED;
    }
    if (Is(name, "splash")) {
        shot->splashTime = (extra ? (float)atof(extra) : 1500) / 1000.0f;
        return SCREEN_SPLASH;
    }
    if (Is(name, "gallery") || Is(name, "skillgallery")) {
        showSkillLogos = Is(name, "skillgallery");
        return SCREEN_GALLERY;
    }
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
    if (Is(name, "editor")) {  // the free editor needs a user for its draft
        if (!AccountCurrent()) return Home();
        CodeSetDraftFolder(AccountCodeDir());
        CodeOpenPlayground();
        const char *runFirst = getenv("INCREMENT_SHOT_RUN");
        if (runFirst && runFirst[0] == '1') CodeRunNow();
        return SCREEN_CODE;
    }
    // enter <index> <ms>: the zoom into a level from its journey, ms after the click.
    if (Is(name, "enter")) {
        if (lessons->count == 0 || lessons->items[lessonIndex].stepCount == 0) return Home();
        DashboardOpen();
        DashboardShow(HOME_VIEW_JOURNEY, lessons->items[lessonIndex].track);
        AmbienceWarm((Rectangle){ 0, TOPBAR_H, (float)GetScreenWidth(), (float)GetScreenHeight() - TOPBAR_H });
        JourneyViewDemoClick(lessonIndex);
        LevelEntryDemo(scroll / 1000.0f);
        return SCREEN_HOME;
    }
    // leave <index> <ms>: the way out of a level back to its journey, ms after Back.
    if (Is(name, "leave")) {
        if (lessons->count == 0 || lessons->items[lessonIndex].stepCount == 0) return Home();
        DashboardOpen();
        AmbienceWarm((Rectangle){ 0, TOPBAR_H, (float)GetScreenWidth(), (float)GetScreenHeight() - TOPBAR_H });
        LevelScreenOpen(lessons, lessonIndex, AccountCurrent() ? AccountCodeDir() : NULL);
        LevelScreenDemo(RoadmapResumeStep(&lessons->items[lessonIndex]), 0, false);
        LevelScreenDemoBack();
        LevelEntryDemo(scroll / 1000.0f);
        return SCREEN_LEVEL;
    }
    if (Is(name, "home") || Is(name, "menu") || Is(name, "dashboard") || Is(name, "statistics")) {
        // home: the journey of lesson <index>'s track; statistics: the statistics view.
        HomeView view = Is(name, "statistics") ? HOME_VIEW_STATISTICS : HOME_VIEW_JOURNEY;
        DashboardOpen();
        DashboardShow(view, lessons->count ? lessons->items[lessonIndex].track : TRACK_C);
        DashboardDemo(scroll);
        // INCREMENT_SHOT_TRAIL=1: the journey's mouse trail, as if the mouse swept across it.
        const char *trail = getenv("INCREMENT_SHOT_TRAIL");
        // INCREMENT_SHOT_AMBIENCE=<style name>: the journey's background in that style.
        const char *ambience = getenv("INCREMENT_SHOT_AMBIENCE");
        for (int s = 0; ambience && s < AMBIENCE_STYLE_COUNT; s++)
            if (strcmp(ambience, AmbienceStyleName((AmbienceStyle)s)) == 0) AmbienceSetStyle((AmbienceStyle)s);
        AmbienceWarm((Rectangle){ 0, TOPBAR_H, (float)GetScreenWidth(), (float)GetScreenHeight() - TOPBAR_H });
        if (trail && trail[0] == '1') MouseTrailDemo((Vector2){ 300, 640 }, (Vector2){ 760, 470 });
        return SCREEN_HOME;
    }

    // level <index> [step]: a level at a step (1-based; even a locked one), its first run
    // finished; INCREMENT_SHOT_HINTS=<n> reveals hints. levelsolved: the same step with its
    // solution run and passed. levelcomplete: the celebration after the last step.
    if (Is(name, "level") || Is(name, "levelsolved") || Is(name, "levelcomplete")) {
        if (lessons->count == 0 || lessons->items[lessonIndex].stepCount == 0) return Home();
        LevelScreenOpen(lessons, lessonIndex, AccountCurrent() ? AccountCodeDir() : NULL);
        const char *hints = getenv("INCREMENT_SHOT_HINTS");
        LevelScreenDemo(extra ? atoi(extra) - 1 : 0, hints ? atoi(hints) : 0, Is(name, "levelsolved"));
        if (Is(name, "levelcomplete")) LevelScreenDemoCelebration();
        // INCREMENT_SHOT_RUN=1: run the step's draft as the learner would, to show its checks.
        const char *runFirst = getenv("INCREMENT_SHOT_RUN");
        if (Is(name, "level") && runFirst && runFirst[0] == '1') LevelScreenDemoRun();
        return SCREEN_LEVEL;
    }
    return Home();  // a name we don't know
}

DevShot DevShotParse(int argc, char **argv, const LessonSet *lessons)
{
    DevShot shot = { .outputPath = NULL, .screen = SCREEN_SPLASH, .splashTime = -1 };
    if (argc < 3 || strcmp(argv[1], "--shot") != 0) return shot;
    shot.outputPath = argv[2];
    shot.framesLeft = 3;
    // INCREMENT_SHOT_DELAY=<ms>: capture that long after the screen is ready (and any program
    // has finished), to catch animations part-way through.
    const char *delay = getenv("INCREMENT_SHOT_DELAY");
    if (delay) shot.delayFrames = atoi(delay) * 60 / 1000;

    const char *height = getenv("INCREMENT_SHOT_HEIGHT");
    if (height) SetWindowSize(1280, atoi(height));

    const char *name = argc > 3 ? argv[3] : "menu";
    int lessonIndex = argc > 4 ? atoi(argv[4]) : 0;
    if (lessonIndex < 0 || lessonIndex >= lessons->count) lessonIndex = 0;
    const char *extra = argc > 5 ? argv[5] : NULL;
    shot.lessonIndex = lessonIndex;

    // Content screens normally require a login; shots may sign in to show a user's state.
    const char *user = getenv("INCREMENT_SHOT_USER"), *password = getenv("INCREMENT_SHOT_PASS");
    const char *err;
    if (user && password && AccountLogin(user, password, &err)) {
        ProgressLoad(AccountProgressPath());
        ReviewLoad(AccountReviewPath());
        SkillsLoad(AccountSkillsPath());
    }
    ApplyUserAppearance();

    shot.screen = PrepareScreen(name, lessons, lessonIndex, extra, &shot);
    AccountScreensDemo(name);
    // INCREMENT_SHOT_REWARD=<ms>: the skill badges after an answer, that far into the animation.
    const char *reward = getenv("INCREMENT_SHOT_REWARD");
    if (reward) SkillRewardDemo((float)atof(reward) / 1000.0f);
    return shot;
}

bool DevShotCapture(DevShot *shot)
{
    if (!shot->outputPath) return false;
    // A shot of the code editor or a level waits for the compile and run to finish (up to ~30 s).
    bool running = CodeBusy() || LevelScreenBusy();
    if (running && shot->framesLeft == 1 && shot->framesWaited++ < 1800) return false;
    if (shot->framesLeft == 1 && shot->delayFrames-- > 0) return false;  // let effects play on
    if (--shot->framesLeft > 0) return false;

    rlDrawRenderBatchActive();  // pending batched draws aren't in the framebuffer yet
    Image image = LoadImageFromScreen();
    ExportImage(image, shot->outputPath);
    UnloadImage(image);
    return true;
}

// Every skill logo, large in a tile and small in the accent colour, with the skill's name.
static void SkillLogoGallery(void)
{
    UiText(gFonts.bold, "Skill logos", 40, 24, 28, COL_TEXT);
    for (int s = 0; s < SKILL_COUNT; s++) {
        float cx = 92 + (s % 7) * 178.0f, cy = 130 + (s / 7) * 170.0f;
        UiRoundRect((Rectangle){ cx - 44, cy - 44, 88, 88 }, 12, COL_PANEL);
        SkillIconDraw((Skill)s, (Vector2){ cx, cy }, 56, COL_TEXT);
        SkillIconDraw((Skill)s, (Vector2){ cx + 62, cy + 30 }, 24, COL_ACCENT);
        UiTextCentered(gFonts.text, SkillTitle((Skill)s), cx, cy + 52, 16, COL_DIM);
    }
}

void DevGalleryScreen(void)
{
    if (showSkillLogos) {
        SkillLogoGallery();
        return;
    }
    static const char *names[ICON_COUNT] = {
        "back", "next", "check", "cross", "gear", "logout", "power", "user", "key", "lock",
        "theme", "trash", "info", "book", "retry", "play", "trophy", "copy", "target", "code", "mail",
        "bulb", "plus", "star", "bolt",
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
    UiButtonIcon((Rectangle){ 40, 690, 240, 50 }, ICON_PLAY, "Run", true);
    UiButtonIconEx((Rectangle){ 300, 690, 160, 50 }, ICON_GEAR, "Settings", 20, COL_PANEL, COL_SECONDARY_HOVER,
                   COL_TEXT, true);
    UiIconText(ICON_CHECK, gFonts.bold, "Correct!", 490, 702, 26, COL_GOOD);
}
