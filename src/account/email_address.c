// Email address validation and masking. See email_address.h.
#include "account/email_address.h"

#include <stdio.h>
#include <string.h>

bool EmailAddressValid(const char *address)
{
    size_t length = strlen(address);
    if (length < 3 || length > EMAIL_MAX) return false;

    const char *at = strchr(address, '@');
    if (!at || at == address || strchr(at + 1, '@')) return false;  // exactly one '@', not first
    const char *lastDot = strrchr(at, '.');
    if (!lastDot || lastDot == at + 1 || lastDot[1] == '\0') return false;  // need name.tld after '@'

    for (const char *p = address; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool forbidden = c <= ' ' || c >= 127 || c == '"' || c == '\\' || c == '<' || c == '>' || c == ',';
        if (forbidden) return false;
    }
    return true;
}

void EmailAddressMask(const char *address, char *out, size_t size)
{
    const char *at = strchr(address, '@');
    if (!at || at == address) {
        snprintf(out, size, "your email");
        return;
    }
    snprintf(out, size, "%c***%s", address[0], at);
}
