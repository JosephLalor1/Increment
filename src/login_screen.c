// The sign-in screen and its modes: sign in, create an account, and reset a forgotten
// password by email code or recovery code.
#include "account_screens.h"
#include "account_screens_shared.h"
#include "account.h"
#include "email.h"
#include "email_address.h"
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

// The form's state. Each text field's buffer is also its focus identity (see UiTextBox).
static struct {
    LoginMode mode;
    char name[USERNAME_MAX + 1];
    char email[EMAIL_MAX + 1];             // create mode, when email is configured
    char password[PASSWORD_MAX + 1];       // the new password in the reset modes
    char confirmPassword[PASSWORD_MAX + 1];
    char recoveryCode[32];                 // reset with a recovery code
    char emailCode[16];                    // code from a password reset email
    const char *error;                     // shown in red under the fields, or NULL
    char errorText[320];                   // storage for errors built at run time (e.g. from the email service)
    char notice[160];                      // shown in green under the fields ("" for none)
    char maskedResetEmail[EMAIL_MAX + 8];  // "a***@example.com": where the reset code went
    bool sending;                          // a reset email is on its way
    bool remember;                         // "Remember me on this computer" is ticked
    double resendAt;                       // GetTime() when "Send a new code" is allowed again
} login;

// ---------------------------------------------------------------------------
// Form fields

static bool IsResetMode(LoginMode m) { return m == LOGIN_RESET || m == LOGIN_RESET_EMAIL || m == LOGIN_RESET_CODE; }

// Signing in and creating an account offer "Remember me"; the reset steps don't.
static bool OffersRemember(LoginMode m) { return m == LOGIN_SIGNIN || m == LOGIN_CREATE; }
#define REMEMBER_ROW_H 38
#define REMEMBER_ROW_COMPACT_H 30

// The text fields the current mode shows, top to bottom (also the Tab order).
static char **LoginFields(int *count)
{
    static char *order[5];
    int n = 0;
    if (login.mode != LOGIN_RESET_CODE) order[n++] = login.name;
    if (login.mode == LOGIN_CREATE && EmailConfigured()) order[n++] = login.email;
    if (login.mode == LOGIN_RESET) order[n++] = login.recoveryCode;
    if (login.mode == LOGIN_RESET_CODE) order[n++] = login.emailCode;
    if (login.mode != LOGIN_RESET_EMAIL) order[n++] = login.password;
    if (login.mode != LOGIN_SIGNIN && login.mode != LOGIN_RESET_EMAIL) order[n++] = login.confirmPassword;
    *count = n;
    return order;
}

static const char *LoginLabel(const char *buf)
{
    bool reset = IsResetMode(login.mode);
    if (buf == login.name) return "Username";
    if (buf == login.email) return "Email";
    if (buf == login.recoveryCode) return "Recovery code";
    if (buf == login.emailCode) return "Code from the email";
    if (buf == login.password) return reset ? "New password" : "Password";
    return reset ? "Confirm new password" : "Confirm password";
}

// Puts the caret in the first empty field, or the last field when all are filled.
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
    WIPE(login.password);
    WIPE(login.confirmPassword);
    WIPE(login.recoveryCode);
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

// Shows an error built from a format with one %s, such as a message from the email service.
static void SetError(const char *format, const char *detail)
{
    snprintf(login.errorText, sizeof login.errorText, format, detail);
    login.error = login.errorText;
}

// ---------------------------------------------------------------------------
// Password reset by email

// Creates a reset code for the account's verified address and starts emailing it.
static void SendResetCode(void)
{
    char address[EMAIL_MAX + 1], code[EMAIL_CODE_SIZE];
    const char *err = NULL;
    login.notice[0] = '\0';
    if (!AccountStartPasswordReset(login.name, address, code, &err)) {
        login.error = err;
        return;
    }
    EmailSendCode(EMAIL_RESET, address, login.name, code);
    WIPE(code);
    EmailAddressMask(address, login.maskedResetEmail, sizeof login.maskedResetEmail);
    login.error = NULL;
    login.sending = true;
    login.resendAt = GetTime() + RESEND_COOLDOWN;
    login.mode = LOGIN_RESET_CODE;
    WIPE(login.emailCode);
    UiFocus(login.emailCode);
}

