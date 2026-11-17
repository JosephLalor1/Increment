// The free code editor: editors for code, test cases (each an input and, optionally, the
// output expected for it) and output, running programs on every test case, saved programs
// and a per-user draft. See code_screen.h.
#include "sandbox/code_screen.h"
#include "core/text_copy.h"
#include "sandbox/code_library_view.h"
#include "code/case_runner.h"
#include "code/drafts.h"
#include "ui/mascot.h"
#include "code/editor.h"
#include "ui/layout.h"
#include "code/runner.h"
#include "ui/ui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_MAX_CASES 6  // test cases in the free editor (at most CASE_RUNNER_MAX)

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

    Editor code, output;
    // The test cases: what the program reads, and what it should print (may be left empty).
    Editor inputs[CODE_MAX_CASES], expects[CODE_MAX_CASES];
    int caseCount;         // 1 to CODE_MAX_CASES
    int shownCase;         // the case whose input, expected output and output are on show
    int ranCases;          // how many cases the last run covered; 0 when there's nothing to show

    CaseRunner runner;
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
// Test cases

// Back to a single empty test case, and no results.
static void ClearCases(void)
{
    for (int i = 0; i < CODE_MAX_CASES; i++) {
        EditorSetText(&cs.inputs[i], "");
        EditorSetText(&cs.expects[i], "");
    }
    cs.caseCount = 1;
    cs.shownCase = 0;
    cs.ranCases = 0;
}

// Whether a case says what it should print (more than blank space).
static bool HasExpectation(int index)
{
    for (const char *p = cs.expects[index].text; *p; p++)
        if (!isspace((unsigned char)*p)) return true;
    return false;
}

// Whether case index ran and did what was asked of it: finished, and printed what it should
// (when that was given).
static bool CasePassed(int index)
{
    const CaseOutcome *outcome = &cs.runner.outcomes[index];
    if (outcome->result != RESULT_OK) return false;
    return !HasExpectation(index) || RunnerOutputMatches(outcome->output, cs.expects[index].text, NULL);
}

// Fills the output editor with what the case on show printed (or the compiler's messages).
static void ShowOutput(void)
{
    if (cs.shownCase >= cs.ranCases || CaseRunnerBusy(&cs.runner)) {
        EditorSetText(&cs.output, "");
        return;
    }
    const CaseOutcome *outcome = &cs.runner.outcomes[cs.shownCase];
    if (outcome->result == RESULT_COMPILE_ERROR) {
        EditorSetText(&cs.output, cs.runner.runner.compileOutput ? cs.runner.runner.compileOutput : "");
    } else if (outcome->truncated) {
        char *text = malloc(strlen(outcome->output) + 64);
        sprintf(text, "%s\n[output cut off: too much to show]", outcome->output);
        EditorSetText(&cs.output, text);
        free(text);
    } else {
        EditorSetText(&cs.output, outcome->output);
    }
}

static void ShowCase(int index)
{
    cs.shownCase = index;
    ShowOutput();
}

static void AddCase(void)
{
    if (cs.caseCount >= CODE_MAX_CASES) return;
    int added = cs.caseCount++;
    EditorSetText(&cs.inputs[added], "");
    EditorSetText(&cs.expects[added], "");
    ShowCase(added);
    UiFocus(&cs.inputs[added]);
}

// Removes the case on show; the ones after it move up. Results no longer line up with the
// cases, so they're dropped.
static void RemoveCase(void)
{
    if (cs.caseCount < 2) return;
    for (int i = cs.shownCase; i + 1 < cs.caseCount; i++) {
        EditorSetText(&cs.inputs[i], cs.inputs[i + 1].text);
        EditorSetText(&cs.expects[i], cs.expects[i + 1].text);
    }
    cs.caseCount--;
    if (cs.shownCase >= cs.caseCount) cs.shownCase = cs.caseCount - 1;
    cs.ranCases = 0;
    ShowOutput();
    UiFocus(&cs.code);
}

// ---------------------------------------------------------------------------
// Opening

static void Prepare(bool cpp)
{
    if (!cs.ready) {
        EditorInit(&cs.code, "", true);
        EditorInit(&cs.output, "", false);
        cs.output.readOnly = true;
        for (int i = 0; i < CODE_MAX_CASES; i++) {
            EditorInit(&cs.inputs[i], "", false);
            EditorInit(&cs.expects[i], "", false);
            cs.inputs[i].placeholder = "Text your program reads with scanf or cin";
            cs.expects[i].placeholder = "What it should print (optional)";
        }
        cs.ready = true;
    }
    CodeClose();
    cs.open = true;
    cs.cpp = cpp;
    cs.savedName[0] = '\0';
    cs.warning[0] = '\0';
    cs.wasBusy = false;
    ClearCases();
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);

    float size = 19 * UiTextScale();
    cs.code.fontSize = size;
    cs.output.fontSize = size * 0.92f;
    for (int i = 0; i < CODE_MAX_CASES; i++) cs.inputs[i].fontSize = cs.expects[i].fontSize = size * 0.92f;
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
    CaseRunnerCancel(&cs.runner);
    cs.open = false;
    UiFocus(NULL);
}

