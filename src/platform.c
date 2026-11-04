#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

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