// Called every frame: turns the background send's outcome into a notice or an error.
static void PollResetSend(void)
{
    if (!login.sending) return;
    EmailState s = EmailUpdate();
    if (s == EMAIL_SENT) {
        login.sending = false;
        snprintf(login.notice, sizeof login.notice, "%s", CodeSentNotice());
    } else if (s == EMAIL_FAILED) {
        login.sending = false;
        login.resendAt = 0;  // let them try again straight away
        SetError("Couldn't send the email: %s", EmailError());
    }
}

// ---------------------------------------------------------------------------
// Submitting

// Acts on the form for the current mode. Returns true once the user is signed in.
static bool SubmitLogin(void)
{
    const char *err = NULL;
    bool ok = false;
    char newRecoveryCode[RECOVERY_CODE_SIZE] = "";  // issued by create and recovery-code reset
    bool needEmail = login.mode == LOGIN_CREATE && EmailConfigured();
    bool confirms = login.mode == LOGIN_CREATE || login.mode == LOGIN_RESET || login.mode == LOGIN_RESET_CODE;

    if (login.mode == LOGIN_RESET_EMAIL) {
        SendResetCode();
        return false;
    }
    if (needEmail && !EmailAddressValid(login.email)) {
        err = "Enter a valid email address. We'll send it a code.";
    } else if (confirms && strcmp(login.password, login.confirmPassword) != 0) {
        err = "The passwords don't match.";
    } else if (login.mode == LOGIN_CREATE) {
        ok = AccountCreate(login.name, login.password, newRecoveryCode, &err);
        if (ok && needEmail) VerifySendTo(login.email);  // the verify screen shows the result
    } else if (login.mode == LOGIN_RESET) {
        ok = AccountResetPassword(login.name, login.recoveryCode, login.password, newRecoveryCode, &err);
    } else if (login.mode == LOGIN_RESET_CODE) {
        ok = AccountResetPasswordWithEmail(login.name, login.emailCode, login.password, &err);
    } else {
        ok = AccountLogin(login.name, login.password, &err);
    }

    if (ok) {
        // Remember the sign-in on this computer if asked; otherwise forget any older one.
        const char *rememberErr;
        if (OffersRemember(login.mode) && login.remember) AccountRememberMe(&rememberErr);
        else AccountForgetMe();
        // A new account or a recovery-code reset issues a fresh recovery code to show.
        bool issuedCode = login.mode == LOGIN_CREATE || login.mode == LOGIN_RESET;
        if (issuedCode) RecoveryOffer(newRecoveryCode);
        else RecoveryDismiss();
        WIPE(newRecoveryCode);
        WipeLoginSecrets();
        login.error = NULL;
        login.notice[0] = '\0';
        login.sending = false;
        UiFocus(NULL);
        ApplyUserAppearance();  // the signed-in user's theme and text size
        return true;
    }
    WIPE(newRecoveryCode);
    login.error = err;
    login.notice[0] = '\0';
    WIPE(login.password);
    WIPE(login.confirmPassword);
    if (login.mode == LOGIN_RESET_CODE) WIPE(login.emailCode);
    FocusFirstEmpty();
    return false;
}

// ---------------------------------------------------------------------------
// Drawing

// One centred link; returns true when clicked.
static bool CenteredLink(float cx, float y, const char *text)
{
    float linkW = MeasureTextEx(gFonts.text, text, 18, 0).x;
    return UiLink(cx - linkW / 2, y, text, 18);
}

