#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>

// OS services that need <windows.h>, kept out of raylib translation units
// (windows.h and raylib.h declare clashing names like Rectangle and CloseWindow).

#define PASSWORD_HASH_LEN 32

// Fills buf with cryptographically secure random bytes.
bool PlatformRandom(unsigned char *buf, size_t len);

// PBKDF2-HMAC-SHA256 of password with salt; writes PASSWORD_HASH_LEN bytes to out.
bool PlatformHashPassword(const char *password, const unsigned char *salt, size_t saltLen,
                          unsigned long iterations, unsigned char out[PASSWORD_HASH_LEN]);

// Atomically replaces dst with src (both on the same volume).
bool PlatformReplaceFile(const char *src, const char *dst);

// Deletes every file in dir (not subfolders), then the folder itself. Missing dir is fine.
bool PlatformRemoveDir(const char *dir);

// Writes the system temp folder (with a trailing slash) to out.
bool PlatformTempDir(char *out, size_t size);

// Looks for an executable on PATH, e.g. "gcc.exe". Writes its full path to out.
bool PlatformFindOnPath(const char *exe, char *out, size_t size);

// The system's "Save as" and "Open" file dialogs, for one kind of file: kind is its name in
// the dialog ("Increment backup") and extension its ending without the dot. They block
// until the user chooses or cancels; true with the chosen path in out, false if cancelled.
// suggestedName is the file name the save dialog starts with.
bool PlatformSaveFileDialog(const char *kind, const char *extension, const char *suggestedName, char *out, size_t size);
bool PlatformOpenFileDialog(const char *kind, const char *extension, char *out, size_t size);

// ---------------------------------------------------------------------------
// HTTPS POST on a background thread (WinHTTP), so the UI never waits on the network.

typedef struct PlatformHttp PlatformHttp;

// Starts POST https://host/path. extraHeaders is "Name: value\r\n" lines (may be NULL).
PlatformHttp *PlatformHttpPostStart(const char *host, const char *path, const char *extraHeaders,
                                    const char *body);
bool PlatformHttpDone(const PlatformHttp *h);
int PlatformHttpStatus(const PlatformHttp *h);            // HTTP status, or 0 if the request failed
const char *PlatformHttpResponse(const PlatformHttp *h);  // response body ("" if none)
const char *PlatformHttpError(const PlatformHttp *h);     // network error text when status is 0
void PlatformHttpFree(PlatformHttp *h);                   // waits for the request to finish

// ---------------------------------------------------------------------------
// Child processes, for compiling and running learners' programs.
// The process runs without a console window, in a job object that limits its memory and
// lets the whole process tree be killed. stdout and stderr are captured together.

typedef struct PlatformProcess PlatformProcess;

// Puts dir at the front of PATH for this process and its children (once per dir), so a
// compiler finds its own tools before any other toolchain installed on the machine.
void PlatformPrependPath(const char *dir);

#define PROCESS_OUTPUT_MAX (256 * 1024)     // output beyond this is dropped
#define PROCESS_MEMORY_MAX (512u << 20)     // per-process memory limit, bytes

// Starts cmdline in workDir and feeds it stdinText (may be NULL), then closes its stdin.
// Returns NULL on failure and writes a message to err.
PlatformProcess *PlatformProcessStart(const char *cmdline, const char *workDir, const char *stdinText,
                                      char *err, size_t errSize);

// Collects any new output. Returns true while the process is still running.
bool PlatformProcessUpdate(PlatformProcess *p);

const char *PlatformProcessOutput(const PlatformProcess *p);  // '\0'-terminated
bool PlatformProcessTruncated(const PlatformProcess *p);      // output hit PROCESS_OUTPUT_MAX
unsigned long PlatformProcessExitCode(const PlatformProcess *p);
void PlatformProcessKill(PlatformProcess *p);
void PlatformProcessFree(PlatformProcess *p);  // kills it if still running

#endif
