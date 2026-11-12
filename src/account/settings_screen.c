// The settings screen (appearance, window, email, password, recovery code, backup, delete account)
// and applying the user's appearance settings to the UI.
#include "account/account_screens.h"
#include "account/account_screens_shared.h"
#include "account/account.h"
#include "home/ambience.h"
#include "account/backup.h"
#include "home/mouse_trail.h"
#include "account/email.h"
#include "ui/layout.h"
#include "core/platform.h"
#include "ui/ui.h"
#include "app/window_mode.h"

#include <stdio.h>
#include <string.h>

// The forms' fields and results. Every password buffer is wiped once it has been used.
static struct {
    char currentPassword[PASSWORD_MAX + 1];     // change password
    char newPassword[PASSWORD_MAX + 1];
    char confirmNewPassword[PASSWORD_MAX + 1];
    char recoveryPassword[PASSWORD_MAX + 1];    // make a new recovery code
    char deletePassword[PASSWORD_MAX + 1];      // delete account
    bool confirmingDelete;                      // showing "Delete forever?" after the password
    const char *passwordMessage;                // result of the last password change, or NULL
    bool passwordChanged;                       // ... and whether it worked (green) or not (red)
    const char *recoveryMessage;                // why a new recovery code failed, or NULL
    const char *deleteMessage;                  // why deleting failed, or NULL
    const char *windowMessage;                  // why the window setting couldn't be saved, or NULL
    char backupToRestore[1024];                 // a backup file chosen to restore, waiting for "Replace"
    const char *backupMessage;                  // result of the last export or restore, or NULL
    bool backupWorked;                          // ... and whether it worked (green) or not (red)
    float scroll;
    float contentH;                             // measured last frame, for clamping the scroll
} settings;

// ---------------------------------------------------------------------------
// Appearance

void ApplyUserAppearance(void)
{
    static const float textScales[TEXT_SIZE_COUNT] = { 0.87f, 1.0f, 1.18f };  // Small, Medium, Large
    AccountSettings s = AccountGetSettings();
    UiSetTheme(s.theme == THEME_LIGHT, s.accent);
    bool validSize = s.textSize >= 0 && s.textSize < TEXT_SIZE_COUNT;
    UiSetTextScale(validSize ? textScales[s.textSize] : 1.0f);
    AmbienceSetStyle((AmbienceStyle)s.ambience);
    MouseTrailEnable(s.mouseTrail);
}

// ---------------------------------------------------------------------------
// Small widgets used by the sections

void SettingsOpen(void)
{
    WIPE(settings.currentPassword);
    WIPE(settings.newPassword);
    WIPE(settings.confirmNewPassword);
    WIPE(settings.recoveryPassword);
    WIPE(settings.deletePassword);
    settings.confirmingDelete = false;
    settings.passwordMessage = settings.recoveryMessage = settings.deleteMessage = settings.windowMessage = NULL;
    settings.backupMessage = NULL;
    settings.backupToRestore[0] = '\0';
    settings.scroll = settings.contentH = 0;
    UiFocus(NULL);
}

// A row of buttons where one is selected (like a radio group). Returns the selected index.
static int Segmented(float x, float y, float segW, const char **labels, int n, int selected)
{
    for (int i = 0; i < n; i++) {
        bool active = i == selected;
        Rectangle r = { x + i * (segW + 6), y, segW, 40 };
        if (UiButtonEx(r, labels[i], 19, active ? COL_ACCENT : COL_PANEL_HI, active ? COL_ACCENT : COL_SECONDARY_HOVER,
                       active ? COL_ON_ACCENT : COL_TEXT, true)) {
            selected = i;
        }
    }
    return selected;
}

// A section title with its icon; moves *y below it.
static void Heading(Icon icon, const char *title, float x, float *y)
{
    UiIconText(icon, gFonts.bold, title, x, *y, 28, COL_TEXT);
    *y += 48;
}