// The explanation above the fields in the two email-reset steps ("" in other modes).
static void ResetInfoText(char *out, size_t size)
{
    out[0] = '\0';
    if (login.mode == LOGIN_RESET_EMAIL) {
        snprintf(out, size, "Enter your username and we'll email a reset code to its verified address.");
    } else if (login.mode == LOGIN_RESET_CODE) {
        const char *format = EmailOutboxPath()[0] ? "Development mode: find the 6-digit code in your local outbox for %s."
                           : login.sending        ? "Sending a 6-digit code to %s..."
                                                  : "We sent a 6-digit code to %s.";
        snprintf(out, size, format, login.maskedResetEmail);
    }
}

// Shown under the create form: whether this installation sends real emails, writes them to
// a local outbox, or has no email at all.
static const char *EmailModeText(void)
{
    if (EmailOutboxPath()[0]) {
        return "Development mode: codes are saved locally.\nNo email is sent. Use your launcher's outbox folder.";
    }
    if (EmailConfigured()) return "Email verification enabled.\nWe'll email you a code to confirm your address.";
    return "Email verification is unavailable on this installation.\nYou can create a local account without email.";
}

// Sign in / Create account tabs, or the reset title with a way back.
static void DrawModeHeader(float x, float y, float w)
{
    if (IsResetMode(login.mode)) {
        UiText(gFonts.text, "Reset your password", x, y + 4, 26, COL_TEXT);
        const char *back = "Back to sign in";
        float backW = MeasureTextEx(gFonts.text, back, 18, 0).x;
        if (UiLink(x + w - backW, y + 10, back, 18)) SetLoginMode(LOGIN_SIGNIN);
        return;
    }
    float tabW = (w - 8) / 2;
    for (int i = 0; i < 2; i++) {
        bool isCreateTab = i == 1;
        bool active = (login.mode == LOGIN_CREATE) == isCreateTab;
        Rectangle tab = { x + i * (tabW + 8), y, tabW, 40 };
        bool clicked = UiButtonEx(tab, isCreateTab ? "Create account" : "Sign in", 20,
                                  active ? COL_ACCENT : COL_PANEL_HI, active ? COL_ACCENT : COL_SECONDARY_HOVER,
                                  active ? COL_ON_ACCENT : COL_TEXT, true);
        if (clicked && !active) SetLoginMode(isCreateTab ? LOGIN_CREATE : LOGIN_SIGNIN);
    }
}

static int FieldCapacity(const char *field)
{
    if (field == login.name) return (int)sizeof login.name;
    if (field == login.email) return (int)sizeof login.email;
    if (field == login.recoveryCode) return (int)sizeof login.recoveryCode;
    if (field == login.emailCode) return (int)sizeof login.emailCode;
    return (int)sizeof login.password;  // both password fields
}

// Placeholder text showing the expected format.
static const char *FieldHint(const char *field)
{
    if (field == login.recoveryCode) return "XXXX-XXXX-XXXX-XXXX";
    if (field == login.email) return "you@example.com";
    if (field == login.emailCode) return "123456";
    return "";
}

typedef struct {
    float labelGap, fieldH, fieldGap;  // vertical spacing for each labelled field
} FieldSpacing;

// Draws each field with its label from *y. Returns true when Enter is pressed in the last
// field (Enter in an earlier field moves to the next). Tab / Shift+Tab cycle the fields.
static bool DrawFields(float x, float *y, float w, FieldSpacing spacing)
{
    int count;
    char **fields = LoginFields(&count);
    bool submit = false;
    for (int i = 0; i < count; i++) {
        char *field = fields[i];
        bool secret = field == login.password || field == login.confirmPassword;
        UiText(gFonts.text, LoginLabel(field), x, *y, 18, COL_DIM);
        *y += spacing.labelGap;
        if (UiTextBox((Rectangle){ x, *y, w, spacing.fieldH }, field, FieldCapacity(field), FieldHint(field), secret)) {
            if (i + 1 < count) UiFocus(fields[i + 1]);
            else submit = true;
        }
        *y += spacing.fieldH + spacing.fieldGap;
    }

    if (IsKeyPressed(KEY_TAB)) {
        int focused = 0;
        for (int i = 0; i < count; i++) {
            if (UiHasFocus(fields[i])) focused = i;
        }
        bool backwards = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        UiFocus(fields[(focused + (backwards ? count - 1 : 1)) % count]);
    }
    return submit;
}

