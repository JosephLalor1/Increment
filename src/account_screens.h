#ifndef ACCOUNT_SCREENS_H
#define ACCOUNT_SCREENS_H

#include <stdbool.h>

// Sign-in, email verification, recovery-code and settings screens. main.c owns
// navigation and acts on the returned action.
typedef enum {
    ACCOUNT_STAY,          // nothing to do
    ACCOUNT_SIGNED_IN,     // a user just signed in (check RecoveryPending for a new code to show)
    ACCOUNT_SIGNED_OUT,    // the account was deleted, or the user signed out
    ACCOUNT_SHOW_CODE,     // a new recovery code is waiting to be shown
    ACCOUNT_VERIFIED,      // the email address was just verified
    ACCOUNT_CHANGE_EMAIL,  // settings: open the verify screen to change the address
    ACCOUNT_BACK,          // leave this screen
    ACCOUNT_QUIT,
} AccountAction;

void LoginReset(void);          // fresh sign-in form (call when showing the login screen)
AccountAction LoginScreen(void);

// Email verification. Required only when email sending is configured (EmailConfigured).
bool VerifyNeeded(void);              // signed in, email configured, address not verified
void VerifyOpen(bool changing);       // changing: from Settings, to replace a verified address
void VerifySendTo(const char *email); // creates a code and starts emailing it
bool VerifyChanging(void);
AccountAction VerifyScreen(void);     // ACCOUNT_VERIFIED, ACCOUNT_SIGNED_OUT or ACCOUNT_BACK

bool RecoveryPending(void);
AccountAction RecoveryScreen(void); // ACCOUNT_BACK once the user has saved the code

void SettingsOpen(void);        // clear the settings forms (call when entering or leaving)
AccountAction SettingsScreen(void);

void ApplySettings(void);       // apply the current user's theme
float TextScale(void);          // lesson/quiz text size multiplier for the current user

// Dev aid for --shot: "register", "reset", "recovery", "delete", "verify" or
// "addemail", "resetemail" or "resetcode" pre-fills demo state.
void AccountScreensDemo(const char *what);

#endif
