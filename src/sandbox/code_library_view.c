// The page for saving the free editor's code under a name and opening saved programs.
// See code_library_view.h.
#include "sandbox/code_library_view.h"
#include "ui/layout.h"
#include "ui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char folder[600];              // the learner's code folder
    char name[CODE_NAME_MAX + 1];  // the name typed for saving
    char message[180];             // the result of the last save or open, "" for none
    const char *source;            // the editor's code, borrowed while the page is open
    bool cpp;                      // the editor's language
    bool replace;                  // the typed name exists: offer to replace it
    bool warning;                  // some saved file couldn't be read
    CodeSkills skills;             // the concepts in the editor's code
    SavedProgram *items;           // the saved programs
    int count;
    int pendingOpen;               // the program being asked about before opening, -1 for none
    float scroll, height;
} panel;
void CodeLibraryViewClear(void)
{
    free(panel.items);
    memset(&panel, 0, sizeof panel);
    UiFocus(NULL);
}

// Reads the list of saved programs again, after a save or when the page opens.
static void Reload(void)
{
    free(panel.items);
    panel.items = NULL;
    panel.pendingOpen = -1;
    panel.warning = !CodeLibraryList(panel.folder, &panel.items, &panel.count);
}

void CodeLibraryViewOpen(const char *folder, const char *source, bool cpp, const char *name)
{
    CodeLibraryViewClear();
    snprintf(panel.folder, sizeof panel.folder, "%s", folder ? folder : "");
    snprintf(panel.name, sizeof panel.name, "%s", name ? name : "");
    panel.source = source;
    panel.cpp = cpp;
    panel.pendingOpen = -1;
    panel.skills = CodeSkillsDetect(source, cpp);
    Reload();
}

// Draws the concepts found in a program as one line of text. Returns the height used.
static float Skills(const CodeSkills *skills, float x, float y, float w, float size)
{
    char text[1400] = "Detected concepts: ";
    int count = 0;
    for (int s = 0; s < SKILL_COUNT; s++)
        if (skills->present[s]) {
            size_t len = strlen(text);
            snprintf(text + len, sizeof text - len, "%s%s", count++ ? ", " : "", SkillTitle((Skill)s));
        }
    if (!count) strcat(text, skills->complete ? "No supported patterns found yet." : "Analysis unavailable.");
    return UiParagraph(text, x, y, w, size, COL_DIM);
}

