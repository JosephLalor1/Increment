#include "account_screens.h"
#include "account.h"
#include "layout.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

typedef enum { LOGIN_SIGNIN, LOGIN_CREATE, LOGIN_RESET } LoginMode;

static struct {
    LoginMode mode;
    char name[USERNAME_MAX + 1];
    char pass[PASSWORD_MAX + 1];
    char confirm[PASSWORD_MAX + 1];
    char code[32];
    const char *error;
} login;

static struct {
    char code[RECOVERY_CODE_SIZE];
    bool pending;
    bool copied;
} recovery;

static struct {
    char current[PASSWORD_MAX + 1];
    char next[PASSWORD_MAX + 1];
    char confirm[PASSWORD_MAX + 1];
    char codePass[PASSWORD_MAX + 1];
    char deletePass[PASSWORD_MAX + 1];
    bool confirmDelete;
    const char *passwordMsg;
    bool passwordOk;
    const char *codeMsg;
    const char *deleteMsg;
    float scroll;
    float contentH;
} settings;

// volatile so the compiler can't drop the wipe of a buffer that's never read again
static void Wipe(char *buf, size_t n)
{
    volatile char *p = buf;
    while (n--) *p++ = 0;
}

#define WIPE(buf) Wipe((buf), sizeof (buf))

// ---------------------------------------------------------------------------
// Settings application

void ApplySettings(void)
{
    AccountSettings s = AccountGetSettings();
    UiSetTheme(s.theme == THEME_LIGHT, s.accent);
}

float TextScale(void)
{
    static const float scales[TEXT_SIZE_COUNT] = { 0.87f, 1.0f, 1.18f };
    AccountSettings s = AccountGetSettings();
    return (s.textSize >= 0 && s.textSize < TEXT_SIZE_COUNT) ? scales[s.textSize] : 1.0f;
}

// ---------------------------------------------------------------------------
// Login

static char **LoginFields(int *count)
{
    static char *order[4];
    int n = 0;
    order[n++] = login.name;
    if (login.mode == LOGIN_RESET) order[n++] = login.code;
    order[n++] = login.pass;
    if (login.mode != LOGIN_SIGNIN) order[n++] = login.confirm;
    *count = n;
    return order;
}

static const char *LoginLabel(const char *buf)
{
    if (buf == login.name) return "Username";
    if (buf == login.code) return "Recovery code";
    if (buf == login.pass) return login.mode == LOGIN_RESET ? "New password" : "Password";
    return login.mode == LOGIN_RESET ? "Confirm new password" : "Confirm password";
}

static void FocusFirstEmpty(void)
{
    int count;
    char **fields = LoginFields(&count);
    for (int i = 0; i < count; i++) {
        if (fields[i][0] == '\0') {
            UiFocus(fields[i]);
            return;
        }
    }
    UiFocus(fields[count - 1]);
}

static void WipeLoginSecrets(void)
{
    WIPE(login.pass);
    WIPE(login.confirm);
    WIPE(login.code);
}

static void SetLoginMode(LoginMode mode)
{
    login.mode = mode;
    login.error = NULL;
    WipeLoginSecrets();
    FocusFirstEmpty();
}

void LoginReset(void)
{
    snprintf(login.name, sizeof login.name, "%s", AccountLastUser());
    SetLoginMode(AccountCount() == 0 ? LOGIN_CREATE : LOGIN_SIGNIN);
}

static bool SubmitLogin(void)
{
    const char *err = NULL;
    bool ok = false;
    if (login.mode != LOGIN_SIGNIN && strcmp(login.pass, login.confirm) != 0) {
        err = "The passwords don't match.";
    } else if (login.mode == LOGIN_CREATE) {
        ok = AccountCreate(login.name, login.pass, recovery.code, &err);
    } else if (login.mode == LOGIN_RESET) {
        ok = AccountResetPassword(login.name, login.code, login.pass, recovery.code, &err);
    } else {
        ok = AccountLogin(login.name, login.pass, &err);
    }

    if (ok) {
        recovery.pending = login.mode != LOGIN_SIGNIN;
        recovery.copied = false;
        WipeLoginSecrets();
        login.error = NULL;
        UiFocus(NULL);
        ApplySettings();
        return true;
    }
    login.error = err;
    WIPE(login.pass);
    WIPE(login.confirm);
    FocusFirstEmpty();
    return false;
}

