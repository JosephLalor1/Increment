// The code editor screen: editors for code, input and output, running programs with the
// runner, exercise checking and per-user drafts. See code_screen.h.
#include "code_screen.h"
#include "mascot.h"
#include "editor.h"
#include "exercise_hints.h"
#include "layout.h"
#include "progress.h"
#include "runner.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { CODE_PLAYGROUND, CODE_SNIPPET, CODE_EXERCISE } CodeMode;

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
    CodeMode mode;
    bool cpp;
    char title[128];

    const Exercise *ex;
    char lessonId[32];     // exercises: the lesson it belongs to ("" otherwise)
    int exerciseIndex;     // exercises: zero-based index within the lesson
    char *original;        // what Reset restores: starter, snippet or template

    Editor code, input, output;
    Runner runner;
    bool wasBusy;
    char draftPath[800];   // empty when this code isn't saved
    unsigned markedVersion;

    bool checked, passed;  // exercise result of the last run
    int badLine;
    int gotLines, wantLines;  // output line counts, ignoring trailing blank lines
    char wantLine[160], gotLine[160];
    char warning[220];     // first compiler warning, shown under the status

    float taskScroll, taskH;
    ExerciseHints hints;
} cs;

static char *CopyStr(const char *s)
{
    size_t n = strlen(s);
    char *c = malloc(n + 1);
    memcpy(c, s, n + 1);
    return c;
}

static void SetOriginal(const char *s)
{
    free(cs.original);
    cs.original = CopyStr(s);
}

// ---------------------------------------------------------------------------
// Drafts

static void SaveDraft(void)
{
    if (!cs.draftPath[0]) return;
    FILE *f = fopen(cs.draftPath, "wb");
    if (!f) return;
    fwrite(cs.code.text, 1, (size_t)cs.code.len, f);
    fclose(f);
}

// Points cs.draftPath at <draft folder>/<key>.c(pp) and loads it if it exists.
// Returns true if a draft was loaded into the editor.
static bool UseDraft(const char *key)
{
    if (!draftFolder[0]) {
        cs.draftPath[0] = '\0';
        return false;
    }
    snprintf(cs.draftPath, sizeof cs.draftPath, "%s/%s.%s", draftFolder, key, cs.cpp ? "cpp" : "c");
    if (!FileExists(cs.draftPath)) return false;
    char *text = LoadFileText(cs.draftPath);
    if (!text) return false;
    EditorSetText(&cs.code, text);
    UnloadFileText(text);
    return true;
}

// ---------------------------------------------------------------------------
// Opening

static void Prepare(CodeMode mode, const char *title, bool cpp)
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
    cs.mode = mode;
    cs.cpp = cpp;
    cs.ex = NULL;
    ExerciseHintsOpen(&cs.hints, NULL);
    cs.lessonId[0] = '\0';
    cs.exerciseIndex = 0;
    cs.draftPath[0] = '\0';
    cs.checked = cs.passed = false;
    cs.warning[0] = '\0';
    cs.taskScroll = 0;
    cs.taskH = 0;
    cs.wasBusy = false;
    snprintf(cs.title, sizeof cs.title, "%s", title);
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
    Prepare(CODE_PLAYGROUND, "Code editor", lastCpp);
    SetOriginal(cs.cpp ? helloCpp : helloC);
    if (!UseDraft("playground")) EditorSetText(&cs.code, cs.original);
}

// Turns a lesson's "// stdin: 3 7 4" marker into input lines.
static void InputFromMarker(const char *code)
{
    const char *m = strstr(code, "// stdin:");
    if (!m) return;
    m += 9;
    char buf[512];
    size_t o = 0;
    bool gap = false;
    for (; *m && *m != '\n' && o < sizeof buf - 2; m++) {
        if (*m == ' ' || *m == '\t') { gap = o > 0; continue; }
        if (gap) buf[o++] = '\n';
        gap = false;
        buf[o++] = *m;
    }
    buf[o] = '\0';
    EditorSetText(&cs.input, buf);
}

void CodeOpenSnippet(const char *title, const char *code, bool cpp)
{
    Prepare(CODE_SNIPPET, title, cpp);
    SetOriginal(code);
    EditorSetText(&cs.code, code);
    InputFromMarker(code);
}