void CodeShutdown(void)
{
    CodeClose();
    if (!cs.ready) return;
    CaseRunnerFree(&cs.runner);
    EditorFree(&cs.code);
    EditorFree(&cs.output);
    for (int i = 0; i < CODE_MAX_CASES; i++) {
        EditorFree(&cs.inputs[i]);
        EditorFree(&cs.expects[i]);
    }
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
    const char *inputs[CODE_MAX_CASES];
    for (int i = 0; i < cs.caseCount; i++) inputs[i] = cs.inputs[i].text;
    cs.ranCases = cs.caseCount;
    CaseRunnerStart(&cs.runner, cs.code.text, cs.cpp, inputs, cs.caseCount);
}

void CodeRunNow(void) { StartRun(); }
bool CodeBusy(void) { return CaseRunnerBusy(&cs.runner); }

// Called once when a run finishes: marks error lines, shows the first case that went wrong
// (or the one on show), and the first compiler warning.
static void RunFinished(void)
{
    const Runner *build = &cs.runner.runner;
    if (cs.runner.buildFailed && cs.runner.outcomes[0].result == RESULT_COMPILE_ERROR) {
        int lines[RUNNER_MAX_DIAGS], n = 0;
        for (int i = 0; i < build->diagCount; i++) if (build->diags[i].error) lines[n++] = build->diags[i].line;
        EditorSetMarks(&cs.code, lines, n);
        cs.markedVersion = cs.code.version;
        if (n > 0) EditorSetCursor(&cs.code, EditorPosOf(&cs.code, lines[0] - 1, 0), false);
        ShowOutput();
        return;
    }
    int firstWrong = -1;
    for (int i = 0; i < cs.ranCases && firstWrong < 0; i++)
        if (!CasePassed(i)) firstWrong = i;
    ShowCase(firstWrong >= 0 ? firstWrong : cs.shownCase);
    for (int i = 0; i < build->diagCount; i++) {
        if (!build->diags[i].error) {
            snprintf(cs.warning, sizeof cs.warning, "Warning on line %d: %s", build->diags[i].line, build->diags[i].message);
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

// What the last run did with the case on show, in a line; NULL while running or before a run.
static const char *StatusText(void)
{
    const CaseOutcome *outcome = &cs.runner.outcomes[cs.shownCase];
    if (CaseRunnerBusy(&cs.runner) || cs.shownCase >= cs.ranCases || outcome->result == RESULT_NONE) return NULL;
    if (outcome->result == RESULT_OK && !CasePassed(cs.shownCase))  // ran, but printed something else
        return "The output doesn't match what this test case should print.";
    return outcome->summary;
}

// What the run did with the case on show.
static void StatusLine(float x, float y, float w)
{
    const Runner *r = &cs.runner.runner;
    if (CaseRunnerBusy(&cs.runner)) {
        int dots = (int)(GetTime() * 3) % 4;
        const char *label = r->phase == RUN_COMPILING ? "Compiling" : "Running";
        if (r->phase == RUN_RUNNING && cs.runner.count > 1)
            label = TextFormat("Running test case %d of %d", cs.runner.running + 1, cs.runner.count);
        UiText(gFonts.text, TextFormat("%s%.*s", label, dots, "..."), x, y, 17, COL_DIM);
        return;
    }
    const CaseOutcome *outcome = &cs.runner.outcomes[cs.shownCase];
    if (!StatusText()) {
        UiText(gFonts.text, "Press Run (F5 or Ctrl+Enter) to compile and run your code.", x, y, 17, COL_DIM);
        return;
    }
    bool ok = CasePassed(cs.shownCase);
    Icon icon = ok ? ICON_CHECK : (outcome->result == RESULT_TIMEOUT ? ICON_RETRY : ICON_CROSS);
    float iconSize = 17 * 0.95f;
    IconDraw(icon, (Vector2){ x + iconSize / 2, y + 17 * 0.56f }, iconSize, ok ? COL_GOOD : COL_BAD);
    UiWrappedText(gFonts.text, StatusText(), (Vector2){ x + 26, y }, w - 26, 17, ok ? COL_TEXT : COL_BAD, true);
}

static float StatusHeight(float w)
{
    const char *text = StatusText();
    if (!text) return 24;
    return UiWrappedText(gFonts.text, text, (Vector2){ 0, 0 }, w - 26, 17, COL_TEXT, false);
}

static bool SecondaryButton(Rectangle r, Icon icon, const char *label, bool enabled)
{
    return UiButtonIconEx(r, icon, label, 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, enabled);
}

// The test case tabs: one per case, numbered, the one on show filled; after a run each gets a
// tick (it ran, and printed what it should if that was given) or a cross. Then a button to
// add a case and, with more than one, a button to remove the one on show.
static void CaseTabs(float x, float y, float right, bool busy)
{
    float tabW = 62, tabH = 30, gap = 6;
    for (int i = 0; i < cs.caseCount; i++, x += tabW + gap) {
        Rectangle tab = { x, y, tabW, tabH };
        bool shown = i == cs.shownCase, over = !shown && UiMouseOver(tab);
        if (over) UiWantHand();
        UiRoundRect(tab, 7, shown ? COL_ACCENT : over ? COL_SECONDARY_HOVER : COL_PANEL);
        Color ink = shown ? COL_ON_ACCENT : COL_TEXT;
        UiText(gFonts.bold, TextFormat("%d", i + 1), tab.x + 12, tab.y + 5, 18, ink);
        Vector2 mark = { tab.x + tabW - 17, tab.y + tabH / 2 };
        bool ran = i < cs.ranCases && cs.runner.outcomes[i].result != RESULT_NONE;
        if (ran) {
            bool passed = CasePassed(i);
            Color sign = passed ? COL_GOOD : COL_BAD;
            DrawCircleV(mark, 9, shown ? COL_ON_ACCENT : sign);
            IconDraw(passed ? ICON_CHECK : ICON_CROSS, mark, 11, shown ? sign : COL_ON_ACCENT);
        } else {
            DrawCircleV(mark, 3.5f, Fade(ink, 0.35f));
        }
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) ShowCase(i);
    }
    if (cs.caseCount < CODE_MAX_CASES && x + tabH <= right) {
        if (UiButtonIconEx((Rectangle){ x, y, tabH + 6, tabH }, ICON_PLUS, NULL, 16, COL_PANEL, COL_SECONDARY_HOVER,
                           COL_TEXT, !busy))
            AddCase();
        x += tabH + 6 + gap;
    }
    if (cs.caseCount > 1 && x + tabH <= right) {
        if (UiButtonIconEx((Rectangle){ x, y, tabH + 6, tabH }, ICON_TRASH, NULL, 16, COL_PANEL, COL_SECONDARY_HOVER,
                           COL_TEXT, !busy))
            RemoveCase();
    }
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
    cs.ranCases = 0;  // the test cases stay: they may suit the other language's program too
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
                ClearCases();
                EditorSetText(&cs.output, "");
                cs.warning[0] = '\0';
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
    CaseRunnerUpdate(&cs.runner);
    bool busy = CaseRunnerBusy(&cs.runner);
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
            CaseRunnerCancel(&cs.runner);
            cs.wasBusy = false;
            cs.ranCases = 0;
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

    // Columns: code on the left; on the right the test case tabs, the case's input and
    // expected output, then what the program printed for it.
    float top = barY + barH + 12, bottom = H - pad;
    float codeW = (W - pad * 3) * 0.58f;
    float rx = pad * 2 + codeW, rw = W - pad - rx;
    EditorWidget(&cs.code, (Rectangle){ pad, top, codeW, bottom - top });

    UiText(gFonts.bold, "Test cases", rx, top + 6, 17, COL_DIM);
    float tabsX = rx + UiTextWidth(gFonts.bold, "Test cases", 17) + 14;
    CaseTabs(tabsX, top, rx + rw, busy);
    float y = top + 40;

    y = Label("Input", rx, y);
    float inputH = 80;
    EditorWidget(&cs.inputs[cs.shownCase], (Rectangle){ rx, y, rw, inputH });
    y += inputH + 10;
    y = Label("Expected output", rx, y);
    float expectH = 64;
    EditorWidget(&cs.expects[cs.shownCase], (Rectangle){ rx, y, rw, expectH });
    y += expectH + 12;

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

void CodeDemoCases(void)
{
    if (!cs.open) return;
    EditorSetText(&cs.code,
                  "#include <stdio.h>\n\nint main() {\n    int n;\n    scanf(\"%d\", &n);\n"
                  "    printf(\"%d squared is %d\\n\", n, n * n);\n    return 0;\n}\n");
    static const char *inputs[] = { "3", "12", "-5" };
    static const char *expected[] = { "3 squared is 9", "12 squared is 144", "-5 squared is -25" };
    cs.caseCount = 3;
    for (int i = 0; i < 3; i++) {
        EditorSetText(&cs.inputs[i], inputs[i]);
        EditorSetText(&cs.expects[i], expected[i]);
    }
    ShowCase(0);
}
