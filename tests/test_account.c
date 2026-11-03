// Tests for src/account.c. Run via test.bat; uses a throwaway data directory.
#include "../src/account.h"
#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while (0)

static char dir[1024];

static const char *DataFile(const char *name) { return TextFormat("%s%s", dir, name); }

static void WriteFile(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    fputs(text, f);
    fclose(f);
}

static bool FileContains(const char *path, const char *needle)
{
    char *text = LoadFileText(path);
    bool found = text && strstr(text, needle);
    UnloadFileText(text);
    return found;
}

static bool LooksLikeCode(const char *code)
{
    if (strlen(code) != 19) return false;
    for (int i = 0; i < 19; i++) {
        if (i % 5 == 4) { if (code[i] != '-') return false; }
        else if (!isupper((unsigned char)code[i]) && !isdigit((unsigned char)code[i])) return false;
    }
    return strpbrk(code, "IO01") == NULL;
}

static void TestCreateAndLogin(void)
{
    const char *err = NULL;
    char code[RECOVERY_CODE_SIZE];

    // Legacy progress from before accounts existed should move to the first account.
    WriteFile(DataFile("progress.txt"), "c_01_hello 100\n");

    AccountsInit(dir);
    CHECK(AccountCount() == 0);
    CHECK(AccountCurrent() == NULL);

    // Validation
    CHECK(!AccountCreate("ab", "password", code, &err));            // name too short
    CHECK(!AccountCreate("bad name", "password", code, &err));      // space
    CHECK(!AccountCreate("../evil", "password", code, &err));       // path characters
    CHECK(!AccountCreate("abcdefghijklmnopqrstu", "password", code, &err)); // 21 chars
    CHECK(!AccountCreate("ada", "short", code, &err));               // password too short
    CHECK(AccountCount() == 0);

    // Create logs in, issues a code, migrates legacy progress
    CHECK(AccountCreate("Ada", "correct horse", code, &err));
    CHECK(LooksLikeCode(code));
    CHECK(AccountHasRecovery());
    CHECK(AccountCurrent() && strcmp(AccountCurrent(), "Ada") == 0);
    CHECK(strstr(AccountProgressPath(), "users/ada.txt") != NULL);
    CHECK(FileExists(DataFile("users/ada.txt")));
    CHECK(!FileExists(DataFile("progress.txt")));

    // Duplicate names are case-insensitive
    CHECK(!AccountCreate("ADA", "another pass", code, &err));
    CHECK(err && strstr(err, "taken"));

    // A second account does not get migrated progress
    WriteFile(DataFile("progress.txt"), "c_02_variables 100\n");
    CHECK(AccountCreate("grace_h", "hopper1906", code, &err));
    CHECK(!FileExists(DataFile("users/grace_h.txt")));
    CHECK(AccountCount() == 2);

    // Neither passwords nor recovery codes are stored in plain text
    CHECK(!FileContains(DataFile("accounts.txt"), "correct horse"));
    CHECK(!FileContains(DataFile("accounts.txt"), "hopper1906"));
    char plainCode[17] = { 0 };
    for (int i = 0, o = 0; code[i]; i++) if (code[i] != '-') plainCode[o++] = code[i];
    CHECK(!FileContains(DataFile("accounts.txt"), plainCode));

    // Reload from disk and log in
    AccountLogout();
    CHECK(AccountCurrent() == NULL);
    AccountsInit(dir);
    CHECK(AccountCount() == 2);
    CHECK(strcmp(AccountLastUser(), "grace_h") == 0);
    CHECK(!AccountLogin("Ada", "wrong password", &err));
    CHECK(AccountCurrent() == NULL);
    CHECK(!AccountLogin("nobody", "correct horse", &err));
    CHECK(!AccountLogin("Ada", "", &err));
    CHECK(AccountLogin("ada", "correct horse", &err)); // username is case-insensitive
    CHECK(AccountCurrent() && strcmp(AccountCurrent(), "Ada") == 0);
    CHECK(strcmp(AccountLastUser(), "Ada") == 0);
    AccountLogout();
}