// Password field; returns true when Enter is pressed in it.
static bool PasswordField(const char *label, char *buf, float x, float *y, float w)
{
    UiText(gFonts.text, label, x, *y, 18, COL_DIM);
    *y += 26;
    bool enter = UiTextBox((Rectangle){ x, *y, w, 46 }, buf, PASSWORD_MAX + 1, "", true);
    *y += 46 + 14;
    return enter;
}

// A one-line result under a form: green when ok, red otherwise. Nothing when msg is NULL.
static void Message(const char *msg, bool ok, float x, float *y)
{
    if (!msg) return;
    UiText(gFonts.text, msg, x, *y, 18, ok ? COL_GOOD : COL_BAD);
    *y += 32;
}

// A red button with a bin icon, for destructive actions.
static bool DangerButton(Rectangle r, const char *label)
{
    return UiButtonIconEx(r, ICON_TRASH, label, 20, COL_BAD, ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true);
}

// ---------------------------------------------------------------------------
// Sections. Each draws from y and returns the y below it.

// Theme, accent colour, lesson text size (with a preview), the journey's code effects and
// window mode. Changes apply
// and save at once.
static float AppearanceSection(float x, float y, float w)
{
    static const char *themes[] = { "Dark", "Light" };
    static const char *sizes[] = { "Small", "Medium", "Large" };
    static const char *windows[WINDOW_MODE_COUNT] = { "Maximised", "Full screen", "Window" };  // by WindowMode
    AccountSettings s = AccountGetSettings(), before = s;
    float cx = x + 220; // control column

    Heading(ICON_THEME, "Appearance", x, &y);

    UiText(gFonts.text, "Theme", x, y + 9, 20, COL_TEXT);
    s.theme = (ThemeMode)Segmented(cx, y, 120, themes, 2, s.theme);
    y += 60;

    UiText(gFonts.text, "Accent colour", x, y + 9, 20, COL_TEXT);
    bool light = s.theme == THEME_LIGHT;
    for (int i = 0; i < UI_ACCENT_COUNT; i++) {
        Vector2 c = { cx + 20 + i * 52, y + 20 };
        Rectangle hit = { c.x - 20, c.y - 20, 40, 40 };
        bool over = UiMouseOver(hit);
        if (over) UiWantHand();
        if (i == s.accent) {
            DrawCircleV(c, 20, COL_TEXT);
            DrawCircleV(c, 17, COL_BG);
        }
        DrawCircleV(c, over && i != s.accent ? 16 : 14, UiAccentSwatch(i, light));
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) s.accent = i;
    }
    UiText(gFonts.text, UiAccentName(s.accent), cx + UI_ACCENT_COUNT * 52 + 10, y + 9, 20, COL_DIM);
    y += 60;

    UiText(gFonts.text, "Lesson text size", x, y + 9, 20, COL_TEXT);
    s.textSize = (TextSize)Segmented(cx, y, 110, sizes, 3, s.textSize);
    y += 56;

    // Preview of lesson text at the chosen size, under the text size buttons.
    float scale = UiTextScale();
    UiRoundRect((Rectangle){ cx, y, w - 220, 22 * scale * 1.35f + 28 }, 8, COL_PANEL);
    UiText(gFonts.text, "Lessons will look like this.", cx + 16, y + 14, 22 * scale, COL_TEXT);
    y += 22 * scale * 1.35f + 28 + 20;

    // The journey's code effects, each switchable on its own.
    static const char *onOff[] = { "On", "Off" };
    UiText(gFonts.text, "Background code", x, y + 9, 20, COL_TEXT);
    // The styles in two rows of four: Off, Particles, Swirl, Downpour / Mandala, Life, Waves, Fox.
    const char *styles[AMBIENCE_STYLE_COUNT];
    for (int i = 0; i < AMBIENCE_STYLE_COUNT; i++) styles[i] = AmbienceStyleName((AmbienceStyle)i);
    int perRow = AMBIENCE_STYLE_COUNT / 2;
    for (int row = 0; row < 2; row++) {
        int first = row * perRow, chosen = s.ambience - first;
        int picked = Segmented(cx, y, 104, styles + first, perRow, chosen >= 0 && chosen < perRow ? chosen : -1);
        if (picked >= 0) s.ambience = first + picked;
        y += 48;
    }
    y += 8;
    UiText(gFonts.text, "Mouse trail", x, y + 9, 20, COL_TEXT);
    s.mouseTrail = Segmented(cx, y, 90, onOff, 2, s.mouseTrail ? 0 : 1) == 0;
    y += 48;
    UiText(gFonts.text, "Faint code characters on the journey: in the background, and following the mouse.", cx, y,
           17, COL_DIM);
    y += 40;

    if (memcmp(&s, &before, sizeof s) != 0) {
        AccountSetSettings(s);
        ApplyUserAppearance();
    }

    // The window mode belongs to this computer, not the account (see window_mode.h).
    UiText(gFonts.text, "Window", x, y + 9, 20, COL_TEXT);
    WindowMode window = WindowModeGet();
    WindowMode chosen = (WindowMode)Segmented(cx, y, 140, windows, WINDOW_MODE_COUNT, window);
    if (chosen != window && !WindowModeSet(chosen)) settings.windowMessage = "Couldn't save the window setting.";
    y += 48;
    UiText(gFonts.text, "How Increment opens on this computer. F11 switches full screen on and off.", cx, y, 17,
           COL_DIM);
    y += 28;
    if (settings.windowMessage) Message(settings.windowMessage, false, cx, &y);
    return y;
}

