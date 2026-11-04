// Helpers shared by the account screens, and the --shot demo entry point for all of them.
#include "account_screens.h"
#include "account_screens_shared.h"
#include "email.h"
#include "raylib.h"  // TextFormat

const char *CodeSentNotice(void)
{
    return EmailOutboxPath()[0] ? "Code saved to the local outbox. No email was sent."
                                : "Code sent. Check your inbox (and spam folder).";
}

const char *ResendWaitText(bool sending, double secondsLeft)
{
    return sending ? "Sending..." : TextFormat("Send again in %d s", (int)secondsLeft + 1);
}

void AccountScreensDemo(const char *what)
{
    LoginDemo(what);
    VerifyDemo(what);
    RecoveryDemo(what);
    SettingsDemo(what);
}
