// Windows services (random bytes, PBKDF2, files, processes, HTTPS). The only file that
// includes windows.h. See platform.h.
#include "core/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>
#include <commdlg.h>
#include <winhttp.h>

bool PlatformRandom(unsigned char *buf, size_t len)
{
    return BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool PlatformHashPassword(const char *password, const unsigned char *salt, size_t saltLen,
                          unsigned long iterations, unsigned char out[PASSWORD_HASH_LEN])
{
    BCRYPT_ALG_HANDLE alg;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return false;
    }
    NTSTATUS status = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)password, (ULONG)strlen(password), (PUCHAR)salt,
                                            (ULONG)saltLen, iterations, out, PASSWORD_HASH_LEN, 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return BCRYPT_SUCCESS(status);
}

// One dialog for both: saving asks before overwriting, opening needs an existing file.
static bool FileDialog(bool saving, const char *kind, const char *extension, const char *suggestedName, char *out,
                       size_t size)
{
    // The filter is two strings and an empty one, back to back: "Kind (*.ext)", "*.ext".
    char filter[200] = { 0 };
    int shown = snprintf(filter, sizeof filter - 2, "%s (*.%s)", kind, extension);
    snprintf(filter + shown + 1, sizeof filter - (size_t)shown - 3, "*.%s", extension);

    char path[MAX_PATH] = "";
    if (suggestedName) snprintf(path, sizeof path, "%s", suggestedName);
    OPENFILENAMEA dialog = { 0 };
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof path;
    dialog.lpstrDefExt = extension;
    // NOCHANGEDIR: the app finds its files relative to where it started.
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY
                 | (saving ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    BOOL chosen = saving ? GetSaveFileNameA(&dialog) : GetOpenFileNameA(&dialog);
    if (!chosen || strlen(path) >= size) return false;
    snprintf(out, size, "%s", path);
    return true;
}

bool PlatformSaveFileDialog(const char *kind, const char *extension, const char *suggestedName, char *out, size_t size)
{
    return FileDialog(true, kind, extension, suggestedName, out, size);
}

bool PlatformOpenFileDialog(const char *kind, const char *extension, char *out, size_t size)
{
    return FileDialog(false, kind, extension, NULL, out, size);
}

bool PlatformReplaceFile(const char *src, const char *dst)
{
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool PlatformRemoveDir(const char *dir)
{
    char pattern[MAX_PATH];
    if (snprintf(pattern, sizeof pattern, "%s\\*", dir) >= (int)sizeof pattern) return false;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char path[MAX_PATH];
            if (snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName) < (int)sizeof path) DeleteFileA(path);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return RemoveDirectoryA(dir) || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
}

bool PlatformTempDir(char *out, size_t size)
{
    DWORD n = GetTempPathA((DWORD)size, out);
    return n > 0 && n < size;
}

bool PlatformFindOnPath(const char *exe, char *out, size_t size)
{
    DWORD n = SearchPathA(NULL, exe, NULL, (DWORD)size, out, NULL);
    return n > 0 && n < size;
}

// ---------------------------------------------------------------------------
// HTTPS

struct PlatformHttp {
    HANDLE thread;
    volatile LONG done;
    wchar_t *host, *path, *headers;
    char *body;
    int status;
    char *response;
    char error[160];
};

static wchar_t *Widen(const char *s)
{
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static void HttpFail(PlatformHttp *h, const char *what)
{
    snprintf(h->error, sizeof h->error, "%s (error %lu)", what, GetLastError());
}

static DWORD WINAPI HttpWorker(LPVOID arg)
{
    PlatformHttp *h = arg;
    HINTERNET session = NULL, conn = NULL, req = NULL;
    size_t len = 0, cap = 4096;
    h->response = malloc(cap);
    h->response[0] = '\0';

    session = WinHttpOpen(L"Increment/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { HttpFail(h, "Could not start the network connection"); goto done; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
    conn = WinHttpConnect(session, h->host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!conn) { HttpFail(h, "Could not reach the email service"); goto done; }
    req = WinHttpOpenRequest(conn, L"POST", h->path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             WINHTTP_FLAG_SECURE);
    if (!req) { HttpFail(h, "Could not create the request"); goto done; }

    DWORD bodyLen = (DWORD)strlen(h->body);
    if (!WinHttpSendRequest(req, h->headers ? h->headers : WINHTTP_NO_ADDITIONAL_HEADERS, (DWORD)-1L, h->body,
                            bodyLen, bodyLen, 0) ||
        !WinHttpReceiveResponse(req, NULL)) {
        HttpFail(h, "Could not connect to the email service. Check your internet connection");
        goto done;
    }

    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    DWORD avail;
    while (WinHttpQueryDataAvailable(req, &avail) && avail > 0 && len < 65536) {
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            h->response = realloc(h->response, cap);
        }
        DWORD got = 0;
        if (!WinHttpReadData(req, h->response + len, avail, &got) || got == 0) break;
        len += got;
    }
    h->response[len] = '\0';
    h->status = (int)status;

done:
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    if (session) WinHttpCloseHandle(session);
    InterlockedExchange(&h->done, 1);
    return 0;
}

PlatformHttp *PlatformHttpPostStart(const char *host, const char *path, const char *extraHeaders, const char *body)
{
    PlatformHttp *h = calloc(1, sizeof *h);
    h->host = Widen(host);
    h->path = Widen(path);
    h->headers = Widen(extraHeaders);
    h->body = _strdup(body ? body : "");
    h->thread = CreateThread(NULL, 0, HttpWorker, h, 0, NULL);
    if (!h->thread) {
        HttpFail(h, "Could not start the network request");
        h->response = _strdup("");
        h->done = 1;
    }
    return h;
}

bool PlatformHttpDone(const PlatformHttp *h) { return h->done != 0; }
int PlatformHttpStatus(const PlatformHttp *h) { return h->done ? h->status : 0; }
const char *PlatformHttpResponse(const PlatformHttp *h) { return h->done && h->response ? h->response : ""; }
const char *PlatformHttpError(const PlatformHttp *h) { return h->error; }

void PlatformHttpFree(PlatformHttp *h)
{
    if (!h) return;
    if (h->thread) {
        WaitForSingleObject(h->thread, INFINITE);  // requests time out within ~30 s
        CloseHandle(h->thread);
    }
    free(h->host);
    free(h->path);
    free(h->headers);
    free(h->body);
    free(h->response);
    free(h);
}

// ---------------------------------------------------------------------------
// Child processes

struct PlatformProcess {
    HANDLE process;
    HANDLE job;
    HANDLE outRead;  // our end of the child's stdout/stderr pipe
    char *output;
    size_t outLen;
    bool truncated;
    bool running;
    DWORD exitCode;
};

void PlatformPrependPath(const char *dir)
{
    char old[32767];
    DWORD n = GetEnvironmentVariableA("PATH", old, sizeof old);
    if (n >= sizeof old) return;
    if (n == 0) old[0] = '\0';
    size_t dlen = strlen(dir);
    if (strncmp(old, dir, dlen) == 0 && (old[dlen] == ';' || old[dlen] == '\0')) return;  // already first
    char *path = malloc(dlen + 1 + strlen(old) + 1);
    sprintf(path, "%s;%s", dir, old);
    SetEnvironmentVariableA("PATH", path);
    free(path);
}

PlatformProcess *PlatformProcessStart(const char *cmdline, const char *workDir, const char *stdinText,
                                      char *err, size_t errSize)
{
    // Children inherit the error mode. Without this, a crashing program waits on Windows Error
    // Reporting instead of exiting, and only the time limit stops it.
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE outRead = NULL, outWrite = NULL, inRead = NULL, inWrite = NULL;
    size_t inLen = stdinText ? strlen(stdinText) : 0;

    // The stdin pipe's buffer holds all the input, so writing it never blocks on the child.
    if (!CreatePipe(&outRead, &outWrite, &sa, 1 << 16) || !CreatePipe(&inRead, &inWrite, &sa, (DWORD)inLen + 4096)) {
        snprintf(err, errSize, "Could not create pipes (error %lu).", GetLastError());
        if (outRead) { CloseHandle(outRead); CloseHandle(outWrite); }
        return NULL;
    }
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);  // only the child's ends are inherited
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);

    HANDLE job = CreateJobObjectA(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = { 0 };
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = PROCESS_MEMORY_MAX;
    if (job) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);

    STARTUPINFOA si = { sizeof si };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inRead;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;
    PROCESS_INFORMATION pi;

    char *cmd = _strdup(cmdline);  // CreateProcess may modify the command line
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, workDir, &si, &pi);
    DWORD startError = GetLastError();
    free(cmd);
    CloseHandle(outWrite);  // the child holds its own copies now
    CloseHandle(inRead);
    if (!ok) {
        snprintf(err, errSize, "Could not start the program (error %lu).", startError);
        CloseHandle(outRead);
        CloseHandle(inWrite);
        if (job) CloseHandle(job);
        return NULL;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    if (inLen) {
        DWORD written;
        WriteFile(inWrite, stdinText, (DWORD)inLen, &written, NULL);
    }
    CloseHandle(inWrite);  // end of input: scanf / cin see EOF

    PlatformProcess *p = calloc(1, sizeof *p);
    p->process = pi.hProcess;
    p->job = job;
    p->outRead = outRead;
    p->output = malloc(1);
    p->output[0] = '\0';
    p->running = true;
    return p;
}