// Current, new and confirm fields; changes the password on Enter in the last one or the button.
static float PasswordSection(float x, float y, float fieldW)
{
    Heading(ICON_LOCK, "Change password", x, &y);
    bool go = false;
    if (PasswordField("Current password", settings.currentPassword, x, &y, fieldW)) UiFocus(settings.newPassword);
    if (PasswordField("New password", settings.newPassword, x, &y, fieldW)) UiFocus(settings.confirmNewPassword);
    if (PasswordField("Confirm new password", settings.confirmNewPassword, x, &y, fieldW)) go = true;
    Message(settings.passwordMessage, settings.passwordChanged, x, &y);
    if (UiButtonIcon((Rectangle){ x, y, 260, 48 }, ICON_LOCK, "Change password", true)) go = true;
    y += 48;

    if (go) {
        const char *err = NULL;
        if (strcmp(settings.newPassword, settings.confirmNewPassword) != 0) {
            settings.passwordMessage = "The new passwords don't match.";
            settings.passwordChanged = false;
        } else if (AccountChangePassword(settings.currentPassword, settings.newPassword, &err)) {
            settings.passwordMessage = "Password changed.";
            settings.passwordChanged = true;
        } else {
            settings.passwordMessage = err;
            settings.passwordChanged = false;
        }
        WIPE(settings.currentPassword);
        WIPE(settings.newPassword);
        WIPE(settings.confirmNewPassword);
        UiFocus(NULL);
    }
    return y;
}

// Makes a new recovery code (the old one stops working); sets *action to show it.
static float RecoverySection(float x, float y, float w, float fieldW, AccountAction *action)
{
    Heading(ICON_KEY, "Recovery code", x, &y);
    bool has = AccountHasRecovery();
    y += UiWrappedText(gFonts.text,
                       has ? "You have a recovery code. If you've lost it, make a new one and the old code will stop working."
                           : "You don't have a recovery code yet. Make one so you can reset a forgotten password.",
                       (Vector2){ x, y }, w, 20, has ? COL_DIM : COL_BAD, true) + 14;

    bool go = PasswordField("Password", settings.recoveryPassword, x, &y, fieldW);
    Message(settings.recoveryMessage, false, x, &y);
    if (UiButtonIcon((Rectangle){ x, y, 260, 48 }, ICON_KEY, "Make a new code", true)) go = true;
    y += 48;

    if (go) {
        const char *err = NULL;
        char newCode[RECOVERY_CODE_SIZE];
        if (AccountNewRecoveryCode(settings.recoveryPassword, newCode, &err)) {
            RecoveryOffer(newCode);  // the recovery screen shows it next
            settings.recoveryMessage = NULL;
            *action = ACCOUNT_SHOW_CODE;
        } else {
            settings.recoveryMessage = err;
        }
        WIPE(newCode);
        WIPE(settings.recoveryPassword);
        UiFocus(NULL);
    }
    return y;
}