// The row under the main button, which depends on the step: "Forgot your password?",
// the email-mode explanation, switches between the two reset methods, or "Send a new code".
static void DrawModeLinks(float cx, float x, float y, float w)
{
    float linkY = y + 16;
    switch (login.mode) {
    case LOGIN_SIGNIN:
        if (CenteredLink(cx, linkY, "Forgot your password?")) {
            SetLoginMode(EmailConfigured() ? LOGIN_RESET_EMAIL : LOGIN_RESET);
        }
        break;
    case LOGIN_CREATE:
        UiWrappedText(gFonts.text, EmailModeText(), (Vector2){ x, y + 14 }, w, 16, COL_DIM, true);
        break;
    case LOGIN_RESET:
        if (!EmailConfigured()) {
            UiTextCentered(gFonts.text, "Use the recovery code you saved when signing up.", cx, y + 18, 16, COL_DIM);
        } else if (CenteredLink(cx, linkY, "Email me a code instead")) {
            SetLoginMode(LOGIN_RESET_EMAIL);
        }
        break;
    case LOGIN_RESET_EMAIL:
        if (CenteredLink(cx, linkY, "Use a recovery code instead")) SetLoginMode(LOGIN_RESET);
        break;
    case LOGIN_RESET_CODE: {
        double secondsLeft = login.resendAt - GetTime();
        if (login.sending || secondsLeft > 0) {
            UiText(gFonts.text, ResendWaitText(login.sending, secondsLeft), x, linkY, 17, COL_DIM);
        } else if (UiLink(x, linkY, "Send a new code", 17)) {
            SendResetCode();
        }
        const char *other = "Use a recovery code instead";
        float otherW = MeasureTextEx(gFonts.text, other, 17, 0).x;
        if (UiLink(x + w - otherW, linkY, other, 17)) SetLoginMode(LOGIN_RESET);
    } break;
    }
}

