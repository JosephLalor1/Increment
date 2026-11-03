#include "raylib.h"
#include "rlgl.h"

#include "account.h"
#include "account_screens.h"
#include "layout.h"
#include "lesson.h"
#include "progress.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS_PERCENT 70

typedef enum {
    SCREEN_LOGIN, SCREEN_RECOVERY, SCREEN_SETTINGS,
    SCREEN_MENU, SCREEN_LIST, SCREEN_LESSON, SCREEN_QUIZ, SCREEN_RESULT,
} Screen;

static LessonSet lessons;

static struct {
    Screen screen;
    Track track;
    int lesson;          // index into lessons.items
    float scroll;        // lesson / quiz scroll
    float contentH;      // measured last frame, for scroll clamping
    float listScroll;
    float listH;
    int question;
    int picked;          // -1 until the current question is answered
    int correctCount;
    int lastPercent;
    Screen afterRecovery; // where to go once a new recovery code has been saved
    bool quit;
} app;

// ---------------------------------------------------------------------------
// Helpers

static bool Passed(const Lesson *L) { return ProgressBest(L->id) >= PASS_PERCENT; }

static void TrackStats(Track t, int *done, int *total)
{
    *done = *total = 0;
    for (int i = 0; i < lessons.count; i++) {
        if (lessons.items[i].track != t) continue;
        (*total)++;
        if (Passed(&lessons.items[i])) (*done)++;
    }
}

static int NextLessonInTrack(int from)
{
    for (int i = from + 1; i < lessons.count; i++) {
        if (lessons.items[i].track == lessons.items[from].track) return i;
    }
    return -1;
}

static void Go(Screen s)
{
    app.screen = s;
    app.scroll = 0;
    app.contentH = 0;
}

static void OpenLesson(int index)
{
    app.lesson = index;
    Go(SCREEN_LESSON);
}

static void StartQuiz(void)
{
    Lesson *L = &lessons.items[app.lesson];
    app.question = 0;
    app.picked = -1;
    app.correctCount = 0;
    if (L->questionCount == 0) {
        app.lastPercent = 100;
        ProgressRecord(L->id, 100);
        Go(SCREEN_RESULT);
    } else {
        Go(SCREEN_QUIZ);
    }
}


static void DrawCheck(float cx, float cy, float s, Color c)
{
    DrawLineEx((Vector2){ cx - s, cy }, (Vector2){ cx - s * 0.3f, cy + s * 0.7f }, 3, c);
    DrawLineEx((Vector2){ cx - s * 0.3f, cy + s * 0.7f }, (Vector2){ cx + s, cy - s * 0.7f }, 3, c);
}

// ---------------------------------------------------------------------------
// Screens

