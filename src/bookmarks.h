#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <stdbool.h>

// The signed-in user's saved (bookmarked) lessons, kept as lesson ids, one per line, in
// users/<name>.bookmarks.txt. Ids rather than lesson indexes, so bookmarks survive lessons
// being added or reordered. Plain data: nothing here draws.

#define BOOKMARKS_MAX 256  // far more than the course has lessons

// Loads the user's bookmarks from path; "" clears them (signed out). A missing file means
// no bookmarks yet.
void BookmarksLoad(const char *path);

bool BookmarkIsSet(const char *lessonId);

// Adds or removes a bookmark and saves straight away. Returns false if the file couldn't be
// written (the change still applies for this session). Does nothing while signed out.
bool BookmarkSet(const char *lessonId, bool saved);
bool BookmarkToggle(const char *lessonId);

// The bookmarks in the order they were saved, for listing.
int BookmarkCount(void);
const char *BookmarkAt(int index);  // NULL when out of range

#endif