static void TestRecoveryReset(void)
{
    const char *err = NULL;
    char code[RECOVERY_CODE_SIZE], next[RECOVERY_CODE_SIZE], third[RECOVERY_CODE_SIZE];

    AccountsInit(dir);
    CHECK(AccountCreate("linus", "penguins", code, &err));
    AccountLogout();

    // Wrong code, wrong user, malformed code
    CHECK(!AccountResetPassword("linus", "AAAA-AAAA-AAAA-AAAA", "newpass1", next, &err));
    CHECK(!AccountResetPassword("nobody", code, "newpass1", next, &err));
    CHECK(!AccountResetPassword("linus", "short", "newpass1", next, &err));
    CHECK(AccountCurrent() == NULL);

    // Right code but a too-short new password fails without using up the code
    CHECK(!AccountResetPassword("linus", code, "tiny", next, &err));
    CHECK(AccountLogin("linus", "penguins", &err));
    AccountLogout();

    // Codes are accepted in lowercase and without dashes
    char loose[32];
    int o = 0;
    for (int i = 0; code[i]; i++) if (code[i] != '-') loose[o++] = (char)tolower((unsigned char)code[i]);
    loose[o] = '\0';
    CHECK(AccountResetPassword("LINUS", loose, "kernel42", next, &err));
    CHECK(AccountCurrent() && strcmp(AccountCurrent(), "linus") == 0);
    CHECK(LooksLikeCode(next) && strcmp(next, code) != 0);
    AccountLogout();

    // Old password and old code are dead; new ones work
    CHECK(!AccountLogin("linus", "penguins", &err));
    CHECK(AccountLogin("linus", "kernel42", &err));
    AccountLogout();
    CHECK(!AccountResetPassword("linus", code, "another1", third, &err));
    CHECK(AccountResetPassword("linus", next, "another1", third, &err));

    // Survives a reload
    AccountLogout();
    AccountsInit(dir);
    CHECK(AccountLogin("linus", "another1", &err));
    AccountLogout();
}

static void TestChangePasswordAndNewCode(void)
{
    const char *err = NULL;
    char code[RECOVERY_CODE_SIZE], fresh[RECOVERY_CODE_SIZE], unused[RECOVERY_CODE_SIZE];

    AccountsInit(dir);
    CHECK(!AccountChangePassword("x", "whatever1", &err)); // not logged in
    CHECK(AccountCreate("margaret", "apollo11", code, &err));

    CHECK(!AccountChangePassword("wrong", "newpass1", &err));
    CHECK(!AccountChangePassword("apollo11", "tiny", &err));
    CHECK(AccountChangePassword("apollo11", "moonshot", &err));
    AccountLogout();
    CHECK(!AccountLogin("margaret", "apollo11", &err));
    CHECK(AccountLogin("margaret", "moonshot", &err));

    // A new recovery code replaces the old one
    CHECK(!AccountNewRecoveryCode("wrong", fresh, &err));
    CHECK(AccountNewRecoveryCode("moonshot", fresh, &err));
    CHECK(LooksLikeCode(fresh) && strcmp(fresh, code) != 0);
    AccountLogout();
    CHECK(!AccountResetPassword("margaret", code, "reset123", unused, &err));
    CHECK(AccountResetPassword("margaret", fresh, "reset123", unused, &err));
    AccountLogout();
}

