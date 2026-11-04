#include "raylib.h"
#include "rlgl.h"

#include "account.h"
#include "account_screens.h"
#include "brand.h"
#include "code_screen.h"
#include "dashboard.h"
#include "email.h"
#include "layout.h"
#include "lesson.h"
#include "progress.h"
#include "roadmap.h"
#include "search.h"
#include "splash.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SCREEN_SPLASH, SCREEN_GALLERY, SCREEN_LOGIN, SCREEN_RECOVERY, SCREEN_VERIFY, SCREEN_SETTINGS, SCREEN_CODE,
    SCREEN_MENU, SCREEN_LIST, SCREEN_LESSON, SCREEN_QUIZ, SCREEN_RESULT, SCREEN_DASHBOARD,
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
    bool plainList;
    char search[SEARCH_QUERY_MAX + 1];      // lesson search box on the list screen
    char searchSeen[SEARCH_QUERY_MAX + 1];  // last frame's query, to reset scrolling when it changes
    SearchFilter searchFilter;
    float searchScroll;
    float searchH;
    float dashboardScroll;
    float dashboardH;
    Screen lessonReturn;
    int question;
    int picked;          // -1 until the current question is answered
    int correctCount;
    int lastPercent;
    Screen afterRecovery; // where to go once a new recovery code has been saved
    double splashStart;   // GetTime() when the splash began
    float shotTime;       // --shot splash: fixed animation time, or < 0 to run live
    Screen codeReturn;    // where the code editor goes back to
    float savedScroll;    // lesson scroll position while the code editor is open
    float savedContentH;
    bool quit;
} app;

// ---------------------------------------------------------------------------
// Helpers

static bool Passed(const Lesson *L) { return ProgressBest(L->id) >= PASS_PERCENT; }

