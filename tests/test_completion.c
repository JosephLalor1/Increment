// Tests for name completion (src/completion.c). Run via test.bat.
#include "../src/completion.h"

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

// Completions for code with the caret where its '|' is.
static CompletionList Find(const char *codeWithCaret)
{
    char code[1024];
    const char *caret = strchr(codeWithCaret, '|');
    int cursor = (int)(caret - codeWithCaret);
    snprintf(code, sizeof code, "%.*s%s", cursor, codeWithCaret, caret + 1);
    CompletionList list;
    CompletionFind(code, (int)strlen(code), cursor, &list);
    return list;
}

static bool Has(const CompletionList *list, const char *name)
{
    for (int i = 0; i < list->count; i++)
        if (strcmp(list->items[i].name, name) == 0) return true;
    return false;
}

static const Completion *Item(const CompletionList *list, const char *name)
{
    for (int i = 0; i < list->count; i++)
        if (strcmp(list->items[i].name, name) == 0) return &list->items[i];
    return NULL;
}

static void TestNamesFromTheCode(void)
{
    // The code's own names come first, the nearest first; then the language's.
    CompletionList list = Find("int total = 0;\nint tally = 1;\nta|");
    CHECK(list.typed == 2);
    CHECK(list.count == 1 && strcmp(list.items[0].name, "tally") == 0);
    list = Find("int count = 0;\nint counter = 1;\nint x = co|");
    CHECK(list.count >= 3);
    CHECK(strcmp(list.items[0].name, "counter") == 0);
    CHECK(strcmp(list.items[1].name, "count") == 0);
    CHECK(Has(&list, "continue") && Has(&list, "const"));

    // A name is never offered for itself, nor a shorter one.
    list = Find("int age = 1;\nage|");
    CHECK(!Has(&list, "age"));
    // Names used after the caret count too.
    list = Find("su|\nint sum = 0;");
    CHECK(Has(&list, "sum"));
    // Each name once, however often it is used.
    list = Find("int width; width = 2; width++;\nwi|");
    CHECK(list.count == 1);
}

static void TestWhenNothingIsOffered(void)
{
    CHECK(Find("int total;\nt|").count == 0);             // one letter is too few
    CHECK(Find("int total;\nto|x").count == 0);           // in the middle of a word
    CHECK(Find("int total;\nprintf(\"to|\");").count == 0);  // in a string
    CHECK(Find("int total;\n// to|").count == 0);         // in a comment
    CHECK(Find("int total;\n/* to| */").count == 0);
    CHECK(Find("int total;\nchar c = 'to|").count == 0);
    CHECK(Find("int x = 12|").count == 0);                // a number
    CHECK(Find("zz|").count == 0);                        // nothing starts like that
    // Names inside strings and comments aren't names.
    CompletionList list = Find("printf(\"Grade: %c\");  // Great\nGr|");
    CHECK(list.count == 0);
}

static void TestShapes(void)
{
    // printf comes with its brackets and quotes, the caret between the quotes.
    CompletionList list = Find("pri|");
    const Completion *printfItem = Item(&list, "printf");
    CHECK(printfItem && strcmp(printfItem->after, "(\"\")") == 0 && printfItem->caretBack == 2);
    // A loop keyword comes with its condition's brackets.
    list = Find("wh|");
    CHECK(Item(&list, "while") && strcmp(Item(&list, "while")->after, " ()") == 0);
    // The code's own functions come with brackets; its variables don't.
    list = Find("int addTwo(int a) { return a + 2; }\nint addend = 1;\nint x = add|");
    CHECK(Item(&list, "addTwo") && strcmp(Item(&list, "addTwo")->after, "()") == 0 && Item(&list, "addTwo")->caretBack == 1);
    CHECK(Item(&list, "addend") && Item(&list, "addend")->after[0] == '\0');
    // No second pair of brackets when one already follows.
    list = Find("pri|(\"hi\");");
    CHECK(Item(&list, "printf") && Item(&list, "printf")->after[0] == '\0');
}

static void TestDirectivesAndHeaders(void)
{
    CompletionList list = Find("#inc|");
    CHECK(list.count == 1 && strcmp(list.items[0].name, "include") == 0);
    CHECK(strcmp(list.items[0].after, " <>") == 0 && list.items[0].caretBack == 1);
    // A header after "#include <", from one letter, closing the bracket if it is open.
    list = Find("#include <s|");
    CHECK(list.typed == 1);
    CHECK(Has(&list, "stdio.h") && Has(&list, "stdlib.h") && Has(&list, "string.h"));
    CHECK(strcmp(Item(&list, "stdio.h")->after, ">") == 0);
    list = Find("#include <stdi|>");
    CHECK(list.count == 1 && list.items[0].after[0] == '\0');
    // Headers aren't names elsewhere, and names aren't headers.
    list = Find("#include <stdio.h>\nint stdioCount;\nstd|");
    CHECK(list.count == 1 && strcmp(list.items[0].name, "stdioCount") == 0);
    CHECK(Find("int main;\n#include <ma|").count == 1);  // math.h only
}

static void TestCppWords(void)
{
    // C++ words are offered only in code that looks like C++.
    CompletionList list = Find("#include <stdio.h>\nco|");
    CHECK(!Has(&list, "cout"));
    list = Find("#include <iostream>\nco|");
    CHECK(Has(&list, "cout"));
    list = Find("class Dog {};\npu|");
    CHECK(Has(&list, "public"));
}

int main(void)
{
    TestNamesFromTheCode();
    TestWhenNothingIsOffered();
    TestShapes();
    TestDirectivesAndHeaders();
    TestCppWords();
    if (failures) {
        printf("%d completion test(s) failed\n", failures);
        return 1;
    }
    printf("All completion tests passed\n");
    return 0;
}
