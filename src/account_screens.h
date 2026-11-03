#ifndef ACCOUNT_SCREENS_H
#define ACCOUNT_SCREENS_H

#include <stdbool.h>

// Sign-in, recovery-code and settings screens. main.c owns navigation and acts on
// the returned action.
typedef enum {
    ACCOUNT_STAY,        // nothing to do
    ACCOUNT_SIGNED_IN,   // a user just signed in (check RecoveryPending for a new code to show)
    ACCOUNT_SIGNED_OUT,  // the account was deleted
    ACCOUNT_SHOW_CODE,   // a new recovery code is waiting to be shown
    ACCOUNT_BACK,        // leave this screen
    ACCOUNT_QUIT,
} AccountAction;

void LoginReset(void);          // fresh sign-in form (call when showing the login screen)
AccountAction LoginScreen(void);

bool RecoveryPending(void);
AccountAction RecoveryScreen(void); // ACCOUNT_BACK once the user has saved the code

void SettingsOpen(void);        // clear the settings forms (call when entering or leaving)
AccountAction SettingsScreen(void);

void ApplySettings(void);       // apply the current user's theme
float TextScale(void);          // lesson/quiz text size multiplier for the current user

// Dev aid for --shot: "register", "reset", "recovery" or "delete" pre-fills demo state.
void AccountScreensDemo(const char *what);

#endif
