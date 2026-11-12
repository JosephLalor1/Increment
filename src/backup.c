// A learner's work exported to one file and restored from it. See backup.h.
#include "backup.h"

#include "raylib.h"  // directory listing and creation only

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKUP_HEADER   "increment-backup 1"
#define BACKUP_MAX      (32L * 1024 * 1024)  // the largest backup read
#define ENTRY_MAX       (4L * 1024 * 1024)   // the largest single file in one
#define ENTRY_NAME_MAX  200

// ---------------------------------------------------------------------------
// Reading and writing whole files

// The file's bytes (malloc'd, '\0' after them) and its size; NULL if it can't be read or
// is larger than limit.
static char *ReadWhole(const char *path, long limit, long *size)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = length >= 0 && length <= limit ? malloc((size_t)length + 1) : NULL;
    if (bytes && fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        bytes = NULL;
    }
    fclose(file);
    if (bytes) {
        bytes[length] = '\0';
        *size = length;
    }
    return bytes;
}

static bool WriteWhole(const char *path, const char *bytes, long size)
{
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool written = fwrite(bytes, 1, (size_t)size, file) == (size_t)size;
    return fclose(file) == 0 && written;
}

static bool Given(const char *path) { return path && path[0]; }

// ---------------------------------------------------------------------------
// Export

// Adds one file to the backup under a name; a file that doesn't exist is skipped.
static bool AddFile(FILE *backup, const char *what, const char *path)
{
    if (!Given(path)) return true;
    long size;
    char *bytes = ReadWhole(path, ENTRY_MAX, &size);
    if (!bytes) return !FileExists(path);  // missing is fine; unreadable or huge is not
    bool written = fprintf(backup, "@file %s %ld\n", what, size) > 0
                && fwrite(bytes, 1, (size_t)size, backup) == (size_t)size && fputc('\n', backup) != EOF;
    free(bytes);
    return written;
}

