#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>

// Best quiz score per lesson id, stored as "id percent" lines in a text file.
void ProgressLoad(const char *path);
bool ProgressSave(void);
int ProgressBest(const char *id);              // -1 if never attempted
void ProgressRecord(const char *id, int percent); // keeps the best score and saves

#endif
