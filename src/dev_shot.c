// The --shot dev aid and the icon gallery screen. See dev_shot.h.
#include "dev_shot.h"
#include "account.h"
#include "account_screens.h"
#include "bookmarks.h"
#include "brand.h"
#include "code_rain.h"
#include "code_screen.h"
#include "practice_screen.h"
#include "courses_screen.h"
#include "dashboard.h"
#include "layout.h"
#include "memory_view.h"
#include "notes_panel.h"
#include "lesson_screen.h"
#include "progress.h"
#include "speed_screen.h"
#include "ui.h"

#include "raylib.h"
#include "rlgl.h"

#include <stdlib.h>
#include <string.h>

static bool Is(const char *name, const char *wanted) { return strcmp(name, wanted) == 0; }

// The fallback for shots that can't show what was asked: home, freshly opened.
static AppScreen Home(void)
{
    DashboardOpen();
    return SCREEN_HOME;
}

// INCREMENT_SHOT_FILTER: c, cpp, or unset for both tracks.
static SearchFilter ShotSearchFilter(void)
{
    const char *name = getenv("INCREMENT_SHOT_FILTER");
    if (name && Is(name, "cpp")) return SEARCH_CPP;
    if (name && Is(name, "c")) return SEARCH_C;
    return SEARCH_BOTH;
}

// Opens the code editor for the editor, exercise, quiztask and tryit shots. Returns false when that
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
        const char *hints = getenv("INCREMENT_SHOT_HINTS");
        if (hints) CodeHintsDemo(atoi(hints));
        return true;
    }
    if (Is(name, "quiztask")) {  // the lesson's first quiz coding question
        for (int q = 0; q < lesson->questionCount; q++) {
            if (!lesson->questions[q].task) continue;
            CodeOpenQuizQuestion(lesson, q, true);
            return true;
        }
        return false;
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

    if (Is(name, "practice") || Is(name, "practice-question") || Is(name, "practice-results") || Is(name, "practice-review")) {
        PracticeScreenOpen(lessons);
        PracticeScreenDemo(lessons->count ? lessons->items[lessonIndex].track : TRACK_C,
                           Is(name, "practice-question") ? 1 : Is(name, "practice-results") ? 2 : Is(name, "practice-review") ? 3 : 0);
        return SCREEN_PRACTICE;
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
    if (Is(name, "courses")) {
        CoursesScreenOpen(lessons->count ? lessons->items[lessonIndex].track : TRACK_C);
        return SCREEN_COURSES;
    }

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
    if (Is(name, "editor") || Is(name, "exercise") || Is(name, "tryit") || Is(name, "quiztask")) {
        if (!PrepareCodeShot(name, lessons, lessonIndex, extra ? atoi(extra) : 1)) return Home();
        const char *runFirst = getenv("INCREMENT_SHOT_RUN");
        if (runFirst && runFirst[0] == '1') CodeRunNow();
        return SCREEN_CODE;
    }
    if (Is(name, "home") || Is(name, "menu") || Is(name, "dashboard") || Is(name, "statistics") || Is(name, "saved")
        || Is(name, "search")) {
        // home: the journey of lesson <index>'s track; statistics: the statistics view;
        // saved: the signed-in user's saved lessons; search: INCREMENT_SHOT_SEARCH typed
        // into the box (even " ", which shows the empty-search hint) and INCREMENT_SHOT_FILTER.
        HomeView view = Is(name, "statistics") ? HOME_VIEW_STATISTICS
                      : Is(name, "saved")      ? HOME_VIEW_SAVED
                                               : HOME_VIEW_JOURNEY;
        DashboardOpen();
        DashboardShow(view, lessons->count ? lessons->items[lessonIndex].track : TRACK_C);
        DashboardDemo(scroll);
        if (Is(name, "search")) DashboardDemoSearch(getenv("INCREMENT_SHOT_SEARCH"), ShotSearchFilter());
        // INCREMENT_SHOT_TRAIL=1: the journey's mouse trail, as if the mouse swept across it.
        const char *trail = getenv("INCREMENT_SHOT_TRAIL");
        CodeAmbienceWarm((Rectangle){ 0, TOPBAR_H, (float)GetScreenWidth(), (float)GetScreenHeight() - TOPBAR_H });
        if (trail && trail[0] == '1') CodeTrailDemo((Vector2){ 300, 640 }, (Vector2){ 760, 470 });
        return SCREEN_HOME;
    }

    // The rest show lesson content, so they need lessons.
    if (lessons->count == 0) return Home();
    LessonPage page = Is(name, "lesson") || Is(name, "notes")    ? LESSON_PAGE_READING
                    : Is(name, "quiz") || Is(name, "answered")
                      || Is(name, "quizcode") || Is(name, "quizsolved")  ? LESSON_PAGE_QUIZ
                    : Is(name, "result")                         ? LESSON_PAGE_RESULT
                    : Is(name, "review")                         ? LESSON_PAGE_REVIEW
                    : Is(name, "celebration") || Is(name, "graduation") ? LESSON_PAGE_CELEBRATION
                                                                 : (LessonPage)-1;
    if ((int)page < 0) return Home();  // a name we don't know
    NotesPanelSetFolder(AccountCurrent() ? AccountNotesDir() : NULL);
    LessonScreenOpen(lessons, lessonIndex, "Home");
    if (Is(name, "notes")) NotesPanelDemo(getenv("INCREMENT_SHOT_NOTES"));  // the user's saved notes if unset
    // INCREMENT_SHOT_DIAGRAM="step[,box]": pointer diagrams start at that step (1-based),
    // with that box (a cell index) selected.
    const char *diagram = getenv("INCREMENT_SHOT_DIAGRAM");
    if (diagram) {
        const char *comma = strchr(diagram, ',');
        MemoryViewDemo(atoi(diagram) - 1, comma ? atoi(comma + 1) : -1);
    }
    LessonScreenDemo(page, scroll, Is(name, "answered") || Is(name, "graduation"));
    // quizcode / quizsolved: the quiz at its coding question, before and after it's solved.
    if (Is(name, "quizcode") || Is(name, "quizsolved")) {
        if (LessonScreenDemoCoding(Is(name, "quizsolved")) < 0) return Home();
        shot->framesLeft = 24;
    }
    if (page == LESSON_PAGE_CELEBRATION || Is(name, "answered")) shot->framesLeft = 24; // let the expression crossfade settle
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
    if (user && password && AccountLogin(user, password, &err)) {
        ProgressLoad(AccountProgressPath());
        BookmarksLoad(AccountBookmarksPath());
    }
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