CodeLibraryAction CodeLibraryView(void)
{
    CodeLibraryAction action = { 0 };
    if (TopBar("Saved Sandbox Programs")) {
        action.closed = true;
        if (CodeLibraryNameValid(panel.name)) snprintf(action.name, sizeof action.name, "%s", panel.name);
        return action;
    }
    Rectangle view = ScrollView();
    UpdateScroll(&panel.scroll, panel.height, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale(), top = view.y + 24 - panel.scroll, y = top;

    // Saving the editor's code under a name.
    y += UiParagraph("Keep what you create", x, y, w, 30 * scale, COL_TEXT) + 14;
    y +=
        UiParagraph(
            "Save a named copy of your sandbox code. Input and output are not included. Save changes before opening another program.",
            x, y, w, 19 * scale, COL_DIM) +
        16;
    y += UiParagraph(panel.cpp ? "Save current C++ program (.cpp)" : "Save current C program (.c)", x, y, w, 22 * scale,
              COL_TEXT) +
        10;
    char before[sizeof panel.name];  // to notice the name being edited
    memcpy(before, panel.name, sizeof before);
    bool enter = UiTextBox((Rectangle){ x, y, w - 156, 46 }, panel.name, sizeof panel.name, "Program name", false);
    bool valid = CodeLibraryNameValid(panel.name) && panel.folder[0];
    bool save = UiButtonIcon((Rectangle){ x + w - 144, y, 144, 46 }, ICON_BOOK, "Save copy", valid);
    if (strcmp(before, panel.name)) {
        panel.replace = false;
        panel.message[0] = 0;
    }
    y += 58;
    y += UiParagraph("Use letters, numbers, spaces, underscores or hyphens (up to 48 characters).", x, y, w, 17 * scale,
              COL_DIM) +
        12;
    if (save || (enter && valid)) {
        CodeSaveResult result = CodeLibrarySave(panel.folder, panel.name, panel.cpp, panel.source, false);
        panel.replace = result == CODE_SAVE_EXISTS;
        snprintf(panel.message, sizeof panel.message, "%s",
                 result == CODE_SAVE_OK ? "Program saved. Your skill XP and lesson progress are unchanged."
                     : result == CODE_SAVE_EXISTS
                     ? "That name already exists in this language. Replace it or choose another name."
                     : "Could not save. Check the folder and the 256 KiB source limit.");
        if (result == CODE_SAVE_OK) Reload();
    }
    if (panel.message[0]) y += UiParagraph(panel.message, x, y, w, 19 * scale, COL_ACCENT) + 12;
    if (panel.replace) {
        if (UiButtonIcon((Rectangle){ x, y, 220, 44 }, ICON_CHECK, "Replace existing", true)) {
            CodeSaveResult result = CodeLibrarySave(panel.folder, panel.name, panel.cpp, panel.source, true);
            snprintf(panel.message, sizeof panel.message, "%s",
                     result == CODE_SAVE_OK ? "Program replaced successfully."
                                            : "Could not replace the program. The previous copy was kept.");
            panel.replace = false;
            Reload();
        }
        if (UiButtonIcon((Rectangle){ x + 232, y, 132, 44 }, ICON_CROSS, "Cancel", true)) panel.replace = false;
        y += 58;
    }

    // What the editor's code uses, then the saved programs.
    y += Skills(&panel.skills, x, y, w, 18 * scale) + 10;
    y +=
        UiParagraph(
            "Concept hints use local syntax patterns, ignoring comments and strings. They may miss advanced code and do not prove correctness or mastery.",
            x, y, w, 17 * scale, COL_DIM) +
        24;
    y += UiParagraph("Your saved programs", x, y, w, 26 * scale, COL_TEXT) + 12;
    if (panel.warning)
        y += UiParagraph("Some files could not be read. Unsupported, binary or oversized files are omitted.", x, y, w,
                  18 * scale, COL_BAD) +
            12;
    if (!panel.count)
        y += UiParagraph("No saved programs yet. Give your code a name above to save the first one.", x, y, w, 20 * scale,
                  COL_DIM) +
            20;
    float listTop = y;

    // Opening replaces the editor's code, so it asks first, at the top of the list.
    if (panel.pendingOpen >= 0 && panel.pendingOpen < panel.count) {
        SavedProgram *item = &panel.items[panel.pendingOpen];
        y += UiParagraph(TextFormat("Open %s.%s? This replaces the editor contents. Save your current changes first.",
                             item->name, item->cpp ? "cpp" : "c"),
                  x, y, w, 19 * scale, COL_ACCENT) +
            10;
        if (UiButtonIcon((Rectangle){ x, y, 210, 44 }, ICON_BOOK, "Open program", true)) {
            action.source = CodeLibraryLoad(panel.folder, item->name, item->cpp);
            if (action.source) {
                action.closed = true;
                action.cpp = item->cpp;
                snprintf(action.name, sizeof action.name, "%s", item->name);
            } else
                snprintf(panel.message, sizeof panel.message,
                         "Could not open the saved program. Your editor contents were kept.");
            panel.pendingOpen = -1;
        }
        if (UiButtonIcon((Rectangle){ x + 222, y, 132, 44 }, ICON_CROSS, "Cancel", true)) panel.pendingOpen = -1;
        y += 58;
    }
    for (int i = 0; i < panel.count; i++) {
        SavedProgram *item = &panel.items[i];
        y += UiParagraph(TextFormat("%s.%s", item->name, item->cpp ? "cpp" : "c"), x, y, w, 23 * scale, COL_TEXT) + 8;
        y += Skills(&item->skills, x, y, w, 17 * scale) + 10;
        if (UiButtonIconEx((Rectangle){ x, y, 160, 40 }, ICON_BOOK, "Open", 19, COL_PANEL, COL_SECONDARY_HOVER,
                           COL_TEXT, true)) {
            panel.pendingOpen = i;
            panel.scroll = listTop - top;  // bring the question into view
            UiFocus(NULL);
        }
        y += 58;
    }
    panel.height = y - top + 30;
    EndView();
    PageScrollBar(view, &panel.scroll, panel.height);
    return action;
}
