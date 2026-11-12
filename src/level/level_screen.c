// Playing a level's coding steps. See level_screen.h.
#include "level/level_screen.h"
#include "learning/answers.h"
#include "code/drafts.h"
#include "code/editor.h"
#include "ui/layout.h"
#include "level/level_fx.h"
#include "level/memory_probe.h"
#include "level/memory_view.h"
#include "ui/mascot.h"
#include "learning/progress.h"
#include "learning/roadmap.h"
#include "code/runner.h"
#include "ui/skill_icons.h"
#include "app/skill_reward.h"
#include "level/step_cheats.h"
#include "level/step_check.h"
#include "level/step_hints.h"
#include "level/step_skills.h"
#include "ui/ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIP_RADIUS 13.0f        // a step's circle in the top bar
#define SHAKE_SECONDS 0.35      // the output panel shakes this long after a wrong run
#define PASS_GLOW_SECONDS 1.2   // and glows this long after a pass
#define LINE_REVEAL_SECONDS 0.07  // output lines appear one after another, this far apart
#define LINE_TYPE_SECONDS 0.012   // and each is typed out, this long a character

// After the output matches the target, the step still has to be solved honestly
// (step_check.h): the constructs it is about must be there, and a second run with different
// values must match too.
typedef enum {
    CHECK_NONE,               // nothing checked (yet)
    CHECK_EXPECTING,          // running the solution with changed values, to learn what they give
    CHECK_RUNNING,            // running the learner's code with the same changed values
    CHECK_OK,                 // solved: the pass counts
    CHECK_VALUES_FAILED,      // with other values, the output is wrong: the answer was written in
    CHECK_GIVEN_CHANGED,      // a given value was edited instead of worked with
    CHECK_CONSTRUCT_MISSING,  // the code lacks something the step is about (a loop, say)
    CHECK_ANSWER_WRITTEN,     // the target's computed values are typed into the code (step_cheats.h)
    CHECK_FUNCTION_UNTOUCHED, // a function the step is about was left as it came
    CHECK_INDEXED,            // [ ] where the step is about pointer arithmetic
} CheckState;

// What a hint costs: the share of a pass's XP left after revealing that many hints.
static const float hintXpShare[STEP_HINT_LEVELS + 1] = { 1.0f, 0.85f, 0.65f, 0.4f };

static struct {
    bool ready;        // editors initialised
    bool open;
    const LessonSet *set;
    int lessonIndex;
    const Lesson *lesson;
    int step;          // the step being played
    char draftFolder[600];

    Editor code;
    Runner runner;
    bool wasBusy;
    bool learnerRun;   // the latest run was started by the learner (not the automatic first run)
    unsigned markedVersion;

    bool ran;          // a run has finished since the step opened
    bool passed;       // the last run printed the target output
    bool rewarded;     // this opening of the step has already earned its XP
    // A step passed before earns XP again only for new work (StepReplayEarns): measured from
    // the code it opened with, or from the starter once Reset is pressed and not undone.
    char *openedCode;  // the code the step opened with (usually its draft)
    int resetDepth;    // the editor's undo depth just after the latest Reset; 0 for none
    double unearnedAt; // when a pass earned nothing: Reset beckons for a while
    double passedAt, failedAt;

    CheckState check;
    Runner checker;           // runs the second run, apart from the learner's own runs
    bool checkerWasBusy;
    StepVariation variation;  // this step's second run; its expected output once known
    char *variedExpected;     // NULL until the changed solution has run; "" if it's no use
    char *variedCode;         // the learner's code with the changed values, while it runs
    char checkNote[STEP_CHECK_SUMMARY];  // the values involved in a failed check, as code
    Skill missingConstruct;

    // The picture of memory under the editor, for code with pointers (memory_view.h). The
    // scan follows the code as it is typed; after each run that works, a second, instrumented
    // run on its own Runner reports where everything really is.
    ProbeScan scan;
    unsigned scanVersion;      // the code version the scan was made for
    Runner probe;
    bool probeWasBusy;
    unsigned probingVersion;   // the code version the probe is running
    ProbeSnapshot snapshot;    // the last report read
    unsigned snapshotVersion;  // the code version it is of: current, or a memory
    float memoryHeight;        // the panel's height now: it eases open and shut

    int hintsShown;    // 0 to STEP_HINT_LEVELS
    double hintAt;     // when the latest hint was revealed: the strip animates from then
    StepHint hint;
    unsigned hintVersion;  // the code version the hint was worked out for

    float targetScroll, outputScroll;
    double shownAt;        // when the output now on show arrived: its lines appear from then
    int linesRevealed;     // how many of them have appeared (each sparks once as it does)
    unsigned typedVersion; // the code version the typing sparks last saw
    unsigned ranVersion;   // the code version of the last run: Run beckons once it's stale
    LevelFxLayout fxLayout;
    bool celebrating;  // the level was just completed
    double celebrateAt;
    bool demo;         // set up for a --shot: nothing is saved
    bool demoBack;     // --shot: leave the level as if Back had been clicked
} ls;

static bool Cpp(void) { return ls.lesson->track == TRACK_CPP; }
static const Step *CurrentStep(void) { return &ls.lesson->steps[ls.step]; }

static void DraftKey(char *out, size_t size) { snprintf(out, size, "%s_step%d", ls.lesson->id, ls.step + 1); }

static void SaveDraft(void)
{
    if (ls.demo) return;
    char key[64];
    DraftKey(key, sizeof key);
    DraftSave(ls.draftFolder, key, Cpp(), ls.code.text, (size_t)ls.code.len);
}

// ---------------------------------------------------------------------------
// Running and checking

// Keeps the scan of the code's variables up to date with the editor.
static void UpdateScan(void)
{
    if (ls.scanVersion == ls.code.version) return;
    ProbeScanCode(ls.code.text, &ls.scan);
    ls.scanVersion = ls.code.version;
}

// After a run that worked: runs the code again with its memory report added, if it has
// pointers to show. The learner isn't kept waiting for it.
static void StartProbe(void)
{
    RunnerCancel(&ls.probe);
    UpdateScan();
    if (!ls.scan.hasPointers || ls.code.version != ls.ranVersion) return;
    char *instrumented = ProbeInstrument(ls.code.text);
    if (!instrumented) return;
    ls.probingVersion = ls.code.version;
    RunnerStart(&ls.probe, instrumented, Cpp(), CurrentStep()->input);
    free(instrumented);
}

// Called once when the probe finishes: a whole report replaces the picture's facts; a
// failed one leaves the last report as a memory.
static void ProbeFinished(void)
{
    ProbeSnapshot report;
    if (ls.probe.result == RESULT_OK && ProbeRead(ls.probe.programOutput, &report)) {
        ls.snapshot = report;
        ls.snapshotVersion = ls.probingVersion;
    }
}

