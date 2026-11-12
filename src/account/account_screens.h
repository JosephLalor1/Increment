#ifndef ACCOUNT_SCREENS_H
#define ACCOUNT_SCREENS_H

#include <stdbool.h>

// The account screens: sign-in (login_screen.c), email verification (verify_screen.c),
// recovery code (recovery_screen.c) and settings (settings_screen.c). They never navigate:
// each returns an AccountAction and main.c decides where to go next.

typedef enum {
    ACCOUNT_STAY,          // nothing to do
    ACCOUNT_SIGNED_IN,     // a user just signed in (check RecoveryPending for a new code to show)
    ACCOUNT_SIGNED_OUT,    // the account was deleted, or the user signed out
    ACCOUNT_SHOW_CODE,     // a new recovery code is waiting to be shown
    ACCOUNT_VERIFIED,      // the email address was just verified
    ACCOUNT_CHANGE_EMAIL,  // settings: open the verify screen to change the address
    ACCOUNT_DATA_RESTORED, // settings: a backup replaced the user's files; load them again
    ACCOUNT_BACK,          // leave this screen
    ACCOUNT_QUIT,
} AccountAction;

// Sign-in
void LoginReset(void);          // fresh sign-in form (call when showing the login screen)
AccountAction LoginScreen(void);

// Email verification. Required only when email sending is configured (EmailConfigured).
bool VerifyNeeded(void);              // signed in, email configured, address not verified
void VerifyOpen(bool changing);       // changing: from Settings, to replace a verified address
void VerifySendTo(const char *email); // creates a code and starts emailing it
bool VerifyChanging(void);
AccountAction VerifyScreen(void);     // ACCOUNT_VERIFIED, ACCOUNT_SIGNED_OUT or ACCOUNT_BACK

// Recovery code
bool RecoveryPending(void);           // a new code is waiting to be shown
AccountAction RecoveryScreen(void);   // ACCOUNT_BACK once the user has saved the code

// Settings
void SettingsOpen(void);        // clear the settings forms (call when entering or leaving)
AccountAction SettingsScreen(void);

// Applies the current user's (or last user's) theme, accent and text size to the UI.
// Call after signing in or out and whenever the settings change.
void ApplyUserAppearance(void);

// Dev aid for --shot: "register", "reset", "recovery", "delete", "verify", "addemail",
// "resetemail", "resetcode" or "backup" (the settings' Backup section, asking before a restore) pre-fills demo state on the matching screen.
void AccountScreensDemo(const char *what);

#endif