// A code file's name is safe to restore anywhere: letters, digits, '_', '-' and '.', not
// starting with a dot. (The app only ever writes such names.)
static bool SafeFileName(const char *name)
{
    if (!name[0] || name[0] == '.' || strlen(name) > 120) return false;
    for (const char *c = name; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-' && *c != '.') return false;
    return true;
}

// Adds every file in a folder (not its subfolders) under "<prefix><file name>".
static bool AddFolder(FILE *backup, const char *prefix, const char *folder)
{
    if (!DirectoryExists(folder)) return true;
    FilePathList list = LoadDirectoryFiles(folder);
    bool ok = true;
    for (unsigned int i = 0; ok && i < list.count; i++) {
        const char *name = GetFileName(list.paths[i]);
        if (!IsPathFile(list.paths[i]) || !SafeFileName(name)) continue;
        char what[ENTRY_NAME_MAX];
        snprintf(what, sizeof what, "%s%s", prefix, name);
        ok = AddFile(backup, what, list.paths[i]);
    }
    UnloadDirectoryFiles(list);
    return ok;
}

bool BackupExport(const char *backupPath, const char *owner, const BackupFiles *files, const char **err)
{
    FILE *backup = fopen(backupPath, "wb");
    if (!backup) {
        *err = "Couldn't create the backup file.";
        return false;
    }
    bool ok = fprintf(backup, "%s %s\n", BACKUP_HEADER, owner && owner[0] ? owner : "-") > 0
           && AddFile(backup, "progress", files->progress) && AddFile(backup, "skills", files->skills)
           && AddFile(backup, "review", files->review) && AddFile(backup, "settings", files->settings);
    if (ok && Given(files->codeFolder)) {
        char saved[1200];
        snprintf(saved, sizeof saved, "%s/saved", files->codeFolder);
        ok = AddFolder(backup, "code/", files->codeFolder) && AddFolder(backup, "code/saved/", saved);
    }
    ok = ok && fputs("@end\n", backup) != EOF;
    if (fclose(backup) != 0) ok = false;
    if (!ok) {
        remove(backupPath);  // half a backup is worse than none
        *err = "Couldn't write the backup file.";
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Import

// Where a backup entry belongs among the account's files, written to out; false if the
// name isn't one a backup may hold (or the account has nowhere for it).
static bool TargetPath(const char *what, const BackupFiles *files, char *out, size_t size)
{
    const char *single = strcmp(what, "progress") == 0 ? files->progress
                       : strcmp(what, "skills") == 0 ? files->skills
                       : strcmp(what, "review") == 0 ? files->review
                       : strcmp(what, "settings") == 0 ? files->settings : NULL;
    if (single) {
        snprintf(out, size, "%s", single);
        return single[0] != '\0';
    }
    if (strncmp(what, "code/", 5) != 0 || !Given(files->codeFolder)) return false;
    const char *name = what + 5;
    bool saved = strncmp(name, "saved/", 6) == 0;
    if (saved) name += 6;
    if (!SafeFileName(name)) return false;  // no folders, no "..": it stays inside the code folder
    snprintf(out, size, "%s/%s%s", files->codeFolder, saved ? "saved/" : "", name);
    return true;
}

// Walks a backup's entries. With write false it only checks that every entry is sound;
// with write true it restores them (call it only after a clean check). *had records which
// of the four single files the backup holds.
static bool Walk(const char *bytes, long size, const BackupFiles *files, bool write, bool had[4])
{
    static const char *const singles[4] = { "progress", "skills", "review", "settings" };
    const char *at = bytes, *end = bytes + size;
    const char *line = memchr(at, '\n', (size_t)(end - at));
    if (!line || strncmp(at, BACKUP_HEADER " ", strlen(BACKUP_HEADER) + 1) != 0) return false;
    at = line + 1;
    for (;;) {
        line = memchr(at, '\n', (size_t)(end - at));
        if (!line) return false;  // cut off before "@end"
        if (line - at == 4 && strncmp(at, "@end", 4) == 0) return true;
        char what[ENTRY_NAME_MAX];
        long length = -1;
        char header[ENTRY_NAME_MAX + 40];
        if (line - at >= (long)sizeof header) return false;
        memcpy(header, at, (size_t)(line - at));
        header[line - at] = '\0';
        if (sscanf(header, "@file %199s %ld", what, &length) != 2 || length < 0 || length > ENTRY_MAX) return false;
        const char *content = line + 1;
        if (length > end - content - 1 || content[length] != '\n') return false;
        char path[1400];
        if (!TargetPath(what, files, path, sizeof path)) return false;
        for (int i = 0; i < 4; i++)
            if (strcmp(what, singles[i]) == 0) had[i] = true;
        if (write && !WriteWhole(path, content, length)) return false;
        at = content + length + 1;
    }
}

bool BackupImport(const char *backupPath, const BackupFiles *files, const char **err)
{
    long size;
    char *bytes = ReadWhole(backupPath, BACKUP_MAX, &size);
    if (!bytes) {
        *err = "Couldn't read that file.";
        return false;
    }
    bool had[4] = { false, false, false, false };
    if (!Walk(bytes, size, files, false, had)) {
        free(bytes);
        *err = "That isn't an Increment backup, or it is damaged.";
        return false;
    }
    // Sound from start to finish: now replace the account's files with it.
    if (Given(files->codeFolder)) {
        char saved[1200];
        snprintf(saved, sizeof saved, "%s/saved", files->codeFolder);
        MakeDirectory(saved);  // makes the code folder too
    }
    const char *singles[4] = { files->progress, files->skills, files->review, files->settings };
    for (int i = 0; i < 4; i++)
        if (!had[i] && Given(singles[i])) remove(singles[i]);  // the backup has none: nor does the account now
    bool restored = Walk(bytes, size, files, true, had);
    free(bytes);
    if (!restored) *err = "Couldn't write the restored files.";
    return restored;
}