static void StartRun(bool byLearner)
{
    SaveDraft();
    RunnerCancel(&ls.probe);
    RunnerCancel(&ls.checker);
    ls.check = CHECK_NONE;
    ls.learnerRun = byLearner;
    ls.ranVersion = ls.code.version;
    LevelFxEmit(FX_RUN_STARTED);
    EditorSetMarks(&ls.code, NULL, 0);
    RunnerStart(&ls.runner, ls.code.text, Cpp(), CurrentStep()->input);
}

// Whether this pass earns XP. A first pass always does. A step passed before earns again
// only for new work since the code it started from: the starter if Reset was pressed (and
// not undone since: undoing it takes the undo depth below where it left it), otherwise the
// code it opened with.
static bool EarnsXp(bool passedBefore)
{
    if (!passedBefore) return true;
    bool resetStands = ls.resetDepth > 0 && ls.code.undoCount >= ls.resetDepth;
    const char *before = resetStands ? CurrentStep()->starter : ls.openedCode ? ls.openedCode : "";
    return StepReplayEarns(CurrentStep(), before, ls.code.text);
}

// The learner's first pass of this opening of the step: progress, XP in the skills their code
// used (less for hints) with the reward animation, and the celebration if the level is done.
// Replaying a passed step earns XP only for new work (see EarnsXp).
static void RecordPass(void)
{
    if (ls.demo) return;  // a shot records nothing
    const Step *step = CurrentStep();
    bool passedBefore = ProgressStepDone(ls.lesson->id, ls.step);
    bool levelBefore = RoadmapLevelComplete(ls.lesson);
    ProgressRecordStep(ls.lesson->id, ls.step);
    if (!ls.rewarded && !EarnsXp(passedBefore)) {
        ls.unearnedAt = GetTime();  // no XP for running an old answer: Reset shows the way
    } else if (!ls.rewarded) {
        SkillWeight skills[MAX_QUESTION_SKILLS];
        int count = StepSkills(step, ls.code.text, Cpp(), skills);
        for (int i = 0; i < count; i++) skills[i].weight *= hintXpShare[ls.hintsShown];
        SkillGains gains = AnswerRecordStep(skills, count, step->difficulty, passedBefore);
        SkillRewardShow(SkillRewardOrigin(), &gains);
        ls.rewarded = true;
    }
    MascotReact(FOX_EDITOR, FOX_CORRECT);
    if (!levelBefore && RoadmapLevelComplete(ls.lesson)) {
        ls.celebrating = true;
        ls.celebrateAt = GetTime();
        UiFocus(NULL);
    }
}

// Whether the step counts as solved on screen: the learner's run matched and passed its
// checks, or (for the automatic first run of a draft) it was solved before.
static bool StepCleared(void)
{
    if (!ls.passed) return false;
    return ls.learnerRun ? ls.check == CHECK_OK : ls.demo || ProgressStepDone(ls.lesson->id, ls.step);
}

static void CheckPassed(void)
{
    ls.check = CHECK_OK;
    ls.passedAt = GetTime();
    LevelFxEmit(FX_PASSED);
    RecordPass();
}

static void CheckFailed(CheckState why)
{
    ls.check = why;
    ls.failedAt = GetTime();
    LevelFxEmit(FX_REFUSED);
    MascotReact(FOX_EDITOR, FOX_INCORRECT);
}

// Runs the learner's code with the step's changed values; its output is compared when done.
static void RunVariedCode(void)
{
    ls.check = CHECK_RUNNING;
    RunnerStart(&ls.checker, ls.variedCode, Cpp(), ls.variation.input);
}

// The output matched the target: now make sure the step was really solved.
static void BeginCheck(void)
{
    const Step *step = CurrentStep();
    if (StepMissingConstructs(step, ls.code.text, Cpp(), &ls.missingConstruct, 1) > 0) {
        CheckFailed(CHECK_CONSTRUCT_MISSING);
        return;
    }
    StepCheat cheat = StepCheatFind(step, ls.code.text);  // what the code alone gives away
    if (cheat.kind != CHEAT_NONE) {
        snprintf(ls.checkNote, sizeof ls.checkNote, "%s", cheat.note);
        CheckFailed(cheat.kind == CHEAT_ANSWER_WRITTEN ? CHECK_ANSWER_WRITTEN
                    : cheat.kind == CHEAT_FUNCTION_UNTOUCHED ? CHECK_FUNCTION_UNTOUCHED : CHECK_INDEXED);
        return;
    }
    bool useless = ls.variedExpected && !ls.variedExpected[0];  // the second run can't tell anything
    if (!ls.variation.solution || useless) {
        CheckPassed();
        return;
    }
    int missing;
    free(ls.variedCode);
    ls.variedCode = StepVariationApply(step, ls.code.text, &missing, ls.checkNote);
    if (missing > 0) {
        CheckFailed(CHECK_GIVEN_CHANGED);
    } else if (!ls.variedCode) {
        CheckPassed();
    } else if (ls.variedExpected) {
        RunVariedCode();
    } else {  // first time on this step: find out what the changed values should give
        ls.check = CHECK_EXPECTING;
        RunnerStart(&ls.checker, ls.variation.solution, Cpp(), ls.variation.input);
    }
}

// Called once when the checker finishes a run.
static void CheckerFinished(void)
{
    Runner *r = &ls.checker;
    const char *out = r->result == RESULT_OK && r->programOutput && !r->outputTruncated ? r->programOutput : NULL;
    int badLine;
    if (ls.check == CHECK_EXPECTING) {
        // If the changed solution fails or prints the same as before, the second run proves
        // nothing: remember that, and let the pass stand.
        bool useful = out && !RunnerOutputMatches(out, CurrentStep()->expected, &badLine);
        free(ls.variedExpected);
        ls.variedExpected = malloc(strlen(useful ? out : "") + 1);
        if (ls.variedExpected) strcpy(ls.variedExpected, useful ? out : "");
        if (useful && ls.variedExpected) RunVariedCode();
        else CheckPassed();
    } else if (ls.check == CHECK_RUNNING) {
        if (out && RunnerOutputMatches(out, ls.variedExpected, &badLine)) {
            CheckPassed();
        } else {
            snprintf(ls.checkNote, sizeof ls.checkNote, "%s", ls.variation.summary);
            ls.outputScroll = ls.targetScroll = 0;
            ls.shownAt = GetTime();  // the second run's output is typed out in its turn
            ls.linesRevealed = 0;
            CheckFailed(CHECK_VALUES_FAILED);
        }
    }
}

