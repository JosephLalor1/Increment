// The free code editor: editors for code, input and output, running programs with the
// runner, saved programs and a per-user draft. See code_screen.h.
#include "sandbox/code_screen.h"
#include "core/text_copy.h"
#include "sandbox/code_library_view.h"
#include "code/drafts.h"
#include "ui/mascot.h"
#include "code/editor.h"
#include "ui/layout.h"
#include "code/runner.h"
#include "ui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *helloC =
    "#include <stdio.h>\n"
    "\n"
    "int main() {\n"
    "    printf(\"Hello from C!\\n\");\n"
    "    return 0;\n"
    "}\n";

static const char *helloCpp =
    "#include <iostream>\n"
    "using namespace std;\n"
    "\n"
    "int main() {\n"
    "    cout << \"Hello from C++!\" << endl;\n"
    "    return 0;\n"
    "}\n";

static bool lastCpp;  // the free editor reopens in the language used last time
static char draftFolder[600];  // where drafts are saved ("" = not saved); see CodeSetDraftFolder

static struct {
    bool ready;        // editors initialised
    bool open;
    bool cpp;
    bool libraryOpen;
    char savedName[CODE_NAME_MAX+1];
    char *original;        // what Reset restores: the template or the opened program

    Editor code, input, output;
    Runner runner;
    bool wasBusy;
    unsigned markedVersion;
    char warning[220];     // first compiler warning, shown under the status
} cs;

static void SetOriginal(const char *s)
{
    free(cs.original);
    cs.original = CopyText(s);
}

// ---------------------------------------------------------------------------
// The draft: one per language, so switching keeps both

static void SaveDraft(void)
{
    DraftSave(draftFolder, "playground", cs.cpp, cs.code.text, (size_t)cs.code.len);
}

// Loads this language's draft into the editor. Returns false when there's none.
static bool LoadDraft(void)
{
    char *text = DraftLoad(draftFolder, "playground", cs.cpp);
    if (!text) return false;
    EditorSetText(&cs.code, text);
    free(text);
    return true;
}

// ---------------------------------------------------------------------------
// Opening

static void Prepare(bool cpp)
{
    if (!cs.ready) {
        EditorInit(&cs.code, "", true);
        EditorInit(&cs.input, "", false);
        EditorInit(&cs.output, "", false);
        cs.output.readOnly = true;
        cs.input.placeholder = "Text your program reads with scanf or cin";
        cs.ready = true;
    }
    CodeClose();
    cs.open = true;
    cs.cpp = cpp;
    cs.savedName[0] = '\0';
    cs.warning[0] = '\0';
    cs.wasBusy = false;
    cs.runner.phase = RUN_IDLE;
    cs.runner.summary[0] = '\0';
    EditorSetText(&cs.input, "");
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);

    float size = 19 * UiTextScale();
    cs.code.fontSize = size;
    cs.input.fontSize = size * 0.92f;
    cs.output.fontSize = size * 0.92f;
    UiFocus(&cs.code);
}

void CodeOpenPlayground(void)
{
    Prepare(lastCpp);
    SetOriginal(cs.cpp ? helloCpp : helloC);
    if (!LoadDraft()) EditorSetText(&cs.code, cs.original);
}

void CodeSetDraftFolder(const char *folder)
{
    snprintf(draftFolder, sizeof draftFolder, "%s", folder ? folder : "");
}

void CodeClose(void)
{
    if (!cs.open) return;
    CodeLibraryViewClear();
    cs.libraryOpen = false;
    SaveDraft();
    RunnerCancel(&cs.runner);
    cs.open = false;
    UiFocus(NULL);
}

void CodeShutdown(void)
{
    CodeClose();
    if (!cs.ready) return;
    RunnerFree(&cs.runner);
    EditorFree(&cs.code);
    EditorFree(&cs.input);
    EditorFree(&cs.output);
    free(cs.original);
    cs.original = NULL;
    cs.ready = false;
}

// ---------------------------------------------------------------------------
// Running

static void StartRun(void)
{
    SaveDraft();
    cs.warning[0] = '\0';
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);
    RunnerStart(&cs.runner, cs.code.text, cs.cpp, cs.input.text);
}

void CodeRunNow(void) { StartRun(); }
bool CodeBusy(void) { return RunnerBusy(&cs.runner); }

