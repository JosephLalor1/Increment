// Tests for src/email.c. Run via test.bat.
// Set RESEND_LIVE_TEST=1 to also send one real email through Resend (to its simulated
// delivered@resend.dev inbox) using build/email_config.txt.
#include "../src/email.h"
#include "../src/email_address.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while (0)

static EmailState WaitForSend(void)
{
    double start = GetTime();
    EmailState s;
    while ((s = EmailUpdate()) == EMAIL_SENDING && GetTime() - start < 60) WaitTime(0.05);
    return s;
}

int main(int argc, char **argv)
{
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "test_email");  // GetTime needs the window system

    // Address validation
    CHECK(EmailAddressValid("ada@example.com"));
    CHECK(EmailAddressValid("first.last+tag@sub.example.co.uk"));
    CHECK(!EmailAddressValid("ada"));
    CHECK(!EmailAddressValid("ada@"));
    CHECK(!EmailAddressValid("@example.com"));
    CHECK(!EmailAddressValid("ada@example"));
    CHECK(!EmailAddressValid("ada@example."));
    CHECK(!EmailAddressValid("ada@@example.com"));
    CHECK(!EmailAddressValid("a da@example.com"));
    CHECK(!EmailAddressValid("ada@exa\"mple.com"));
    CHECK(!EmailAddressValid("Ada <ada@example.com>"));

    // Address masking, for showing where a code went
    char masked[64];
    EmailAddressMask("ada.lovelace@example.com", masked, sizeof masked);
    CHECK(strcmp(masked, "a***@example.com") == 0);
    EmailAddressMask("not-an-address", masked, sizeof masked);
    CHECK(strcmp(masked, "your email") == 0);
    EmailAddressMask("@example.com", masked, sizeof masked);
    CHECK(strcmp(masked, "your email") == 0);

    // JSON helpers
    char buf[128];
    EmailJsonEscape("say \"hi\"\\\nnext\t", buf, sizeof buf);
    CHECK(strcmp(buf, "say \\\"hi\\\"\\\\\\nnext\\u0009") == 0);
    EmailJsonField("{\"statusCode\":403,\"message\":\"You can only send \\\"testing\\\" emails\",\"name\":\"x\"}",
                   "message", buf, sizeof buf);
    CHECK(strcmp(buf, "You can only send \"testing\" emails") == 0);
    EmailJsonField("{\"id\":\"abc-123\"}", "id", buf, sizeof buf);
    CHECK(strcmp(buf, "abc-123") == 0);
    EmailJsonField("{\"id\":\"abc\"}", "message", buf, sizeof buf);
    CHECK(buf[0] == '\0');

    // Test outbox: the email is written to a file instead of being sent
    if (argc > 1) {
        char outbox[600];
        snprintf(outbox, sizeof outbox, "%s/outbox", argv[1]);
        _putenv_s("INCREMENT_EMAIL_OUTBOX", outbox);
        EmailInit(NULL, 0);
        CHECK(EmailConfigured());
        CHECK(strcmp(EmailOutboxPath(), outbox) == 0);
        EmailSendCode(EMAIL_VERIFY, "ada@example.com", "ada", "482913");
        CHECK(WaitForSend() == EMAIL_SENT);
        char *mail = LoadFileText(TextFormat("%s/ada.txt", outbox));
        CHECK(mail && strstr(mail, "To: ada@example.com") && strstr(mail, "482913"));
        UnloadFileText(mail);

        EmailSendCode(EMAIL_VERIFY, "not-an-address", "ada", "111111");
        CHECK(WaitForSend() == EMAIL_FAILED && strstr(EmailError(), "doesn't look right"));
        _putenv_s("INCREMENT_EMAIL_OUTBOX", "");
    }

    // No config and no outbox: sending fails with a clear message, without touching the network
    EmailInit(NULL, 0);
    CHECK(!EmailConfigured());
    CHECK(EmailOutboxPath()[0] == '\0');
    EmailSendCode(EMAIL_VERIFY, "ada@example.com", "ada", "123456");
    CHECK(WaitForSend() == EMAIL_FAILED && strstr(EmailError(), "email_config.txt"));

    // Optional live send through Resend
    const char *live = getenv("RESEND_LIVE_TEST");
    if (live && live[0] == '1') {
        const char *dirs[] = { "build" };
        EmailInit(dirs, 1);
        CHECK(EmailConfigured());
        EmailSendCode(EMAIL_RESET, "delivered@resend.dev", "livetest", "123456");
        EmailState s = WaitForSend();
        if (s != EMAIL_SENT) printf("live send failed: %s\n", EmailError());
        CHECK(s == EMAIL_SENT);
        printf("live send: %s\n", s == EMAIL_SENT ? "accepted by Resend" : "failed");
    }

    EmailShutdown();
    CloseWindow();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("All email tests passed\n");
    return 0;
}