// Called once when a run finishes: error marks, or the output checked against the target.
static void RunFinished(void)
{
    Runner *r = &ls.runner;
    ls.ran = true;
    ls.outputScroll = 0;
    ls.shownAt = GetTime();
    ls.linesRevealed = 0;
    if (r->result == RESULT_COMPILE_ERROR) {
        int lines[RUNNER_MAX_DIAGS], n = 0;
        for (int i = 0; i < r->diagCount; i++) if (r->diags[i].error) lines[n++] = r->diags[i].line;
        EditorSetMarks(&ls.code, lines, n);
        ls.markedVersion = ls.code.version;
        if (ls.learnerRun) {  // lightning from the first faulty line
            if (n > 0) LevelFxLightning(EditorTextPoint(&ls.code, ls.fxLayout.editor, EditorPosOf(&ls.code, lines[0] - 1, 0)));
            LevelFxEmit(FX_COMPILE_FAILED);
        }
    }
    if (r->result == RESULT_OK) StartProbe();
    int badLine;
    bool wasPassed = ls.passed;
    ls.passed = r->result == RESULT_OK && r->programOutput && !r->outputTruncated
             && RunnerOutputMatches(r->programOutput, CurrentStep()->expected, &badLine);
    if (ls.passed) {
        if (!wasPassed) ls.passedAt = GetTime();
        if (ls.learnerRun) BeginCheck();
    } else if (ls.learnerRun) {
        ls.failedAt = GetTime();
        MascotReact(FOX_EDITOR, FOX_INCORRECT);
        if (r->result != RESULT_COMPILE_ERROR) LevelFxEmit(FX_OUTPUT_WRONG);
    }
}

// ---------------------------------------------------------------------------
// Opening steps

// Plays step index. When leaving another step of this level, saves its draft first.
static void OpenStep(int index, bool leavingStep)
{
    if (leavingStep) SaveDraft();
    RunnerCancel(&ls.runner);
    RunnerCancel(&ls.checker);
    RunnerCancel(&ls.probe);
    memset(&ls.snapshot, 0, sizeof ls.snapshot);
    MemoryViewReset();
    ls.step = index;
    const Step *step = CurrentStep();
    StepVariationFree(&ls.variation);
    StepVariationMake(step, &ls.variation);
    free(ls.variedExpected);
    ls.variedExpected = NULL;
    char key[64];
    DraftKey(key, sizeof key);
    char *draft = DraftLoad(ls.draftFolder, key, Cpp());
    EditorSetText(&ls.code, draft ? draft : step->starter);
    free(draft);
    free(ls.openedCode);
    ls.openedCode = malloc((size_t)ls.code.len + 1);
    if (ls.openedCode) memcpy(ls.openedCode, ls.code.text, (size_t)ls.code.len + 1);
    ls.resetDepth = 0;
    ls.unearnedAt = -10;
    ls.scanVersion = ls.code.version - 1;  // scan the new code
    UpdateScan();
    ls.memoryHeight = 0;
    ls.code.fontSize = 19 * UiTextScale();
    ls.ran = ls.passed = ls.rewarded = false;
    ls.passedAt = ls.failedAt = -10;
    ls.hintsShown = 0;
    ls.targetScroll = ls.outputScroll = 0;
    EditorSetHint(&ls.code, 0, false);
    UiFocus(&ls.code);
    ls.typedVersion = ls.code.version;
    LevelFxEmit(FX_STEP_OPENED);
    StartRun(false);  // so the learner sees straight away what the code prints now
}

void LevelScreenOpen(const LessonSet *set, int lessonIndex, const char *draftFolder)
{
    if (!ls.ready) {
        EditorInit(&ls.code, "", true);
        ls.ready = true;
    }
    LevelScreenClose();
    ls.set = set;
    ls.lessonIndex = lessonIndex;
    ls.lesson = &set->items[lessonIndex];
    snprintf(ls.draftFolder, sizeof ls.draftFolder, "%s", draftFolder ? draftFolder : "");
    ls.celebrating = ls.demo = false;
    LevelFxReset();
    OpenStep(RoadmapResumeStep(ls.lesson), false);
    ls.open = true;
}

void LevelScreenClose(void)
{
    if (!ls.open) return;
    SaveDraft();
    RunnerCancel(&ls.runner);
    RunnerCancel(&ls.checker);
    RunnerCancel(&ls.probe);
    ls.open = false;
    UiFocus(NULL);
}

void LevelScreenShutdown(void)
{
    LevelScreenClose();
    if (!ls.ready) return;
    RunnerFree(&ls.runner);
    RunnerFree(&ls.checker);
    RunnerFree(&ls.probe);
    StepVariationFree(&ls.variation);
    free(ls.variedExpected);
    free(ls.variedCode);
    free(ls.openedCode);
    ls.variedExpected = ls.variedCode = ls.openedCode = NULL;
    EditorFree(&ls.code);
    ls.ready = false;
}

// ---------------------------------------------------------------------------
// Hints

// Keeps the hint pointing at the first difference between the code and the solution, as the
// learner edits.
static void UpdateHint(void)
{
    if (ls.hintsShown == 0) {
        EditorSetHint(&ls.code, 0, false);
        return;
    }
    if (ls.hintVersion == ls.code.version && ls.hint.kind != STEP_HINT_NONE) return;
    ls.hint = StepHintFind(ls.code.text, CurrentStep()->solution);
    ls.hintVersion = ls.code.version;
    if (ls.hint.kind == STEP_HINT_NONE) EditorSetHint(&ls.code, 0, false);
    else EditorSetHint(&ls.code, ls.hint.line + 1, ls.hint.kind == STEP_HINT_ADD);
}

