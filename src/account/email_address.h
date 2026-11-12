#ifndef EMAIL_ADDRESS_H
#define EMAIL_ADDRESS_H

#include <stdbool.h>
#include <stddef.h>

// Plain helpers for email addresses, shared by the account store (which validates the
// addresses it saves), the email sender, and the screens (which mask addresses on screen).
// No I/O, so anything can use them without pulling in networking.

#define EMAIL_MAX 254  // longest address allowed by the email standards

// A practical check, not the full RFC: one '@', a name before it, a dot-separated domain
// after it, and no spaces, quotes, angle brackets, commas or non-ASCII characters.
bool EmailAddressValid(const char *address);

// "ada.lovelace@example.com" -> "a***@example.com", so a screen can say where a code went
// without revealing the whole address. Writes "your email" if address has no name part.
void EmailAddressMask(const char *address, char *out, size_t size);

#endif