AccountAction LoginScreen(void)
{
    static const char *buttonLabel[] = { "Sign in", "Create account", "Reset password", "Email me a code",
                                         "Reset password" };  // indexed by LoginMode
    PollResetSend();

    int fieldCount;
    LoginFields(&fieldCount);
    float cx = GetScreenWidth() / 2.0f;
    float cardW = 460, pad = 32, w = cardW - pad * 2;

    // Measure the optional text: info above the fields, error or notice below them, and the
    // footer under the button. Everything but the fields has a fixed height.
    char info[300];
    ResetInfoText(info, sizeof info);
    const char *message = login.error ? login.error : login.notice;
    float infoH = info[0] ? UiWrappedText(gFonts.text, info, (Vector2){ 0, 0 }, w, 18, COL_TEXT, false) + 14 : 0;
    float messageH = message[0] ? UiWrappedText(gFonts.text, message, (Vector2){ 0, 0 }, w, 17, COL_TEXT, false) + 10 : 0;
    float footerH = login.mode == LOGIN_CREATE
        ? UiWrappedText(gFonts.text, EmailModeText(), (Vector2){ 0, 0 }, w, 16, COL_DIM, false) + 30 : 48;
    float rememberH = OffersRemember(login.mode) ? REMEMBER_ROW_H : 0;
    float rememberRowH = REMEMBER_ROW_H;  // the row drawn under the fields (tighter when compact)
    float fixedH = 28 + 64 + infoH + messageH + rememberH + 50 + footerH + 20;

    // Full layout: logo, subtitle, card, Quit. Short windows drop the subtitle and Quit and
    // tighten the fields, so even the four-field reset form fits at the minimum height.
    float fullH = 146 + (fixedH + fieldCount * 90) + 74;
    bool compact = fullH > GetScreenHeight();
    FieldSpacing spacing = compact ? (FieldSpacing){ 22, 42, 10 } : (FieldSpacing){ 26, 48, 16 };
    if (compact && login.mode == LOGIN_CREATE) {
        spacing.labelGap = 20;
        spacing.fieldH = 38;
        spacing.fieldGap = 8;  // leave room for the email-mode explanation, validation errors and "Remember me"
    }
    if (compact && OffersRemember(login.mode)) {  // a tighter "Remember me" row too
        fixedH -= REMEMBER_ROW_H - REMEMBER_ROW_COMPACT_H;
        rememberRowH = REMEMBER_ROW_COMPACT_H;
    }
    float cardH = fixedH + fieldCount * (spacing.labelGap + spacing.fieldH + spacing.fieldGap);
    float top = compact ? 16 : (GetScreenHeight() - fullH) / 2.0f;
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
    LoginMode modeAtStart = login.mode;

    DrawModeHeader(x, y, w);  // may change the mode, and so the fields below
    y += 40 + 24;
    if (info[0]) y += UiWrappedText(gFonts.text, info, (Vector2){ x, y }, w, 18, COL_TEXT, true) + 14;

    bool submit = DrawFields(x, &y, w, spacing);
    if (message[0]) {
        Color color = login.error ? COL_BAD : COL_GOOD;
        y += UiWrappedText(gFonts.text, message, (Vector2){ x, y - 4 }, w, 17, color, true) + 10;
    }
    if (OffersRemember(login.mode)) {
        UiCheckbox(x, y - 2, "Remember me on this computer", 18, &login.remember);
        y += rememberRowH;
    }
    bool waitingToSend = login.mode == LOGIN_RESET_EMAIL && login.sending;
    if (UiButton((Rectangle){ x, y, w, 50 }, buttonLabel[login.mode], !waitingToSend)) submit = true;
    y += 50;

    DrawModeLinks(cx, x, y, w);
    if (login.mode != modeAtStart) submit = false;  // a tab or link changed the step this frame

    bool quit = !compact && UiButtonIconEx((Rectangle){ cx - 70, card.y + cardH + 28, 140, 46 }, ICON_POWER, "Quit",
                                           20, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true);
    if (submit && SubmitLogin()) return ACCOUNT_SIGNED_IN;
    return quit ? ACCOUNT_QUIT : ACCOUNT_STAY;
}

// ---------------------------------------------------------------------------

void LoginDemo(const char *what)
{
    if (strcmp(what, "register") == 0) {  // create form showing a mismatch error
        login.mode = LOGIN_CREATE;
        snprintf(login.name, sizeof login.name, "ada");
        snprintf(login.email, sizeof login.email, "ada@example.com");
        snprintf(login.password, sizeof login.password, "secret1");
        login.error = "The passwords don't match.";
        UiFocus(login.confirmPassword);
    } else if (strcmp(what, "reset") == 0) {  // recovery-code reset: its tallest state
        login.mode = LOGIN_RESET;
        snprintf(login.name, sizeof login.name, "ada");
        snprintf(login.recoveryCode, sizeof login.recoveryCode, "K7QM-");
        login.error = "Wrong username or recovery code.";
        UiFocus(login.recoveryCode);
    } else if (strcmp(what, "resetemail") == 0) {  // forgot password, step 1
        snprintf(login.name, sizeof login.name, "ada");
        SetLoginMode(LOGIN_RESET_EMAIL);
    } else if (strcmp(what, "resetcode") == 0) {  // step 2, after a (test outbox) send
        snprintf(login.name, sizeof login.name, "ada");
        SetLoginMode(LOGIN_RESET_EMAIL);
        SendResetCode();
        snprintf(login.emailCode, sizeof login.emailCode, "4829");
    }
}