// The strip under the editor while hints are showing: what to do (an icon), on which line,
// and from the second hint the line's shape, from the third the line itself.
static void HintStrip(Rectangle r)
{
    // A new hint lights the strip up in gold; the bulb swells, and the first hint slides the
    // strip into place.
    float age = (float)(GetTime() - ls.hintAt), fresh = age < 0.8f ? 1 - age / 0.8f : 0;
    if (ls.hintsShown == 1 && age < 0.25f) r.y += (1 - age / 0.25f) * (1 - age / 0.25f) * 50;
    UiRoundRect(r, 8, ColorLerp(COL_NOTE_BG, COL_GOLD, fresh * 0.35f));
    if (fresh > 0) DrawRectangleRoundedLinesEx(r, 0.3f, 8, 2, Fade(COL_GOLD, fresh));
    float size = 18 * UiTextScale(), cy = r.y + r.height / 2;
    DrawCircleV((Vector2){ r.x + 22, cy }, 16 + fresh * 8, Fade(COL_GOLD, 0.25f * fresh));
    IconDraw(ICON_BULB, (Vector2){ r.x + 22, cy }, 22 * (1 + fresh * 0.35f), fresh > 0 ? ColorLerp(COL_ACCENT, COL_GOLD, fresh) : COL_ACCENT);
    if (ls.hint.kind == STEP_HINT_NONE) {
        IconDraw(ICON_CHECK, (Vector2){ r.x + 58, cy }, 20, COL_GOOD);
        return;
    }
    Icon kind = ls.hint.kind == STEP_HINT_ADD ? ICON_PLUS : ls.hint.kind == STEP_HINT_REMOVE ? ICON_TRASH : ICON_RETRY;
    IconDraw(kind, (Vector2){ r.x + 58, cy }, 20, COL_ACCENT);
    const char *line = TextFormat("%d", ls.hint.line + 1);
    UiText(gFonts.mono, line, r.x + 78, cy - size * 0.6f, size, COL_ACCENT);
    if (ls.hintsShown < 2 || ls.hint.kind == STEP_HINT_REMOVE) return;
    // The code is typed out when its hint is revealed.
    char code[STEP_HINT_TEXT];
    snprintf(code, sizeof code, "%s", ls.hintsShown >= 3 ? ls.hint.target : ls.hint.shape);
    int typed = (int)(age / 0.018f);
    if (typed < (int)strlen(code)) code[typed] = '\0';
    BeginScissorMode((int)r.x + 120, (int)r.y, (int)r.width - 132, (int)r.height);
    UiCodeLine(code, r.x + 124, cy - size * 0.6f, size);
    EndScissorMode();
}

// ---------------------------------------------------------------------------
// Output panels

// Lines in program output, not counting trailing blank lines.
static int OutputLines(const char *text)
{
    if (!text) return 0;
    int n = (int)strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r' || text[n - 1] == ' ')) n--;
    if (n == 0) return 0;
    int lines = 1;
    for (int i = 0; i < n; i++) lines += text[i] == '\n';
    return lines;
}

// Whether line n (1-based) is the same in both outputs, as the check sees it (trailing
// spaces don't count).
static bool SameLine(const char *a, const char *b, int n)
{
    char la[512], lb[512];
    RunnerGetLine(a, n, la, sizeof la);
    RunnerGetLine(b, n, lb, sizeof lb);
    size_t na = strlen(la), nb = strlen(lb);
    while (na && (la[na - 1] == ' ' || la[na - 1] == '\r')) la[--na] = '\0';
    while (nb && (lb[nb - 1] == ' ' || lb[nb - 1] == '\r')) lb[--nb] = '\0';
    return strcmp(la, lb) == 0;
}

// A panel's frame and title (an icon and a word); returns the area inside for its lines.
// backdrop draws the panel's moving background (level_fx.h), or is NULL.
static Rectangle Panel(Rectangle r, Icon icon, const char *title, Color edge, void (*backdrop)(Rectangle))
{
    UiRoundRect(r, 10, COL_CODE_BG);
    if (backdrop) backdrop(r);
    if (edge.a) DrawRectangleRoundedLinesEx(r, 20 / fminf(r.width, r.height), 8, 2.5f, edge);
    UiIconText(icon, gFonts.bold, title, r.x + 16, r.y + 12, 17, COL_DIM);
    return (Rectangle){ r.x + 10, r.y + 44, r.width - 20, r.height - 54 };
}

// How big a tick or cross is drawn `age` seconds after its line appeared: it pops past full
// size and settles.
static float PopScale(float age)
{
    float t = age / 0.22f;
    if (t >= 1) return 1;
    return t < 0.6f ? t / 0.6f * 1.35f : 1.35f - (t - 0.6f) / 0.4f * 0.35f;
}

// Lines of output in area, scrolled by *scroll (the wheel scrolls it). When compareWith is
// set, each line is ticked where it matches the same line there, or crossed and tinted red,
// and lines missing from the end show as empty red slots; the lines then appear one after
// another from ls.shownAt, typed out, each sparking as its tick or cross pops in.
static void OutputLinesView(Rectangle area, const char *text, const char *compareWith, float *scroll)
{
    float shownFor = compareWith ? (float)(GetTime() - ls.shownAt) : 1000;
    float size = 18 * UiTextScale(), lineH = size * 1.45f, gutter = compareWith ? 30 : 6;
    int lines = OutputLines(text);
    int rows = compareWith && OutputLines(compareWith) > lines ? OutputLines(compareWith) : lines;
    float contentH = rows * lineH;
    if (UiMouseOver(area)) *scroll -= GetMouseWheelMove() * lineH * 3;
    if (*scroll > contentH - area.height) *scroll = contentH - area.height;
    if (*scroll < 0) *scroll = 0;

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    char line[512];
    for (int i = 1; i <= rows; i++) {
        float y = area.y + (i - 1) * lineH - *scroll;
        float age = shownFor - (i - 1) * (float)LINE_REVEAL_SECONDS;  // since this line appeared
        if (age < 0) break;
        bool onScreen = y + lineH >= area.y && y <= area.y + area.height;
        if (compareWith) {
            bool same = SameLine(text, compareWith, i);
            if (i > ls.linesRevealed) {  // it has just appeared
                ls.linesRevealed = i;
                if (onScreen) LevelFxLine((Vector2){ area.x + 14, y + lineH / 2 }, same);
            }
            if (!onScreen) continue;
            Color tint = same ? COL_GOOD : COL_BAD;
            float pulse = same ? 0 : (sinf((float)GetTime() * 5 + i) + 1) * 0.04f;  // wrong lines throb
            if (!same) UiRoundRect((Rectangle){ area.x, y, area.width, lineH - 3 }, 5, Fade(COL_BAD, 0.12f + pulse));
            if (i > lines) DrawRectangleRoundedLinesEx((Rectangle){ area.x + gutter, y + 2, area.width - gutter - 6, lineH - 7 },
                                                       0.4f, 6, 1.5f, Fade(COL_BAD, 0.6f));
            IconDraw(same ? ICON_CHECK : ICON_CROSS, (Vector2){ area.x + 14, y + lineH / 2 - 1 }, 15 * PopScale(age), tint);
        }
        if (i > lines || !onScreen) continue;
        RunnerGetLine(text, i, line, sizeof line);
        int typed = (int)(age / (float)LINE_TYPE_SECONDS);
        if (typed < (int)strlen(line)) line[typed] = '\0';
        UiText(gFonts.mono, line, area.x + gutter, y + (lineH - size) / 2 - 1, size, COL_TEXT);
    }
    EndScissorMode();
}

