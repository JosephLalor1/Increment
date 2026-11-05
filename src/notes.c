// A student's notes on each lesson, stored as text files. See notes.h.
#include "notes.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lesson ids become file names, so only allow characters that are safe in one.
static bool SafeId(const char *id)
{
    if (!id || !id[0] || strlen(id) > 64) return false;
    for (const char *c = id; *c; c++) {
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-') return false;
    }
    return true;
}

// Builds folder/<id>.txt. Returns false (setting *err) when the folder or id can't be used.
static bool NotesPath(const char *folder, const char *lessonId, char *out, size_t size, const char **err)
{
    if (!folder || !folder[0]) {
        *err = "Notes can only be saved while you are signed in.";
        return false;
    }
    if (!SafeId(lessonId)) {
        *err = "This lesson's id can't be used as a file name.";
        return false;
    }
    int n = snprintf(out, size, "%s/%s.txt", folder, lessonId);
    if (n < 0 || (size_t)n >= size) {
        *err = "The notes folder's path is too long.";
        return false;
    }
    return true;
}

bool NotesBlank(const char *text)
{
    for (const char *c = text; c && *c; c++) {
        if (!isspace((unsigned char)*c)) return false;
    }
    return true;
}

char *NotesLoad(const char *folder, const char *lessonId, const char **err)
{
    char path[1100];
    if (!NotesPath(folder, lessonId, path, sizeof path, err)) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) {  // no notes yet
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        else *err = "Out of memory.";
        return empty;
    }
    char *text = malloc(NOTES_MAX_BYTES + 1);
    if (!text) {
        fclose(f);
        *err = "Out of memory.";
        return NULL;
    }
    size_t n = fread(text, 1, NOTES_MAX_BYTES + 1, f);
    bool failed = ferror(f);
    fclose(f);
    if (failed || n > NOTES_MAX_BYTES) {
        free(text);
        *err = failed ? "Your notes file couldn't be read." : "Your notes file is too large to open (over 64 KB).";
        return NULL;
    }
    // Drop carriage returns, so a file edited in Notepad still has plain '\n' line endings.
    size_t kept = 0;
    for (size_t i = 0; i < n; i++) {
        if (text[i] != '\r') text[kept++] = text[i];
    }
    text[kept] = '\0';
    return text;
}

bool NotesSave(const char *folder, const char *lessonId, const char *text, const char **err)
{
    char path[1100];
    if (!NotesPath(folder, lessonId, path, sizeof path, err)) return false;
    if (NotesBlank(text)) {  // cleared: no file at all, rather than an empty one
        FILE *existing = fopen(path, "rb");
        if (!existing) return true;
        fclose(existing);
        if (remove(path) != 0) {
            *err = "Your old notes file couldn't be deleted.";
            return false;
        }
        return true;
    }
    if (strlen(text) > NOTES_MAX_BYTES) {
        *err = "These notes are too long to save (over 64 KB).";
        return false;
    }

    char temp[1200];
    snprintf(temp, sizeof temp, "%s.tmp", path);
    FILE *f = fopen(temp, "wb");
    if (!f) {
        *err = "Your notes couldn't be written. Check that the app's folder isn't read-only.";
        return false;
    }
    bool ok = fputs(text, f) >= 0;
    ok = (fflush(f) == 0) && ok;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !PlatformReplaceFile(temp, path)) {
        remove(temp);
        *err = "Your notes couldn't be written. The disk may be full.";
        return false;
    }
    return true;
}
