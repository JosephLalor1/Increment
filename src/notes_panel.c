// The lesson notes panel. See notes_panel.h.
#include "notes_panel.h"
#include "editor.h"
#include "notes.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTOSAVE_DELAY 1.0   // seconds after the last keystroke
#define SAVED_FRESH    4.0   // how long "Saved" stays green before it settles to grey

typedef enum {
    STATUS_NONE,     // nothing to report
    STATUS_SAVED,    // the last save worked (statusTime says when)
    STATUS_CLEARED,  // Clear emptied the notes and saved that
    STATUS_FAILED,   // the last save failed; error says why
} SaveStatus;

static struct {
    Editor editor;
    bool editorReady;
    bool shown;
    bool readOnly;            // the notes file couldn't be read, so saving would overwrite it
    char folder[1100];
    char lessonId[32];

    unsigned savedVersion;    // editor.version when the notes last matched the file
    unsigned seenVersion;     // editor.version last frame, to spot new edits
    double changedTime;       // when the last unsaved edit was made
    unsigned failedVersion;   // the version a save failed for: don't retry it every frame

    SaveStatus status;
    double statusTime;
    const char *error;        // static message from notes.c
} panel;

static void EnsureEditor(void)
{
    if (panel.editorReady) return;
    EditorInit(&panel.editor, "", false);
    panel.editor.placeholder = "Write explanations and reminders in your own words.";
    panel.editorReady = true;
}

static bool Dirty(void) { return panel.editorReady && panel.lessonId[0] && panel.editor.version != panel.savedVersion; }

static bool Save(void)
{
    if (!Dirty()) return true;
    if (panel.readOnly) return false;  // the error from loading is still showing
    const char *err = NULL;
    if (!NotesSave(panel.folder, panel.lessonId, panel.editor.text, &err)) {
        panel.status = STATUS_FAILED;
        panel.error = err;
        panel.failedVersion = panel.editor.version;
        return false;
    }
    panel.savedVersion = panel.editor.version;
    if (panel.status != STATUS_CLEARED || !NotesBlank(panel.editor.text)) panel.status = STATUS_SAVED;
    panel.statusTime = GetTime();
    return true;
}

void NotesPanelSetFolder(const char *folder)
{
    if (!folder) folder = "";
    if (strcmp(folder, panel.folder) == 0) return;
    // A different account: finish with this one's notes, and never let them be saved into
    // the new account's folder.
    NotesPanelFlush();
    snprintf(panel.folder, sizeof panel.folder, "%s", folder);
    panel.lessonId[0] = '\0';
    if (panel.editorReady) EditorSetText(&panel.editor, "");
    panel.savedVersion = panel.seenVersion = panel.editor.version;
    panel.status = STATUS_NONE;
}

void NotesPanelOpenLesson(const char *lessonId)
{
    NotesPanelFlush();
    EnsureEditor();
    snprintf(panel.lessonId, sizeof panel.lessonId, "%s", lessonId ? lessonId : "");
    panel.status = STATUS_NONE;
    panel.readOnly = false;

    const char *err = NULL;
    char *text = NotesLoad(panel.folder, panel.lessonId, &err);
    if (text) {
        EditorSetText(&panel.editor, text);
        free(text);
    } else {
        EditorSetText(&panel.editor, "");
        panel.readOnly = true;  // keep whatever is in the file rather than saving over it
        panel.status = STATUS_FAILED;
        panel.error = err;
    }
    panel.editor.readOnly = panel.readOnly;
    panel.savedVersion = panel.seenVersion = panel.editor.version;
    panel.failedVersion = 0;
}

bool NotesPanelFlush(void) { return Save(); }

bool NotesPanelShown(void) { return panel.shown; }

void NotesPanelShow(bool shown)
{
    if (!shown) {
        NotesPanelFlush();
        if (UiHasFocus(&panel.editor)) UiFocus(NULL);
    }
    panel.shown = shown;
}

bool NotesPanelHasNotes(void) { return panel.editorReady && !NotesBlank(panel.editor.text); }

float NotesPanelWidth(float screenWidth)
{
    float w = screenWidth * 0.34f;
    return w < 360 ? 360 : (w > 560 ? 560 : w);
}

void NotesPanelShutdown(void)
{
    NotesPanelFlush();
    if (panel.editorReady) EditorFree(&panel.editor);
    panel.editorReady = false;
}

void NotesPanelDemo(const char *text)
{
    EnsureEditor();
    panel.shown = true;
    if (text) {
        EditorSetText(&panel.editor, text);
        panel.savedVersion = panel.seenVersion = panel.editor.version;
        panel.status = STATUS_SAVED;
        panel.statusTime = GetTime();
    }
}

