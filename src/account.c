#include "account.h"
#include "platform.h"
#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_ACCOUNTS    64
#define SALT_LEN        16
#define HASH_ITERATIONS 210000UL // OWASP-recommended floor for PBKDF2-HMAC-SHA256

typedef struct {
    char name[USERNAME_MAX + 1];
    unsigned long iterations;
    unsigned char salt[SALT_LEN];
    unsigned char hash[PASSWORD_HASH_LEN];
} Account;

static Account accounts[MAX_ACCOUNTS];
static int accountCount;
static int current = -1;
static char dataDir[1024];
static char accountsPath[1100];
static char progressPath[1100];
static char lastUser[USERNAME_MAX + 1];

// ---------------------------------------------------------------------------
// Helpers

static void ToHex(const unsigned char *bytes, size_t n, char *out)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 15];
    }
    out[n * 2] = '\0';
}

static int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool FromHex(const char *hex, unsigned char *out, size_t n)
{
    if (strlen(hex) != n * 2) return false;
    for (size_t i = 0; i < n; i++) {
        int hi = HexDigit(hex[i * 2]), lo = HexDigit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (unsigned char)(hi * 16 + lo);
    }
    return true;
}

static bool ValidName(const char *name)
{
    size_t n = strlen(name);
    if (n < USERNAME_MIN || n > USERNAME_MAX) return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_' && c != '-') return false;
    }
    return true;
}

static void CopyName(char *dst, const char *src)
{
    size_t n = strlen(src);
    if (n > USERNAME_MAX) n = USERNAME_MAX;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool SameName(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    }
    return *a == *b;
}

static int FindAccount(const char *name)
{
    for (int i = 0; i < accountCount; i++) {
        if (SameName(accounts[i].name, name)) return i;
    }
    return -1;
}

// Constant-time so comparison timing doesn't leak how many hash bytes matched.
static bool HashesEqual(const unsigned char *a, const unsigned char *b)
{
    unsigned char diff = 0;
    for (int i = 0; i < PASSWORD_HASH_LEN; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

static const char *UserProgressPath(const char *name)
{
    static char path[1100];
    char lower[USERNAME_MAX + 1];
    size_t n = strlen(name);
    for (size_t i = 0; i <= n; i++) lower[i] = (char)tolower((unsigned char)name[i]);
    snprintf(path, sizeof path, "%susers/%s.txt", dataDir, lower);
    return path;
}

// ---------------------------------------------------------------------------
// Storage: "@last <name>" plus one "<name> <iterations> <salt hex> <hash hex>" line per account.

static void LoadAccounts(void)
{
    accountCount = 0;
    lastUser[0] = '\0';
    FILE *f = fopen(accountsPath, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        char name[64], saltHex[128], hashHex[128];
        unsigned long iterations;
        if (sscanf(line, "@last %63s", name) == 1) {
            if (ValidName(name)) CopyName(lastUser, name);
            continue;
        }
        if (sscanf(line, "%63s %lu %127s %127s", name, &iterations, saltHex, hashHex) != 4) continue;
        if (accountCount >= MAX_ACCOUNTS || !ValidName(name) || iterations == 0) continue;

        Account *a = &accounts[accountCount];
        if (!FromHex(saltHex, a->salt, SALT_LEN) || !FromHex(hashHex, a->hash, PASSWORD_HASH_LEN)) {
            TraceLog(LOG_WARNING, "ACCOUNT: skipping malformed entry for %s", name);
            continue;
        }
        CopyName(a->name, name);
        a->iterations = iterations;
        accountCount++;
    }
    fclose(f);
}

static bool SaveAccounts(void)
{
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s.tmp", accountsPath);
    FILE *f = fopen(tmp, "w");
    if (!f) return false;

    if (lastUser[0]) fprintf(f, "@last %s\n", lastUser);
    for (int i = 0; i < accountCount; i++) {
        char saltHex[SALT_LEN * 2 + 1], hashHex[PASSWORD_HASH_LEN * 2 + 1];
        ToHex(accounts[i].salt, SALT_LEN, saltHex);
        ToHex(accounts[i].hash, PASSWORD_HASH_LEN, hashHex);
        fprintf(f, "%s %lu %s %s\n", accounts[i].name, accounts[i].iterations, saltHex, hashHex);
    }
    bool ok = fflush(f) == 0;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !PlatformReplaceFile(tmp, accountsPath)) {
        remove(tmp);
        TraceLog(LOG_WARNING, "ACCOUNT: could not write %s", accountsPath);
        return false;
    }
    return true;
}

static void SetCurrent(int index)
{
    current = index;
    CopyName(lastUser, accounts[index].name);
    snprintf(progressPath, sizeof progressPath, "%s", UserProgressPath(accounts[index].name));
    MakeDirectory(TextFormat("%susers", dataDir));
    SaveAccounts(); // remembers lastUser; failure here isn't fatal
}

// ---------------------------------------------------------------------------

void AccountsInit(const char *dir)
{
    snprintf(dataDir, sizeof dataDir, "%s", dir);
    snprintf(accountsPath, sizeof accountsPath, "%saccounts.txt", dataDir);
    current = -1;
    LoadAccounts();
}

int AccountCount(void) { return accountCount; }

bool AccountCreate(const char *name, const char *password, const char **err)
{
    if (!ValidName(name)) {
        *err = "Usernames are 3-20 letters, digits, - or _.";
        return false;
    }
    if (FindAccount(name) >= 0) {
        *err = "That username is taken.";
        return false;
    }
    size_t plen = strlen(password);
    if (plen < PASSWORD_MIN || plen > PASSWORD_MAX) {
        *err = "Passwords need 6 to 64 characters.";
        return false;
    }
    if (accountCount >= MAX_ACCOUNTS) {
        *err = "This computer has reached the account limit.";
        return false;
    }

    Account a = { 0 };
    CopyName(a.name, name);
    a.iterations = HASH_ITERATIONS;
    if (!PlatformRandom(a.salt, SALT_LEN) || !PlatformHashPassword(password, a.salt, SALT_LEN, a.iterations, a.hash)) {
        *err = "Could not secure the password. Please try again.";
        return false;
    }

    accounts[accountCount++] = a;
    if (!SaveAccounts()) {
        accountCount--;
        *err = "Could not save the account file.";
        return false;
    }

    // The first account inherits progress saved before accounts existed.
    if (accountCount == 1) {
        const char *legacy = TextFormat("%sprogress.txt", dataDir);
        const char *dest = UserProgressPath(a.name);
        MakeDirectory(TextFormat("%susers", dataDir));
        if (FileExists(legacy) && !FileExists(dest)) PlatformReplaceFile(legacy, dest);
    }

    SetCurrent(accountCount - 1);
    return true;
}

bool AccountLogin(const char *name, const char *password, const char **err)
{
    int i = FindAccount(name);
    unsigned char hash[PASSWORD_HASH_LEN];
    if (i < 0 || !PlatformHashPassword(password, accounts[i].salt, SALT_LEN, accounts[i].iterations, hash)
        || !HashesEqual(hash, accounts[i].hash)) {
        *err = "Wrong username or password.";
        return false;
    }
    SetCurrent(i);
    return true;
}

void AccountLogout(void) { current = -1; }

const char *AccountCurrent(void) { return current >= 0 ? accounts[current].name : NULL; }

const char *AccountProgressPath(void) { return progressPath; }

const char *AccountLastUser(void) { return lastUser; }
