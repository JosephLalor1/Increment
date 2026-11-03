#include "account.h"
#include "platform.h"
#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_ACCOUNTS    64
#define SALT_LEN        16
#define HASH_ITERATIONS 210000UL // OWASP-recommended floor for PBKDF2-HMAC-SHA256
#define CODE_CHARS      16       // recovery code length without dashes: 16 x 5 bits = 80 bits

// A salted, iterated hash of a password or recovery code.
typedef struct {
    unsigned long iterations;
    unsigned char salt[SALT_LEN];
    unsigned char hash[PASSWORD_HASH_LEN];
} Secret;

typedef struct {
    char name[USERNAME_MAX + 1];
    Secret password;
    bool hasRecovery;
    Secret recovery;
} Account;

static const AccountSettings defaultSettings = { THEME_DARK, 0, TEXT_MEDIUM };

static Account accounts[MAX_ACCOUNTS];
static int accountCount;
static int current = -1;
static AccountSettings settings;
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

static bool ValidPassword(const char *password, const char **err)
{
    size_t n = strlen(password);
    if (n < PASSWORD_MIN || n > PASSWORD_MAX) {
        *err = "Passwords need 6 to 64 characters.";
        return false;
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

static bool MakeSecret(const char *plain, Secret *out)
{
    out->iterations = HASH_ITERATIONS;
    return PlatformRandom(out->salt, SALT_LEN)
        && PlatformHashPassword(plain, out->salt, SALT_LEN, out->iterations, out->hash);
}

static bool CheckSecret(const char *plain, const Secret *s)
{
    unsigned char hash[PASSWORD_HASH_LEN];
    if (!PlatformHashPassword(plain, s->salt, SALT_LEN, s->iterations, hash)) return false;
    // Constant-time so comparison timing doesn't leak how many hash bytes matched.
    unsigned char diff = 0;
    for (int i = 0; i < PASSWORD_HASH_LEN; i++) diff |= hash[i] ^ s->hash[i];
    return diff == 0;
}

// Recovery codes use 32 symbols with no look-alikes (no I, O, 0 or 1).
static const char codeAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

// Strips dashes and spaces and uppercases, so "abcd efgh-..." matches "ABCD-EFGH-...".
static bool NormalizeCode(const char *in, char out[CODE_CHARS + 1])
{
    int n = 0;
    for (; *in; in++) {
        if (*in == '-' || *in == ' ') continue;
        if (n == CODE_CHARS) return false;
        out[n++] = (char)toupper((unsigned char)*in);
    }
    out[n] = '\0';
    return n == CODE_CHARS;
}

static bool NewRecoveryCode(Account *a, char out[RECOVERY_CODE_SIZE])
{
    unsigned char bytes[CODE_CHARS];
    char plain[CODE_CHARS + 1];
    if (!PlatformRandom(bytes, sizeof bytes)) return false;
    for (int i = 0; i < CODE_CHARS; i++) plain[i] = codeAlphabet[bytes[i] & 31]; // 32 symbols: no modulo bias
    plain[CODE_CHARS] = '\0';
    if (!MakeSecret(plain, &a->recovery)) return false;
    a->hasRecovery = true;

    int o = 0;
    for (int i = 0; i < CODE_CHARS; i++) {
        if (i && i % 4 == 0) out[o++] = '-';
        out[o++] = plain[i];
    }
    out[o] = '\0';
    return true;
}

static const char *UserFile(const char *name, const char *suffix)
{
    static char path[1100];
    char lower[USERNAME_MAX + 1];
    size_t n = strlen(name);
    for (size_t i = 0; i <= n; i++) lower[i] = (char)tolower((unsigned char)name[i]);
    snprintf(path, sizeof path, "%susers/%s%s", dataDir, lower, suffix);
    return path;
}

static bool WriteFileAtomic(const char *path, const char *text)
{
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return false;
    bool ok = fputs(text, f) >= 0;
    ok = (fflush(f) == 0) && ok;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !PlatformReplaceFile(tmp, path)) {
        remove(tmp);
        TraceLog(LOG_WARNING, "ACCOUNT: could not write %s", path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Storage. accounts.txt holds "@last <name>" and one line per account:
//   <name> <iterations> <salt> <hash> [<recovery iterations> <recovery salt> <recovery hash>]

static void SecretToText(const Secret *s, char *out, size_t size)
{
    char saltHex[SALT_LEN * 2 + 1], hashHex[PASSWORD_HASH_LEN * 2 + 1];
    ToHex(s->salt, SALT_LEN, saltHex);
    ToHex(s->hash, PASSWORD_HASH_LEN, hashHex);
    snprintf(out, size, "%lu %s %s", s->iterations, saltHex, hashHex);
}

static bool SecretFromText(unsigned long iterations, const char *saltHex, const char *hashHex, Secret *out)
{
    out->iterations = iterations;
    return iterations > 0 && FromHex(saltHex, out->salt, SALT_LEN) && FromHex(hashHex, out->hash, PASSWORD_HASH_LEN);
}

static void LoadAccounts(void)
{
    accountCount = 0;
    lastUser[0] = '\0';
    FILE *f = fopen(accountsPath, "r");
    if (!f) return;

    char line[600];
    while (fgets(line, sizeof line, f)) {
        char name[64], salt[128], hash[128], rsalt[128], rhash[128];
        unsigned long iter, riter;
        if (sscanf(line, "@last %63s", name) == 1) {
            if (ValidName(name)) CopyName(lastUser, name);
            continue;
        }
        int fields = sscanf(line, "%63s %lu %127s %127s %lu %127s %127s", name, &iter, salt, hash, &riter, rsalt, rhash);
        if (fields != 4 && fields != 7) continue;
        if (accountCount >= MAX_ACCOUNTS || !ValidName(name) || FindAccount(name) >= 0) continue;

        Account *a = &accounts[accountCount];
        memset(a, 0, sizeof *a);
        if (!SecretFromText(iter, salt, hash, &a->password)) {
            TraceLog(LOG_WARNING, "ACCOUNT: skipping malformed entry for %s", name);
            continue;
        }
        if (fields == 7) a->hasRecovery = SecretFromText(riter, rsalt, rhash, &a->recovery);
        CopyName(a->name, name);
        accountCount++;
    }
    fclose(f);
}

static bool SaveAccounts(void)
{
    static char text[MAX_ACCOUNTS * 400 + 64];
    size_t len = 0;
    if (lastUser[0]) len += (size_t)snprintf(text + len, sizeof text - len, "@last %s\n", lastUser);
    for (int i = 0; i < accountCount; i++) {
        char pw[200], rec[200] = "";
        SecretToText(&accounts[i].password, pw, sizeof pw);
        if (accounts[i].hasRecovery) {
            rec[0] = ' ';
            SecretToText(&accounts[i].recovery, rec + 1, sizeof rec - 1);
        }
        len += (size_t)snprintf(text + len, sizeof text - len, "%s %s%s\n", accounts[i].name, pw, rec);
    }
    return WriteFileAtomic(accountsPath, text);
}

static AccountSettings LoadSettings(const char *name)
{
    AccountSettings s = defaultSettings;
    FILE *f = fopen(UserFile(name, ".settings.txt"), "r");
    if (!f) return s;
    char key[32];
    int value;
    while (fscanf(f, "%31s %d", key, &value) == 2) {
        if (strcmp(key, "theme") == 0 && (value == THEME_DARK || value == THEME_LIGHT)) s.theme = (ThemeMode)value;
        else if (strcmp(key, "accent") == 0 && value >= 0 && value < 16) s.accent = value;
        else if (strcmp(key, "text") == 0 && value >= 0 && value < TEXT_SIZE_COUNT) s.textSize = (TextSize)value;
    }
    fclose(f);
    return s;
}

static void SetCurrent(int index)
{
    current = index;
    CopyName(lastUser, accounts[index].name);
    snprintf(progressPath, sizeof progressPath, "%s", UserFile(accounts[index].name, ".txt"));
    MakeDirectory(TextFormat("%susers", dataDir));
    settings = LoadSettings(accounts[index].name);
    SaveAccounts(); // remembers lastUser; failure here isn't fatal
}

static bool RequireLogin(const char **err)
{
    if (current < 0) {
        *err = "You are not signed in.";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

void AccountsInit(const char *dir)
{
    snprintf(dataDir, sizeof dataDir, "%s", dir);
    snprintf(accountsPath, sizeof accountsPath, "%saccounts.txt", dataDir);
    current = -1;
    LoadAccounts();
    settings = (lastUser[0] && FindAccount(lastUser) >= 0) ? LoadSettings(lastUser) : defaultSettings;
}

int AccountCount(void) { return accountCount; }

bool AccountCreate(const char *name, const char *password, char recoveryOut[RECOVERY_CODE_SIZE], const char **err)
{
    if (!ValidName(name)) {
        *err = "Usernames are 3-20 letters, digits, - or _.";
        return false;
    }
    if (FindAccount(name) >= 0) {
        *err = "That username is taken.";
        return false;
    }
    if (!ValidPassword(password, err)) return false;
    if (accountCount >= MAX_ACCOUNTS) {
        *err = "This computer has reached the account limit.";
        return false;
    }

    Account a = { 0 };
    CopyName(a.name, name);
    if (!MakeSecret(password, &a.password) || !NewRecoveryCode(&a, recoveryOut)) {
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
        char legacy[1100];
        snprintf(legacy, sizeof legacy, "%sprogress.txt", dataDir);
        MakeDirectory(TextFormat("%susers", dataDir));
        const char *dest = UserFile(a.name, ".txt");
        if (FileExists(legacy) && !FileExists(dest)) PlatformReplaceFile(legacy, dest);
    }

    SetCurrent(accountCount - 1);
    return true;
}

bool AccountLogin(const char *name, const char *password, const char **err)
{
    int i = FindAccount(name);
    if (i < 0 || !CheckSecret(password, &accounts[i].password)) {
        *err = "Wrong username or password.";
        return false;
    }
    SetCurrent(i);
    return true;
}

bool AccountResetPassword(const char *name, const char *recoveryCode, const char *newPassword,
                          char recoveryOut[RECOVERY_CODE_SIZE], const char **err)
{
    int i = FindAccount(name);
    char code[CODE_CHARS + 1];
    if (i < 0 || !accounts[i].hasRecovery || !NormalizeCode(recoveryCode, code)
        || !CheckSecret(code, &accounts[i].recovery)) {
        *err = "Wrong username or recovery code.";
        return false;
    }
    if (!ValidPassword(newPassword, err)) return false;

    // Each code works once: resetting also issues a new one.
    Account updated = accounts[i];
    if (!MakeSecret(newPassword, &updated.password) || !NewRecoveryCode(&updated, recoveryOut)) {
        *err = "Could not secure the password. Please try again.";
        return false;
    }
    Account old = accounts[i];
    accounts[i] = updated;
    if (!SaveAccounts()) {
        accounts[i] = old;
        *err = "Could not save the account file.";
        return false;
    }
    SetCurrent(i);
    return true;
}

void AccountLogout(void) { current = -1; }

bool AccountChangePassword(const char *currentPassword, const char *newPassword, const char **err)
{
    if (!RequireLogin(err)) return false;
    if (!CheckSecret(currentPassword, &accounts[current].password)) {
        *err = "Your current password is wrong.";
        return false;
    }
    if (!ValidPassword(newPassword, err)) return false;

    Secret old = accounts[current].password;
    if (!MakeSecret(newPassword, &accounts[current].password)) {
        accounts[current].password = old;
        *err = "Could not secure the password. Please try again.";
        return false;
    }
    if (!SaveAccounts()) {
        accounts[current].password = old;
        *err = "Could not save the account file.";
        return false;
    }
    return true;
}

bool AccountNewRecoveryCode(const char *password, char recoveryOut[RECOVERY_CODE_SIZE], const char **err)
{
    if (!RequireLogin(err)) return false;
    if (!CheckSecret(password, &accounts[current].password)) {
        *err = "Wrong password.";
        return false;
    }
    Account old = accounts[current];
    if (!NewRecoveryCode(&accounts[current], recoveryOut) || !SaveAccounts()) {
        accounts[current] = old;
        *err = "Could not create a recovery code. Please try again.";
        return false;
    }
    return true;
}

bool AccountDelete(const char *password, const char **err)
{
    if (!RequireLogin(err)) return false;
    if (!CheckSecret(password, &accounts[current].password)) {
        *err = "Wrong password.";
        return false;
    }

    Account removed = accounts[current];
    char savedLast[USERNAME_MAX + 1];
    CopyName(savedLast, lastUser);
    for (int i = current; i < accountCount - 1; i++) accounts[i] = accounts[i + 1];
    accountCount--;
    if (SameName(lastUser, removed.name)) lastUser[0] = '\0';

    if (!SaveAccounts()) {
        for (int i = accountCount; i > current; i--) accounts[i] = accounts[i - 1];
        accounts[current] = removed;
        accountCount++;
        CopyName(lastUser, savedLast);
        *err = "Could not save the account file.";
        return false;
    }

    remove(UserFile(removed.name, ".txt"));
    remove(UserFile(removed.name, ".settings.txt"));
    current = -1;
    settings = defaultSettings;
    return true;
}

const char *AccountCurrent(void) { return current >= 0 ? accounts[current].name : NULL; }

bool AccountHasRecovery(void) { return current >= 0 && accounts[current].hasRecovery; }

const char *AccountProgressPath(void) { return progressPath; }

const char *AccountLastUser(void) { return lastUser; }

AccountSettings AccountGetSettings(void) { return settings; }

void AccountSetSettings(AccountSettings s)
{
    settings = s;
    if (current < 0) return;
    char text[128];
    snprintf(text, sizeof text, "theme %d\naccent %d\ntext %d\n", (int)s.theme, s.accent, (int)s.textSize);
    WriteFileAtomic(UserFile(accounts[current].name, ".settings.txt"), text);
}