AccountAction LoginScreen(void)
{
    static const char *buttonLabel[] = { "Sign in", "Create account", "Reset password" };
    static const char *footer[] = {
        NULL,
        "Accounts are stored on this computer only.",
        "Use the recovery code you saved when signing up.",
    };

    int count;
    char **fields = LoginFields(&count);
    float cx = GetScreenWidth() / 2.0f;
    float cardW = 460, pad = 32;
    float fixedH = 28 + 64 + (login.error ? 36 : 0) + 50 + 48 + 20; // everything but the fields
    float cardH = fixedH + count * 90;

    // Full layout: title, subtitle, card, Quit. Short windows drop the subtitle and Quit
    // and tighten the fields so even the four-field reset form fits at the minimum height.
    float full = 146 + cardH + 74;
    bool compact = full > GetScreenHeight();
    float labelGap = compact ? 22 : 26, fieldH = compact ? 42 : 48, fieldGap = compact ? 10 : 16;
    cardH = fixedH + count * (labelGap + fieldH + fieldGap);
    float top = compact ? 16 : (GetScreenHeight() - full) / 2.0f;
    if (compact) {
        UiTextCentered(gFonts.text, "Increment", cx, top, 48, COL_TEXT);
        top += 72;
    } else {
        UiTextCentered(gFonts.text, "Increment", cx, top, 72, COL_TEXT);
        UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, top + 86, 24, COL_DIM);
        top += 146;
    }
    Rectangle card = { cx - cardW / 2, top, cardW, cardH };
    UiRoundRect(card, 14, COL_PANEL);

    float x = card.x + pad, w = cardW - pad * 2;
    float y = card.y + 28;

    if (login.mode == LOGIN_RESET) {
        UiText(gFonts.text, "Reset your password", x, y + 4, 26, COL_TEXT);
        const char *back = "Back to sign in";
        float lw = MeasureTextEx(gFonts.text, back, 18, 0).x;
        if (UiLink(x + w - lw, y + 10, back, 18)) SetLoginMode(LOGIN_SIGNIN);
    } else {
        float tabW = (w - 8) / 2;
        for (int i = 0; i < 2; i++) {
            bool active = (login.mode == LOGIN_CREATE) == (i == 1);
            Rectangle tab = { x + i * (tabW + 8), y, tabW, 40 };
            if (UiButtonEx(tab, i ? "Create account" : "Sign in", 20, active ? COL_ACCENT : COL_PANEL_HI,
                           active ? COL_ACCENT : COL_SECONDARY_HOVER, active ? COL_ON_ACCENT : COL_TEXT, true)
                && !active) {
                SetLoginMode(i ? LOGIN_CREATE : LOGIN_SIGNIN);
            }
        }
    }
    y += 40 + 24;

    // Fields may change if a tab was clicked above.
    fields = LoginFields(&count);
    bool submit = false;
    for (int i = 0; i < count; i++) {
        char *buf = fields[i];
        bool secret = buf == login.pass || buf == login.confirm;
        int size = buf == login.name ? (int)sizeof login.name
                 : buf == login.code ? (int)sizeof login.code : (int)sizeof login.pass;
        UiText(gFonts.text, LoginLabel(buf), x, y, 18, COL_DIM);
        y += labelGap;
        if (UiTextBox((Rectangle){ x, y, w, fieldH }, buf, size, buf == login.code ? "XXXX-XXXX-XXXX-XXXX" : "", secret)) {
            // Enter moves to the next field, or submits from the last one.
            if (i + 1 < count) UiFocus(fields[i + 1]);
            else submit = true;
        }
        y += fieldH + fieldGap;
    }

    if (IsKeyPressed(KEY_TAB)) {
        int cur = 0;
        for (int i = 0; i < count; i++) if (UiHasFocus(fields[i])) cur = i;
        bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        UiFocus(fields[(cur + (back ? count - 1 : 1)) % count]);
    }

    if (login.error) {
        UiText(gFonts.text, login.error, x, y - 4, 18, COL_BAD);
        y += 36;
    }
    if (UiButton((Rectangle){ x, y, w, 50 }, buttonLabel[login.mode], true)) submit = true;
    y += 50;

    if (login.mode == LOGIN_SIGNIN) {
        const char *forgot = "Forgot your password?";
        float lw = MeasureTextEx(gFonts.text, forgot, 18, 0).x;
        if (UiLink(cx - lw / 2, y + 16, forgot, 18)) {
            SetLoginMode(LOGIN_RESET);
            submit = false;
        }
    } else {
        UiTextCentered(gFonts.text, footer[login.mode], cx, y + 18, 16, COL_DIM);
    }

    bool quit = !compact && UiButtonEx((Rectangle){ cx - 70, card.y + cardH + 28, 140, 46 }, "Quit", 20,
                                       COL_PANEL, COL_PANEL_HI, COL_TEXT, true);
    if (submit && SubmitLogin()) return ACCOUNT_SIGNED_IN;
    return quit ? ACCOUNT_QUIT : ACCOUNT_STAY;
}

