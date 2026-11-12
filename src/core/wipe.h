#ifndef WIPE_H
#define WIPE_H

#include <stddef.h>

// Zeroes memory that held a secret (password, recovery code, one-time code) once it's no
// longer needed. Writing through a volatile pointer stops the compiler from removing the
// wipe of a buffer that is never read again.
static inline void SecureWipe(void *buffer, size_t size)
{
    volatile unsigned char *p = buffer;
    while (size--) *p++ = 0;
}

// Wipes a whole fixed-size array (not a pointer).
#define WIPE(array) SecureWipe((array), sizeof (array))

#endif
