// Saved lessons: adding, removing, the saved order, and that each account's bookmarks
// persist in its own file. Usage: test_bookmarks <empty temp dir>
#include "../src/bookmarks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char dir[1024];

static const char *PathFor(const char *user)
{
    static char path[1100];
    snprintf(path, sizeof path, "%s/%s.bookmarks.txt", dir, user);
    return path;
}

static void TestAddRemoveAndOrder(void)
{
    BookmarksLoad(PathFor("ada"));  // no file yet: nothing saved
    assert(BookmarkCount() == 0 && !BookmarkIsSet("c_01_intro") && BookmarkAt(0) == NULL);

    assert(BookmarkSet("c_03_pointers", true));
    assert(BookmarkSet("cpp_02_references", true));
    assert(BookmarkSet("c_01_intro", true));
    assert(BookmarkSet("c_03_pointers", true));  // saving twice keeps one bookmark
    assert(BookmarkCount() == 3);
    assert(strcmp(BookmarkAt(0), "c_03_pointers") == 0 && strcmp(BookmarkAt(2), "c_01_intro") == 0);

    assert(BookmarkSet("cpp_02_references", false));  // removing keeps the others in order
    assert(BookmarkCount() == 2 && !BookmarkIsSet("cpp_02_references"));
    assert(strcmp(BookmarkAt(0), "c_03_pointers") == 0 && strcmp(BookmarkAt(1), "c_01_intro") == 0);
    assert(BookmarkSet("not_saved", false) && BookmarkCount() == 2);  // removing a missing one is harmless

    assert(BookmarkToggle("c_01_intro") && !BookmarkIsSet("c_01_intro"));
    assert(BookmarkToggle("c_01_intro") && BookmarkIsSet("c_01_intro"));
}

static void TestPersistsPerAccount(void)
{
    // Ada's bookmarks from the test above come back from her file.
    BookmarksLoad(PathFor("ada"));
    assert(BookmarkCount() == 2 && BookmarkIsSet("c_03_pointers") && BookmarkIsSet("c_01_intro"));

    // Another account starts empty and doesn't see Ada's.
    BookmarksLoad(PathFor("grace"));
    assert(BookmarkCount() == 0 && !BookmarkIsSet("c_03_pointers"));
    assert(BookmarkSet("cpp_05_classes", true));

    BookmarksLoad(PathFor("ada"));
    assert(BookmarkCount() == 2 && !BookmarkIsSet("cpp_05_classes"));
    BookmarksLoad(PathFor("grace"));
    assert(BookmarkCount() == 1 && BookmarkIsSet("cpp_05_classes"));
}

static void TestSignedOut(void)
{
    BookmarksLoad("");  // signing out drops the bookmarks from memory...
    assert(BookmarkCount() == 0 && !BookmarkIsSet("cpp_05_classes"));
    assert(!BookmarkSet("c_01_intro", true) && BookmarkCount() == 0);  // ...and nothing can be saved
    BookmarksLoad(PathFor("grace"));  // ...but the file is untouched
    assert(BookmarkCount() == 1);
}

static void TestHandEditedFile(void)
{
    FILE *f = fopen(PathFor("linus"), "w");
    assert(f);
    fputs("c_02_lab1\n\nc_02_lab1\n  cpp_01_intro  \n", f);  // a duplicate, a blank line, stray spaces
    fclose(f);
    BookmarksLoad(PathFor("linus"));
    assert(BookmarkCount() == 2 && BookmarkIsSet("c_02_lab1") && BookmarkIsSet("cpp_01_intro"));
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_bookmarks <empty temp dir>\n");
        return 2;
    }
    snprintf(dir, sizeof dir, "%s", argv[1]);
    TestAddRemoveAndOrder();
    TestPersistsPerAccount();
    TestSignedOut();
    TestHandEditedFile();
    printf("bookmarks: all tests passed\n");
    return 0;
}