// Saves a moment after typing stops, so notes are kept without having to remember Save.
static void Autosave(void)
{
    double now = GetTime();
    if (panel.editor.version != panel.seenVersion) {
        panel.seenVersion = panel.editor.version;
        panel.changedTime = now;
    }
    bool alreadyFailed = panel.status == STATUS_FAILED && panel.failedVersion == panel.editor.version;
    if (Dirty() && !alreadyFailed && now - panel.changedTime >= AUTOSAVE_DELAY) Save();
}

// The status line: unsaved, saved, cleared, or why saving failed. Returns its height.
static float DrawStatus(float x, float y, float w)
{
    const float size = 16;
    if (panel.status == STATUS_FAILED) {
        const char *why = panel.error ? panel.error : "";
        const char *message = panel.readOnly ? TextFormat("Couldn't open your notes. %s The file is left as it is.", why)
                                             : TextFormat("Not saved. %s", why);
        IconDraw(ICON_CROSS, (Vector2){ x + 8, y + 9 }, 14, COL_BAD);
        return UiWrappedText(gFonts.text, message, (Vector2){ x + 24, y }, w - 24, size, COL_BAD, true);
    }
    if (Dirty()) {
        UiText(gFonts.text, "Unsaved changes", x, y, size, COL_DIM);
    } else if (panel.status == STATUS_CLEARED) {
        UiText(gFonts.text, "Notes cleared. Press Ctrl+Z to bring them back.", x, y, size, COL_DIM);
    } else if (panel.status == STATUS_SAVED) {
        bool fresh = GetTime() - panel.statusTime < SAVED_FRESH;
        UiIconText(ICON_CHECK, gFonts.text, fresh ? "Saved" : "All changes saved", x, y, size, fresh ? COL_GOOD : COL_DIM);
    } else if (!panel.folder[0]) {
        UiText(gFonts.text, "Sign in to save notes.", x, y, size, COL_DIM);
    }
    return size + 4;
}

void NotesPanelDraw(Rectangle r)
{
    EnsureEditor();
    DrawRectangleRec(r, COL_PANEL);
    DrawRectangle((int)r.x, (int)r.y, 1, (int)r.height, COL_PANEL_HI);
    float pad = 18, x = r.x + pad, w = r.width - pad * 2;

    UiIconText(ICON_NOTE, gFonts.bold, "My notes", x, r.y + 16, 22, COL_TEXT);
    if (UiButtonIconEx((Rectangle){ r.x + r.width - pad - 36, r.y + 12, 36, 36 }, ICON_CROSS, NULL, 18, COL_PANEL,
                       COL_SECONDARY_HOVER, COL_DIM, true)) {
        NotesPanelShow(false);
        return;
    }
    UiWrappedText(gFonts.text, "Only you can see these. They save as you type.", (Vector2){ x, r.y + 50 }, w, 16,
                  COL_DIM, true);

    float footer = 112;  // status line (up to three lines) and the buttons
    Rectangle box = { x, r.y + 82, w, r.height - 82 - footer };
    panel.editor.fontSize = 17 * UiTextScale();
    EditorWidget(&panel.editor, box);

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool saveNow = ctrl && IsKeyPressed(KEY_S);
    Autosave();

    float buttonsY = r.y + r.height - 56;
    DrawStatus(x, box.y + box.height + 12, w);

    bool hasText = !NotesBlank(panel.editor.text);
    Rectangle clear = { x + w - 96 - 10 - 96, buttonsY, 96, 40 };
    if (UiButtonIconEx(clear, ICON_TRASH, "Clear", 17, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT,
                       hasText && !panel.readOnly)) {
        EditorReplaceAll(&panel.editor, "", GetTime());  // undoable, so a mistaken Clear isn't a disaster
        UiFocus(&panel.editor);                         // so Ctrl+Z works straight away
        panel.status = STATUS_CLEARED;
        saveNow = true;
    }
    Rectangle save = { x + w - 96, buttonsY, 96, 40 };
    if (UiButtonIconEx(save, ICON_CHECK, "Save", 17, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, !panel.readOnly)) {
        saveNow = true;
    }
    if (saveNow) {
        if (!Dirty() && panel.status != STATUS_FAILED && panel.status != STATUS_CLEARED) {
            panel.status = STATUS_SAVED;  // nothing changed, but say so: pressing Save always answers
            panel.statusTime = GetTime();
        }
        Save();
    }
}
