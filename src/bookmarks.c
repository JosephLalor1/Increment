// The signed-in user's saved lessons. See bookmarks.h.
#include "bookmarks.h"

#include <stdio.h>
#include <string.h>

#define ID_SIZE 32  // matches Lesson.id

static char ids[BOOKMARKS_MAX][ID_SIZE];
static int count;
static char savePath[1100];  // "" when signed out

static int Find(const char *lessonId)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(ids[i], lessonId) == 0) return i;
    }
    return -1;
}

static void Add(const char *lessonId)
{
    if (count >= BOOKMARKS_MAX || !lessonId[0] || strlen(lessonId) >= ID_SIZE || Find(lessonId) >= 0) return;
    snprintf(ids[count++], ID_SIZE, "%s", lessonId);
}

static bool Save(void)
{
    if (!savePath[0]) return false;
    FILE *f = fopen(savePath, "w");
    if (!f) return false;
    for (int i = 0; i < count; i++) fprintf(f, "%s\n", ids[i]);
    return fclose(f) == 0;
}

void BookmarksLoad(const char *path)
{
    snprintf(savePath, sizeof savePath, "%s", path ? path : "");
    count = 0;
    if (!savePath[0]) return;
    FILE *f = fopen(savePath, "r");
    if (!f) return;
    char id[ID_SIZE];
    while (fscanf(f, "%31s", id) == 1) Add(id);  // Add skips duplicates, so a hand-edited file is fine
    fclose(f);
}

bool BookmarkIsSet(const char *lessonId) { return Find(lessonId) >= 0; }

bool BookmarkSet(const char *lessonId, bool saved)
{
    if (!savePath[0]) return false;
    int at = Find(lessonId);
    if (saved && at < 0) {
        Add(lessonId);
    } else if (!saved && at >= 0) {
        memmove(ids[at], ids[at + 1], (size_t)(count - at - 1) * ID_SIZE);  // keep the saved order
        count--;
    }
    return Save();
}

bool BookmarkToggle(const char *lessonId) { return BookmarkSet(lessonId, !BookmarkIsSet(lessonId)); }

int BookmarkCount(void) { return count; }

const char *BookmarkAt(int index) { return index >= 0 && index < count ? ids[index] : NULL; }
