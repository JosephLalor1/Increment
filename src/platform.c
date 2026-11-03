#include "platform.h"

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