// The signed-in user's files, as a backup sees them.
static BackupFiles AccountFiles(void)
{
    return (BackupFiles){ AccountProgressPath(), AccountSkillsPath(), AccountReviewPath(), AccountSettingsPath(),
                          AccountCodeDir() };
}

// Export the user's work to a file, or restore it from one (two steps: choose the file,
// then "Replace"). Sets *action when a backup has been restored.
static float BackupSection(float x, float y, float w, AccountAction *action)
{
    Heading(ICON_COPY, "Backup", x, &y);
    y += UiWrappedText(gFonts.text,
                       "Save your progress, skills and code to a file, to keep it safe or to move it to another "
                       "computer. Your password and email stay out of it.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 14;

    if (!settings.backupToRestore[0]) {
        Message(settings.backupMessage, settings.backupWorked, x, &y);
        if (UiButtonIcon((Rectangle){ x, y, 220, 48 }, ICON_COPY, "Export backup", true)) {
            char path[1024];
            if (PlatformSaveFileDialog("Increment backup", "increment", TextFormat("Increment-%s", AccountCurrent()),
                                       path, sizeof path)) {
                BackupFiles files = AccountFiles();
                const char *err = NULL;
                settings.backupWorked = BackupExport(path, AccountCurrent(), &files, &err);
                settings.backupMessage = settings.backupWorked ? "Backup saved." : err;
            }
        }
        if (UiButtonIconEx((Rectangle){ x + 236, y, 220, 48 }, ICON_RETRY, "Restore backup", 20, COL_PANEL_HI,
                           COL_SECONDARY_HOVER, COL_TEXT, true)) {
            settings.backupMessage = NULL;
            if (!PlatformOpenFileDialog("Increment backup", "increment", settings.backupToRestore,
                                        sizeof settings.backupToRestore)) {
                settings.backupToRestore[0] = '\0';
            }
        }
        return y + 48;
    }

    y += UiWrappedText(gFonts.text,
                       TextFormat("Replace %s's progress, skills and settings with this backup? This can't be undone.",
                                  AccountCurrent()),
                       (Vector2){ x, y }, w, 22, COL_BAD, true) + 8;
    UiText(gFonts.text, GetFileName(settings.backupToRestore), x, y, 18, COL_DIM);
    y += 34;
    if (UiButtonEx((Rectangle){ x, y, 160, 48 }, "Cancel", 20, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        settings.backupToRestore[0] = '\0';
    } else if (UiButtonIcon((Rectangle){ x + 176, y, 220, 48 }, ICON_RETRY, "Replace", true)) {
        BackupFiles files = AccountFiles();
        const char *err = NULL;
        settings.backupWorked = BackupImport(settings.backupToRestore, &files, &err);
        settings.backupMessage = settings.backupWorked ? "Backup restored." : err;
        settings.backupToRestore[0] = '\0';
        if (settings.backupWorked) {
            AccountReloadSettings();  // the restored look applies at once; main.c reloads the rest
            ApplyUserAppearance();
            *action = ACCOUNT_DATA_RESTORED;
        }
    }
    return y + 48;
}

// Two steps: the password, then "Delete forever?". Sets *action when the account is gone.
static float DeleteSection(float x, float y, float w, float fieldW, AccountAction *action)
{
    Heading(ICON_TRASH, "Delete account", x, &y);
    y += UiWrappedText(gFonts.text, "This permanently deletes your account, progress and settings from this computer.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 14;

    if (!settings.confirmingDelete) {
        bool go = PasswordField("Password", settings.deletePassword, x, &y, fieldW);
        Message(settings.deleteMessage, false, x, &y);
        if (DangerButton((Rectangle){ x, y, 260, 48 }, "Delete account")) go = true;
        y += 48;
        if (go) {
            if (settings.deletePassword[0] == '\0') {
                settings.deleteMessage = "Enter your password first.";
            } else {
                settings.deleteMessage = NULL;
                settings.confirmingDelete = true;
                UiFocus(NULL);
            }
        }
        return y;
    }

    UiText(gFonts.text, TextFormat("Delete %s forever? This can't be undone.", AccountCurrent()), x, y, 22, COL_BAD);
    y += 40;
    if (UiButtonEx((Rectangle){ x, y, 160, 48 }, "Cancel", 20, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        settings.confirmingDelete = false;
        WIPE(settings.deletePassword);
    }
    if (DangerButton((Rectangle){ x + 176, y, 220, 48 }, "Delete forever")) {
        const char *err = NULL;
        if (AccountDelete(settings.deletePassword, &err)) {
            SettingsOpen();
            ApplyUserAppearance();  // back to the last user's (or default) look
            *action = ACCOUNT_SIGNED_OUT;
        } else {
            settings.deleteMessage = err;
            settings.confirmingDelete = false;
        }
        WIPE(settings.deletePassword);
    }
    return y + 48;
}

// The verified address and a button to add or change it (sets *action).
static float EmailSection(float x, float y, AccountAction *action)
{
    Heading(ICON_MAIL, "Email", x, &y);
    const char *email = AccountEmail();
    if (email[0]) {
        float tw = UiIconText(ICON_CHECK, gFonts.text, email, x, y, 20, COL_TEXT);
        UiText(gFonts.text, "verified", x + tw + 12, y + 2, 17, COL_GOOD);
    } else {
        UiText(gFonts.text, "No verified email yet.", x, y, 20, COL_BAD);
    }
    y += 40;
    if (UiButtonIcon((Rectangle){ x, y, 220, 48 }, ICON_MAIL, email[0] ? "Change email" : "Add email", true)) {
        *action = ACCOUNT_CHANGE_EMAIL;
    }
    return y + 48;
}

AccountAction SettingsScreen(void)
{
    if (TopBar("Settings")) {
        SettingsOpen();
        return ACCOUNT_BACK;
    }

    Rectangle view = ScrollView();
    UpdateScroll(&settings.scroll, settings.contentH, view.height);
    BeginView(view);

    float x = ContentX(), w = ContentWidth();
    float fieldW = w < 440 ? w : 440;
    float top = view.y + 32 - settings.scroll;
    AccountAction action = ACCOUNT_STAY;

    float y = AppearanceSection(x, top, w);
    if (EmailConfigured()) y = EmailSection(x, y + 48, &action);
    y = PasswordSection(x, y + 48, fieldW);
    y = RecoverySection(x, y + 48, w, fieldW, &action);
    if (action == ACCOUNT_STAY) y = BackupSection(x, y + 48, w, &action);
    if (action == ACCOUNT_STAY) y = DeleteSection(x, y + 48, w, fieldW, &action);

    settings.contentH = y - top + 64;
    EndView();
    PageScrollBar(view, &settings.scroll, settings.contentH);
    return action;
}


void SettingsDemo(const char *what)
{
    if (strcmp(what, "delete") == 0) {  // the "Delete forever?" step, scrolled to the bottom
        snprintf(settings.deletePassword, sizeof settings.deletePassword, "x");
        settings.confirmingDelete = true;
        settings.scroll = 100000;
        settings.contentH = 100000;
    }
    if (strcmp(what, "backup") == 0) {  // the Backup section, asking before a restore
        snprintf(settings.backupToRestore, sizeof settings.backupToRestore, "Increment-demo.increment");
        settings.scroll = 100000;
        settings.contentH = 100000;
    }
}