static void Drain(PlatformProcess *p)
{
    DWORD avail = 0;
    while (PeekNamedPipe(p->outRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        char buf[4096];
        DWORD got = 0;
        if (!ReadFile(p->outRead, buf, avail < sizeof buf ? avail : sizeof buf, &got, NULL) || got == 0) break;
        size_t room = PROCESS_OUTPUT_MAX - p->outLen;
        size_t keep = got < room ? got : room;
        if (keep < got) p->truncated = true;
        if (keep) {
            p->output = realloc(p->output, p->outLen + keep + 1);
            memcpy(p->output + p->outLen, buf, keep);
            p->outLen += keep;
            p->output[p->outLen] = '\0';
        }
    }
}

bool PlatformProcessUpdate(PlatformProcess *p)
{
    if (!p->running) return false;
    Drain(p);
    if (WaitForSingleObject(p->process, 0) == WAIT_OBJECT_0) {
        Drain(p);  // anything written just before exiting
        GetExitCodeProcess(p->process, &p->exitCode);
        p->running = false;
    }
    return p->running;
}

const char *PlatformProcessOutput(const PlatformProcess *p) { return p->output; }
bool PlatformProcessTruncated(const PlatformProcess *p) { return p->truncated; }
unsigned long PlatformProcessExitCode(const PlatformProcess *p) { return p->exitCode; }

void PlatformProcessKill(PlatformProcess *p)
{
    if (!p->running) return;
    if (p->job) TerminateJobObject(p->job, 1);
    else TerminateProcess(p->process, 1);
    WaitForSingleObject(p->process, 2000);
    Drain(p);
    p->running = false;
    p->exitCode = 1;
}

void PlatformProcessFree(PlatformProcess *p)
{
    if (!p) return;
    PlatformProcessKill(p);
    CloseHandle(p->process);
    CloseHandle(p->outRead);
    if (p->job) CloseHandle(p->job);
    free(p->output);
    free(p);
}
