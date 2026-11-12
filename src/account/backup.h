#ifndef BACKUP_H
#define BACKUP_H

#include <stdbool.h>

// A learner's work in one file, to keep safe or carry to another computer: passed steps,
// skill XP, answer history, appearance settings and code (each step's draft, the code
// editor's drafts and saved programs). Never the password, recovery code, email address or
// "remember me" token: a backup is restored into an account that already exists.
//
// The file is text: a first line "increment-backup 1 <name>", then for each file
// "@file <what> <bytes>" followed by exactly that many bytes and a newline, and "@end" last
// (so a cut-off file is noticed). <what> is progress, skills, review, settings, code/<file>
// or code/saved/<file>.
//
// This module only knows files: the caller says where the account's files are.

typedef struct {
    const char *progress;    // each of these may be NULL or "" for none
    const char *skills;
    const char *review;
    const char *settings;
    const char *codeFolder;  // drafts in it, saved programs in its "saved" folder
} BackupFiles;

// Writes a backup of the files to backupPath. Files that don't exist are left out.
// On failure returns false and sets *err (a static string).
bool BackupExport(const char *backupPath, const char *owner, const BackupFiles *files, const char **err);

// Restores a backup over the files: progress, skills, answer history and settings are
// replaced (and removed if the backup has none); code files are written over those of the
// same name, others are kept. The whole backup is checked before anything is changed, so a
// damaged file changes nothing. On failure returns false and sets *err.
bool BackupImport(const char *backupPath, const BackupFiles *files, const char **err);

#endif