// What the program did, when it didn't print anything to compare: compile errors (line and
// message), a crash or a timeout.
static void ProblemView(Rectangle area)
{
    Runner *r = &ls.runner;
    float size = 17 * UiTextScale(), y = area.y;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    if (r->result == RESULT_COMPILE_ERROR && r->errorCount > 0) {
        for (int i = 0; i < r->diagCount && y < area.y + area.height; i++) {
            if (!r->diags[i].error) continue;
            const char *number = TextFormat("%d", r->diags[i].line);
            UiRoundRect((Rectangle){ area.x, y + 1, 40, size * 1.3f }, 6, COL_BAD_BG);
            UiTextCentered(gFonts.mono, number, area.x + 20, y + 3, size, COL_BAD);
            y += UiWrappedText(gFonts.text, r->diags[i].message, (Vector2){ area.x + 52, y + 2 }, area.width - 56, size,
                               COL_TEXT, true) + 12;
        }
    } else {
        IconDraw(ICON_CROSS, (Vector2){ area.x + 12, y + size * 0.6f }, size, COL_BAD);
        y += UiWrappedText(gFonts.text, r->summary, (Vector2){ area.x + 30, y }, area.width - 34, size, COL_BAD, true) + 10;
        // A failed build with no line to point at (a link error, say): the compiler's own words.
        if (r->result == RESULT_COMPILE_ERROR && r->compileOutput)
            UiWrappedText(gFonts.mono, r->compileOutput, (Vector2){ area.x, y }, area.width, size * 0.85f, COL_DIM, true);
    }
    EndScissorMode();
}

// Why a run that printed the target doesn't count, without words: a strip along the bottom
// of the output panel with a cross, then the changed values the code failed with (as code),
// the given values that must stay (a padlock, then code), the missing construct's logo, or
// what gave a cheat away: the target sign and the answer found typed in, the functions logo
// and the function left unwritten, the pointer arithmetic logo and the indexing used.
static void CheckStrip(Rectangle r)
{
    // It slides up into place, and its cross wobbles for a moment.
    float age = (float)(GetTime() - ls.failedAt), slide = age < 0.3f ? 1 - age / 0.3f : 0;
    r.y += slide * slide * 60;
    float wobble = age < 0.7f ? sinf(age * 38) * 5 * (1 - age / 0.7f) : 0;
    UiRoundRect(r, 8, COL_BAD_BG);
    float size = 17 * UiTextScale(), cy = r.y + r.height / 2;
    IconDraw(ICON_CROSS, (Vector2){ r.x + 20 + wobble, cy }, 18, COL_BAD);
    if (ls.check == CHECK_CONSTRUCT_MISSING) {
        SkillIconDraw(ls.missingConstruct, (Vector2){ r.x + 56, cy }, 28, COL_TEXT);
        UiText(gFonts.bold, SkillTitle(ls.missingConstruct), r.x + 80, cy - size * 0.6f, size, COL_TEXT);
        return;
    }
    // What the code shown is: the target itself typed in, a function still to write, [ ]
    // where pointers should walk, givens that must stay (a padlock), or the changed values.
    if (ls.check == CHECK_FUNCTION_UNTOUCHED) SkillIconDraw(SKILL_FUNCTIONS, (Vector2){ r.x + 52, cy }, 24, COL_TEXT);
    else if (ls.check == CHECK_INDEXED) SkillIconDraw(SKILL_POINTER_ARITHMETIC, (Vector2){ r.x + 52, cy }, 24, COL_TEXT);
    else IconDraw(ls.check == CHECK_ANSWER_WRITTEN ? ICON_TARGET : ls.check == CHECK_GIVEN_CHANGED ? ICON_LOCK : ICON_RETRY,
                  (Vector2){ r.x + 52, cy }, 18, COL_TEXT);
    BeginScissorMode((int)r.x + 72, (int)r.y, (int)r.width - 82, (int)r.height);
    UiCodeLine(ls.checkNote, r.x + 76, cy - size * 0.6f, size);
    EndScissorMode();
}

// The right-hand column: the target, the input if the step has one, and what the program
// printed. After a failed second run it shows that run instead: the target, input and output
// for the changed values.
static void OutputColumn(Rectangle column)
{
    const Step *step = CurrentStep();
    Runner *r = &ls.runner;
    double now = GetTime();
    float gap = 12;
    float targetH = (column.height - gap) * 0.42f;
    float y = column.y;
    bool varied = ls.check == CHECK_VALUES_FAILED;
    bool checking = ls.check == CHECK_EXPECTING || ls.check == CHECK_RUNNING;
    bool refused = ls.check == CHECK_VALUES_FAILED || ls.check == CHECK_GIVEN_CHANGED
                || ls.check == CHECK_CONSTRUCT_MISSING || ls.check == CHECK_ANSWER_WRITTEN
                || ls.check == CHECK_FUNCTION_UNTOUCHED || ls.check == CHECK_INDEXED;
    const char *expected = varied ? ls.variedExpected : step->expected;
    const char *input = varied ? ls.variation.input : step->input;

    Rectangle target = { column.x, y, column.width, targetH };
    OutputLinesView(Panel(target, ICON_TARGET, "Target", BLANK, LevelFxTargetBackdrop), expected, NULL, &ls.targetScroll);
    y += targetH + gap;

    // Between the panels: how much of the output matches the target so far.
    const char *shown = varied ? ls.checker.programOutput : r->result == RESULT_OK ? r->programOutput : NULL;
    int wanted = OutputLines(expected), matching = 0;
    for (int i = 1; shown && ls.ran && !RunnerBusy(r) && i <= wanted && i <= ls.linesRevealed; i++)
        matching += SameLine(shown, expected, i);
    int rows = shown && OutputLines(shown) > wanted ? OutputLines(shown) : wanted;
    LevelFxMatchBar((Rectangle){ column.x + 4, y - gap + 4, column.width - 8, 4 }, rows ? (float)matching / rows : 0);

    if (input) {
        float inputH = 44 + 18 * UiTextScale() * 1.45f * (float)(OutputLines(input) > 3 ? 3 : OutputLines(input)) + 10;
        float scroll = 0;
        OutputLinesView(Panel((Rectangle){ column.x, y, column.width, inputH }, ICON_USER, "Input", BLANK, NULL), input,
                        NULL, &scroll);
        y += inputH + gap;
    }

    // The learner's output: shaken after a wrong run, glowing green after a pass.
    float shake = (float)(now - ls.failedAt) < SHAKE_SECONDS
                ? sinf((float)(now - ls.failedAt) * 60) * 8 * (1 - (float)(now - ls.failedAt) / (float)SHAKE_SECONDS) : 0;
    bool cleared = StepCleared();
    float glow = cleared ? 1 - fminf(1, (float)(now - ls.passedAt) / (float)PASS_GLOW_SECONDS) : 0;
    Rectangle mine = { column.x + shake, y, column.width, column.y + column.height - y };
    if (glow > 0) UiRoundRect((Rectangle){ mine.x - 6 * glow, mine.y - 6 * glow, mine.width + 12 * glow, mine.height + 12 * glow },
                              14, Fade(COL_GOOD, 0.25f * glow));
    bool wrong = refused || (!ls.passed && ls.ran && ls.learnerRun && !RunnerBusy(r));
    Color edge = cleared ? COL_GOOD : wrong ? Fade(COL_BAD, 0.6f) : BLANK;
    Rectangle area = Panel(mine, ICON_PLAY, "Output", edge, LevelFxOutputBackdrop);
    ls.fxLayout.target = target;
    ls.fxLayout.output = mine;
    ls.fxLayout.badge = (Vector2){ mine.x + mine.width - 34, mine.y + 26 };
    if (refused) {  // room for the strip that shows why
        area.height -= 54;
        CheckStrip((Rectangle){ mine.x + 10, mine.y + mine.height - 54, mine.width - 20, 44 });
    }
    int dots = (int)(now * 3) % 4;
    if (RunnerBusy(r)) {
        UiText(gFonts.mono, TextFormat("%.*s", dots + 1, "...."), area.x + 6, area.y, 22, COL_DIM);
    } else if (varied) {
        const char *out = ls.checker.result == RESULT_OK && ls.checker.programOutput ? ls.checker.programOutput : "";
        OutputLinesView(area, out, expected, &ls.outputScroll);
    } else if (ls.ran && r->result == RESULT_OK) {
        OutputLinesView(area, r->programOutput ? r->programOutput : "", step->expected, &ls.outputScroll);
    } else if (ls.ran) {
        ProblemView(area);
    }
    Vector2 badge = { mine.x + mine.width - 34, mine.y + 26 };
    if (cleared) {  // a big tick in the corner
        DrawCircleV(badge, 18 + 4 * glow, COL_GOOD);
        IconDraw(ICON_CHECK, badge, 22, COL_ON_ACCENT);
    } else if (checking) {  // the second run is under way
        LevelFxSpinner(badge);
    }
}

