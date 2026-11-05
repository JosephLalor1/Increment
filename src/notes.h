#ifndef NOTES_H
#define NOTES_H

#include <stdbool.h>

// A student's own notes on a lesson, one text file per lesson in a per-account folder
// (users/<name>_notes/<lesson id>.txt, from AccountNotesDir). Plain data: nothing here
// draws, and the folder is passed in, so this module doesn't depend on accounts.

#define NOTES_MAX_BYTES (64 * 1024)  // about 30 pages of notes; longer files are refused

// Reads a lesson's notes into a malloc'd string (caller frees), "" when there are none yet.
// Returns NULL and sets *err when they can't be read.
char *NotesLoad(const char *folder, const char *lessonId, const char **err);

// Saves a lesson's notes, replacing the file in one step so a failed save never leaves
// half a file. Notes that are empty or only whitespace delete the file instead.
// Returns false and sets *err (a static, readable message) on failure.
bool NotesSave(const char *folder, const char *lessonId, const char *text, const char **err);

// Whether the text counts as no notes (empty or whitespace only).
bool NotesBlank(const char *text);

#endif
