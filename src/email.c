#include "email.h"
#include "platform.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ATTEMPTS 3

static struct {
    bool loaded;
    char apiKey[128];
    char from[160];
    char outbox[600];       // test mode: write emails here instead of sending

    EmailState state;
    PlatformHttp *http;
    int attempt;
    double retryAt;         // GetTime() for the next attempt, 0 if none pending
    char body[6144];        // JSON payload, kept for retries (fits the largest escaped fields)
    char headers[512];
    char error[300];
} em;

// ---------------------------------------------------------------------------
// Helpers

bool EmailAddressValid(const char *a)
{
    size_t n = strlen(a);
    if (n < 3 || n > 254) return false;
    const char *at = strchr(a, '@');
    if (!at || at == a || strchr(at + 1, '@')) return false;
    const char *dot = strrchr(at, '.');
    if (!dot || dot == at + 1 || dot[1] == '\0') return false;  // need name.tld after @
    for (const char *p = a; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= ' ' || c >= 127 || c == '"' || c == '\\' || c == '<' || c == '>' || c == ',') return false;
    }
    return true;
}

void EmailJsonEscape(const char *in, char *out, size_t size)
{
    size_t o = 0;
    for (; *in && o + 7 < size; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '"' || c == '\\') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c == '\n') {
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c < 32) {
            o += (size_t)snprintf(out + o, size - o, "\\u%04x", c);
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

// Pulls a top-level string field out of a small JSON object ("" if it isn't there).
void EmailJsonField(const char *json, const char *field, char *out, size_t size)
{
    out[0] = '\0';
    char key[64];
    snprintf(key, sizeof key, "\"%s\"", field);
    const char *p = strstr(json, key);
    if (!p) return;
    p = strchr(p + strlen(key), ':');
    if (!p) return;
    p++;
    while (*p == ' ') p++;
    if (*p != '"') return;
    p++;
    size_t o = 0;
    for (; *p && *p != '"' && o + 1 < size; p++) {
        if (*p == '\\' && p[1]) p++;
        out[o++] = *p;
    }
    out[o] = '\0';
}

static void LoadConfig(const char *path)
{
    char *text = LoadFileText(path);
    if (!text) return;
    char *line = strtok(text, "\r\n");
    while (line) {
        while (*line == ' ') line++;
        if (strncmp(line, "api_key ", 8) == 0) snprintf(em.apiKey, sizeof em.apiKey, "%s", line + 8);
        else if (strncmp(line, "from ", 5) == 0) snprintf(em.from, sizeof em.from, "%s", line + 5);
        line = strtok(NULL, "\r\n");
    }
    UnloadFileText(text);
    // Trim trailing spaces from the key
    for (size_t n = strlen(em.apiKey); n && em.apiKey[n - 1] == ' '; n--) em.apiKey[n - 1] = '\0';
}

void EmailInit(const char **dirs, int count)
{
    memset(&em, 0, sizeof em);
    snprintf(em.from, sizeof em.from, "Increment <onboarding@resend.dev>");
    const char *outbox = getenv("INCREMENT_EMAIL_OUTBOX");
    if (outbox && outbox[0]) snprintf(em.outbox, sizeof em.outbox, "%s", outbox);
    for (int i = 0; i < count && !em.apiKey[0]; i++) {
        const char *path = TextFormat("%s/email_config.txt", dirs[i]);
        if (FileExists(path)) LoadConfig(path);
    }
    em.loaded = true;
    if (!EmailConfigured()) TraceLog(LOG_WARNING, "EMAIL: no email_config.txt with an api_key found; email is off");
}

bool EmailConfigured(void) { return em.outbox[0] || em.apiKey[0]; }

// ---------------------------------------------------------------------------
// Sending

static void Fail(const char *msg)
{
    snprintf(em.error, sizeof em.error, "%s", msg);
    em.state = EMAIL_FAILED;
}

static void StartAttempt(void)
{
    em.attempt++;
    em.retryAt = 0;
    em.http = PlatformHttpPostStart("api.resend.com", "/emails", em.headers, em.body);
}

void EmailSendCode(EmailKind kind, const char *to, const char *username, const char *code)
{
    PlatformHttpFree(em.http);
    em.http = NULL;
    em.error[0] = '\0';
    em.attempt = 0;

    if (!EmailAddressValid(to)) {
        Fail("That email address doesn't look right.");
        return;
    }

    bool reset = kind == EMAIL_RESET;
    const char *what = reset ? "password reset code" : "verification code";
    const char *use = reset ? "Enter it in the app to choose a new password."
                            : "Enter it in the app to finish setting up your account.";
    const char *ignore = reset ? "If you didn't ask to reset your password, you can ignore this email. "
                                 "Your password hasn't changed."
                               : "If you didn't create an Increment account, you can ignore this email.";

    char subject[96], text[700], html[1800];
    snprintf(subject, sizeof subject, "Your Increment %s: %s", what, code);
    snprintf(text, sizeof text, "Hi %s,\n\nYour Increment %s is %s\n\n%s It expires in 15 minutes.\n\n%s", username,
             what, code, use, ignore);
    snprintf(html, sizeof html,
             "<div style=\"font-family:Segoe UI,Arial,sans-serif;max-width:480px;color:#1f2a44\">"
             "<p>Hi %s,</p><p>Your Increment %s is</p>"
             "<p style=\"font-size:32px;font-weight:600;letter-spacing:6px;margin:16px 0\">%s</p>"
             "<p>%s It expires in 15 minutes.</p>"
             "<p style=\"color:#5f6673;font-size:13px\">%s</p></div>",
             username, what, code, use, ignore);

    if (em.outbox[0]) {  // test mode: write the email to a file
        MakeDirectory(em.outbox);
        FILE *f = fopen(TextFormat("%s/%s%s.txt", em.outbox, username, reset ? "_reset" : ""), "wb");
        if (!f) {
            Fail("Could not write to the test outbox.");
            return;
        }
        fprintf(f, "To: %s\nFrom: %s\nSubject: %s\n\n%s\n", to, em.from, subject, text);
        fclose(f);
        em.state = EMAIL_SENT;
        return;
    }
    if (!em.apiKey[0]) {
        Fail("Email isn't set up: email_config.txt with a Resend api_key is missing.");
        return;
    }

    char eFrom[320], eTo[520], eSubject[200], eText[1300], eHtml[3300];
    EmailJsonEscape(em.from, eFrom, sizeof eFrom);
    EmailJsonEscape(to, eTo, sizeof eTo);
    EmailJsonEscape(subject, eSubject, sizeof eSubject);
    EmailJsonEscape(text, eText, sizeof eText);
    EmailJsonEscape(html, eHtml, sizeof eHtml);
    snprintf(em.body, sizeof em.body, "{\"from\":\"%s\",\"to\":[\"%s\"],\"subject\":\"%s\",\"text\":\"%s\",\"html\":\"%s\"}",
             eFrom, eTo, eSubject, eText, eHtml);

    // One idempotency key per send, reused across retries, so a retry never sends twice.
    unsigned char rnd[6];
    PlatformRandom(rnd, sizeof rnd);
    snprintf(em.headers, sizeof em.headers,
             "Authorization: Bearer %s\r\nContent-Type: application/json\r\n"
             "Idempotency-Key: %s/%s/%02x%02x%02x%02x%02x%02x\r\n",
             em.apiKey, reset ? "password-reset" : "verify-email", username, rnd[0], rnd[1], rnd[2], rnd[3], rnd[4],
             rnd[5]);
    em.state = EMAIL_SENDING;
    StartAttempt();
}

EmailState EmailUpdate(void)
{
    if (em.state != EMAIL_SENDING) return em.state;
    if (em.retryAt > 0) {
        if (GetTime() >= em.retryAt) StartAttempt();
        return em.state;
    }
    if (!em.http || !PlatformHttpDone(em.http)) return em.state;

    int status = PlatformHttpStatus(em.http);
    char message[240];
    EmailJsonField(PlatformHttpResponse(em.http), "message", message, sizeof message);
    char netError[160];
    snprintf(netError, sizeof netError, "%s", PlatformHttpError(em.http));
    PlatformHttpFree(em.http);
    em.http = NULL;

    if (status >= 200 && status < 300) {
        em.state = EMAIL_SENT;
        return em.state;
    }
    // Network errors, rate limits and server errors are worth retrying; other 4xx are not.
    bool retry = status == 0 || status == 429 || status >= 500;
    if (retry && em.attempt < MAX_ATTEMPTS) {
        em.retryAt = GetTime() + (em.attempt == 1 ? 1.0 : 3.0);
        return em.state;
    }
    if (status == 0) Fail(netError[0] ? netError : "Could not reach the email service.");
    else if (message[0]) Fail(message);
    else Fail(TextFormat("The email service returned an error (HTTP %d).", status));
    return em.state;
}

const char *EmailError(void) { return em.error; }

void EmailShutdown(void)
{
    PlatformHttpFree(em.http);
    em.http = NULL;
}