static void TrackStats(Track t, int *done, int *total)
{
    RoadmapNext(&lessons, t, done, total);
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
    app.track = lessons.items[index].track;
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

// After signing in (or verifying): show a new recovery code if there is one, then the menu.
static void EnterApp(void)
{
    if (RecoveryPending()) {
        app.afterRecovery = SCREEN_MENU;
        Go(SCREEN_RECOVERY);
    } else {
        Go(SCREEN_MENU);
    }
}

static void SignedIn(void)
{
    ProgressLoad(AccountProgressPath());
    if (VerifyNeeded()) {  // the account isn't usable until its email is verified
        VerifyOpen(false);
        Go(SCREEN_VERIFY);
    } else {
        EnterApp();
    }
}

static void ScreenVerifyWrap(void)
{
    AccountAction a = VerifyScreen();
    if (a == ACCOUNT_VERIFIED) {
        if (VerifyChanging()) Go(SCREEN_SETTINGS);
        else EnterApp();
    } else if (a == ACCOUNT_BACK) {
        Go(SCREEN_SETTINGS);
    } else if (a == ACCOUNT_SIGNED_OUT) {
        LogOut();
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
    } else if (a == ACCOUNT_CHANGE_EMAIL) {
        SettingsOpen();
        VerifyOpen(true);
        Go(SCREEN_VERIFY);
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
    if (UiButtonIconEx(logout, ICON_LOGOUT, "Log out", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        LogOut();
        return false;
    }
    if (UiButtonIconEx(gear, ICON_GEAR, "Settings", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        SettingsOpen();
        Go(SCREEN_SETTINGS);
        return false;
    }
    const char *label = TextFormat("Signed in as %s", who);
    float w = 18 * 1.4f + MeasureTextEx(gFonts.text, label, 18, 0).x;  // icon + gap + text
    UiIconText(ICON_USER, gFonts.text, label, gear.x - 16 - w, 30, 18, COL_DIM);
    return true;
}

static void ScreenMenu(void)
{
    if (!AccountCorner()) return;

    float cx = GetScreenWidth() / 2.0f;
    DrawLogoLockup(cx, 84, 76);
    UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, 185, 26, COL_DIM);

    float cardW = 340, cardH = 220, gap = 40;
    float x0 = cx - (cardW * 2 + gap) / 2.0f;
    float y = 260;
    for (int t = 0; t < TRACK_COUNT; t++) {
        Rectangle r = { x0 + t * (cardW + gap), y, cardW, cardH };
        if (TrackCard(r, (Track)t)) {
            app.track = (Track)t;
            app.listScroll = 0;
            app.search[0] = '\0';  // a new visit starts with normal navigation
            Go(SCREEN_LIST);
        }
    }

    if (lessons.count == 0) {
        UiTextCentered(gFonts.text, "No lessons found. Put lesson .txt files in assets/lessons.", cx, y + cardH + 30, 20, COL_BAD);
    } else if (AccountCurrent() && !AccountHasRecovery()) {
        // Accounts made before recovery codes existed have none.
        const char *msg = "You don't have a recovery code yet.";
        const char *link = "Make one in Settings";
        float mw = 18 * 1.4f + MeasureTextEx(gFonts.text, msg, 18, 0).x;
        float lw = MeasureTextEx(gFonts.text, link, 18, 0).x;
        float x = cx - (mw + 8 + lw) / 2;
        UiIconText(ICON_KEY, gFonts.text, msg, x, y + cardH + 30, 18, COL_DIM);
        if (UiLink(x + mw + 8, y + cardH + 30, link, 18)) {
            SettingsOpen();
            Go(SCREEN_SETTINGS);
        }
    }
    if (UiButtonIcon((Rectangle){ cx - 390, y + cardH + 80, 250, 46 }, ICON_CODE, "Open code editor", true)) {
        CodeOpenPlayground();
        app.codeReturn = SCREEN_MENU;
        Go(SCREEN_CODE);
    }
    if (UiButtonIconEx((Rectangle){ cx + 240, y + cardH + 80, 150, 46 }, ICON_POWER, "Quit", 20, COL_PANEL,
                       COL_SECONDARY_HOVER, COL_TEXT, true)) {
        app.quit = true;
    }
    if (UiButtonIconEx((Rectangle){ cx - 120, y + cardH + 80, 340, 46 }, ICON_TROPHY,
                       "Progress dashboard", 20, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        app.dashboardScroll = 0;
        app.dashboardH = 0;
        DashboardReset();
        Go(SCREEN_DASHBOARD);
    }
}

static void ScreenDashboard(void)
{
    if (TopBar("Learning Progress Dashboard")) {
        Go(SCREEN_MENU);
        return;
    }
    Rectangle view = ScrollView();
    UpdateScroll(&app.dashboardScroll, app.dashboardH, view.height);
    BeginView(view);
    int clicked = DashboardDraw(&lessons, ContentX(), view.y + 24 - app.dashboardScroll,
                                ContentWidth(), TextScale(), &app.dashboardH);
    app.dashboardH += 24;
    EndView();
    if (clicked >= 0) {
        app.lessonReturn = SCREEN_DASHBOARD;
        OpenLesson(clicked);
    }
}

// True for paragraphs like "Output:" or "Example output:" that introduce program output.
static bool IntroducesOutput(const char *text)
{
    size_t n = strlen(text);
    const char *tail = "output:";
    if (n < 7) return false;
    for (size_t i = 0; i < 7; i++) {
        char c = text[n - 7 + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != tail[i]) return false;
    }
    return true;
}

static void OpenCodeFromLesson(void)
{
    app.savedScroll = app.scroll;
    app.savedContentH = app.contentH;
    app.codeReturn = SCREEN_LESSON;
    Go(SCREEN_CODE);
}

static void ScreenCodeWrap(void)
{
    if (!CodeScreen()) return;
    Go(app.codeReturn);
    if (app.codeReturn == SCREEN_LESSON) {  // back to where the learner was reading
        app.scroll = app.savedScroll;
        app.contentH = app.savedContentH;
    }
}

// Cuts text (in place) to fit maxW, ending it with "..." when anything was cut.
static void FitText(char *text, Font f, float size, float maxW)
{
    float sp = UiSpacing(f, size);
    if (MeasureTextEx(f, text, size, sp).x <= maxW) return;
    int n = (int)strlen(text);
    while (n > 0) {
        n--;
        while (n > 0 && text[n - 1] == ' ') n--;
        memcpy(text + n, "...", 4);
        if (MeasureTextEx(f, text, size, sp).x <= maxW) return;
    }
}

// The open lesson's track changes app.track; a search result keeps the list on its own track.
static void OpenFromSearch(int index)
{
    Track keep = app.track;
    OpenLesson(index);
    app.track = keep;
}

// One search result row: track badge, title, progress, and the text around the match.
static bool SearchRow(const Lesson *L, float x, float y, float w)
{
    Rectangle row = { x, y, w, 92 };
    bool over = UiMouseOver(row);
    if (over) UiWantHand();
    UiRoundRect(row, 10, over ? COL_PANEL_HI : COL_PANEL);

    const char *track = TrackName(L->track);
    float pillW = MeasureTextEx(gFonts.bold, track, 17, UiSpacing(gFonts.bold, 17)).x + 22;
    UiRoundRect((Rectangle){ x + 20, y + 18, pillW, 28 }, 14, COL_BG);
    UiTextCentered(gFonts.bold, track, x + 20 + pillW / 2, y + 22, 17, COL_ACCENT);

    int best = ProgressBest(L->id);
    bool passed = best >= PASS_PERCENT;
    const char *status = best < 0 ? "New" : TextFormat("Best: %d%%", best);
    float statusW = MeasureTextEx(gFonts.text, status, 18, UiSpacing(gFonts.text, 18)).x;
    UiText(gFonts.text, status, x + w - statusW - 24, y + 22, 18, passed ? COL_GOOD : COL_DIM);
    if (passed) IconDraw(ICON_CHECK, (Vector2){ x + w - statusW - 44, y + 32 }, 18, COL_GOOD);

    char title[128];
    snprintf(title, sizeof title, "%s", L->title);
    float titleX = x + 20 + pillW + 14;
    FitText(title, gFonts.text, 24, x + w - statusW - 64 - titleX);
    UiText(gFonts.text, title, titleX, y + 16, 24, COL_TEXT);

    // The excerpt, with the matched word picked out in the accent colour.
    char snip[256];
    int at, len;
    float sy = y + 58, sx = x + 20, size = 18, maxW = w - 40;
    if (!SearchSnippet(L, app.search, snip, sizeof snip, &at, &len)) {
        UiText(gFonts.text, "Matches the lesson title", sx, sy, size, COL_DIM);
    } else {
        FitText(snip, gFonts.text, size, maxW);
        int n = (int)strlen(snip);
        if (at > n) at = n;
        if (at + len > n) len = n - at;
        char part[256];
        float sp = UiSpacing(gFonts.text, size);
        snprintf(part, sizeof part, "%.*s", at, snip);
        UiText(gFonts.text, part, sx, sy, size, COL_DIM);
        sx += MeasureTextEx(gFonts.text, part, size, sp).x;
        snprintf(part, sizeof part, "%.*s", len, snip + at);
        UiText(gFonts.text, part, sx, sy, size, COL_ACCENT);
        sx += MeasureTextEx(gFonts.text, part, size, sp).x;
        UiText(gFonts.text, snip + at + len, sx, sy, size, COL_DIM);
    }
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// Search results in place of the roadmap/list. Returns the lesson to open, or -1.
static int SearchResults(Rectangle view, bool submitted)
{
    static int found[1024];
    int n = SearchLessons(&lessons, app.search, app.searchFilter, found, 1024);
    if (submitted && n > 0) return found[0];  // Enter opens the top result

    UpdateScroll(&app.searchScroll, app.searchH, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth();
    float top = view.y + 8 - app.searchScroll, y = top;
    int open = -1;

    // Track filter, then the result count.
    static const char *labels[] = { "C and C++", "C", "C++" };
    static const float widths[] = { 124, 56, 72 };
    float fx = x;
    for (int i = 0; i < 3; i++) {
        bool on = (int)app.searchFilter == i;
        if (UiButtonEx((Rectangle){ fx, y, widths[i], 38 }, labels[i], 18, on ? COL_ACCENT : COL_PANEL,
                       on ? COL_ACCENT_HI : COL_SECONDARY_HOVER, on ? COL_ON_ACCENT : COL_TEXT, true)) {
            app.searchFilter = (SearchFilter)i;
            app.searchScroll = 0;
        }
        fx += widths[i] + 8;
    }
    if (n > 0) {
        const char *count = n == 1 ? "1 lesson found" : TextFormat("%d lessons found", n);
        float cw = MeasureTextEx(gFonts.text, count, 19, UiSpacing(gFonts.text, 19)).x;
        UiText(gFonts.text, count, x + w - cw, y + 8, 19, COL_DIM);
    }
    y += 56;

    if (n == 0) {
        const char *advice = "Check the spelling, or try fewer or shorter words: \"point\" finds pointer and pointers.";
        float adviceH = UiWrappedText(gFonts.text, advice, (Vector2){ 0, 0 }, w - 96, 19, COL_DIM, false);
        Rectangle box = { x, y, w, 68 + adviceH + 64 };
        UiRoundRect(box, 12, COL_PANEL);
        IconDraw(ICON_INFO, (Vector2){ x + 40, y + 42 }, 28, COL_ACCENT);
        char what[160];
        if (app.searchFilter == SEARCH_BOTH) snprintf(what, sizeof what, "No lessons match \"%s\".", app.search);
        else snprintf(what, sizeof what, "No %s lessons match \"%s\".", SearchFilterName(app.searchFilter), app.search);
        FitText(what, gFonts.bold, 24, w - 96);
        UiText(gFonts.bold, what, x + 72, y + 26, 24, COL_TEXT);
        UiWrappedText(gFonts.text, advice, (Vector2){ x + 72, y + 68 }, w - 96, 19, COL_DIM, true);
        float ly = y + 68 + adviceH + 18, lx = x + 72;
        if (app.searchFilter != SEARCH_BOTH) {
            if (UiLink(lx, ly, "Search C and C++", 19)) {
                app.searchFilter = SEARCH_BOTH;
                app.searchScroll = 0;
            }
            lx += MeasureTextEx(gFonts.text, "Search C and C++", 19, UiSpacing(gFonts.text, 19)).x + 32;
        }
        if (UiLink(lx, ly, "Clear search", 19)) {
            app.search[0] = '\0';
            UiFocus(app.search);
        }
        y += box.height + 16;
    }
    for (int i = 0; i < n; i++) {
        if (SearchRow(&lessons.items[found[i]], x, y, w)) open = found[i];
        y += 104;
    }
    app.searchH = (y - top) + 24;
    EndView();
    return open;
}

static void ScreenList(void)
{
    app.lessonReturn = SCREEN_LIST;
    if (TopBar(TextFormat("%s track", TrackName(app.track)))) {
        Go(SCREEN_MENU);
        return;
    }

    bool searching = !SearchIsBlank(app.search);
    if (!searching && UiButtonEx((Rectangle){ GetScreenWidth() - 206.0f, 12, 190, 40 },
                                 app.plainList ? "Show roadmap" : "Show lesson list", 18,
                                 COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        app.plainList = !app.plainList;
        app.listScroll = 0;
        app.listH = 0;
    }

    // Search box, fixed above the scrolling content. Ctrl+F jumps to it.
    float bx = ContentX(), bw = ContentWidth();
    Rectangle box = { bx, TOPBAR_H + 16, bw, 48 };
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_F)) UiFocus(app.search);
    bool submitted = UiTextBox(box, app.search, sizeof app.search, "Search all lessons  (Ctrl+F)", false);
    if (app.search[0] && UiButtonIconEx((Rectangle){ box.x + box.width - 44, box.y + 6, 36, 36 }, ICON_CROSS, NULL,
                                        18, COL_CODE_BG, COL_SECONDARY_HOVER, COL_DIM, true)) {
        app.search[0] = '\0';  // clearing brings back the roadmap or list, where it was
        UiFocus(app.search);
    }
    if (strcmp(app.search, app.searchSeen) != 0) {
        snprintf(app.searchSeen, sizeof app.searchSeen, "%s", app.search);
        app.searchScroll = 0;
        app.searchH = 0;
    }
    searching = !SearchIsBlank(app.search);
    float barBottom = box.y + box.height + 12;
    if (!searching && UiHasFocus(app.search)) {
        // An empty search: say what the box does instead of searching for nothing.
        const char *hint = "Type a word to search every lesson's title and text, like \"pointer\" or \"malloc\".";
        UiIconText(ICON_INFO, gFonts.text, hint, bx + 2, barBottom - 2, 18, COL_DIM);
        barBottom += 30;
    }

    Rectangle view = ScrollView();
    view.height -= barBottom - view.y;
    view.y = barBottom;
    if (searching) {
        int open = SearchResults(view, submitted);
        if (open >= 0) {
            UiFocus(NULL);
            OpenFromSearch(open);
        }
        return;
    }

    UpdateScroll(&app.listScroll, app.listH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float top = view.y + 24 - app.listScroll;
    float y = top;
    if (!app.plainList) {
        int clicked = RoadmapDraw(&lessons, app.track, x, y, w, &app.listH);
        app.listH += 24;
        EndView();
        if (clicked >= 0) OpenLesson(clicked);
        return;
    }
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
        Vector2 badge = { x + 40, y + 36 };
        DrawCircleV(badge, 20, passed ? COL_GOOD_BG : COL_BG);
        if (passed) IconDraw(ICON_CHECK, badge, 22, COL_GOOD);
        else if (L->practice) IconDraw(ICON_TARGET, badge, 22, COL_ACCENT);
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
        Go(app.lessonReturn);
        return;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&app.scroll, app.contentH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float ts = TextScale(); // user's lesson text size
    float top = view.y + 32 - app.scroll;
    float y = top;
    const char *trySnippet = NULL;  // acted on after the view is closed
    int openExercise = -1;

    y += UiWrappedText(gFonts.text, L->title, (Vector2){ x, y }, w, 44 * ts, COL_TEXT, true) + 16;
    float preparationH = RoadmapPrerequisites(&lessons, L, x, y, w, 19 * ts, true);
    if (preparationH) y += preparationH + 24;

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
            float iconSize = 22 * ts, textX = 30 + iconSize;  // icon column, then the text
            float h = UiWrappedText(gFonts.text, b->text, (Vector2){ 0, 0 }, w - textX - 24, 21 * ts, COL_TEXT, false);
            UiRoundRect((Rectangle){ x, y, w, h + 28 }, 8, COL_NOTE_BG);
            IconDraw(ICON_INFO, (Vector2){ x + 18 + iconSize / 2, y + 14 + 21 * ts * 0.6f }, iconSize, COL_ACCENT);
            UiWrappedText(gFonts.text, b->text, (Vector2){ x + textX, y + 14 }, w - textX - 24, 21 * ts, COL_TEXT, true);
            y += h + 28 + 16;
        } break;
        case BLOCK_CODE: {
            // A block after "Output:" (or "Example output:") is program output: plain, not highlighted.
            bool isOutput = i > 0 && L->blocks[i - 1].type == BLOCK_TEXT && IntroducesOutput(L->blocks[i - 1].text);
            float h = isOutput ? UiOutputBlock(b->text, (Vector2){ x, y }, w, 20 * ts, true)
                               : UiCodeBlock(b->text, (Vector2){ x, y }, w, 20 * ts, true);
            if (strstr(b->text, "main(")) {  // a complete program: offer to run it
                Rectangle tryIt = { x + w - 112, y + 10, 100, 34 };
                if (UiButtonIconEx(tryIt, ICON_PLAY, "Try it", 17, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
                    trySnippet = b->text;
                }
            }
            y += h + 16;
        } break;
        case BLOCK_BULLET: {
            float size = 22 * ts;
            DrawCircleV((Vector2){ x + 9, y + size * 0.62f }, 3.5f * ts, COL_ACCENT);
            y += UiWrappedText(gFonts.text, b->text, (Vector2){ x + 28, y }, w - 28, size, COL_TEXT, true) + 4;
            bool lastInList = i + 1 >= L->blockCount || L->blocks[i + 1].type != BLOCK_BULLET;
            if (lastInList) y += 10;
        } break;
        }
    }

    // Exercises, done in the code editor
    if (L->exerciseCount > 0) {
        y += 14;
        y += UiWrappedText(gFonts.text, "Exercises", (Vector2){ x, y }, w, 30 * ts, COL_ACCENT, true) + 10;
        for (int i = 0; i < L->exerciseCount; i++) {
            Rectangle row = { x, y, w, 64 };
            bool done = CodeExerciseDone(L, i);
            UiRoundRect(row, 10, COL_PANEL);
            Vector2 badge = { x + 34, y + 32 };
            DrawCircleV(badge, 18, done ? COL_GOOD_BG : COL_BG);
            IconDraw(done ? ICON_CHECK : ICON_CODE, badge, 20, done ? COL_GOOD : COL_ACCENT);
            UiText(gFonts.text, L->exercises[i].title, x + 66, y + 20, 21, COL_TEXT);
            if (UiButtonIcon((Rectangle){ x + w - 150, y + 12, 136, 40 }, ICON_NEXT, done ? "Open" : "Start", true)) {
                openExercise = i;
            }
            y += 76;
        }
    }

    y += 16;
    const char *label = L->questionCount
        ? TextFormat("Start quiz (%d question%s)", L->questionCount, L->questionCount == 1 ? "" : "s")
        : "Mark as complete";
    Icon startIcon = L->questionCount ? ICON_PLAY : ICON_CHECK;
    if (UiButtonIcon((Rectangle){ x, y, 340, 52 }, startIcon, label, true) || IsKeyPressed(KEY_ENTER)) StartQuiz();
    y += 52 + 48;

    app.contentH = y - top + 32;
    EndView();

    if (trySnippet) {
        CodeOpenSnippet(TextFormat("Try it: %s", L->title), trySnippet, L->track == TRACK_CPP);
        OpenCodeFromLesson();
    } else if (openExercise >= 0) {
        CodeOpenExercise(L, openExercise);
        OpenCodeFromLesson();
    }
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
        UiIconText(right ? ICON_CHECK : ICON_CROSS, gFonts.bold, right ? "Correct!" : "Not quite.", x, y, 26,
                   right ? COL_GOOD : COL_BAD);
        y += 40;
        if (q->explain) y += UiWrappedText(gFonts.text, q->explain, (Vector2){ x, y }, w, 21 * ts, COL_TEXT, true) + 16;

        bool last = app.question + 1 >= L->questionCount;
        if (UiButtonIcon((Rectangle){ x, y, 240, 50 }, last ? ICON_TROPHY : ICON_NEXT,
                         last ? "See results" : "Next question", true)
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
        Go(app.lessonReturn);
        return;
    }

    float cx = GetScreenWidth() / 2.0f;
    float y = 100;
    bool passed = app.lastPercent >= PASS_PERCENT;

    // Trophy for a pass, an open book (go back and review) otherwise.
    DrawCircleV((Vector2){ cx, y + 50 }, 50, passed ? COL_GOOD_BG : COL_BAD_BG);
    IconDraw(passed ? ICON_TROPHY : ICON_BOOK, (Vector2){ cx, y + 50 }, 56, passed ? COL_GOOD : COL_BAD);
    y += 124;

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

    if (UiButtonIconEx((Rectangle){ x0, y, bw, bh }, ICON_BOOK, "Review lesson", 22, COL_PANEL, COL_SECONDARY_HOVER,
                       COL_TEXT, true)) {
        Go(SCREEN_LESSON);
    }
    if (UiButtonIconEx((Rectangle){ x0 + bw + gap, y, bw, bh }, L->questionCount ? ICON_RETRY : ICON_BACK,
                       L->questionCount ? "Retry quiz" : app.lessonReturn == SCREEN_DASHBOARD ? "Dashboard" : "All lessons",
                       22, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT,
                       true)) {
        if (L->questionCount) StartQuiz();
        else Go(app.lessonReturn);
    }
    bool canNext = next >= 0;
    if (UiButtonIcon((Rectangle){ x0 + (bw + gap) * 2, y, bw, bh }, next >= 0 ? ICON_NEXT : ICON_TROPHY,
                     next >= 0 ? "Next lesson" : "End of lessons", canNext)
        || (canNext && IsKeyPressed(KEY_ENTER))) {
        OpenLesson(next);
    }
}

// ---------------------------------------------------------------------------

static void ScreenSplash(void)
{
    float t = app.shotTime >= 0 ? app.shotTime : (float)(GetTime() - app.splashStart);
    if (SplashScreen(t) && app.shotTime < 0) Go(SCREEN_LOGIN);
}

// Dev aid (--shot gallery): every icon and the logo at several sizes, to check the artwork.
static void ScreenGallery(void)
{
    static const char *names[ICON_COUNT] = {
        "back", "next", "check", "cross", "gear", "logout", "power", "user", "key", "lock",
        "theme", "trash", "info", "book", "retry", "play", "trophy", "copy", "target", "code", "mail",
    };
    float x = 40, y = 40;
    float sizes[] = { 128, 64, 48, 32, 16 };
    for (int i = 0; i < 5; i++) {
        BrandDrawLogo((Vector2){ x, y + (128 - sizes[i]) }, sizes[i], COL_BRAND, WHITE, LOGO_STATIC, i == 0);
        x += sizes[i] + 28;
    }
    DrawLogoLockup(GetScreenWidth() - 300.0f, y + 30, 70);

    y = 220;
    for (int i = 0; i < ICON_COUNT; i++) {
        float cx = 70 + (i % 10) * 118.0f, cy = y + (i / 10) * 150.0f;
        UiRoundRect((Rectangle){ cx - 44, cy - 44, 88, 88 }, 12, COL_PANEL);
        IconDraw((Icon)i, (Vector2){ cx, cy }, 48, COL_TEXT);
        IconDraw((Icon)i, (Vector2){ cx + 30, cy + 62 }, 18, COL_ACCENT);
        UiTextCentered(gFonts.text, names[i], cx - 10, cy + 52, 16, COL_DIM);
    }
    UiButtonIcon((Rectangle){ 40, 690, 240, 50 }, ICON_PLAY, "Start quiz", true);
    UiButtonIconEx((Rectangle){ 300, 690, 160, 50 }, ICON_GEAR, "Settings", 20, COL_PANEL, COL_SECONDARY_HOVER,
                   COL_TEXT, true);
    UiIconText(ICON_CHECK, gFonts.bold, "Correct!", 490, 702, 26, COL_GOOD);
}

// Window/taskbar icon from the generated PNGs (the .exe icon is embedded by the resource file).
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
// PNG, and exits. Screens: splash gallery login register reset recovery settings delete menu
// list dashboard lesson quiz answered result editor exercise tryit.
typedef struct {
    const char *path;
    int framesLeft;
    int waited;  // frames spent waiting for a program to finish
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
    else if (strcmp(screen, "splash") == 0) {
        app.screen = SCREEN_SPLASH;
        app.shotTime = (argc > 5 ? (float)atof(argv[5]) : 1500) / 1000.0f;  // ms into the animation
    }
    else if (strcmp(screen, "gallery") == 0) app.screen = SCREEN_GALLERY;
    else if (strcmp(screen, "editor") == 0 || strcmp(screen, "exercise") == 0 || strcmp(screen, "tryit") == 0) {
        // editor: free editor; exercise: lesson <index>, exercise <argv[5]> (1-based);
        // tryit: the first runnable example of lesson <index>. INCREMENT_SHOT_RUN=1 runs the code first.
        if (!AccountCurrent()) {
            app.screen = SCREEN_MENU;
        } else if (strcmp(screen, "editor") == 0) {
            CodeOpenPlayground();
            app.screen = SCREEN_CODE;
        } else if (strcmp(screen, "exercise") == 0) {
            int ex = argc > 5 ? atoi(argv[5]) - 1 : 0;
            if (lessons.count && ex >= 0 && ex < lessons.items[index].exerciseCount) {
                CodeOpenExercise(&lessons.items[index], ex);
                app.screen = SCREEN_CODE;
            }
        } else if (lessons.count) {
            Lesson *L = &lessons.items[index];
            for (int b = 0; b < L->blockCount; b++) {
                if (L->blocks[b].type == BLOCK_CODE && strstr(L->blocks[b].text, "main(")) {
                    CodeOpenSnippet(TextFormat("Try it: %s", L->title), L->blocks[b].text, L->track == TRACK_CPP);
                    app.screen = SCREEN_CODE;
                    break;
                }
            }
        }
        app.codeReturn = SCREEN_MENU;
        const char *runIt = getenv("INCREMENT_SHOT_RUN");
        if (app.screen == SCREEN_CODE && runIt && runIt[0] == '1') CodeRunNow();
    }
    else if (strcmp(screen, "register") == 0 || strcmp(screen, "reset") == 0 || strcmp(screen, "resetemail") == 0
             || strcmp(screen, "resetcode") == 0) app.screen = SCREEN_LOGIN;
    else if (strcmp(screen, "recovery") == 0) app.screen = SCREEN_RECOVERY;
    else if (strcmp(screen, "verify") == 0 || strcmp(screen, "addemail") == 0) app.screen = AccountCurrent() ? SCREEN_VERIFY : SCREEN_LOGIN;
    else if (strcmp(screen, "settings") == 0 || strcmp(screen, "delete") == 0) {
        SettingsOpen();
        app.screen = SCREEN_SETTINGS;
    }
    else if (strcmp(screen, "list") == 0) {
        app.screen = SCREEN_LIST;
        // INCREMENT_SHOT_SEARCH types into the search box (even "" focuses it, for the empty-search hint);
        // INCREMENT_SHOT_FILTER is c or cpp.
        const char *query = getenv("INCREMENT_SHOT_SEARCH"), *filter = getenv("INCREMENT_SHOT_FILTER");
        if (query) {
            snprintf(app.search, sizeof app.search, "%s", query);
            UiFocus(app.search);
        }
        if (filter) app.searchFilter = strcmp(filter, "cpp") == 0 ? SEARCH_CPP : strcmp(filter, "c") == 0 ? SEARCH_C : SEARCH_BOTH;
    }
    else if (strcmp(screen, "dashboard") == 0) app.screen = SCREEN_DASHBOARD;
    else if (strcmp(screen, "lesson") == 0) app.screen = SCREEN_LESSON;
    else if (strcmp(screen, "quiz") == 0 || strcmp(screen, "answered") == 0) app.screen = SCREEN_QUIZ;
    else if (strcmp(screen, "result") == 0) app.screen = SCREEN_RESULT;
    if (app.screen > SCREEN_MENU && app.screen != SCREEN_DASHBOARD && lessons.count == 0) app.screen = SCREEN_MENU;

    app.picked = -1;
    if (strcmp(screen, "answered") == 0 && lessons.count && lessons.items[index].questionCount) {
        app.picked = 0; // first choice, right or wrong
    }
    app.scroll = argc > 5 ? (float)atof(argv[5]) : 0;
    app.contentH = 100000; // let the requested scroll survive the first frame's clamp
    app.listScroll = app.scroll;
    app.listH = 100000;
    app.dashboardScroll = app.scroll;
    app.dashboardH = 100000;
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
    LoadWindowIcons(assetDir);
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
    const char *emailDirs[] = { GetApplicationDirectory(), dataDir };  // where email_config.txt may live
    EmailInit(emailDirs, 2);

    ShowLogin();
    app.lessonReturn = SCREEN_LIST;
    app.screen = SCREEN_SPLASH;  // ShowLogin prepared the login form; show the splash first
    app.splashStart = GetTime();
    app.shotTime = -1;
    Shot shot = ParseShot(argc, argv);

    while (!WindowShouldClose() && !app.quit) {
        UiBeginFrame();
        BeginDrawing();
        ClearBackground(COL_BG);

        switch (app.screen) {
        case SCREEN_SPLASH:   ScreenSplash();       break;
        case SCREEN_GALLERY:  ScreenGallery();      break;
        case SCREEN_LOGIN:    ScreenLoginWrap();    break;
        case SCREEN_RECOVERY: ScreenRecoveryWrap(); break;
        case SCREEN_VERIFY:   ScreenVerifyWrap();   break;
        case SCREEN_SETTINGS: ScreenSettingsWrap(); break;
        case SCREEN_CODE:     ScreenCodeWrap();     break;
        case SCREEN_MENU:   ScreenMenu();   break;
        case SCREEN_LIST:   ScreenList();   break;
        case SCREEN_DASHBOARD: ScreenDashboard(); break;
        case SCREEN_LESSON: ScreenLesson(); break;
        case SCREEN_QUIZ:   ScreenQuiz();   break;
        case SCREEN_RESULT: ScreenResult(); break;
        }

        if (shot.path && CodeBusy() && shot.framesLeft == 1 && shot.waited++ < 1800) {
            // a shot of the code editor waits for the compile and run to finish (up to ~30 s)
        } else if (shot.path && --shot.framesLeft == 0) {
            rlDrawRenderBatchActive(); // pending batched draws aren't in the framebuffer yet
            Image img = LoadImageFromScreen();
            ExportImage(img, shot.path);
            UnloadImage(img);
            app.quit = true;
        }
        EndDrawing();
        UiEndFrame();
    }

    CodeShutdown();
    EmailShutdown();
    LessonsUnload(&lessons);
    UiUnloadFonts();
    CloseWindow();
    return 0;
}