// ---------------------------------------------------------------------------
// The step pips and the toolbar

// A circle per step in the top bar: passed ones ticked, the current one ringed with its
// number, locked ones dim with a padlock. Clicking an open step plays it.
static void StepPips(void)
{
    int count = ls.lesson->stepCount;
    float gap = 8, width = count * (PIP_RADIUS * 2 + gap) - gap;
    float x = GetScreenWidth() - 24 - width + PIP_RADIUS, y = TOPBAR_H / 2;
    for (int i = 0; i < count; i++, x += PIP_RADIUS * 2 + gap) {
        Vector2 c = { x, y };
        bool done = ProgressStepDone(ls.lesson->id, i), open = RoadmapStepOpen(ls.lesson, i), current = i == ls.step;
        bool over = open && !current && CheckCollisionPointCircle(GetMousePosition(), c, PIP_RADIUS + 3)
                 && UiMouseOver((Rectangle){ c.x - PIP_RADIUS, c.y - PIP_RADIUS, PIP_RADIUS * 2, PIP_RADIUS * 2 });
        if (over) UiWantHand();
        if (current) {  // ringed, with a dot in orbit
            DrawRing(c, PIP_RADIUS + 3, PIP_RADIUS + 5.5f, 0, 360, 48, COL_ACCENT);
            float orbit = (float)GetTime() * 2.2f;
            DrawCircleV((Vector2){ c.x + cosf(orbit) * (PIP_RADIUS + 4.25f), c.y + sinf(orbit) * (PIP_RADIUS + 4.25f) }, 3,
                        COL_ON_ACCENT);
            ls.fxLayout.pip = c;
        }
        DrawCircleV(c, PIP_RADIUS, done ? COL_ACCENT : over ? COL_SECONDARY_HOVER : COL_PANEL_HI);
        if (done) IconDraw(ICON_CHECK, c, 14, COL_ON_ACCENT);
        else if (open || current) UiTextCentered(gFonts.bold, TextFormat("%d", i + 1), c.x, c.y - 9, 15,
                                                 current ? COL_ACCENT : COL_TEXT);
        else IconDraw(ICON_LOCK, c, 12, COL_DIM);
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) OpenStep(i, true);
    }
}

// How hard the step is: three bars, as many lit as its difficulty.
static void DifficultyMeter(float right, float y)
{
    int difficulty = CurrentStep()->difficulty;
    for (int i = 0; i < SKILL_MAX_DIFFICULTY; i++) {
        float h = 10 + i * 7;
        Rectangle bar = { right - (SKILL_MAX_DIFFICULTY - i) * 12, y + 34 - h, 8, h };
        UiRoundRect(bar, 3, i < difficulty ? COL_ACCENT : COL_PANEL_HI);
    }
}

// The hint button: a bulb, with a dot per hint already shown.
static bool HintButton(Rectangle r, bool enabled)
{
    bool pressed = UiButtonIconEx(r, ICON_BULB, "Hint", 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, enabled);
    for (int i = 0; i < STEP_HINT_LEVELS; i++)
        DrawCircleV((Vector2){ r.x + r.width - 30 + i * 10, r.y + r.height - 9 }, 3,
                    i < ls.hintsShown ? COL_ACCENT : COL_PANEL_HI);
    return pressed;
}

// The next ready level of the same track after this one, or -1.
static int NextLevel(void)
{
    for (int i = ls.lessonIndex + 1; i < ls.set->count; i++) {
        const Lesson *lesson = &ls.set->items[i];
        if (lesson->track == ls.lesson->track && RoadmapLevelReady(lesson)) return i;
    }
    return -1;
}