void CodeOpenExercise(const Lesson *L, int index)
{
    const Exercise *ex = &L->exercises[index];
    Prepare(CODE_EXERCISE, ex->title, L->track == TRACK_CPP);
    cs.ex = ex;
    ExerciseHintsOpen(&cs.hints, ex);
    snprintf(cs.lessonId, sizeof cs.lessonId, "%s", L->id);
    cs.exerciseIndex = index;
    SetOriginal(ex->starter);
    char key[64];
    snprintf(key, sizeof key, "%s_ex%d", L->id, index + 1);
    if (!UseDraft(key)) EditorSetText(&cs.code, ex->starter);
    if (ex->input) EditorSetText(&cs.input, ex->input);
}

void CodeSetDraftFolder(const char *folder)
{
    snprintf(draftFolder, sizeof draftFolder, "%s", folder ? folder : "");
}

void CodeHintsDemo(int count)
{
    while (cs.hints.revealed < count && ExerciseHintsReveal(&cs.hints)) {}
    if (cs.hints.revealed) cs.taskScroll = cs.taskH = 100000; // settle at the revealed clues on the next frame
}

void CodeClose(void)
{
    if (!cs.open) return;
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
    cs.checked = false;
    cs.warning[0] = '\0';
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);
    RunnerStart(&cs.runner, cs.code.text, cs.cpp, cs.input.text);
}

void CodeRunNow(void) { StartRun(); }
bool CodeBusy(void) { return RunnerBusy(&cs.runner); }

// Lines of output, not counting trailing blank lines or a final newline.
static int CountLines(const char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) n--;
    if (n == 0) return 0;
    int lines = 1;
    for (int i = 0; i < n; i++) if (s[i] == '\n') lines++;
    return lines;
}

// Called once when a run finishes: fills the output, marks error lines, checks exercises.
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

    if (cs.mode == CODE_EXERCISE && cs.ex && r->result == RESULT_OK) {
        cs.checked = true;
        cs.passed = RunnerOutputMatches(out, cs.ex->expected, &cs.badLine);
        cs.gotLines = CountLines(out);
        cs.wantLines = CountLines(cs.ex->expected);
        if (cs.passed) {
            ProgressRecordExercise(cs.lessonId, cs.exerciseIndex);
        } else {
            RunnerGetLine(cs.ex->expected, cs.badLine, cs.wantLine, sizeof cs.wantLine);
            RunnerGetLine(out, cs.badLine, cs.gotLine, sizeof cs.gotLine);
        }
    } else if (cs.mode == CODE_EXERCISE && r->result != RESULT_NONE) {
        cs.checked = true;  // crashed or timed out: not passed
        cs.passed = false;
        cs.badLine = 0;
    }
}

// ---------------------------------------------------------------------------
// Drawing

static float Label(const char *text, float x, float y)
{
    UiText(gFonts.bold, text, x, y, 17, COL_DIM);
    return y + 26;
}

