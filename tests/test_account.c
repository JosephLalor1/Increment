// Tests for src/account.c. Run via test.bat; uses a throwaway data directory.
#include "../src/account.h"
#include "raylib.h"

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

int main(int argc, char **argv)
{
    SetTraceLogLevel(LOG_WARNING);
    if (argc < 2) {
        printf("usage: test_account <empty temp dir>\n");
        return 2;
    }
    snprintf(dir, sizeof dir, "%s/", argv[1]);
    const char *err = NULL;

    // Legacy progress from before accounts existed should move to the first account.
    WriteFile(DataFile("progress.txt"), "c_01_hello 100\n");

    AccountsInit(dir);
    CHECK(AccountCount() == 0);
    CHECK(AccountCurrent() == NULL);

    // Validation
    CHECK(!AccountCreate("ab", "password", &err));            // name too short
    CHECK(!AccountCreate("bad name", "password", &err));      // space
    CHECK(!AccountCreate("../evil", "password", &err));       // path characters
    CHECK(!AccountCreate("abcdefghijklmnopqrstu", "password", &err)); // 21 chars
    CHECK(!AccountCreate("ada", "short", &err));               // password too short
    CHECK(AccountCount() == 0);

    // Create logs in, migrates legacy progress
    CHECK(AccountCreate("Ada", "correct horse", &err));
    CHECK(AccountCurrent() && strcmp(AccountCurrent(), "Ada") == 0);
    CHECK(strstr(AccountProgressPath(), "users/ada.txt") != NULL);
    CHECK(FileExists(DataFile("users/ada.txt")));
    CHECK(!FileExists(DataFile("progress.txt")));

    // Duplicate names are case-insensitive
    CHECK(!AccountCreate("ADA", "another pass", &err));
    CHECK(err && strstr(err, "taken"));

    // A second account does not get migrated progress
    WriteFile(DataFile("progress.txt"), "c_02_variables 100\n");
    CHECK(AccountCreate("grace_h", "hopper1906", &err));
    CHECK(!FileExists(DataFile("users/grace_h.txt")));
    CHECK(AccountCount() == 2);

    // The password is not stored in plain text
    char *accountsText = LoadFileText(DataFile("accounts.txt"));
    CHECK(accountsText && !strstr(accountsText, "correct horse") && !strstr(accountsText, "hopper1906"));
    UnloadFileText(accountsText);

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

    // Same password, different accounts: salts make the stored hashes differ
    CHECK(AccountCreate("twin1", "samepass", &err));
    CHECK(AccountCreate("twin2", "samepass", &err));
    accountsText = LoadFileText(DataFile("accounts.txt"));
    char hash1[80] = "", hash2[80] = "";
    const char *l1 = strstr(accountsText, "twin1 "), *l2 = strstr(accountsText, "twin2 ");
    CHECK(l1 && l2);
    if (l1 && l2) {
        sscanf(l1, "%*s %*s %*s %79s", hash1);
        sscanf(l2, "%*s %*s %*s %79s", hash2);
        CHECK(strlen(hash1) == 64 && strcmp(hash1, hash2) != 0);
    }
    UnloadFileText(accountsText);

    // Corrupt lines are skipped, not fatal
    WriteFile(DataFile("accounts.txt"), "garbage\nzed 1000 nothex nothex\n");
    AccountsInit(dir);
    CHECK(AccountCount() == 0);

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All account tests passed\n");
    return 0;
}
