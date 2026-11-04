// Email verification: ask for an address, email it a 6-digit code, and check the code.
#include "account_screens.h"
#include "account_screens_shared.h"
#include "account.h"
#include "email.h"
#include "layout.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

// The screen has two modes: asking for an address (askEmail) and asking for the code.
static struct {
    bool changing;              // opened from Settings to change a verified address
    bool askEmail;              // asking for an address rather than a code
    bool sending;               // a code email is on its way
    char email[EMAIL_MAX + 1];  // the address field
    char code[16];              // the code field
    char error[300];            // shown in red ("" for none)
    char notice[200];           // shown in green ("" for none)
    double resendAt;            // GetTime() when "Send a new code" is allowed again
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

// Called every frame: turns the background send's outcome into a notice or an error.
static void PollSend(void)
{
    if (!verify.sending) return;
    EmailState s = EmailUpdate();
    if (s == EMAIL_SENT) {
        verify.sending = false;
        snprintf(verify.notice, sizeof verify.notice, "%s", CodeSentNotice());
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
            UiText(gFonts.text, ResendWaitText(verify.sending, wait), x, y, 17, COL_DIM);
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


void VerifyDemo(const char *what)
{
    if (strcmp(what, "verify") == 0) {  // code mode, after a (test outbox) send
        VerifyOpen(false);
        VerifySendTo("ada@example.com");
        snprintf(verify.code, sizeof verify.code, "4829");
    } else if (strcmp(what, "addemail") == 0) {  // asking for an address
        VerifyOpen(false);
        verify.askEmail = true;
    }
}