// The level is complete: confetti, a trophy, every step ticked, and on to the next level or
// back to the journey.
static LevelAction Celebration(void)
{
    LevelAction action = { LEVEL_STAY, -1 };
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    double age = GetTime() - ls.celebrateAt;
    DrawRectangle(0, 0, (int)W, (int)H, Fade(COL_BG, 0.82f));
    MascotConfetti((Rectangle){ 0, 0, W, H }, age, false);

    float cardW = 520, cardH = 330, x = (W - cardW) / 2, y = (H - cardH) / 2;
    float pop = 1 - powf(1 - fminf(1, (float)age / 0.35f), 3);
    y += (1 - pop) * 40;
    UiRoundRect((Rectangle){ x, y + 6, cardW, cardH }, 18, COL_PANEL_HI);
    UiRoundRect((Rectangle){ x, y, cardW, cardH }, 18, COL_PANEL);
    IconDraw(ICON_TROPHY, (Vector2){ W / 2, y + 62 }, 64, COL_GOLD);
    UiTextCentered(gFonts.bold, ls.lesson->title, W / 2, y + 112, 28, COL_TEXT);

    int count = ls.lesson->stepCount;
    float gap = 8, rowW = count * (PIP_RADIUS * 2 + gap) - gap, px = W / 2 - rowW / 2 + PIP_RADIUS;
    for (int i = 0; i < count; i++, px += PIP_RADIUS * 2 + gap) {
        float appear = fminf(1, fmaxf(0, (float)(age - 0.3 - i * 0.06) / 0.2f));  // they tick in one by one
        DrawCircleV((Vector2){ px, y + 178 }, PIP_RADIUS * (0.6f + 0.4f * appear), appear > 0 ? COL_ACCENT : COL_PANEL_HI);
        if (appear >= 1) IconDraw(ICON_CHECK, (Vector2){ px, y + 178 }, 14, COL_ON_ACCENT);
    }

    int next = NextLevel();
    float buttonY = y + cardH - 86, buttonW = next >= 0 ? 220 : 300;
    Rectangle journey = { next >= 0 ? W / 2 - buttonW - 10 : W / 2 - buttonW / 2, buttonY, buttonW, 54 };
    if (UiButtonIconEx(journey, ICON_BACK, "Journey", 21, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true))
        action.kind = LEVEL_BACK;
    if (next >= 0 && (UiButtonIcon((Rectangle){ W / 2 + 10, buttonY, buttonW, 54 }, ICON_NEXT, "Next level", true)
                      || IsKeyPressed(KEY_ENTER))) {
        action.kind = LEVEL_OPEN_LEVEL;
        action.lessonIndex = next;
    }
    if (IsKeyPressed(KEY_ESCAPE)) action.kind = LEVEL_BACK;
    return action;
}

LevelAction LevelScreen(void)
{
    LevelAction action = { LEVEL_STAY, -1 };
    if (!ls.open) return (LevelAction){ LEVEL_BACK, -1 };

    // Finish runs (the learner's, then the check's second run); drop error marks once the
    // code has been edited.
    RunnerUpdate(&ls.runner);
    bool running = RunnerBusy(&ls.runner);
    if (ls.wasBusy && !running) RunFinished();
    ls.wasBusy = running;
    RunnerUpdate(&ls.checker);
    bool checking = RunnerBusy(&ls.checker);
    if (ls.checkerWasBusy && !checking) CheckerFinished();
    ls.checkerWasBusy = checking = RunnerBusy(&ls.checker);  // one check run may start the next
    bool busy = RunnerBusy(&ls.runner) || checking;
    RunnerUpdate(&ls.probe);  // in the background: it never holds the learner up
    if (ls.probeWasBusy && !RunnerBusy(&ls.probe)) ProbeFinished();
    ls.probeWasBusy = RunnerBusy(&ls.probe);
    UpdateScan();
    LevelFxActivity(RunnerBusy(&ls.runner), checking);
    LevelFxUpdate(GetFrameTime());
    if (ls.code.markCount && ls.code.version != ls.markedVersion) EditorSetMarks(&ls.code, NULL, 0);
    UpdateHint();

    // While celebrating, the step underneath is only a picture: nothing in it reacts.
    if (ls.celebrating) UiSetClip((Rectangle){ 0, 0, 0, 0 });

    bool back = TopBar(ls.lesson->title);
    StepPips();
    LevelFxProgressLine(TOPBAR_H - 3, (float)RoadmapStepsDone(ls.lesson) / (float)ls.lesson->stepCount);

    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pad = 16, barY = TOPBAR_H + 12, barH = 42;
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Toolbar: run (or stop), reset, hint, and once passed, the next step.
    float x = pad;
    bool run = !busy && !ls.celebrating && (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_ENTER)));
    if (busy) {
        if (UiButtonIconEx((Rectangle){ x, barY, 130, barH }, ICON_CROSS, "Stop", 20, COL_BAD,
                           ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true)) {
            RunnerCancel(&ls.runner);
            RunnerCancel(&ls.checker);
            RunnerCancel(&ls.probe);
            ls.wasBusy = ls.checkerWasBusy = false;
            ls.check = CHECK_NONE;
        }
    } else {
        Rectangle runButton = { x, barY, 130, barH };
        if (ls.code.version != ls.ranVersion) {  // the code has changed since it last ran: Run beckons
            float pulse = (sinf((float)GetTime() * 4) + 1) / 2;
            Rectangle halo = { runButton.x - 3 - pulse * 2, runButton.y - 3 - pulse * 2, runButton.width + 6 + pulse * 4,
                               runButton.height + 6 + pulse * 4 };
            DrawRectangleRoundedLinesEx(halo, 0.3f, 8, 2, Fade(COL_ACCENT, 0.25f + 0.45f * pulse));
        }
        if (UiButtonIconEx(runButton, ICON_PLAY, "Run", 20, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, true)) run = true;
    }
    x += 142;
    Rectangle resetButton = { x, barY, 120, barH };
    // A pass that earned nothing (an old answer run again): Reset beckons in gold, since
    // starting the step afresh is how to earn its XP again.
    float sinceUnearned = (float)(GetTime() - ls.unearnedAt);
    if (sinceUnearned < 6) {
        float pulse = (sinf((float)GetTime() * 4) + 1) / 2, fade = sinceUnearned > 5 ? 6 - sinceUnearned : 1;
        Rectangle halo = { resetButton.x - 3 - pulse * 2, resetButton.y - 3 - pulse * 2, resetButton.width + 6 + pulse * 4,
                           resetButton.height + 6 + pulse * 4 };
        DrawRectangleRoundedLinesEx(halo, 0.3f, 8, 2, Fade(COL_GOLD, (0.3f + 0.5f * pulse) * fade));
    }
    if (UiButtonIconEx(resetButton, ICON_RETRY, "Reset", 19, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, !busy)) {
        EditorReplaceAll(&ls.code, CurrentStep()->starter, GetTime());  // Ctrl+Z brings the old code back
        ls.resetDepth = ls.code.undoCount;  // a fresh start: work from here earns XP again
        ls.unearnedAt = -10;
        ls.typedVersion = ls.code.version;  // a reset isn't typing
        LevelFxRewind((Vector2){ x + 28, barY + barH / 2 });
    }
    x += 132;
    bool cleared = StepCleared();
    if (HintButton((Rectangle){ x, barY, 130, barH }, ls.hintsShown < STEP_HINT_LEVELS && !cleared)) {
        ls.hintsShown++;
        ls.hintAt = GetTime();
        ls.hintVersion = ls.code.version - 1;  // work it out afresh
        UpdateHint();
        // The bulb's light flies to the line the hint is about (where the editor was last drawn).
        int line = ls.hint.line < EditorLineCount(&ls.code) ? ls.hint.line : EditorLineCount(&ls.code) - 1;
        LevelFxHint((Vector2){ x + 28, barY + barH / 2 },
                    EditorTextPoint(&ls.code, ls.fxLayout.editor, EditorPosOf(&ls.code, line, 0)),
                    ls.hint.kind != STEP_HINT_NONE);
        MascotReact(FOX_EDITOR, FOX_TIP);
    }
    x += 142;
    bool hasNext = ls.step + 1 < ls.lesson->stepCount;
    if (cleared && hasNext) {
        float pulse = (sinf((float)GetTime() * 5) + 1) / 2;
        Rectangle next = { x, barY, 150, barH };
        DrawRectangleRoundedLinesEx((Rectangle){ next.x - 3, next.y - 3, next.width + 6, next.height + 6 }, 0.3f, 8, 2,
                                    Fade(COL_ACCENT, 0.3f + 0.4f * pulse));
        if (UiButtonIconEx(next, ICON_NEXT, "Next", 20, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, !busy)) {
            OpenStep(ls.step + 1, true);
            busy = true;  // the new step's first run has just started
        }
    }
    float right = W - pad;
    DifficultyMeter(right, barY);
    const char *place = TextFormat("%s  %d/%d", TrackName(ls.lesson->track), ls.step + 1, ls.lesson->stepCount);
    float placeX = right - 46 - UiTextWidth(gFonts.mono, place, 19);
    UiText(gFonts.mono, place, placeX, barY + 10, 19, COL_DIM);
    // The fox watches from the toolbar: thinking during a run, set on trying again after a
    // miss, and jumping for joy at a pass.
    if (MascotAvailable() && placeX - 76 > x + 160) {
        bool missed = ls.ran && ls.learnerRun && !busy && !cleared;
        FoxEmotion mood = cleared ? FOX_CELEBRATING : busy ? FOX_THINKING : missed ? FOX_DETERMINED : FOX_CODING;
        MascotDrawActor(FOX_EDITOR, (Rectangle){ placeX - 76, barY - 6 - LevelFxHop(), 58, 54 }, mood);
    }

    // Code on the left (with the hint strip under it), target and output on the right.
    float top = barY + barH + 12, bottom = H - pad;
    float codeW = (W - pad * 3) * 0.58f;
    float stripH = ls.hintsShown ? 48 : 0;
    float leftH = bottom - top - (stripH ? stripH + 10 : 0);
    // Code with pointers gets the picture of memory under the editor: as tall as its rows
    // need, up to a share of the column, easing open when the first pointer is declared.
    int memoryRows = ls.scan.count > ls.snapshot.blockCount ? ls.scan.count : ls.snapshot.blockCount;
    float memoryWanted = ls.scan.hasPointers ? fminf(leftH * 0.46f, 62 + memoryRows * 40 * UiTextScale()) : 0;
    ls.memoryHeight += (memoryWanted - ls.memoryHeight) * (1 - expf(-GetFrameTime() * 12));
    if (fabsf(memoryWanted - ls.memoryHeight) < 0.5f || ls.demo) ls.memoryHeight = memoryWanted;
    bool memoryShown = ls.memoryHeight > 70;
    Rectangle editor = { pad, top, codeW, leftH - (memoryShown ? ls.memoryHeight + 10 : 0) };
    if (memoryShown) {
        Rectangle panel = { pad, editor.y + editor.height + 10, codeW, ls.memoryHeight };
        bool fresh = ls.snapshot.valid && ls.snapshotVersion == ls.code.version;
        MemoryViewDraw(Panel(panel, ICON_NEXT, "Memory", BLANK, NULL), &ls.scan, &ls.snapshot, fresh);
        // While the report is on its way, dots run in the panel's corner.
        if (RunnerBusy(&ls.probe)) LevelFxSpinner((Vector2){ panel.x + panel.width - 26, panel.y + 22 });
    }
    EditorWidget(&ls.code, editor);
    ls.fxLayout.editor = editor;
    if (ls.code.version != ls.typedVersion) {  // typed something: sparks at the caret
        ls.typedVersion = ls.code.version;
        if (UiHasFocus(&ls.code)) LevelFxKey(EditorTextPoint(&ls.code, editor, ls.code.cursor));
    }
    LevelFxEditorGlow();
    if (stripH) HintStrip((Rectangle){ pad, bottom - stripH, codeW, stripH });
    OutputColumn((Rectangle){ pad * 2 + codeW, top, W - pad * 3 - codeW, bottom - top });
    LevelFxSetLayout(ls.fxLayout);
    LevelFxDrawOver();

    if (ls.celebrating) {
        UiClearClip();
        action = Celebration();
        if (action.kind != LEVEL_STAY) {
            ls.celebrating = false;
            LevelScreenClose();
        }
        return action;
    }
    if (back || ls.demoBack) {
        ls.demoBack = false;
        LevelScreenClose();
        return (LevelAction){ LEVEL_BACK, -1 };
    }
    if (run && !busy) StartRun(true);
    return action;
}