static void TestSettings(void)
{
    const char *err = NULL;
    char code[RECOVERY_CODE_SIZE];

    AccountsInit(dir);
    CHECK(AccountCreate("barbara", "liskov74", code, &err));
    AccountSettings s = AccountGetSettings();
    CHECK(s.theme == THEME_DARK && s.accent == 0 && s.textSize == TEXT_MEDIUM); // defaults for a new user

    s.theme = THEME_LIGHT;
    s.accent = 3;
    s.textSize = TEXT_LARGE;
    AccountSetSettings(s);
    CHECK(FileExists(DataFile("users/barbara.settings.txt")));

    // Another user has their own settings
    AccountLogout();
    CHECK(AccountLogin("Ada", "correct horse", &err));
    CHECK(AccountGetSettings().theme == THEME_DARK);
    AccountLogout();

    // Settings survive a reload, and the login screen gets the last user's theme
    CHECK(AccountLogin("barbara", "liskov74", &err));
    AccountLogout();
    AccountsInit(dir);
    s = AccountGetSettings();
    CHECK(s.theme == THEME_LIGHT && s.accent == 3 && s.textSize == TEXT_LARGE);

    // Out-of-range values in the file fall back to defaults
    WriteFile(DataFile("users/barbara.settings.txt"), "theme 7\naccent -2\ntext 99\n");
    CHECK(AccountLogin("barbara", "liskov74", &err));
    s = AccountGetSettings();
    CHECK(s.theme == THEME_DARK && s.accent == 0 && s.textSize == TEXT_MEDIUM);
    AccountLogout();
}

static void TestDelete(void)
{
    const char *err = NULL;
    char code[RECOVERY_CODE_SIZE];

    AccountsInit(dir);
    CHECK(!AccountDelete("anything", &err)); // not logged in
    CHECK(AccountCreate("temp_user", "deleteme", code, &err));
    WriteFile(DataFile("users/temp_user.txt"), "c_01_hello 100\n");
    AccountSettings s = AccountGetSettings();
    s.theme = THEME_LIGHT;
    AccountSetSettings(s);
    int before = AccountCount();

    CHECK(!AccountDelete("wrong", &err));
    CHECK(AccountCurrent() != NULL);
    CHECK(AccountCount() == before);

    CHECK(AccountDelete("deleteme", &err));
    CHECK(AccountCurrent() == NULL);
    CHECK(AccountCount() == before - 1);
    CHECK(!FileExists(DataFile("users/temp_user.txt")));
    CHECK(!FileExists(DataFile("users/temp_user.settings.txt")));
    CHECK(strcmp(AccountLastUser(), "") == 0);
    CHECK(AccountGetSettings().theme == THEME_DARK);
    CHECK(!AccountLogin("temp_user", "deleteme", &err));

    // Gone after reload too; the name can be reused
    AccountsInit(dir);
    CHECK(AccountCount() == before - 1);
    CHECK(AccountCreate("temp_user", "brandnew1", code, &err));
    AccountLogout();
}

static void TestLegacyAndCorruptFiles(void)
{
    const char *err = NULL;

    // A pre-recovery-code line (4 fields) still loads; this hash is PBKDF2-SHA256("password1", salt 00..0f, 1000).
    WriteFile(DataFile("accounts.txt"),
              "old_user 1000 000102030405060708090a0b0c0d0e0f "
              "5c0ddf289cd56efa1b8bb0608c967ec5bd07fb5037e51a1e5e44dcbbf84f45f3\n");
    AccountsInit(dir);
    CHECK(AccountCount() == 1);
    CHECK(AccountLogin("old_user", "password1", &err));
    CHECK(!AccountHasRecovery());

    // Making a code upgrades the line to the 7-field format, and the password still works after
    char code[RECOVERY_CODE_SIZE];
    CHECK(AccountNewRecoveryCode("password1", code, &err));
    AccountLogout();
    AccountsInit(dir);
    CHECK(AccountLogin("old_user", "password1", &err));
    CHECK(AccountHasRecovery());
    AccountLogout();

    // Garbage and duplicate lines are skipped, not fatal
    WriteFile(DataFile("accounts.txt"), "garbage\nzed 1000 nothex nothex\n");
    AccountsInit(dir);
    CHECK(AccountCount() == 0);
    CHECK(!AccountLogin("zed", "anything", &err));
}

int main(int argc, char **argv)
{
    SetTraceLogLevel(LOG_ERROR);
    if (argc < 2) {
        printf("usage: test_account <empty temp dir>\n");
        return 2;
    }
    snprintf(dir, sizeof dir, "%s/", argv[1]);

    TestCreateAndLogin();
    TestRecoveryReset();
    TestChangePasswordAndNewCode();
    TestSettings();
    TestDelete();
    TestLegacyAndCorruptFiles();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All account tests passed\n");
    return 0;
}
