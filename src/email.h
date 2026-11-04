#ifndef EMAIL_H
#define EMAIL_H

#include <stdbool.h>
#include <stddef.h>

// Sends verification codes through the Resend API (https://resend.com) in the background.
//
// Settings come from email_config.txt (never committed; see .gitignore):
//   api_key re_...                         a sending-only Resend key
//   from    Increment <onboarding@resend.dev>
// In Resend's test mode the onboarding@resend.dev sender can only deliver to the Resend
// account owner's own address; a verified domain lifts that.
//
// For tests and screenshots, set INCREMENT_EMAIL_OUTBOX to a folder: emails are then
// written there as text files instead of being sent.

typedef enum { EMAIL_IDLE, EMAIL_SENDING, EMAIL_SENT, EMAIL_FAILED } EmailState;

typedef enum { EMAIL_VERIFY, EMAIL_RESET } EmailKind;  // which message the code goes in

// Loads email_config.txt from the first of these folders that has one.
void EmailInit(const char **dirs, int count);
bool EmailConfigured(void);  // a key was found, or the test outbox is set
const char *EmailOutboxPath(void); // nonempty in local development mode; never exposes the API key

// Starts sending a code. Only one send runs at a time; a new one replaces the old.
void EmailSendCode(EmailKind kind, const char *to, const char *username, const char *code);
EmailState EmailUpdate(void);  // call every frame; drives retries
const char *EmailError(void);  // why the last send failed

void EmailShutdown(void);

// JSON helpers for the Resend request and response, exposed for tests.
void EmailJsonEscape(const char *in, char *out, size_t size);
void EmailJsonField(const char *json, const char *field, char *out, size_t size);

#endif
