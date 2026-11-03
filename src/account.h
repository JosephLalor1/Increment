#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdbool.h>

#define USERNAME_MIN 3
#define USERNAME_MAX 20
#define PASSWORD_MIN 6
#define PASSWORD_MAX 64

// Local accounts stored in <dataDir>/accounts.txt; each user's progress lives in
// <dataDir>/users/<lowercased name>.txt. Passwords are salted PBKDF2-SHA256 hashes.
void AccountsInit(const char *dataDir);
int AccountCount(void);

// Both return false and fill err (a static string) on failure. On success the user is logged in.
bool AccountCreate(const char *name, const char *password, const char **err);
bool AccountLogin(const char *name, const char *password, const char **err);
void AccountLogout(void);

const char *AccountCurrent(void);       // display name, or NULL when logged out
const char *AccountProgressPath(void);  // progress file for the current user
const char *AccountLastUser(void);      // last user to log in, "" if none

#endif
