// Shows a newly issued recovery code once, until the user confirms they have saved it.
#include "account/account_screens.h"
#include "account/account_screens_shared.h"
#include "ui/ui.h"

#include <stdio.h>
#include <string.h>

static struct {
    char code[RECOVERY_CODE_SIZE];  // the code to show; wiped once saved
    bool pending;                   // a code is waiting to be shown
    bool copied;                    // "Copy code" was pressed (the button then says "Copied")
} recovery;

void RecoveryOffer(const char code[RECOVERY_CODE_SIZE])
{
    snprintf(recovery.code, sizeof recovery.code, "%s", code);
    recovery.pending = true;
    recovery.copied = false;
}

void RecoveryDismiss(void)
{
    WIPE(recovery.code);
    recovery.pending = false;
    recovery.copied = false;
}

bool RecoveryPending(void) { return recovery.pending; }

AccountAction RecoveryScreen(void)
{
    float cx = GetScreenWidth() / 2.0f;
    float w = 640;
    float x = cx - w / 2;
    float y = GetScreenHeight() / 2.0f - 190;  // leaves room for the key badge above the title

    DrawCircleV((Vector2){ cx, y - 66 }, 42, COL_NOTE_BG);
    IconDraw(ICON_KEY, (Vector2){ cx, y - 66 }, 48, COL_ACCENT);
    UiTextCentered(gFonts.bold, "Save your recovery code", cx, y, 40, COL_TEXT);
    y += 70;
    y += UiWrappedText(gFonts.text,
                       "If you forget your password, this code is the only way back into your account. "
                       "Write it down and keep it somewhere safe. It won't be shown again, and it works once: "
                       "using it gives you a new code.",
                       (Vector2){ x, y }, w, 20, COL_DIM, true) + 28;

    Rectangle box = { x, y, w, 96 };
    UiRoundRect(box, 12, COL_CODE_BG);
    UiTextCentered(gFonts.mono, recovery.code, cx, y + 24, 44, COL_ACCENT);
    y += 96 + 32;

    float buttonW = 220, gap = 20;
    if (UiButtonIconEx((Rectangle){ cx - buttonW - gap / 2, y, buttonW, 52 }, recovery.copied ? ICON_CHECK : ICON_COPY,
                       recovery.copied ? "Copied" : "Copy code", 22, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT,
                       true)) {
        SetClipboardText(recovery.code);
        recovery.copied = true;
    }
    if (UiButtonIcon((Rectangle){ cx + gap / 2, y, buttonW, 52 }, ICON_CHECK, "I've saved it", true)) {
        RecoveryDismiss();
        return ACCOUNT_BACK;
    }
    return ACCOUNT_STAY;
}

void RecoveryDemo(const char *what)
{
    if (strcmp(what, "recovery") == 0) RecoveryOffer("K7QM-9XPA-2HDV-R4TN");
}

