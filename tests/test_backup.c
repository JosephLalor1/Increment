// Tests for exporting and restoring a learner's work (src/backup.c). Run via test.bat with
// a throwaway folder as the argument.
#include "account/backup.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while (0)

static char root[600];

// A path inside the test folder (a few at a time may be in use).
static const char *In(const char *name)
{
    static char paths[16][800];
    static int next;
    char *path = paths[next++ % 16];
    snprintf(path, 800, "%s/%s", root, name);
    return path;
}

static void Put(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    fputs(text, file);
    fclose(file);
}

static bool Holds(const char *path, const char *text)
{
    char got[4096] = "";
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t n = fread(got, 1, sizeof got - 1, file);
    fclose(file);
    got[n] = '\0';
    return strcmp(got, text) == 0;
}

// An account's files under <root>/<who>.
static BackupFiles FilesOf(const char *who)
{
    static char paths[4][5][800];
    static int next;
    char (*p)[800] = paths[next++ % 4];
    snprintf(p[0], 800, "%s/%s.txt", root, who);
    snprintf(p[1], 800, "%s/%s.skills.txt", root, who);
    snprintf(p[2], 800, "%s/%s.review.txt", root, who);
    snprintf(p[3], 800, "%s/%s.settings.txt", root, who);
    snprintf(p[4], 800, "%s/%s_code", root, who);
    return (BackupFiles){ p[0], p[1], p[2], p[3], p[4] };
}

static void TestRoundTrip(void)
{
    BackupFiles ann = FilesOf("ann"), ben = FilesOf("ben");
    const char *err = NULL;
    MakeDirectory(In("ann_code/saved"));
    Put(ann.progress, "c_01_hello/1 100\nc_01_hello/2 100\n");
    Put(ann.skills, "printing 40 3 0\n");
    // No review file: Ann never played a speed round.
    Put(ann.settings, "theme 1\naccent 2\n");
    Put(In("ann_code/c_01_hello_step3.c"), "#include <stdio.h>\r\nint main() { return 0; }\n@end\n@file x 3\n");
    Put(In("ann_code/saved/primes.cpp"), "int main() {}\n");
    Put(In("ann_code/.hidden"), "not ours");

    CHECK(BackupExport(In("ann.increment"), "Ann", &ann, &err));

    // Ben has work of his own, which the restore replaces (and keeps his other code).
    MakeDirectory(In("ben_code"));
    Put(ben.progress, "c_02_variables/1 100\n");
    Put(ben.review, "skill:loops 1 2 3\n");
    Put(In("ben_code/c_01_hello_step3.c"), "old");
    Put(In("ben_code/playground.c"), "mine");
    CHECK(BackupImport(In("ann.increment"), &ben, &err));
    CHECK(Holds(ben.progress, "c_01_hello/1 100\nc_01_hello/2 100\n"));
    CHECK(Holds(ben.skills, "printing 40 3 0\n"));
    CHECK(!FileExists(ben.review));  // the backup had none
    CHECK(Holds(ben.settings, "theme 1\naccent 2\n"));
    // Code comes back byte for byte, even when it looks like the backup's own markers.
    CHECK(Holds(In("ben_code/c_01_hello_step3.c"), "#include <stdio.h>\r\nint main() { return 0; }\n@end\n@file x 3\n"));
    CHECK(Holds(In("ben_code/saved/primes.cpp"), "int main() {}\n"));
    CHECK(Holds(In("ben_code/playground.c"), "mine"));
    CHECK(!FileExists(In("ben_code/.hidden")));
}

static void TestNothingToExport(void)
{
    // A new account with no files at all still exports, and restores to nothing.
    BackupFiles nobody = FilesOf("nobody"), cat = FilesOf("cat");
    const char *err = NULL;
    CHECK(BackupExport(In("empty.increment"), "Nobody", &nobody, &err));
    Put(cat.progress, "c_01_hello/1 100\n");
    CHECK(BackupImport(In("empty.increment"), &cat, &err));
    CHECK(!FileExists(cat.progress));
}

static void TestBadBackupsChangeNothing(void)
{
    BackupFiles dan = FilesOf("dan");
    const char *err = NULL;
    Put(dan.progress, "kept\n");
    static const char *const bad[] = {
        "",                                                              // empty
        "hello\n",                                                       // not a backup
        "increment-backup 1 Ann\n@file progress 5\nlost\n",             // cut off: no @end
        "increment-backup 1 Ann\n@file progress 500\nshort\n@end\n",     // a length past the end
        "increment-backup 1 Ann\n@file progress 4\nlost\n@file code/../../evil.c 1\nx\n@end\n",  // out of the folder
        "increment-backup 1 Ann\n@file code/sub/dir.c 1\nx\n@end\n",
        "increment-backup 1 Ann\n@file passwords 1\nx\n@end\n",          // not a file a backup holds
        "increment-backup 1 Ann\n@file progress -1\n@end\n",
    };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        Put(In("bad.increment"), bad[i]);
        err = NULL;
        CHECK(!BackupImport(In("bad.increment"), &dan, &err));
        CHECK(err != NULL);
        CHECK(Holds(dan.progress, "kept\n"));  // checked before anything was written
    }
    CHECK(!BackupImport(In("missing.increment"), &dan, &err));
    CHECK(!FileExists(In("evil.c")));
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: test_backup <empty folder>\n");
        return 2;
    }
    SetTraceLogLevel(LOG_WARNING);
    snprintf(root, sizeof root, "%s/backup", argv[1]);
    MakeDirectory(root);
    TestRoundTrip();
    TestNothingToExport();
    TestBadBackupsChangeNothing();
    if (failures) {
        printf("%d backup test(s) failed\n", failures);
        return 1;
    }
    printf("All backup tests passed\n");
    return 0;
}
