#include "account_screens.h"
#include "account.h"
#include "email.h"
#include "layout.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    LOGIN_SIGNIN,
    LOGIN_CREATE,
    LOGIN_RESET,        // forgot password: with a recovery code
    LOGIN_RESET_EMAIL,  // forgot password: ask for the username to email a code to
    LOGIN_RESET_CODE,   // forgot password: enter the emailed code and a new password
} LoginMode;

static struct {
    LoginMode mode;
    char name[USERNAME_MAX + 1];
    char email[EMAIL_MAX + 1];
    char pass[PASSWORD_MAX + 1];
    char confirm[PASSWORD_MAX + 1];
    char code[32];            // recovery code
    char emailCode[16];       // code from a password reset email
    const char *error;
    char errorBuf[320];       // for errors built at run time (e.g. from the email service)
    char notice[160];
    char resetTo[EMAIL_MAX + 8];  // masked address the reset code went to
    bool sending;
    double resendAt;
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

#define RESEND_COOLDOWN 30.0  // seconds between "Send a new code" presses

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

static bool IsResetMode(LoginMode m) { return m == LOGIN_RESET || m == LOGIN_RESET_EMAIL || m == LOGIN_RESET_CODE; }

static char **LoginFields(int *count)
{
    static char *order[5];
    int n = 0;
    if (login.mode != LOGIN_RESET_CODE) order[n++] = login.name;
    if (login.mode == LOGIN_CREATE && EmailConfigured()) order[n++] = login.email;
    if (login.mode == LOGIN_RESET) order[n++] = login.code;
    if (login.mode == LOGIN_RESET_CODE) order[n++] = login.emailCode;
    if (login.mode != LOGIN_RESET_EMAIL) order[n++] = login.pass;
    if (login.mode != LOGIN_SIGNIN && login.mode != LOGIN_RESET_EMAIL) order[n++] = login.confirm;
    *count = n;
    return order;
}

static const char *LoginLabel(const char *buf)
{
    bool reset = IsResetMode(login.mode);
    if (buf == login.name) return "Username";
    if (buf == login.email) return "Email";
    if (buf == login.code) return "Recovery code";
    if (buf == login.emailCode) return "Code from the email";
    if (buf == login.pass) return reset ? "New password" : "Password";
    return reset ? "Confirm new password" : "Confirm password";
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
    WIPE(login.emailCode);
}

static void SetLoginMode(LoginMode mode)
{
    login.mode = mode;
    login.error = NULL;
    login.notice[0] = '\0';
    if (mode != LOGIN_RESET_CODE) login.sending = false;
    WipeLoginSecrets();
    FocusFirstEmpty();
}

void LoginReset(void)
{
    snprintf(login.name, sizeof login.name, "%s", AccountLastUser());
    SetLoginMode(AccountCount() == 0 ? LOGIN_CREATE : LOGIN_SIGNIN);
}

static void SetError(const char *fmt, const char *detail)
{
    snprintf(login.errorBuf, sizeof login.errorBuf, fmt, detail);
    login.error = login.errorBuf;
}

// "ada.lovelace@example.com" -> "a***@example.com", so the screen doesn't reveal the address.
static void MaskEmail(const char *addr, char *out, size_t size)
{
    const char *at = strchr(addr, '@');
    if (!at || at == addr) {
        snprintf(out, size, "your email");
        return;
    }
    snprintf(out, size, "%c***%s", addr[0], at);
}

// Creates a reset code for the account's verified address and starts emailing it.
static void SendResetCode(void)
{
    char addr[EMAIL_MAX + 1], code[EMAIL_CODE_SIZE];
    const char *err = NULL;
    login.notice[0] = '\0';
    if (!AccountStartPasswordReset(login.name, addr, code, &err)) {
        login.error = err;
        return;
    }
    EmailSendCode(EMAIL_RESET, addr, login.name, code);
    WIPE(code);
    MaskEmail(addr, login.resetTo, sizeof login.resetTo);
    login.error = NULL;
    login.sending = true;
    login.resendAt = GetTime() + RESEND_COOLDOWN;
    login.mode = LOGIN_RESET_CODE;
    WIPE(login.emailCode);
    UiFocus(login.emailCode);
}

static void PollResetSend(void)
{
    if (!login.sending) return;
    EmailState s = EmailUpdate();
    if (s == EMAIL_SENT) {
        login.sending = false;
        snprintf(login.notice, sizeof login.notice, "%s", EmailOutboxPath()[0]
                 ? "Code saved to the local outbox. No email was sent."
                 : "Code sent. Check your inbox (and spam folder).");
    } else if (s == EMAIL_FAILED) {
        login.sending = false;
        login.resendAt = 0;  // let them try again straight away
        SetError("Couldn't send the email: %s", EmailError());
    }
}

static bool SubmitLogin(void)
{
    const char *err = NULL;
    bool ok = false;
    bool needEmail = login.mode == LOGIN_CREATE && EmailConfigured();
    bool confirms = login.mode == LOGIN_CREATE || login.mode == LOGIN_RESET || login.mode == LOGIN_RESET_CODE;

    if (login.mode == LOGIN_RESET_EMAIL) {
        SendResetCode();
        return false;
    }
    if (needEmail && !EmailAddressValid(login.email)) {
        err = "Enter a valid email address. We'll send it a code.";
    } else if (confirms && strcmp(login.pass, login.confirm) != 0) {
        err = "The passwords don't match.";
    } else if (login.mode == LOGIN_CREATE) {
        ok = AccountCreate(login.name, login.pass, recovery.code, &err);
        if (ok && needEmail) VerifySendTo(login.email);  // the verify screen shows the result
    } else if (login.mode == LOGIN_RESET) {
        ok = AccountResetPassword(login.name, login.code, login.pass, recovery.code, &err);
    } else if (login.mode == LOGIN_RESET_CODE) {
        ok = AccountResetPasswordWithEmail(login.name, login.emailCode, login.pass, &err);
    } else {
        ok = AccountLogin(login.name, login.pass, &err);
    }

    if (ok) {
        // A new account or a recovery-code reset issues a fresh recovery code to show.
        recovery.pending = login.mode == LOGIN_CREATE || login.mode == LOGIN_RESET;
        recovery.copied = false;
        WipeLoginSecrets();
        login.error = NULL;
        login.notice[0] = '\0';
        login.sending = false;
        UiFocus(NULL);
        ApplySettings();
        return true;
    }
    login.error = err;
    login.notice[0] = '\0';
    WIPE(login.pass);
    WIPE(login.confirm);
    if (login.mode == LOGIN_RESET_CODE) WIPE(login.emailCode);
    FocusFirstEmpty();
    return false;
}

// One centred link; returns true when clicked.
static bool CenteredLink(float cx, float y, const char *text)
{
    float lw = MeasureTextEx(gFonts.text, text, 18, 0).x;
    return UiLink(cx - lw / 2, y, text, 18);
}

AccountAction LoginScreen(void)
{
    static const char *buttonLabel[] = { "Sign in", "Create account", "Reset password", "Email me a code",
                                         "Reset password" };
    PollResetSend();

    int count;
    char **fields = LoginFields(&count);
    float cx = GetScreenWidth() / 2.0f;
    float cardW = 460, pad = 32, w = cardW - pad * 2;

    // Optional lines above the fields (reset steps) and below them (error or notice).
    char info[300] = "";
    if (login.mode == LOGIN_RESET_EMAIL) {
        snprintf(info, sizeof info, "Enter your username and we'll email a reset code to its verified address.");
    } else if (login.mode == LOGIN_RESET_CODE) {
        snprintf(info, sizeof info, EmailOutboxPath()[0] ? "Development mode: find the 6-digit code in your local outbox for %s."
                 : login.sending ? "Sending a 6-digit code to %s..." : "We sent a 6-digit code to %s.",
                 login.resetTo);
    }
    const char *message = login.error ? login.error : login.notice;
    float infoH = info[0] ? UiWrappedText(gFonts.text, info, (Vector2){ 0, 0 }, w, 18, COL_TEXT, false) + 14 : 0;
    float msgH = message[0] ? UiWrappedText(gFonts.text, message, (Vector2){ 0, 0 }, w, 17, COL_TEXT, false) + 10 : 0;
    const char *emailMode = EmailOutboxPath()[0]
        ? "Development mode: codes are saved locally.\nNo email is sent. Use your launcher's outbox folder."
        : EmailConfigured() ? "Email verification enabled.\nWe'll email you a code to confirm your address."
        : "Email verification is unavailable on this installation.\nYou can create a local account without email.";
    float footerH = login.mode == LOGIN_CREATE
        ? UiWrappedText(gFonts.text, emailMode, (Vector2){ 0, 0 }, w, 16, COL_DIM, false) + 30 : 48;
    float fixedH = 28 + 64 + infoH + msgH + 50 + footerH + 20;  // everything but the fields
    float cardH = fixedH + count * 90;

    // Full layout: title, subtitle, card, Quit. Short windows drop the subtitle and Quit
    // and tighten the fields so even the four-field reset form fits at the minimum height.
    float full = 146 + cardH + 74;
    bool compact = full > GetScreenHeight();
    float labelGap = compact ? 22 : 26, fieldH = compact ? 42 : 48, fieldGap = compact ? 10 : 16;
    if (compact && login.mode == LOGIN_CREATE) {
        fieldH = 40;
        fieldGap = 8; // leave room for the email-mode explanation and validation errors
    }
    cardH = fixedH + count * (labelGap + fieldH + fieldGap);
    float top = compact ? 16 : (GetScreenHeight() - full) / 2.0f;
    if (compact) {
        DrawLogoLockup(cx, top, 48);
        top += 72;
    } else {
        DrawLogoLockup(cx, top, 70);
        UiTextCentered(gFonts.text, "Learn C and C++, one step at a time.", cx, top + 94, 24, COL_DIM);
        top += 146;
    }
    Rectangle card = { cx - cardW / 2, top, cardW, cardH };
    UiRoundRect(card, 14, COL_PANEL);

    float x = card.x + pad;
    float y = card.y + 28;
    LoginMode startMode = login.mode;

    if (IsResetMode(login.mode)) {
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
    if (info[0]) y += UiWrappedText(gFonts.text, info, (Vector2){ x, y }, w, 18, COL_TEXT, true) + 14;

    // Fields may change if a tab was clicked above.
    fields = LoginFields(&count);
    bool submit = false;
    for (int i = 0; i < count; i++) {
        char *buf = fields[i];
        bool secret = buf == login.pass || buf == login.confirm;
        int size = buf == login.name ? (int)sizeof login.name
                 : buf == login.email ? (int)sizeof login.email
                 : buf == login.code ? (int)sizeof login.code
                 : buf == login.emailCode ? (int)sizeof login.emailCode : (int)sizeof login.pass;
        UiText(gFonts.text, LoginLabel(buf), x, y, 18, COL_DIM);
        y += labelGap;
        const char *hint = buf == login.code ? "XXXX-XXXX-XXXX-XXXX"
                         : buf == login.email ? "you@example.com"
                         : buf == login.emailCode ? "123456" : "";
        if (UiTextBox((Rectangle){ x, y, w, fieldH }, buf, size, hint, secret)) {
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

    if (message[0]) {
        y += UiWrappedText(gFonts.text, message, (Vector2){ x, y - 4 }, w, 17, login.error ? COL_BAD : COL_GOOD, true) + 10;
    }
    bool busy = login.mode == LOGIN_RESET_EMAIL && login.sending;
    if (UiButton((Rectangle){ x, y, w, 50 }, buttonLabel[login.mode], !busy)) submit = true;
    y += 50;

    // The row of links under the button depends on the step.
    float linkY = y + 16;
    switch (login.mode) {
    case LOGIN_SIGNIN:
        if (CenteredLink(cx, linkY, "Forgot your password?")) SetLoginMode(EmailConfigured() ? LOGIN_RESET_EMAIL : LOGIN_RESET);
        break;
    case LOGIN_CREATE:
        UiWrappedText(gFonts.text, emailMode, (Vector2){ x, y + 14 }, w, 16, COL_DIM, true);
        break;
    case LOGIN_RESET:
        if (EmailConfigured()) {
            if (CenteredLink(cx, linkY, "Email me a code instead")) SetLoginMode(LOGIN_RESET_EMAIL);
        } else {
            UiTextCentered(gFonts.text, "Use the recovery code you saved when signing up.", cx, y + 18, 16, COL_DIM);
        }
        break;
    case LOGIN_RESET_EMAIL:
        if (CenteredLink(cx, linkY, "Use a recovery code instead")) SetLoginMode(LOGIN_RESET);
        break;
    case LOGIN_RESET_CODE: {
        double wait = login.resendAt - GetTime();
        if (login.sending || wait > 0) {
            const char *msg = login.sending ? "Sending..." : TextFormat("Send again in %d s", (int)wait + 1);
            UiText(gFonts.text, msg, x, linkY, 17, COL_DIM);
        } else if (UiLink(x, linkY, "Send a new code", 17)) {
            SendResetCode();
        }
        const char *other = "Use a recovery code instead";
        float ow = MeasureTextEx(gFonts.text, other, 17, 0).x;
        if (UiLink(x + w - ow, linkY, other, 17)) SetLoginMode(LOGIN_RESET);
    } break;
    }
    if (login.mode != startMode) submit = false;  // a link changed the step this frame

    bool quit = !compact && UiButtonIconEx((Rectangle){ cx - 70, card.y + cardH + 28, 140, 46 }, ICON_POWER, "Quit",
                                           20, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true);
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
    float y = GetScreenHeight() / 2.0f - 190;  // leaves room for the key badge above the title

    DrawCircleV((Vector2){ cx, y - 66 }, 42, COL_NOTE_BG);
    IconDraw(ICON_KEY, (Vector2){ cx, y - 66 }, 48, COL_ACCENT);
    UiTextCentered(gFonts.bold, "Save your recovery code", cx, y, 40, COL_TEXT);
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
    if (UiButtonIconEx((Rectangle){ cx - bw - gap / 2, y, bw, 52 }, recovery.copied ? ICON_CHECK : ICON_COPY,
                       recovery.copied ? "Copied" : "Copy code", 22,
                   COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true)) {
        SetClipboardText(recovery.code);
        recovery.copied = true;
    }
    if (UiButtonIcon((Rectangle){ cx + gap / 2, y, bw, 52 }, ICON_CHECK, "I've saved it", true)) {
        WIPE(recovery.code);
        recovery.pending = false;
        return ACCOUNT_BACK;
    }
    return ACCOUNT_STAY;
}

// ---------------------------------------------------------------------------
// Email verification


static struct {
    bool changing;        // opened from Settings to change a verified address
    bool askEmail;        // asking for an address rather than a code
    bool sending;
    char email[EMAIL_MAX + 1];
    char code[16];
    char error[300];
    char notice[200];
    double resendAt;
} verify;

void VerifySendTo(const char *email)
{
    char code[EMAIL_CODE_SIZE];
    const char *err = NULL;
    verify.error[0] = verify.notice[0] = '\0';
    if (!AccountStartEmailVerification(email, code, &err)) {
        snprintf(verify.error, sizeof verify.error, "%s", err);
        verify.askEmail = true;
        return;
    }
    EmailSendCode(EMAIL_VERIFY, email, AccountCurrent(), code);
    WIPE(code);
    verify.sending = true;
    verify.askEmail = false;
    verify.resendAt = GetTime() + RESEND_COOLDOWN;
    WIPE(verify.code);
    UiFocus(verify.code);
}

void VerifyOpen(bool changing)
{
    verify.changing = changing;
    verify.error[0] = verify.notice[0] = '\0';
    WIPE(verify.code);
    verify.email[0] = '\0';
    if (changing) {
        verify.askEmail = true;
    } else if (verify.sending || AccountCodeActive()) {
        verify.askEmail = false;  // a code is on its way or already in their inbox
    } else if (AccountPendingEmail()[0]) {
        VerifySendTo(AccountPendingEmail());  // the old code expired: send a fresh one
    } else {
        verify.askEmail = true;   // older accounts have no address yet
    }
    UiFocus(verify.askEmail ? (void *)verify.email : (void *)verify.code);
}

static void PollSend(void)
{
    if (!verify.sending) return;
    EmailState s = EmailUpdate();
    if (s == EMAIL_SENT) {
        verify.sending = false;
        snprintf(verify.notice, sizeof verify.notice, "%s", EmailOutboxPath()[0]
                 ? "Code saved to the local outbox. No email was sent."
                 : "Code sent. Check your inbox (and spam folder).");
    } else if (s == EMAIL_FAILED) {
        verify.sending = false;
        snprintf(verify.error, sizeof verify.error, "Couldn't send the email: %s", EmailError());
        verify.resendAt = 0;  // let them try again straight away
    }
}

AccountAction VerifyScreen(void)
{
    PollSend();
    float cx = GetScreenWidth() / 2.0f;
    float cardW = 480, pad = 32, x0 = cx - cardW / 2, w = cardW - pad * 2;
    const char *pending = AccountPendingEmail();

    const char *title = verify.askEmail ? (verify.changing ? "Change your email" : "Add your email")
                                      : EmailOutboxPath()[0] ? "Check your local outbox" : "Check your email";
    char body[400];
    if (verify.askEmail) {
        snprintf(body, sizeof body, "%s We'll send a 6-digit code to confirm the address is yours.",
                 verify.changing ? "Enter your new address." : "Your account needs a verified email address.");
    } else if (EmailOutboxPath()[0]) {
        snprintf(body, sizeof body, "Development mode: open the outbox text file named after your username to read the 6-digit code. It expires in 15 minutes.");
    } else if (verify.sending) {
        snprintf(body, sizeof body, "Sending a 6-digit code to %s...", pending);
    } else {
        snprintf(body, sizeof body, "We sent a 6-digit code to %s. It expires in 15 minutes.", pending);
    }
    const char *message = verify.error[0] ? verify.error : verify.notice;
    float bodyH = UiWrappedText(gFonts.text, body, (Vector2){ 0, 0 }, w, 19, COL_TEXT, false);
    float msgH = message[0] ? UiWrappedText(gFonts.text, message, (Vector2){ 0, 0 }, w, 17, COL_TEXT, false) + 8 : 0;
    float cardH = 28 + 70 + 44 + bodyH + 20 + 26 + 50 + 16 + msgH + 50 + 20 + 30 + 24;
    float top = (GetScreenHeight() - (cardH + 110)) / 2.0f;
    if (top < 12) top = 12;
    DrawLogoLockup(cx, top, 52);
    Rectangle card = { x0, top + 82, cardW, cardH };
    UiRoundRect(card, 14, COL_PANEL);

    float x = card.x + pad, y = card.y + 28;
    DrawCircleV((Vector2){ cx, y + 30 }, 30, COL_NOTE_BG);
    IconDraw(ICON_MAIL, (Vector2){ cx, y + 30 }, 34, COL_ACCENT);
    y += 70;
    UiTextCentered(gFonts.bold, title, cx, y, 26, COL_TEXT);
    y += 44;
    y += UiWrappedText(gFonts.text, body, (Vector2){ x, y }, w, 19, COL_TEXT, true) + 20;

    bool submit = false;
    UiText(gFonts.text, verify.askEmail ? "Email" : "Verification code", x, y, 18, COL_DIM);
    y += 26;
    if (verify.askEmail) {
        submit = UiTextBox((Rectangle){ x, y, w, 50 }, verify.email, sizeof verify.email, "you@example.com", false);
    } else {
        submit = UiTextBox((Rectangle){ x, y, w, 50 }, verify.code, sizeof verify.code, "123456", false);
    }
    y += 50 + 16;
    if (message[0]) {
        y += UiWrappedText(gFonts.text, message, (Vector2){ x, y }, w, 17, verify.error[0] ? COL_BAD : COL_GOOD, true) + 8;
    }

    const char *label = verify.askEmail ? "Send code" : "Verify";
    if (UiButtonIcon((Rectangle){ x, y, w, 50 }, verify.askEmail ? ICON_MAIL : ICON_CHECK, label, !verify.sending)) {
        submit = true;
    }
    y += 50 + 20;

    AccountAction action = ACCOUNT_STAY;
    if (submit && !verify.sending) {
        if (verify.askEmail) {
            VerifySendTo(verify.email);
        } else {
            const char *err = NULL;
            if (AccountVerifyEmail(verify.code, &err)) {
                WIPE(verify.code);
                UiFocus(NULL);
                action = ACCOUNT_VERIFIED;
            } else {
                snprintf(verify.error, sizeof verify.error, "%s", err);
                verify.notice[0] = '\0';
                WIPE(verify.code);
                UiFocus(verify.code);
            }
        }
    }

    // Links: resend / change address, then sign out or cancel.
    if (!verify.askEmail) {
        double wait = verify.resendAt - GetTime();
        const char *again = "Send a new code";
        const char *other = "Use a different email";
        float ow = MeasureTextEx(gFonts.text, other, 17, 0).x;
        if (wait > 0 || verify.sending) {
            const char *msg = verify.sending ? "Sending..." : TextFormat("Send again in %d s", (int)wait + 1);
            UiText(gFonts.text, msg, x, y, 17, COL_DIM);
        } else if (UiLink(x, y, again, 17)) {
            VerifySendTo(pending);
        }
        if (UiLink(x + w - ow, y, other, 17)) {
            verify.askEmail = true;
            verify.error[0] = verify.notice[0] = '\0';
            UiFocus(verify.email);
        }
    }
    y += 30;
    const char *leave = verify.changing ? "Cancel" : "Sign out";
    if (EmailOutboxPath()[0] && UiLink(x, y, "Copy outbox path", 17)) SetClipboardText(EmailOutboxPath());
    float lw = MeasureTextEx(gFonts.text, leave, 17, 0).x;
    if (action == ACCOUNT_STAY && UiLink(cx - lw / 2, y, leave, 17)) {
        if (verify.changing) AccountCancelEmailChange();
        UiFocus(NULL);
        action = verify.changing ? ACCOUNT_BACK : ACCOUNT_SIGNED_OUT;
    }
    return action;
}

bool VerifyChanging(void) { return verify.changing; }

bool VerifyNeeded(void) { return AccountCurrent() && EmailConfigured() && !AccountEmailVerified(); }

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

static void Message(const char *msg, bool ok, float x, float *y)
{
    if (!msg) return;
    UiText(gFonts.text, msg, x, *y, 18, ok ? COL_GOOD : COL_BAD);
    *y += 32;
}

static bool DangerButton(Rectangle r, const char *label)
{
    return UiButtonIconEx(r, ICON_TRASH, label, 20, COL_BAD, ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true);
}

static float AppearanceSection(float x, float y, float w)
{
    static const char *themes[] = { "Dark", "Light" };
    static const char *sizes[] = { "Small", "Medium", "Large" };
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
    Heading(ICON_LOCK, "Change password", x, &y);
    bool go = false;
    if (PasswordField("Current password", settings.current, x, &y, fieldW)) UiFocus(settings.next);
    if (PasswordField("New password", settings.next, x, &y, fieldW)) UiFocus(settings.confirm);
    if (PasswordField("Confirm new password", settings.confirm, x, &y, fieldW)) go = true;
    Message(settings.passwordMsg, settings.passwordOk, x, &y);
    if (UiButtonIcon((Rectangle){ x, y, 260, 48 }, ICON_LOCK, "Change password", true)) go = true;
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
    Heading(ICON_KEY, "Recovery code", x, &y);
    bool has = AccountHasRecovery();
    y += UiWrappedText(gFonts.text,
                       has ? "You have a recovery code. If you've lost it, make a new one and the old code will stop working."
                           : "You don't have a recovery code yet. Make one so you can reset a forgotten password.",
                       (Vector2){ x, y }, w, 20, has ? COL_DIM : COL_BAD, true) + 14;

    bool go = PasswordField("Password", settings.codePass, x, &y, fieldW);
    Message(settings.codeMsg, false, x, &y);
    if (UiButtonIcon((Rectangle){ x, y, 260, 48 }, ICON_KEY, "Make a new code", true)) go = true;
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
    Heading(ICON_TRASH, "Delete account", x, &y);
    y += UiWrappedText(gFonts.text, "This permanently deletes your account, progress and settings from this computer.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 14;

    if (!settings.confirmDelete) {
        bool go = PasswordField("Password", settings.deletePass, x, &y, fieldW);
        Message(settings.deleteMsg, false, x, &y);
        if (DangerButton((Rectangle){ x, y, 260, 48 }, "Delete account")) go = true;
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

static float EmailSection(float x, float y, float w, AccountAction *action)
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
    (void)w;
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
    if (EmailConfigured()) y = EmailSection(x, y + 48, w, &action);
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
    if (strcmp(what, "register") == 0) {
        snprintf(login.email, sizeof login.email, "ada@example.com");
    } else if (strcmp(what, "verify") == 0) {  // code mode, after a (test outbox) send
        VerifyOpen(false);
        VerifySendTo("ada@example.com");
        snprintf(verify.code, sizeof verify.code, "4829");
    } else if (strcmp(what, "addemail") == 0) {
        VerifyOpen(false);
        verify.askEmail = true;
    } else if (strcmp(what, "resetemail") == 0) {  // forgot password, step 1
        snprintf(login.name, sizeof login.name, "ada");
        SetLoginMode(LOGIN_RESET_EMAIL);
    } else if (strcmp(what, "resetcode") == 0) {   // step 2, after a (test outbox) send
        snprintf(login.name, sizeof login.name, "ada");
        SetLoginMode(LOGIN_RESET_EMAIL);
        SendResetCode();
        snprintf(login.emailCode, sizeof login.emailCode, "4829");
    }
}
