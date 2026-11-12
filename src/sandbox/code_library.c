// A learner's saved programs: named source files in <code folder>/saved. See code_library.h.
// Only source is stored; the concepts shown with each program are worked out from it.
#include "sandbox/code_library.h"
#include "core/platform.h"
#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool CodeLibraryNameValid(const char *name)
{
    if (!name || !*name || strlen(name) > CODE_NAME_MAX || *name == ' ' || name[strlen(name) - 1] == ' ') return false;
    char upper[CODE_NAME_MAX + 1];
    for (size_t i = 0; i <= strlen(name); i++) {
        unsigned char c = (unsigned char)name[i];
        if (c &&
            !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' ||
              c == '-'))
            return false;
        upper[i] = (char)toupper(c);
    }
    if (!strcmp(upper, "CON") || !strcmp(upper, "PRN") || !strcmp(upper, "AUX") || !strcmp(upper, "NUL")) return false;
    if (strlen(upper) == 4 && (!strncmp(upper, "COM", 3) || !strncmp(upper, "LPT", 3)) && upper[3] >= '1' &&
        upper[3] <= '9')
        return false;
    return true;
}

// The file a program of that name and language is kept in. False for a bad name or a
// path too long for out.
static bool Path(char *out, size_t size, const char *folder, const char *name, bool cpp)
{
    if (!folder || !*folder || !CodeLibraryNameValid(name)) return false;
    int n = snprintf(out, size, "%s/saved/%s.%s", folder, name, cpp ? "cpp" : "c");
    return n > 0 && (size_t)n < size;
}

// Plain ASCII text only: no control characters but newlines and tabs, so a binary file
// is never loaded into the editor.
static bool SourceValid(const char *source, size_t length)
{
    for (size_t i = 0; i < length; i++)
        if ((unsigned char)source[i] > 126 ||
            ((unsigned char)source[i] < 32 && source[i] != '\n' && source[i] != '\r' && source[i] != '\t'))
            return false;
    return true;
}

CodeSaveResult CodeLibrarySave(const char *folder, const char *name, bool cpp, const char *source, bool replace)
{
    char path[1200], directory[1200], temp[1220];
    if (!Path(path, sizeof path, folder, name, cpp) || !source || strlen(source) > CODE_SAVE_MAX)
        return CODE_SAVE_ERROR;
    if (!SourceValid(source, strlen(source))) return CODE_SAVE_ERROR;
    if (FileExists(path) && !replace) return CODE_SAVE_EXISTS;
    snprintf(directory, sizeof directory, "%s/saved", folder);
    if (!DirectoryExists(directory) && MakeDirectory(directory) != 0) return CODE_SAVE_ERROR;
    snprintf(temp, sizeof temp, "%s.tmp", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return CODE_SAVE_ERROR;
    size_t len = strlen(source);
    bool ok = fwrite(source, 1, len, file) == len;
    if (fclose(file) != 0) ok = false;
    if (ok) ok = PlatformReplaceFile(temp, path);
    if (!ok) remove(temp);
    return ok ? CODE_SAVE_OK : CODE_SAVE_ERROR;
}

char *CodeLibraryLoad(const char *folder, const char *name, bool cpp)
{
    char path[1200];
    if (!Path(path, sizeof path, folder, name, cpp)) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || size > CODE_SAVE_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return NULL;
    }
    char *source = malloc((size_t)size + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }
    bool ok = fread(source, 1, (size_t)size, file) == (size_t)size;
    if (ferror(file)) ok = false;
    fclose(file);
    if (!SourceValid(source, (size_t)size)) ok = false;
    source[size] = 0;
    if (!ok) {
        free(source);
        return NULL;
    }
    return source;
}

// Orders programs by name, C before C++ for the same name.
static int Compare(const void *a, const void *b)
{
    const SavedProgram *left = a, *right = b;
    int n = strcmp(left->name, right->name);
    return n ? n : (int)left->cpp - (int)right->cpp;
}

bool CodeLibraryList(const char *folder, SavedProgram **items, int *count)
{
    *items = NULL;
    *count = 0;
    if (!folder || !*folder) return false;
    char directory[1200];
    int n = snprintf(directory, sizeof directory, "%s/saved", folder);
    if (n < 0 || (size_t)n >= sizeof directory) return false;
    if (!DirectoryExists(directory)) return !FileExists(directory);
    FilePathList files = LoadDirectoryFiles(directory);
    SavedProgram *list = calloc(files.count ? files.count : 1, sizeof *list);
    if (!list) {
        UnloadDirectoryFiles(files);
        return false;
    }
    bool ok = true;
    for (unsigned i = 0; i < files.count; i++) {
        const char *ext = GetFileExtension(files.paths[i]);
        if (!ext || (strcmp(ext, ".c") && strcmp(ext, ".cpp")) || DirectoryExists(files.paths[i])) continue;
        const char *name = GetFileNameWithoutExt(files.paths[i]);
        if (!CodeLibraryNameValid(name)) continue;
        SavedProgram *item = &list[*count];
        snprintf(item->name, sizeof item->name, "%s", name);
        item->cpp = !strcmp(ext, ".cpp");
        char *source = CodeLibraryLoad(folder, item->name, item->cpp);
        if (!source) {
            ok = false;
            continue;
        }
        item->skills = CodeSkillsDetect(source, item->cpp);
        free(source);
        (*count)++;
    }
    UnloadDirectoryFiles(files);
    qsort(list, *count, sizeof *list, Compare);
    *items = list;
    return ok;
}
