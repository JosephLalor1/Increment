// Lesson search: matching, track filters, ordering and excerpts, on synthetic lessons,
// plus a check against the real lessons in assets/lessons.
#include "../src/search.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Lesson items[4];

static void Add(int i, Track track, const char *title, const char *text, const char *code)
{
    Lesson *L = &items[i];
    memset(L, 0, sizeof *L);
    snprintf(L->title, sizeof L->title, "%s", title);
    L->track = track;
    L->blocks[0] = (Block){ BLOCK_TEXT, (char *)text };
    L->blocks[1] = (Block){ BLOCK_CODE, (char *)code };
    L->blockCount = 2;
}

static void TestMatching(void)
{
    Add(0, TRACK_C, "Pointers", "A pointer holds an address.", "int *p = &x;");
    Add(1, TRACK_C, "Loops", "Use a while loop to repeat.\nPointer arithmetic\twalks arrays.", "while (i < n) i++;");
    Add(2, TRACK_CPP, "References", "A reference is another name.", "int &r = x;");
    Add(3, TRACK_CPP, "Classes", "Classes group data.", "std::cout << \"Hello\";");
    items[3].exercises[0] = (Exercise){ .title = "Bank account", .description = "Write a deposit method." };
    items[3].exerciseCount = 1;
    LessonSet set = { items, 4 };
    int out[8];

    assert(SearchIsBlank("") && SearchIsBlank("   ") && SearchIsBlank("\t") && !SearchIsBlank(" a "));
    assert(SearchLessons(&set, "", SEARCH_BOTH, out, 8) == 0);
    assert(SearchLessons(&set, "   ", SEARCH_BOTH, out, 8) == 0);

    // Case doesn't matter, in titles or text.
    assert(SearchLessons(&set, "LOOPS", SEARCH_BOTH, out, 8) == 1 && out[0] == 1);
    assert(SearchLessons(&set, "ADDRESS", SEARCH_BOTH, out, 8) == 1 && out[0] == 0);
    // Code and exercises count as lesson text.
    assert(SearchLessons(&set, "std::cout", SEARCH_BOTH, out, 8) == 1 && out[0] == 3);
    assert(SearchLessons(&set, "deposit", SEARCH_BOTH, out, 8) == 1 && out[0] == 3);
    assert(SearchLessons(&set, "bank", SEARCH_BOTH, out, 8) == 1 && out[0] == 3);

    // Title matches come first, then text matches in lesson order.
    assert(SearchLessons(&set, "pointer", SEARCH_BOTH, out, 8) == 2 && out[0] == 0 && out[1] == 1);

    // Every word must appear, anywhere in the lesson.
    assert(SearchLessons(&set, "pointer arrays", SEARCH_BOTH, out, 8) == 1 && out[0] == 1);
    assert(SearchLessons(&set, "  loops   repeat ", SEARCH_BOTH, out, 8) == 1 && out[0] == 1);
    assert(SearchLessons(&set, "pointer zebra", SEARCH_BOTH, out, 8) == 0);
    assert(SearchLessons(&set, "zebra", SEARCH_BOTH, out, 8) == 0);

    // Track filters.
    assert(SearchLessons(&set, "x;", SEARCH_BOTH, out, 8) == 2);
    assert(SearchLessons(&set, "x;", SEARCH_C, out, 8) == 1 && out[0] == 0);
    assert(SearchLessons(&set, "x;", SEARCH_CPP, out, 8) == 1 && out[0] == 2);
    assert(SearchLessons(&set, "loop", SEARCH_CPP, out, 8) == 0);
    assert(SearchMatches(&items[2], "reference", SEARCH_CPP) && !SearchMatches(&items[2], "reference", SEARCH_C));

    // Never writes more than max results.
    assert(SearchLessons(&set, "a", SEARCH_BOTH, out, 1) == 1);

    assert(strcmp(SearchFilterName(SEARCH_BOTH), "C and C++") == 0 && strcmp(SearchFilterName(SEARCH_CPP), "C++") == 0);
}

static void TestSnippets(void)
{
    char s[256];
    int at, len;
    // Newlines and tabs become single spaces, and the match is located.
    assert(SearchSnippet(&items[1], "ARITHMETIC", s, sizeof s, &at, &len));
    assert(strstr(s, "\n") == NULL && strstr(s, "\t") == NULL && strstr(s, "  ") == NULL);
    assert(len == 10 && strncmp(s + at, "arithmetic", 10) == 0);

    // Prose is preferred over code when both match.
    assert(SearchSnippet(&items[1], "while", s, sizeof s, &at, &len));
    assert(strncmp(s, "Use a while", 11) == 0 && strncmp(s + at, "while", 5) == 0);

    // Title-only matches have no excerpt.
    assert(!SearchSnippet(&items[2], "references", s, sizeof s, &at, &len) && s[0] == '\0');

    // Long text is cut around the match, with "..." on both sides.
    static char longText[1200];
    memset(longText, 'x', sizeof longText - 1);
    for (int i = 0; i < 1100; i += 10) longText[i] = ' ';
    memcpy(longText + 600, " needle ", 8);
    Add(0, TRACK_C, "Long", longText, "");
    assert(SearchSnippet(&items[0], "NEEDLE", s, sizeof s, &at, &len));
    assert(strncmp(s, "...", 3) == 0 && strcmp(s + strlen(s) - 3, "...") == 0);
    assert(strncmp(s + at, "needle", 6) == 0 && at < 60 && strlen(s) < 200);

    // A tiny buffer never overflows.
    char tiny[20];
    SearchSnippet(&items[0], "needle", tiny, sizeof tiny, &at, &len);
    assert(strlen(tiny) < sizeof tiny && at + len <= (int)strlen(tiny));
}

static void TestRealLessons(void)
{
    LessonSet set;
    assert(LessonsLoad(&set, "assets/lessons"));
    static int out[1024];
    int c = SearchLessons(&set, "pointer", SEARCH_C, out, 1024);
    int cpp = SearchLessons(&set, "pointer", SEARCH_CPP, out, 1024);
    int both = SearchLessons(&set, "POINTER", SEARCH_BOTH, out, 1024);
    assert(c > 0 && cpp > 0 && both == c + cpp);
    for (int i = 0; i < both; i++) assert(out[i] >= 0 && out[i] < set.count);
    assert(SearchLessons(&set, "qwertyuiopzxcv", SEARCH_BOTH, out, 1024) == 0);
    LessonsUnload(&set);
}

int main(void)
{
    TestMatching();
    TestSnippets();
    TestRealLessons();
    puts("Search tests passed.");
    return 0;
}