// ---------------------------------------------------------------------------
// Recovery code

bool RecoveryPending(void) { return recovery.pending; }

AccountAction RecoveryScreen(void)
{
    float cx = GetScreenWidth() / 2.0f;
    float w = 640;
    float x = cx - w / 2;
    float y = GetScreenHeight() / 2.0f - 230;

    UiTextCentered(gFonts.text, "Save your recovery code", cx, y, 40, COL_TEXT);
    y += 70;
    y += UiWrappedText(gFonts.text,
                       "If you forget your password, this code is the only way back into your account. "
                       "Write it down and keep it somewhere safe. It won't be shown again, and it works once: "
                       "using it gives you a new code.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 28;

    Rectangle box = { x, y, w, 96 };
    UiRoundRect(box, 12, COL_CODE_BG);
    UiTextCentered(gFonts.mono, recovery.code, cx, y + 24, 44, COL_ACCENT);
    y += 96 + 32;

    float bw = 220, gap = 20;
    if (UiButtonEx((Rectangle){ cx - bw - gap / 2, y, bw, 52 }, recovery.copied ? "Copied" : "Copy code", 22,
                   COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        SetClipboardText(recovery.code);
        recovery.copied = true;
    }
    if (UiButton((Rectangle){ cx + gap / 2, y, bw, 52 }, "I've saved it", true)) {
        WIPE(recovery.code);
        recovery.pending = false;
        return ACCOUNT_BACK;
    }
    return ACCOUNT_STAY;
}

// ---------------------------------------------------------------------------
// Settings

void SettingsOpen(void)
{
    WIPE(settings.current);
    WIPE(settings.next);
    WIPE(settings.confirm);
    WIPE(settings.codePass);
    WIPE(settings.deletePass);
    settings.confirmDelete = false;
    settings.passwordMsg = settings.codeMsg = settings.deleteMsg = NULL;
    settings.scroll = settings.contentH = 0;
    UiFocus(NULL);
}

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

static void Heading(const char *title, float x, float *y)
{
    UiText(gFonts.text, title, x, *y, 28, COL_TEXT);
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

static void Message(const char *msg, bool ok, float x, float *y)
{
    if (!msg) return;
    UiText(gFonts.text, msg, x, *y, 18, ok ? COL_GOOD : COL_BAD);
    *y += 32;
}

static bool DangerButton(Rectangle r, const char *label)
{
    return UiButtonEx(r, label, 20, COL_BAD, ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true);
}

static float AppearanceSection(float x, float y, float w)
{
    static const char *themes[] = { "Dark", "Light" };
    static const char *sizes[] = { "Small", "Medium", "Large" };
    AccountSettings s = AccountGetSettings(), before = s;
    float cx = x + 220; // control column

    Heading("Appearance", x, &y);

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

    if (memcmp(&s, &before, sizeof s) != 0) {
        AccountSetSettings(s);
        ApplySettings();
    }

    // Preview of lesson text at the chosen size, under the controls.
    float scale = TextScale();
    UiRoundRect((Rectangle){ cx, y, w - 220, 22 * scale * 1.35f + 28 }, 8, COL_PANEL);
    UiText(gFonts.text, "Lessons will look like this.", cx + 16, y + 14, 22 * scale, COL_TEXT);
    y += 22 * scale * 1.35f + 28;
    return y;
}

static float PasswordSection(float x, float y, float fieldW)
{
    Heading("Change password", x, &y);
    bool go = false;
    if (PasswordField("Current password", settings.current, x, &y, fieldW)) UiFocus(settings.next);
    if (PasswordField("New password", settings.next, x, &y, fieldW)) UiFocus(settings.confirm);
    if (PasswordField("Confirm new password", settings.confirm, x, &y, fieldW)) go = true;
    Message(settings.passwordMsg, settings.passwordOk, x, &y);
    if (UiButton((Rectangle){ x, y, 240, 48 }, "Change password", true)) go = true;
    y += 48;

    if (go) {
        const char *err = NULL;
        if (strcmp(settings.next, settings.confirm) != 0) {
            settings.passwordMsg = "The new passwords don't match.";
            settings.passwordOk = false;
        } else if (AccountChangePassword(settings.current, settings.next, &err)) {
            settings.passwordMsg = "Password changed.";
            settings.passwordOk = true;
        } else {
            settings.passwordMsg = err;
            settings.passwordOk = false;
        }
        WIPE(settings.current);
        WIPE(settings.next);
        WIPE(settings.confirm);
        UiFocus(NULL);
    }
    return y;
}

static float RecoverySection(float x, float y, float w, float fieldW, AccountAction *action)
{
    Heading("Recovery code", x, &y);
    bool has = AccountHasRecovery();
    y += UiWrappedText(gFonts.text,
                       has ? "You have a recovery code. If you've lost it, make a new one and the old code will stop working."
                           : "You don't have a recovery code yet. Make one so you can reset a forgotten password.",
                       (Vector2){ x, y }, w, 20, has ? COL_DIM : COL_BAD, true) + 14;

    bool go = PasswordField("Password", settings.codePass, x, &y, fieldW);
    Message(settings.codeMsg, false, x, &y);
    if (UiButton((Rectangle){ x, y, 240, 48 }, "Make a new code", true)) go = true;
    y += 48;

    if (go) {
        const char *err = NULL;
        if (AccountNewRecoveryCode(settings.codePass, recovery.code, &err)) {
            recovery.pending = true;
            recovery.copied = false;
            settings.codeMsg = NULL;
            *action = ACCOUNT_SHOW_CODE;
        } else {
            settings.codeMsg = err;
        }
        WIPE(settings.codePass);
        UiFocus(NULL);
    }
    return y;
}

static float DeleteSection(float x, float y, float w, float fieldW, AccountAction *action)
{
    Heading("Delete account", x, &y);
    y += UiWrappedText(gFonts.text, "This permanently deletes your account, progress and settings from this computer.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 14;

    if (!settings.confirmDelete) {
        bool go = PasswordField("Password", settings.deletePass, x, &y, fieldW);
        Message(settings.deleteMsg, false, x, &y);
        if (DangerButton((Rectangle){ x, y, 240, 48 }, "Delete account")) go = true;
        y += 48;
        if (go) {
            if (settings.deletePass[0] == '\0') {
                settings.deleteMsg = "Enter your password first.";
            } else {
                settings.deleteMsg = NULL;
                settings.confirmDelete = true;
                UiFocus(NULL);
            }
        }
        return y;
    }

    UiText(gFonts.text, TextFormat("Delete %s forever? This can't be undone.", AccountCurrent()), x, y, 22, COL_BAD);
    y += 40;
    if (UiButtonEx((Rectangle){ x, y, 160, 48 }, "Cancel", 20, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        settings.confirmDelete = false;
        WIPE(settings.deletePass);
    }
    if (DangerButton((Rectangle){ x + 176, y, 220, 48 }, "Delete forever")) {
        const char *err = NULL;
        if (AccountDelete(settings.deletePass, &err)) {
            SettingsOpen();
            ApplySettings();
            *action = ACCOUNT_SIGNED_OUT;
        } else {
            settings.deleteMsg = err;
            settings.confirmDelete = false;
        }
        WIPE(settings.deletePass);
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
    y = PasswordSection(x, y + 48, fieldW);
    y = RecoverySection(x, y + 48, w, fieldW, &action);
    if (action == ACCOUNT_STAY) y = DeleteSection(x, y + 48, w, fieldW, &action);

    settings.contentH = y - top + 64;
    EndView();
    return action;
}

// ---------------------------------------------------------------------------

void AccountScreensDemo(const char *what)
{
    if (strcmp(what, "register") == 0) {
        login.mode = LOGIN_CREATE;
        snprintf(login.name, sizeof login.name, "ada");
        snprintf(login.pass, sizeof login.pass, "secret1");
        login.error = "The passwords don't match.";
        UiFocus(login.confirm);
    } else if (strcmp(what, "reset") == 0) {
        login.mode = LOGIN_RESET;
        snprintf(login.name, sizeof login.name, "ada");
        snprintf(login.code, sizeof login.code, "K7QM-");
        login.error = "Wrong username or recovery code."; // tallest form state
        UiFocus(login.code);
    } else if (strcmp(what, "recovery") == 0) {
        snprintf(recovery.code, sizeof recovery.code, "K7QM-9XPA-2HDV-R4TN");
        recovery.pending = true;
    } else if (strcmp(what, "delete") == 0) {
        snprintf(settings.deletePass, sizeof settings.deletePass, "x");
        settings.confirmDelete = true;
        settings.scroll = 100000; // scrolled to the bottom
        settings.contentH = 100000;
    }
}