// The exercise brief: description and expected output, scrollable if it's long.
static void TaskPanel(Rectangle r)
{
    UiRoundRect(r, 10, COL_PANEL);
    bool hasHints = cs.ex->hintCount > 0;
    bool revealedNow = false;
    bool done = cs.lessonId[0] && ProgressExerciseDone(cs.lessonId, cs.exerciseIndex);
    Rectangle body = r;
    if (hasHints) {
        UiIconText(done ? ICON_CHECK : ICON_TARGET, gFonts.bold, done ? "Task (done)" : "Task",
                   r.x + 18, r.y + 14, 19, done ? COL_GOOD : COL_ACCENT);
        const char *label = !ExerciseHintsAvailable(&cs.hints) ? "All hints revealed"
                          : cs.hints.revealed ? "Show next hint" : "Show hint";
        if (UiButtonIconEx((Rectangle){ r.x + r.width - 180, r.y + 8, 162, 32 }, ICON_INFO, label, 15,
                           COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, ExerciseHintsAvailable(&cs.hints))) {
            revealedNow = ExerciseHintsReveal(&cs.hints);
            if (revealedNow) MascotReact(FOX_EDITOR, FOX_TIP);
        }
        body.y += 44;
        body.height -= 44;
    }
    if (UiMouseOver(body)) cs.taskScroll -= GetMouseWheelMove() * 40;
    float maxScroll = cs.taskH - body.height + 16;
    if (cs.taskScroll > maxScroll) cs.taskScroll = maxScroll;
    if (cs.taskScroll < 0) cs.taskScroll = 0;

    BeginScissorMode((int)body.x, (int)body.y + 4, (int)body.width, (int)body.height - 8);
    float x = r.x + 18, w = r.width - 36;
    float top = body.y + 14 - cs.taskScroll, y = top;
    if (!hasHints) {
        UiIconText(done ? ICON_CHECK : ICON_TARGET, gFonts.bold, done ? "Task (done)" : "Task", x, y, 19,
                   done ? COL_GOOD : COL_ACCENT);
        y += 34;
    }
    if (cs.ex->description) {
        y += UiWrappedText(gFonts.text, cs.ex->description, (Vector2){ x, y }, w, 18, COL_TEXT, true) + 12;
    }
    y = Label("Expected output", x, y);
    y += UiOutputBlock(cs.ex->expected, (Vector2){ x, y }, w, 16, true) + 10;
    for (int i = 0; i < cs.hints.revealed; i++) {
        const char *hint = ExerciseHintsGet(&cs.hints, i);
        if (!hint) continue;
        float size = 18 * UiTextScale();
        float textH = UiWrappedText(gFonts.text, hint, (Vector2){ 0, 0 }, w - 24, size, COL_TEXT, false);
        float h = 30 + textH + 20;
        UiRoundRect((Rectangle){ x, y, w, h }, 8, COL_NOTE_BG);
        UiText(gFonts.bold, TextFormat("Hint %d", i + 1), x + 12, y + 10, 16, COL_ACCENT);
        UiWrappedText(gFonts.text, hint, (Vector2){ x + 12, y + 30 }, w - 24, size, COL_TEXT, true);
        y += h + 10;
    }
    cs.taskH = y - top;
    EndScissorMode();
    if (revealedNow) {
        // Bring the newly added clue into view without replacing earlier hints.
        cs.taskScroll = cs.taskH - body.height + 16;
        if (cs.taskScroll < 0) cs.taskScroll = 0;
    }

    if (cs.taskH > body.height - 8 && cs.taskScroll < maxScroll - 1) {  // more below: say so
        const char *more = "Scroll for more";
        float mw = MeasureTextEx(gFonts.text, more, 15, 0).x;
        UiRoundRect((Rectangle){ r.x + r.width - mw - 30, r.y + r.height - 30, mw + 20, 24 }, 12, COL_PANEL_HI);
        UiText(gFonts.text, more, r.x + r.width - mw - 20, r.y + r.height - 27, 15, COL_DIM);
    }
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

// Result of checking an exercise's output; returns the height used.
static float ResultBanner(float x, float y, float w, bool draw)
{
    if (cs.mode != CODE_EXERCISE || !cs.checked) return 0;
    char msg[512];
    if (cs.passed) {
        snprintf(msg, sizeof msg, "Correct! Your output matches the expected output.");
    } else if (cs.badLine > 0 && cs.gotLines == 0) {
        snprintf(msg, sizeof msg, "Not quite. Your program didn't print anything.");
    } else if (cs.badLine > cs.gotLines) {
        snprintf(msg, sizeof msg, "Not quite. Your output stops after %d line%s, but line %d should be \"%s\".",
                 cs.gotLines, cs.gotLines == 1 ? "" : "s", cs.badLine, cs.wantLine);
    } else if (cs.badLine > cs.wantLines) {
        snprintf(msg, sizeof msg, "Not quite. Your output has an extra line %d: \"%s\".", cs.badLine, cs.gotLine);
    } else if (cs.badLine > 0) {
        snprintf(msg, sizeof msg, "Not quite. Line %d should be \"%s\" but your program printed \"%s\".", cs.badLine,
                 cs.wantLine, cs.gotLine);
    } else {
        snprintf(msg, sizeof msg, "Not quite. The program has to finish normally before its output can be checked.");
    }
    float h = UiWrappedText(gFonts.text, msg, (Vector2){ 0, 0 }, w - 56, 17, COL_TEXT, false) + 20;
    if (draw) {
        UiRoundRect((Rectangle){ x, y, w, h }, 8, cs.passed ? COL_GOOD_BG : COL_BAD_BG);
        IconDraw(cs.passed ? ICON_CHECK : ICON_CROSS, (Vector2){ x + 22, y + 10 + 17 * 0.56f }, 17,
                 cs.passed ? COL_GOOD : COL_BAD);
        UiWrappedText(gFonts.text, msg, (Vector2){ x + 40, y + 10 }, w - 56, 17, COL_TEXT, true);
    }
    return h + 10;
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
    SetOriginal(cpp ? helloCpp : helloC);
    if (!UseDraft("playground")) EditorReplaceAll(&cs.code, cs.original, now);
    EditorSetText(&cs.output, "");
    EditorSetMarks(&cs.code, NULL, 0);
    cs.runner.phase = RUN_IDLE;
    lastCpp = cpp;
}

bool CodeScreen(void)
{
    if (!cs.open) return true;
    if (TopBar(cs.title)) {
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
        if (cs.mode == CODE_EXERCISE && cs.ex->input) EditorSetText(&cs.input, cs.ex->input);
    }
    x += 132;
    if (cs.mode == CODE_EXERCISE && cs.ex->solution) {
        if (SecondaryButton((Rectangle){ x, barY, 180, barH }, ICON_BOOK, "Show solution", !busy)) {
            EditorReplaceAll(&cs.code, cs.ex->solution, GetTime());
        }
        x += 192;
    }
    if (cs.mode == CODE_PLAYGROUND) {
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
    } else {
        UiText(gFonts.mono, cs.cpp ? "C++" : "C", x + 4, barY + 10, 19, COL_DIM);
    }
    const char *hint = "F5 or Ctrl+Enter: run    Ctrl+Z: undo";
    float mascotSpace = MascotAvailable() ? 68 : 0;
    Rectangle foxBounds = { W - pad - 58, barY - 6, 58, 54 };
    if (mascotSpace && UiMouseOver(foxBounds))
        hint = cs.checked && !cs.passed ? "You can solve this. Keep trying!" : "Let's build something awesome!";
    float hw = MeasureTextEx(gFonts.text, hint, 16, 0).x;
    if (W - pad - hw - mascotSpace > x + 20)
        UiText(gFonts.text, hint, W - pad - hw - mascotSpace, barY + 12, 16, COL_DIM);
    if (mascotSpace && W - pad - mascotSpace > x + 20) {
        MascotDrawActor(FOX_EDITOR, foxBounds,
                        cs.checked && !cs.passed ? FOX_DETERMINED : FOX_CODING);
    }

    // Columns: code on the left, task / input / output on the right.
    float top = barY + barH + 12, bottom = H - pad;
    float codeW = (W - pad * 3) * 0.58f;
    float rx = pad * 2 + codeW, rw = W - pad - rx;
    EditorWidget(&cs.code, (Rectangle){ pad, top, codeW, bottom - top });

    float y = top;
    if (cs.mode == CODE_EXERCISE) {
        float taskH = (bottom - top) * 0.36f;
        TaskPanel((Rectangle){ rx, y, rw, taskH });
        y += taskH + 12;
    }
    y = Label("Input", rx, y);
    float inputH = cs.mode == CODE_EXERCISE ? 64 : 92;
    EditorWidget(&cs.input, (Rectangle){ rx, y, rw, inputH });
    y += inputH + 12;

    y = Label("Output", rx, y);
    StatusLine(rx, y, rw);
    y += StatusHeight(rw) + 6;
    if (cs.warning[0]) {
        y += UiWrappedText(gFonts.text, cs.warning, (Vector2){ rx, y }, rw, 16, COL_DIM, true) + 4;
    }
    y += ResultBanner(rx, y, rw, true);
    if (bottom - y > 40) EditorWidget(&cs.output, (Rectangle){ rx, y, rw, bottom - y });

    if (run) StartRun();
    return false;
}
