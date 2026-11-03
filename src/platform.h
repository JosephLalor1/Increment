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

#endif
