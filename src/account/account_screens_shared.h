#ifndef ACCOUNT_SCREENS_SHARED_H
#define ACCOUNT_SCREENS_SHARED_H

// Private to the account screens (login_screen.c, verify_screen.c, recovery_screen.c,
// settings_screen.c): what they share with each other. Everything else uses account_screens.h.

#include "account/account.h"  // RECOVERY_CODE_SIZE
#include "core/wipe.h"     // WIPE, for every buffer that held a password or code

#include <stdbool.h>

#define RESEND_COOLDOWN 30.0  // seconds between "Send a new code" presses

// Status text once a code email has gone out: names the local outbox in development mode.
const char *CodeSentNotice(void);

// The text shown in place of the "Send a new code" link while it's unavailable:
// "Sending..." during a send, otherwise "Send again in N s".
const char *ResendWaitText(bool sending, double secondsLeft);

// Recovery screen (recovery_screen.c): queue a freshly issued code to be shown once, or
// drop any queued code (after a sign-in that didn't issue one).
void RecoveryOffer(const char code[RECOVERY_CODE_SIZE]);
void RecoveryDismiss(void);

// Per-screen pieces of AccountScreensDemo; each ignores names that aren't its own.
void LoginDemo(const char *what);
void VerifyDemo(const char *what);
void RecoveryDemo(const char *what);
void SettingsDemo(const char *what);

#endif
