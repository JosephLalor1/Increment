// The main menu. See menu_screen.h.
#include "menu_screen.h"
#include "account.h"
#include "layout.h"
#include "mascot.h"
#include "roadmap.h"
#include "ui.h"

// A track's card: its name, a one-line pitch, and lessons complete. True when clicked.
static bool TrackCard(const LessonSet *set, Rectangle r, Track track)
{
    static const char *pitch[TRACK_COUNT] = {
        "The language underneath everything.",
        "C, plus classes, templates and the STL.",
    };
    bool over = UiMouseOver(r);
    if (over) UiWantHand();
    UiRoundRect(r, 14, over ? COL_PANEL_HI : COL_PANEL);

    int done, total;
    RoadmapNext(set, track, &done, &total);
    UiText(gFonts.mono, TrackName(track), r.x + 28, r.y + 24, 56, COL_ACCENT);
    UiWrappedText(gFonts.text, pitch[track], (Vector2){ r.x + 28, r.y + 92 }, r.width - 56, 20, COL_DIM, true);
    UiText(gFonts.text, TextFormat("%d / %d lessons complete", done, total), r.x + 28, r.y + r.height - 56, 18,
           COL_TEXT);
    Rectangle bar = { r.x + 28, r.y + r.height - 28, r.width - 56, 8 };
    ProgressBar(bar, total ? (float)done / total : 0, COL_BG, COL_GOOD);
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// Top right: "Signed in as ...", Settings and Log out. Returns the button pressed, or MENU_STAY.
static MenuChoice AccountCorner(void)
{
    const char *who = AccountCurrent();
    if (!who) return MENU_STAY;
    float right = GetScreenWidth() - 20.0f;
    Rectangle logout = { right - 110, 20, 110, 40 };
    Rectangle settings = { logout.x - 12 - 110, 20, 110, 40 };
    if (UiButtonIconEx(logout, ICON_LOGOUT, "Log out", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        return MENU_LOG_OUT;
    }
    if (UiButtonIconEx(settings, ICON_GEAR, "Settings", 18, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        return MENU_SETTINGS;
    }
    const char *label = TextFormat("Signed in as %s", who);
    float labelW = 18 * 1.4f + MeasureTextEx(gFonts.text, label, 18, 0).x;  // icon + gap + text
    UiIconText(ICON_USER, gFonts.text, label, settings.x - 16 - labelW, 30, 18, COL_DIM);
    return MENU_STAY;
}

// Under the cards: a warning when no lessons loaded, or a nudge for accounts made before
// recovery codes existed. Returns MENU_SETTINGS if the nudge's link was clicked.
static MenuChoice Notice(const LessonSet *set, float cx, float y)
{
    if (set->count == 0) {
        UiTextCentered(gFonts.text, "No lessons found. Put lesson .txt files in assets/lessons.", cx, y, 20, COL_BAD);
        return MENU_STAY;
    }
    if (!AccountCurrent() || AccountHasRecovery()) return MENU_STAY;

    const char *message = "You don't have a recovery code yet.";
    const char *link = "Make one in Settings";
    float messageW = 18 * 1.4f + MeasureTextEx(gFonts.text, message, 18, 0).x;
    float linkW = MeasureTextEx(gFonts.text, link, 18, 0).x;
    float x = cx - (messageW + 8 + linkW) / 2;
    UiIconText(ICON_KEY, gFonts.text, message, x, y, 18, COL_DIM);
    return UiLink(x + messageW + 8, y, link, 18) ? MENU_SETTINGS : MENU_STAY;
}

MenuChoice MenuScreen(const LessonSet *set, Track *trackOut)
{
    MenuChoice corner = AccountCorner();
    if (corner != MENU_STAY) return corner;  // stop drawing: the screen is about to change

    float cx = GetScreenWidth() / 2.0f;
    DrawLogoLockup(cx, 84, 76);
    if (MascotAvailable()) {
        MascotCompanionActor(FOX_MENU, cx - 300, 168, 600, 76, 20, FOX_HAPPY, "Ready for another coding adventure?");
    } else {
        UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, 185, 26, COL_DIM);
    }

    MenuChoice choice = MENU_STAY;
    float cardW = 340, cardH = 220, gap = 40;
    float x0 = cx - (cardW * 2 + gap) / 2.0f;
    float y = 260;
    for (int t = 0; t < TRACK_COUNT; t++) {
        Rectangle card = { x0 + t * (cardW + gap), y, cardW, cardH };
        if (TrackCard(set, card, (Track)t)) {
            *trackOut = (Track)t;
            choice = MENU_OPEN_TRACK;
        }
    }

    MenuChoice notice = Notice(set, cx, y + cardH + 30);
    if (notice != MENU_STAY) choice = notice;

    float buttonY = y + cardH + 80;
    if (UiButtonIcon((Rectangle){ cx - 390, buttonY, 250, 46 }, ICON_CODE, "Open code editor", true)) {
        choice = MENU_CODE_EDITOR;
    }
    if (UiButtonIconEx((Rectangle){ cx + 240, buttonY, 150, 46 }, ICON_POWER, "Quit", 20, COL_PANEL,
                       COL_SECONDARY_HOVER, COL_TEXT, true)) {
        choice = MENU_QUIT;
    }
    if (UiButtonIconEx((Rectangle){ cx - 120, buttonY, 340, 46 }, ICON_TROPHY, "Progress dashboard", 20, COL_PANEL,
                       COL_SECONDARY_HOVER, COL_TEXT, true)) {
        choice = MENU_DASHBOARD;
    }
    return choice;
}
