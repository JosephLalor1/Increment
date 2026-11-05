// Lesson notes: saving, loading, clearing, separate files per account and lesson, and the
// errors a failed save reports. Usage: test_notes <empty temp dir>
#include "../src/notes.h"
#include "raylib.h"  // MakeDirectory, FileExists
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char root[1024];

static const char *Folder(const char *user)
{
    static char path[2][1100];
    static int which;
    which ^= 1;  // two calls can be live at once
    snprintf(path[which], sizeof path[which], "%s/%s_notes", root, user);
    return path[which];
}

static void ExpectNotes(const char *folder, const char *id, const char *expected)
{
    const char *err = NULL;
    char *text = NotesLoad(folder, id, &err);
    assert(text != NULL);
    assert(strcmp(text, expected) == 0);
    free(text);
}

static void TestSaveLoadAndClear(void)
{
    const char *ada = Folder("ada");
    MakeDirectory(ada);
    const char *err = NULL;
    ExpectNotes(ada, "c_11_pointers", "");  // nothing written yet

    assert(NotesSave(ada, "c_11_pointers", "* gives the value\n& gives the address\n", &err));
    ExpectNotes(ada, "c_11_pointers", "* gives the value\n& gives the address\n");
    assert(NotesSave(ada, "c_11_pointers", "Edited: pointers hold addresses", &err));  // editing replaces
    ExpectNotes(ada, "c_11_pointers", "Edited: pointers hold addresses");

    // Clearing (or leaving only whitespace) removes the file rather than keeping an empty one.
    char path[1200];
    snprintf(path, sizeof path, "%s/c_11_pointers.txt", ada);
    assert(FileExists(path));
    assert(NotesSave(ada, "c_11_pointers", "  \n\t\n", &err));
    assert(!FileExists(path));
    ExpectNotes(ada, "c_11_pointers", "");
    assert(NotesSave(ada, "c_11_pointers", "", &err));  // clearing again is fine
}

static void TestSeparatePerAccountAndLesson(void)
{
    const char *ada = Folder("ada"), *grace = Folder("grace");
    MakeDirectory(grace);
    const char *err = NULL;
    assert(NotesSave(ada, "c_01_hello", "Ada's hello notes", &err));
    assert(NotesSave(ada, "cpp_02_references", "Ada's reference notes", &err));
    assert(NotesSave(grace, "c_01_hello", "Grace's hello notes", &err));
    ExpectNotes(ada, "c_01_hello", "Ada's hello notes");
    ExpectNotes(ada, "cpp_02_references", "Ada's reference notes");
    ExpectNotes(grace, "c_01_hello", "Grace's hello notes");
    ExpectNotes(grace, "cpp_02_references", "");
}

static void TestFailures(void)
{
    const char *err = NULL;
    // Signed out: no folder.
    assert(!NotesSave("", "c_01_hello", "text", &err) && err && strstr(err, "signed in"));
    err = NULL;
    assert(NotesLoad(NULL, "c_01_hello", &err) == NULL && err);

    // A folder that doesn't exist (e.g. deleted underneath us): the write fails with a message.
    err = NULL;
    assert(!NotesSave(Folder("nobody"), "c_01_hello", "text", &err) && err && err[0]);

    // Ids that would escape the folder or make a bad file name are refused.
    const char *ada = Folder("ada");
    err = NULL;
    assert(!NotesSave(ada, "../escape", "text", &err) && err);
    err = NULL;
    assert(!NotesSave(ada, "", "text", &err) && err);

    // Too long to save.
    char *huge = malloc(NOTES_MAX_BYTES + 2);
    memset(huge, 'x', NOTES_MAX_BYTES + 1);
    huge[NOTES_MAX_BYTES + 1] = '\0';
    err = NULL;
    assert(!NotesSave(ada, "c_02_variables", huge, &err) && err);
    free(huge);
}

static void TestWindowsLineEndings(void)
{
    // A notes file edited in Notepad comes back with plain '\n' line endings.
    char path[1200];
    snprintf(path, sizeof path, "%s/c_03_input.txt", Folder("ada"));
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs("line one\r\nline two\r\n", f);
    fclose(f);
    ExpectNotes(Folder("ada"), "c_03_input", "line one\nline two\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_notes <empty temp dir>\n");
        return 2;
    }
    snprintf(root, sizeof root, "%s", argv[1]);
    SetTraceLogLevel(LOG_WARNING);
    TestSaveLoadAndClear();
    TestSeparatePerAccountAndLesson();
    TestFailures();
    TestWindowsLineEndings();
    printf("notes: all tests passed\n");
    return 0;
}