// ---------------------------------------------------------------------------
// Dev aids

void LevelScreenDemoRun(void)
{
    if (ls.open) StartRun(true);
}

void LevelScreenDemo(int stepIndex, int hints, bool solved)
{
    if (!ls.open || stepIndex < 0 || stepIndex >= ls.lesson->stepCount) return;
    ls.demo = true;  // a shot shouldn't save anything
    OpenStep(stepIndex, false);
    if (solved) EditorSetText(&ls.code, CurrentStep()->solution);
    ls.hintsShown = hints < 0 ? 0 : hints > STEP_HINT_LEVELS ? STEP_HINT_LEVELS : hints;
    ls.hintVersion = ls.code.version - 1;
    if (solved) StartRun(false);
}

void LevelScreenDemoBack(void) { ls.demoBack = true; }

void LevelScreenDemoType(const char *typed)
{
    if (!ls.open) return;
    // Over the step's "// ?" line if it has one, otherwise at the end of the code.
    const char *gap = strstr(ls.code.text, "// ?");
    if (gap) {
        EditorSetCursor(&ls.code, (int)(gap - ls.code.text), false);
        EditorSetCursor(&ls.code, (int)(gap - ls.code.text) + 4, true);
        EditorDeleteSelection(&ls.code, 0);
    } else {
        EditorMoveCursor(&ls.code, MOVE_DOC_END, false);
    }
    for (; *typed; typed++) EditorTypeChar(&ls.code, *typed, 0);
    EditorSuggest(&ls.code);
    UiFocus(&ls.code);
}

void LevelScreenDemoCelebration(void)
{
    ls.celebrating = true;
    ls.celebrateAt = GetTime() - 2;  // the pips have all ticked in
}

bool LevelScreenBusy(void)
{
    return ls.open && (RunnerBusy(&ls.runner) || RunnerBusy(&ls.checker) || RunnerBusy(&ls.probe));
}
