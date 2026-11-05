#ifndef NOTES_PANEL_H
#define NOTES_PANEL_H

#include "raylib.h"

#include <stdbool.h>

// The "My notes" panel beside a lesson: a multi-line text box for the student's own
// notes, with Save and Clear buttons and a status line. Notes save by themselves a moment
// after typing stops, with Ctrl+S or Save, and whenever the lesson is left. Reading and
// writing the files is notes.h's job; this file draws and handles input.

// Where notes are kept: the signed-in user's notes folder ("" or NULL: notes can't be
// saved, and the panel says so). main.c sets it, so the panel doesn't depend on accounts.
void NotesPanelSetFolder(const char *folder);

// Shows a lesson's notes, saving the previous lesson's first if they changed.
void NotesPanelOpenLesson(const char *lessonId);

// Saves unsaved changes now, e.g. when leaving the lesson. Returns false if that failed
// (the panel shows the error).
bool NotesPanelFlush(void);

// Whether the panel is open beside the lesson. It stays open or closed across lessons.
bool NotesPanelShown(void);
void NotesPanelShow(bool shown);

// Whether the open lesson has any notes, for marking the Notes button.
bool NotesPanelHasNotes(void);

// The panel's width for a screen this wide, so the lesson can make room for it.
float NotesPanelWidth(float screenWidth);

// Draws the panel in r and handles its input. Call outside any scroll view.
void NotesPanelDraw(Rectangle r);

// Saves and frees the panel's text, at exit.
void NotesPanelShutdown(void);

// Dev aid for --shot: open the panel and put text in it (saved, so no "unsaved" note).
void NotesPanelDemo(const char *text);

#endif