static bool TrackCard(Rectangle r, Track t)
{
    static const char *blurb[TRACK_COUNT] = {
        "The language underneath everything.",
        "C, plus classes, templates and the STL.",
    };
    bool over = UiMouseOver(r);
    if (over) UiWantHand();
    UiRoundRect(r, 14, over ? COL_PANEL_HI : COL_PANEL);

    int done, total;
    TrackStats(t, &done, &total);
    UiText(gFonts.mono, TrackName(t), r.x + 28, r.y + 24, 56, COL_ACCENT);
    UiWrappedText(gFonts.text, blurb[t], (Vector2){ r.x + 28, r.y + 92 }, r.width - 56, 20, COL_DIM, true);
    UiText(gFonts.text, TextFormat("%d / %d lessons complete", done, total), r.x + 28, r.y + r.height - 56, 18, COL_TEXT);
    ProgressBar((Rectangle){ r.x + 28, r.y + r.height - 28, r.width - 56, 8 }, total ? (float)done / total : 0, COL_BG, COL_GOOD);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// ---------------------------------------------------------------------------
// Account navigation

static void ShowLogin(void)
{
    ProgressLoad(""); // drop the previous user's scores from memory
    LoginReset();
    ApplySettings();
    Go(SCREEN_LOGIN);
}

static void LogOut(void)
{
    AccountLogout();
    ShowLogin();
}

static void SignedIn(void)
{
    ProgressLoad(AccountProgressPath());
    if (RecoveryPending()) {
        app.afterRecovery = SCREEN_MENU;
        Go(SCREEN_RECOVERY);
    } else {
        Go(SCREEN_MENU);
    }
}

static void ScreenLoginWrap(void)
{
    AccountAction a = LoginScreen();
    if (a == ACCOUNT_SIGNED_IN) SignedIn();
    else if (a == ACCOUNT_QUIT) app.quit = true;
}

static void ScreenRecoveryWrap(void)
{
    if (RecoveryScreen() == ACCOUNT_BACK) Go(app.afterRecovery);
}

static void ScreenSettingsWrap(void)
{
    AccountAction a = SettingsScreen();
    if (a == ACCOUNT_BACK) {
        Go(SCREEN_MENU);
    } else if (a == ACCOUNT_SIGNED_OUT) {
        ShowLogin();
    } else if (a == ACCOUNT_SHOW_CODE) {
        app.afterRecovery = SCREEN_SETTINGS;
        Go(SCREEN_RECOVERY);
    }
}

// Returns false if the user logged out (the menu should stop drawing).
static bool AccountCorner(void)
{
    const char *who = AccountCurrent();
    if (!who) return true;
    float right = GetScreenWidth() - 20.0f;
    Rectangle logout = { right - 110, 20, 110, 40 };
    Rectangle gear = { logout.x - 12 - 110, 20, 110, 40 };
    if (UiButtonEx(logout, "Log out", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        LogOut();
        return false;
    }
    if (UiButtonEx(gear, "Settings", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        SettingsOpen();
        Go(SCREEN_SETTINGS);
        return false;
    }
    const char *label = TextFormat("Signed in as %s", who);
    Vector2 m = MeasureTextEx(gFonts.text, label, 18, 0);
    UiText(gFonts.text, label, gear.x - 16 - m.x, 30, 18, COL_DIM);
    return true;
}

static void ScreenMenu(void)
{
    if (!AccountCorner()) return;

    float cx = GetScreenWidth() / 2.0f;
    UiTextCentered(gFonts.text, "Increment", cx, 90, 80, COL_TEXT);
    UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, 185, 26, COL_DIM);

    float cardW = 340, cardH = 220, gap = 40;
    float x0 = cx - (cardW * 2 + gap) / 2.0f;
    float y = 260;
    for (int t = 0; t < TRACK_COUNT; t++) {
        Rectangle r = { x0 + t * (cardW + gap), y, cardW, cardH };
        if (TrackCard(r, (Track)t)) {
            app.track = (Track)t;
            app.listScroll = 0;
            Go(SCREEN_LIST);
        }
    }

    if (lessons.count == 0) {
        UiTextCentered(gFonts.text, "No lessons found. Put lesson .txt files in assets/lessons.", cx, y + cardH + 30, 20, COL_BAD);
    } else if (AccountCurrent() && !AccountHasRecovery()) {
        // Accounts made before recovery codes existed have none.
        const char *msg = "You don't have a recovery code yet.";
        const char *link = "Make one in Settings";
        float mw = MeasureTextEx(gFonts.text, msg, 18, 0).x, lw = MeasureTextEx(gFonts.text, link, 18, 0).x;
        float x = cx - (mw + 8 + lw) / 2;
        UiText(gFonts.text, msg, x, y + cardH + 30, 18, COL_DIM);
        if (UiLink(x + mw + 8, y + cardH + 30, link, 18)) {
            SettingsOpen();
            Go(SCREEN_SETTINGS);
        }
    }
    if (UiButtonEx((Rectangle){ cx - 70, y + cardH + 80, 140, 46 }, "Quit", 20, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        app.quit = true;
    }
}

static void ScreenList(void)
{
    if (TopBar(TextFormat("%s track", TrackName(app.track)))) {
        Go(SCREEN_MENU);
        return;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&app.listScroll, app.listH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float top = view.y + 24 - app.listScroll;
    float y = top;
    int number = 0;
    for (int i = 0; i < lessons.count; i++) {
        Lesson *L = &lessons.items[i];
        if (L->track != app.track) continue;
        number++;

        Rectangle row = { x, y, w, 72 };
        bool over = UiMouseOver(row);
        if (over) UiWantHand();
        UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);

        bool passed = Passed(L);
        DrawCircle((int)(x + 40), (int)(y + 36), 20, passed ? COL_GOOD_BG : COL_BG);
        if (passed) DrawCheck(x + 40, y + 36, 9, COL_GOOD);
        else UiTextCentered(gFonts.mono, TextFormat("%d", number), x + 40, y + 25, 22, COL_DIM);

        UiText(gFonts.text, L->title, x + 80, y + 22, 24, COL_TEXT);

        int best = ProgressBest(L->id);
        const char *status = best < 0 ? "New" : TextFormat("Best: %d%%", best);
        Vector2 m = MeasureTextEx(gFonts.text, status, 18, 0);
        UiText(gFonts.text, status, x + w - m.x - 24, y + 26, 18, passed ? COL_GOOD : COL_DIM);

        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) OpenLesson(i);
        y += 84;
    }
    if (number == 0) UiText(gFonts.text, "No lessons in this track yet.", x, y, 22, COL_DIM);

    app.listH = (y - top) + 48;
    EndView();
}

static void ScreenLesson(void)
{
    Lesson *L = &lessons.items[app.lesson];
    if (TopBar(TextFormat("%s track", TrackName(L->track)))) {
        Go(SCREEN_LIST);
        return;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&app.scroll, app.contentH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float ts = TextScale(); // user's lesson text size
    float top = view.y + 32 - app.scroll;
    float y = top;

    y += UiWrappedText(gFonts.text, L->title, (Vector2){ x, y }, w, 44 * ts, COL_TEXT, true) + 16;

    for (int i = 0; i < L->blockCount; i++) {
        Block *b = &L->blocks[i];
        switch (b->type) {
        case BLOCK_HEADING:
            y += 14;
            y += UiWrappedText(gFonts.text, b->text, (Vector2){ x, y }, w, 30 * ts, COL_ACCENT, true) + 8;
            break;
        case BLOCK_TEXT:
            y += UiWrappedText(gFonts.text, b->text, (Vector2){ x, y }, w, 22 * ts, COL_TEXT, true) + 14;
            break;
        case BLOCK_NOTE: {
            float h = UiWrappedText(gFonts.text, b->text, (Vector2){ 0, 0 }, w - 48, 21 * ts, COL_TEXT, false);
            UiRoundRect((Rectangle){ x, y, w, h + 28 }, 8, COL_NOTE_BG);
            DrawRectangle((int)x, (int)y + 6, 4, (int)h + 16, COL_ACCENT);
            UiWrappedText(gFonts.text, b->text, (Vector2){ x + 24, y + 14 }, w - 48, 21 * ts, COL_TEXT, true);
            y += h + 28 + 16;
        } break;
        case BLOCK_CODE:
            y += UiCodeBlock(b->text, (Vector2){ x, y }, w, 20 * ts, true) + 16;
            break;
        case BLOCK_BULLET: {
            float size = 22 * ts;
            DrawCircleV((Vector2){ x + 9, y + size * 0.62f }, 3.5f * ts, COL_ACCENT);
            y += UiWrappedText(gFonts.text, b->text, (Vector2){ x + 28, y }, w - 28, size, COL_TEXT, true) + 4;
            bool lastInList = i + 1 >= L->blockCount || L->blocks[i + 1].type != BLOCK_BULLET;
            if (lastInList) y += 10;
        } break;
        }
    }

    y += 16;
    const char *label = L->questionCount
        ? TextFormat("Start quiz (%d question%s)", L->questionCount, L->questionCount == 1 ? "" : "s")
        : "Mark as complete";
    if (UiButton((Rectangle){ x, y, 320, 52 }, label, true) || IsKeyPressed(KEY_ENTER)) StartQuiz();
    y += 52 + 48;

    app.contentH = y - top + 32;
    EndView();
}

static void FinishQuiz(void)
{
    Lesson *L = &lessons.items[app.lesson];
    app.lastPercent = app.correctCount * 100 / L->questionCount;
    ProgressRecord(L->id, app.lastPercent);
    Go(SCREEN_RESULT);
}

static void ScreenQuiz(void)
{
    Lesson *L = &lessons.items[app.lesson];
    Question *q = &L->questions[app.question];
    if (TopBar(TextFormat("Quiz: %s", L->title))) {
        Go(SCREEN_LESSON);
        return;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&app.scroll, app.contentH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float ts = TextScale(); // user's lesson text size
    float top = view.y + 32 - app.scroll;
    float y = top;

    UiText(gFonts.text, TextFormat("Question %d of %d", app.question + 1, L->questionCount), x, y, 20, COL_DIM);
    y += 32;
    ProgressBar((Rectangle){ x, y, w, 6 }, (float)app.question / L->questionCount, COL_PANEL_HI, COL_ACCENT);
    y += 30;

    y += UiWrappedText(gFonts.text, q->prompt, (Vector2){ x, y }, w, 28 * ts, COL_TEXT, true) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * ts, true) + 20;

    bool answered = app.picked >= 0;
    for (int i = 0; i < q->choiceCount; i++) {
        const char *label = TextFormat("%d.  %s", i + 1, q->choices[i]);
        float h = UiWrappedText(gFonts.text, label, (Vector2){ 0, 0 }, w - 48, 22 * ts, COL_TEXT, false) + 28;
        Rectangle r = { x, y, w, h };
        bool over = !answered && UiMouseOver(r);
        if (over) UiWantHand();

        Color bg = over ? COL_PANEL_HI : COL_PANEL;
        Color fg = COL_TEXT;
        if (answered) {
            if (i == q->correct) bg = COL_GOOD_BG;
            else if (i == app.picked) bg = COL_BAD_BG;
            else fg = COL_DIM;
        }
        UiRoundRect(r, 10, bg);
        UiWrappedText(gFonts.text, label, (Vector2){ x + 24, y + 14 }, w - 48, 22 * ts, fg, true);

        bool chosen = (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || (!answered && IsKeyPressed(KEY_ONE + i));
        if (chosen) {
            app.picked = i;
            if (i == q->correct) app.correctCount++;
            answered = true;
        }
        y += h + 12;
    }

    if (answered) {
        y += 8;
        bool right = app.picked == q->correct;
        UiText(gFonts.text, right ? "Correct!" : "Not quite.", x, y, 26, right ? COL_GOOD : COL_BAD);
        y += 40;
        if (q->explain) y += UiWrappedText(gFonts.text, q->explain, (Vector2){ x, y }, w, 21 * ts, COL_TEXT, true) + 16;

        bool last = app.question + 1 >= L->questionCount;
        if (UiButton((Rectangle){ x, y, 220, 50 }, last ? "See results" : "Next question", true)
            || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (last) {
                FinishQuiz();
            } else {
                app.question++;
                app.picked = -1;
                app.scroll = 0;
            }
        }
        y += 50;
    }

    app.contentH = y - top + 64;
    EndView();
}

static void ScreenResult(void)
{
    Lesson *L = &lessons.items[app.lesson];
    if (TopBar(L->title)) {
        Go(SCREEN_LIST);
        return;
    }

    float cx = GetScreenWidth() / 2.0f;
    float y = 140;
    bool passed = app.lastPercent >= PASS_PERCENT;

    if (L->questionCount == 0) {
        UiTextCentered(gFonts.text, "Lesson complete", cx, y, 56, COL_GOOD);
        y += 90;
    } else {
        UiTextCentered(gFonts.text, TextFormat("%d%%", app.lastPercent), cx, y, 96, passed ? COL_GOOD : COL_BAD);
        y += 120;
        UiTextCentered(gFonts.text, TextFormat("%d of %d correct", app.correctCount, L->questionCount), cx, y, 26, COL_TEXT);
        y += 44;
        UiTextCentered(gFonts.text,
                       passed ? "Nice work. Lesson complete."
                              : TextFormat("You need %d%% to pass. Review the lesson and try again.", PASS_PERCENT),
                       cx, y, 22, COL_DIM);
        y += 70;
    }

    int next = NextLessonInTrack(app.lesson);
    float bw = 220, bh = 52, gap = 20;
    float x0 = cx - (bw * 3 + gap * 2) / 2.0f;

    if (UiButtonEx((Rectangle){ x0, y, bw, bh }, "Review lesson", 22, COL_PANEL, COL_PANEL_HI, COL_TEXT, true)) {
        Go(SCREEN_LESSON);
    }
    if (UiButtonEx((Rectangle){ x0 + bw + gap, y, bw, bh }, L->questionCount ? "Retry quiz" : "All lessons",
                   22, COL_PANEL, COL_PANEL_HI, COL_TEXT, true)) {
        if (L->questionCount) StartQuiz();
        else Go(SCREEN_LIST);
    }
    bool canNext = passed && next >= 0;
    if (UiButton((Rectangle){ x0 + (bw + gap) * 2, y, bw, bh }, next >= 0 ? "Next lesson" : "Track finished", canNext)
        || (canNext && IsKeyPressed(KEY_ENTER))) {
        OpenLesson(next);
    }
}

// ---------------------------------------------------------------------------

static void FindAssetDir(char *out, size_t size)
{
    const char *base = GetApplicationDirectory();
    const char *candidates[] = { "%sassets", "%s../assets", "assets" };
    for (int i = 0; i < 3; i++) {
        const char *path = TextFormat(candidates[i], base);
        if (DirectoryExists(path)) {
            snprintf(out, size, "%s", path);
            return;
        }
    }
    snprintf(out, size, "assets");
}

// Dev aid: --shot out.png <screen> [lessonIndex] [scroll] renders one screen, saves it as a
// PNG, and exits. Screens: login register reset recovery settings delete menu list lesson
// quiz answered result.
typedef struct {
    const char *path;
    int framesLeft;
} Shot;

static Shot ParseShot(int argc, char **argv)
{
    Shot shot = { 0 };
    if (argc < 3 || strcmp(argv[1], "--shot") != 0) return shot;
    shot.path = argv[2];
    shot.framesLeft = 3;
    const char *height = getenv("INCREMENT_SHOT_HEIGHT");
    if (height) SetWindowSize(1280, atoi(height));

    const char *screen = argc > 3 ? argv[3] : "menu";
    int index = argc > 4 ? atoi(argv[4]) : 0;
    if (index < 0 || index >= lessons.count) index = 0;
    app.lesson = index;
    app.track = lessons.count ? lessons.items[index].track : TRACK_C;

    // Content screens normally require a login; shots may sign in via env vars to show a user's state.
    app.screen = SCREEN_MENU;
    const char *user = getenv("INCREMENT_SHOT_USER"), *pass = getenv("INCREMENT_SHOT_PASS");
    const char *err;
    if (user && pass && AccountLogin(user, pass, &err)) ProgressLoad(AccountProgressPath());
    ApplySettings();

    if (strcmp(screen, "login") == 0) app.screen = SCREEN_LOGIN;
    else if (strcmp(screen, "register") == 0 || strcmp(screen, "reset") == 0) app.screen = SCREEN_LOGIN;
    else if (strcmp(screen, "recovery") == 0) app.screen = SCREEN_RECOVERY;
    else if (strcmp(screen, "settings") == 0 || strcmp(screen, "delete") == 0) {
        SettingsOpen();
        app.screen = SCREEN_SETTINGS;
    }
    else if (strcmp(screen, "list") == 0) app.screen = SCREEN_LIST;
    else if (strcmp(screen, "lesson") == 0) app.screen = SCREEN_LESSON;
    else if (strcmp(screen, "quiz") == 0 || strcmp(screen, "answered") == 0) app.screen = SCREEN_QUIZ;
    else if (strcmp(screen, "result") == 0) app.screen = SCREEN_RESULT;
    if (app.screen > SCREEN_MENU && lessons.count == 0) app.screen = SCREEN_MENU;

    app.picked = -1;
    if (strcmp(screen, "answered") == 0 && lessons.count && lessons.items[index].questionCount) {
        app.picked = 0; // first choice, right or wrong
    }
    app.scroll = argc > 5 ? (float)atof(argv[5]) : 0;
    app.contentH = 100000; // let the requested scroll survive the first frame's clamp
    app.lastPercent = 75;
    app.correctCount = 3;
    AccountScreensDemo(screen);
    return shot;
}

int main(int argc, char **argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Increment - learn C and C++");
    SetWindowMinSize(900, 640);
    SetExitKey(KEY_NULL); // Esc means "back", not "quit"
    SetTargetFPS(60);

    char assetDir[1024];
    FindAssetDir(assetDir, sizeof assetDir);
    UiLoadFonts(assetDir);
    LessonsLoad(&lessons, TextFormat("%s/lessons", assetDir));

    // Accounts and progress live next to the exe unless INCREMENT_DATA_DIR says otherwise.
    char dataDir[1024];
    const char *envDir = getenv("INCREMENT_DATA_DIR");
    if (envDir && envDir[0]) {
        size_t n = strlen(envDir);
        bool slash = envDir[n - 1] == '/' || envDir[n - 1] == '\\';
        snprintf(dataDir, sizeof dataDir, "%s%s", envDir, slash ? "" : "/");
    } else {
        snprintf(dataDir, sizeof dataDir, "%s", GetApplicationDirectory());
    }
    AccountsInit(dataDir);

    ShowLogin();
    Shot shot = ParseShot(argc, argv);

    while (!WindowShouldClose() && !app.quit) {
        UiBeginFrame();
        BeginDrawing();
        ClearBackground(COL_BG);

        switch (app.screen) {
        case SCREEN_LOGIN:    ScreenLoginWrap();    break;
        case SCREEN_RECOVERY: ScreenRecoveryWrap(); break;
        case SCREEN_SETTINGS: ScreenSettingsWrap(); break;
        case SCREEN_MENU:   ScreenMenu();   break;
        case SCREEN_LIST:   ScreenList();   break;
        case SCREEN_LESSON: ScreenLesson(); break;
        case SCREEN_QUIZ:   ScreenQuiz();   break;
        case SCREEN_RESULT: ScreenResult(); break;
        }

        if (shot.path && --shot.framesLeft == 0) {
            rlDrawRenderBatchActive(); // pending batched draws aren't in the framebuffer yet
            Image img = LoadImageFromScreen();
            ExportImage(img, shot.path);
            UnloadImage(img);
            app.quit = true;
        }
        EndDrawing();
        UiEndFrame();
    }

    LessonsUnload(&lessons);
    UiUnloadFonts();
    CloseWindow();
    return 0;
}