// Called once when a run finishes: fills the output and marks error lines.
static void RunFinished(void)
{
    Runner *r = &cs.runner;
    if (r->result == RESULT_COMPILE_ERROR) {
        EditorSetText(&cs.output, r->compileOutput);
        int lines[RUNNER_MAX_DIAGS], n = 0;
        for (int i = 0; i < r->diagCount; i++) if (r->diags[i].error) lines[n++] = r->diags[i].line;
        EditorSetMarks(&cs.code, lines, n);
        cs.markedVersion = cs.code.version;
        if (n > 0) EditorSetCursor(&cs.code, EditorPosOf(&cs.code, lines[0] - 1, 0), false);
        return;
    }

    const char *out = r->programOutput ? r->programOutput : "";
    if (r->outputTruncated) {
        char *text = malloc(strlen(out) + 64);
        sprintf(text, "%s\n[output cut off: too much to show]", out);
        EditorSetText(&cs.output, text);
        free(text);
    } else {
        EditorSetText(&cs.output, out);
    }
    for (int i = 0; i < r->diagCount; i++) {
        if (!r->diags[i].error) {
            snprintf(cs.warning, sizeof cs.warning, "Warning on line %d: %s", r->diags[i].line, r->diags[i].message);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing

static float Label(const char *text, float x, float y)
{
    UiText(gFonts.bold, text, x, y, 17, COL_DIM);
    return y + 26;
}

static void StatusLine(float x, float y, float w)
{
    Runner *r = &cs.runner;
    if (RunnerBusy(r)) {
        int dots = (int)(GetTime() * 3) % 4;
        const char *label = r->phase == RUN_COMPILING ? "Compiling" : "Running";
        UiText(gFonts.text, TextFormat("%s%.*s", label, dots, "..."), x, y, 17, COL_DIM);
        return;
    }
    if (r->phase != RUN_DONE || !r->summary[0]) {
        UiText(gFonts.text, "Press Run (F5 or Ctrl+Enter) to compile and run your code.", x, y, 17, COL_DIM);
        return;
    }
    bool ok = r->result == RESULT_OK;
    Icon icon = ok ? ICON_CHECK : (r->result == RESULT_TIMEOUT ? ICON_RETRY : ICON_CROSS);
    float iconSize = 17 * 0.95f;
    IconDraw(icon, (Vector2){ x + iconSize / 2, y + 17 * 0.56f }, iconSize, ok ? COL_GOOD : COL_BAD);
    UiWrappedText(gFonts.text, r->summary, (Vector2){ x + 26, y }, w - 26, 17, ok ? COL_TEXT : COL_BAD, true);
}

static float StatusHeight(float w)
{
    if (cs.runner.phase != RUN_DONE || !cs.runner.summary[0]) return 24;
    return UiWrappedText(gFonts.text, cs.runner.summary, (Vector2){ 0, 0 }, w - 26, 17, COL_TEXT, false);
}

static bool SecondaryButton(Rectangle r, Icon icon, const char *label, bool enabled)
{
    return UiButtonIconEx(r, icon, label, 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, enabled);
}

static void SwitchLanguage(bool cpp)
{
    if (cpp == cs.cpp) return;
    SaveDraft();
    double now = GetTime();
    cs.cpp = cpp;
    cs.savedName[0] = '\0';
    SetOriginal(cpp ? helloCpp : helloC);
    if (!LoadDraft()) EditorReplaceAll(&cs.code, cs.original, now);
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);
    cs.runner.phase = RUN_IDLE;
    lastCpp = cpp;
}

bool CodeScreen(void)
{
    if (!cs.open) return true;
    if (cs.libraryOpen) {
        CodeLibraryAction action = CodeLibraryView();
        if (action.closed) {
            if(action.name[0]) snprintf(cs.savedName,sizeof cs.savedName,"%s",action.name);
            if (action.source) {
                SaveDraft();
                cs.cpp = lastCpp = action.cpp;  // the program replaces this language's draft
                EditorSetText(&cs.code, action.source);
                SetOriginal(action.source);
                EditorSetText(&cs.input, "");
                EditorSetText(&cs.output, "");
                cs.runner.phase = RUN_IDLE;
                cs.runner.summary[0] = cs.warning[0] = '\0';
                snprintf(cs.savedName, sizeof cs.savedName, "%s", action.name);
                free(action.source);
            }
            CodeLibraryViewClear();
            cs.libraryOpen = false;
            UiFocus(&cs.code);
        }
        return false;
    }
    if (TopBar("Code editor")) {
        CodeClose();
        return true;
    }

    // Finish runs, and drop error marks once the code has been edited.
    RunnerUpdate(&cs.runner);
    bool busy = RunnerBusy(&cs.runner);
    if (cs.wasBusy && !busy) RunFinished();
    cs.wasBusy = busy;
    if (cs.code.markCount && cs.code.version != cs.markedVersion) EditorSetMarks(&cs.code, NULL, 0);

    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pad = 16;
    float barY = TOPBAR_H + 12, barH = 42;
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Toolbar
    float x = pad;
    bool run = busy ? false : (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_ENTER)));
    if (busy) {
        if (UiButtonIconEx((Rectangle){ x, barY, 130, barH }, ICON_CROSS, "Stop", 20, COL_BAD,
                           ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true)) {
            RunnerCancel(&cs.runner);
            cs.wasBusy = false;
        }
    } else if (UiButtonIconEx((Rectangle){ x, barY, 130, barH }, ICON_PLAY, "Run", 20, COL_ACCENT, COL_ACCENT_HI,
                              COL_ON_ACCENT, true)) {
        run = true;
    }
    x += 142;
    if (SecondaryButton((Rectangle){ x, barY, 120, barH }, ICON_RETRY, "Reset", !busy)) {
        EditorReplaceAll(&cs.code, cs.original, GetTime());  // Ctrl+Z brings the old code back
    }
    x += 132;
    {
        static const char *langs[] = { "C", "C++" };
        for (int i = 0; i < 2; i++) {
            bool active = cs.cpp == (i == 1);
            Rectangle seg = { x + i * 76, barY, 70, barH };
            if (UiButtonEx(seg, langs[i], 19, active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT : COL_SECONDARY_HOVER,
                           active ? COL_ON_ACCENT : COL_TEXT, !busy) && !active) {
                SwitchLanguage(i == 1);
            }
        }
        x += 164;
        bool shortcut = ctrl && IsKeyPressed(KEY_S);
        if (SecondaryButton((Rectangle){x,barY,190,barH},ICON_BOOK,"Save / My programs",!busy && draftFolder[0]) ||
            (shortcut && !busy && draftFolder[0])) {
            SaveDraft();
            CodeLibraryViewOpen(draftFolder,cs.code.text,cs.cpp,cs.savedName);
            cs.libraryOpen = true;
            return false;
        }
        x += 202;
    }
    const char *hint = "F5 or Ctrl+Enter: run    Ctrl+Z: undo";
    float mascotSpace = MascotAvailable() ? 68 : 0;
    Rectangle foxBounds = { W - pad - 58, barY - 6, 58, 54 };
    if (mascotSpace && UiMouseOver(foxBounds))
        hint = "Let's build something awesome!";
    float hw = MeasureTextEx(gFonts.text, hint, 16, 0).x;
    if (W - pad - hw - mascotSpace > x + 20)
        UiText(gFonts.text, hint, W - pad - hw - mascotSpace, barY + 12, 16, COL_DIM);
    if (mascotSpace && W - pad - mascotSpace > x + 20) {
        MascotDrawActor(FOX_EDITOR, foxBounds, FOX_CODING);
    }

    // Columns: code on the left, input and output on the right.
    float top = barY + barH + 12, bottom = H - pad;
    float codeW = (W - pad * 3) * 0.58f;
    float rx = pad * 2 + codeW, rw = W - pad - rx;
    EditorWidget(&cs.code, (Rectangle){ pad, top, codeW, bottom - top });

    float y = Label("Input", rx, top);
    float inputH = 92;
    EditorWidget(&cs.input, (Rectangle){ rx, y, rw, inputH });
    y += inputH + 12;

    y = Label("Output", rx, y);
    StatusLine(rx, y, rw);
    y += StatusHeight(rw) + 6;
    if (cs.warning[0]) {
        y += UiWrappedText(gFonts.text, cs.warning, (Vector2){ rx, y }, rw, 16, COL_DIM, true) + 4;
    }
    if (bottom - y > 40) EditorWidget(&cs.output, (Rectangle){ rx, y, rw, bottom - y });

    if (run) StartRun();
    return false;
}
