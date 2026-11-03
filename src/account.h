#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdbool.h>

#define USERNAME_MIN 3
#define USERNAME_MAX 20
#define PASSWORD_MIN 6
#define PASSWORD_MAX 64
#define RECOVERY_CODE_SIZE 20 // "XXXX-XXXX-XXXX-XXXX" plus terminator

typedef enum { THEME_DARK, THEME_LIGHT } ThemeMode;
typedef enum { TEXT_SMALL, TEXT_MEDIUM, TEXT_LARGE, TEXT_SIZE_COUNT } TextSize;

typedef struct {
    ThemeMode theme;
    int accent;        // 0 .. UI_ACCENT_COUNT-1
    TextSize textSize; // lesson and quiz text
} AccountSettings;

// Local accounts stored in <dataDir>/accounts.txt; each user's progress lives in
// <dataDir>/users/<lowercased name>.txt and settings in .settings.txt beside it.
// Passwords and recovery codes are stored only as salted PBKDF2-SHA256 hashes.
void AccountsInit(const char *dataDir);
int AccountCount(void);

// Functions returning bool fill err (a static string) on failure.
// Create and reset log the user in and write a fresh one-time recovery code to recoveryOut.
bool AccountCreate(const char *name, const char *password, char recoveryOut[RECOVERY_CODE_SIZE], const char **err);
bool AccountLogin(const char *name, const char *password, const char **err);
bool AccountResetPassword(const char *name, const char *recoveryCode, const char *newPassword,
                          char recoveryOut[RECOVERY_CODE_SIZE], const char **err);
void AccountLogout(void);

// These act on the logged-in user and require their current password.
bool AccountChangePassword(const char *currentPassword, const char *newPassword, const char **err);
bool AccountNewRecoveryCode(const char *password, char recoveryOut[RECOVERY_CODE_SIZE], const char **err);
bool AccountDelete(const char *password, const char **err); // also deletes progress and settings; logs out

const char *AccountCurrent(void);       // display name, or NULL when logged out
bool AccountHasRecovery(void);          // whether the logged-in user has a recovery code
const char *AccountProgressPath(void);  // progress file for the current user
const char *AccountLastUser(void);      // last user to log in, "" if none

// The current user's settings; when logged out, the last user's (so the login screen
// keeps their theme) or defaults. Set saves immediately when someone is logged in.
AccountSettings AccountGetSettings(void);
void AccountSetSettings(AccountSettings s);

#endif
