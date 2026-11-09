#ifndef ACCOUNT_H
#define ACCOUNT_H

// Local user accounts: creating, signing in, passwords, recovery codes, email verification,
// password reset by email, and per-user settings.

#include "email_address.h"  // EMAIL_MAX

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
    bool codeAmbience; // faint code characters behind the journey (on by default)
    bool mouseTrail;   // code characters trailing the mouse on the journey (on by default)
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
bool AccountDelete(const char *password, const char **err); // also deletes progress, settings, bookmarks, review history, skill XP, notes and drafts; logs out

// Email verification for the logged-in user. Codes are 6 digits, stored hashed, valid
// for 15 minutes and 5 attempts. Sending the email is the caller's job (see email.h).
#define EMAIL_CODE_SIZE 7   // 6 digits + terminator
bool AccountEmailVerified(void);          // has a verified address
const char *AccountEmail(void);           // verified address, or ""
const char *AccountPendingEmail(void);    // address waiting for its code, or ""
bool AccountCodeActive(void);             // a code was sent and can still be used
bool AccountStartEmailVerification(const char *email, char codeOut[EMAIL_CODE_SIZE], const char **err);
bool AccountVerifyEmail(const char *code, const char **err);
void AccountCancelEmailChange(void);      // drop a pending change, keeping the verified address

// Password reset by email, while signed out. Start emails nothing itself: it creates a
// reset code for the account's verified address and returns both for the caller to send.
// The reset signs the user in. Recovery codes (AccountResetPassword) still work too.
bool AccountStartPasswordReset(const char *name, char emailOut[EMAIL_MAX + 1], char codeOut[EMAIL_CODE_SIZE],
                               const char **err);
bool AccountResetPasswordWithEmail(const char *name, const char *code, const char *newPassword, const char **err);
void AccountSetClockForTests(long long unixTime);  // 0 = real clock

const char *AccountCurrent(void);       // display name, or NULL when logged out
bool AccountHasRecovery(void);          // whether the logged-in user has a recovery code
const char *AccountProgressPath(void);  // progress file for the current user
const char *AccountBookmarksPath(void); // saved-lessons file for the current user, "" when logged out
const char *AccountReviewPath(void);    // answer history / review schedule file (review.h), "" when logged out
const char *AccountSkillsPath(void);    // skill XP file (skills.h), "" when logged out
const char *AccountCodeDir(void);       // folder for the current user's code editor drafts (created), or NULL
const char *AccountNotesDir(void);      // folder for the current user's lesson notes (created), or NULL
const char *AccountLastUser(void);      // last user to log in, "" if none

// The current user's settings; when logged out, the last user's (so the login screen
// keeps their theme) or defaults. Set saves immediately when someone is logged in.
AccountSettings AccountGetSettings(void);
void AccountSetSettings(AccountSettings s);

#endif
