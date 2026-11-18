// Playing a level's coding steps. See level_screen.h.
#include "level/level_screen.h"
#include "learning/answers.h"
#include "code/case_runner.h"
#include "code/drafts.h"
#include "core/text_copy.h"
#include "code/editor.h"
#include "ui/easing.h"
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
#include "level/step_novelty.h"
#include "level/piece_demo_view.h"
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
#define AUTO_NEXT_SECONDS 2.2     // after a pass, the next step opens by itself this much later
#define NEW_GLOW_SECONDS 5.0      // what a step shows for the first time glows in the code this long
#define EXAMPLE_DELAY 0.9         // a step not yet passed plays its first new piece's example this much later
#define NEW_BURST_SECONDS 1.2     // after a burst of rings round each place it appears

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
    bool practice;
    const LessonSet *set;
    int lessonIndex;
    const Lesson *lesson;
    int step;          // the step being played
    char draftFolder[600];

    Editor code;
    CaseRunner runner; // runs the learner's code on every test case of the step
    bool wasBusy;
    bool learnerRun;   // the latest run was started by the learner (not the automatic first run)
    unsigned markedVersion;

    bool ran;          // a run has finished since the step opened
    bool passed;       // the last run printed every test case's target output
    int shownCase;     // the test case the target, input and output panels show
    bool rewarded;     // this opening of the step has already earned its XP
    // A step passed before earns XP again only for new work (StepReplayEarns): measured from
    // the code it opened with, or from the starter once Reset is pressed and not undone.
    char *openedCode;  // the code the step opened with (usually its draft)
    int resetDepth;    // the editor's undo depth just after the latest Reset; 0 for none
    double unearnedAt; // when a pass earned nothing: Reset beckons for a while
    double passedAt, failedAt;
    // After a pass the next step opens by itself: at autoNextAt (< 0 when none is due), unless
    // the code is edited first (it was autoNextVersion when the pass came).
    double autoNextAt;
    unsigned autoNextVersion;

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
    // The level's review page, over the step: shown when the last step of the level is passed
    // (then with confetti and its stages ticking in: reviewFresh) and from a finished level's
    // last step. Each stage's tile previews the code written for it (reviewCode, malloc'd).
    bool reviewOpen;
    bool reviewFresh;
    double reviewOpenedAt;
    char *reviewCode[MAX_STEPS];
    float reviewHover[MAX_STEPS];  // each review tile's hover, eased (0 to 1)

    // Animation of the case chips: where the highlight under the chip on show is (it slides
    // to a newly chosen chip; < 0 snaps it there), and when each case's result arrived (its
    // tick or cross pops in then; < 0 while it hasn't).
    float chipHighlightX;
    double caseResultAt[MAX_STEP_CASES];
    float stepButtonAppear;  // the toolbar's Next or Review button gliding in (0 to 1)

    // What the step shows for the first time (step_novelty.h): a shelf of its pieces pops into
    // the toolbar as the step opens (at noveltyAt) and each place they appear in the code
    // lights up. Hovering a piece's chip (noveltyFocus, from noveltyFocusAt) lights its places
    // up again and shows a model line using it (noveltyModel, as a shape: names blanked)
    // under the chip (noveltyModelAt).
    StepNovelty novelty;
    double noveltyAt;
    int noveltyFocus;
    double noveltyFocusAt;
    char noveltyModel[STEP_HINT_TEXT];
    Vector2 noveltyModelAt;
    int noveltyDemoHover;  // --shot: a chip to treat as hovered (-1 none)

    // A new piece's example playing (piece_demo_view.h): which chip's (-1 none), when it
    // started (a replay starts it again), and where its panel was drawn last frame, so
    // clicks over it don't reach what is underneath. A step not yet passed plays its first
    // piece's example by itself at exampleAutoAt (-1 once done or not wanted).
    int examplePiece;
    double exampleAt;
    Rectangle exampleBox;
    double exampleAutoAt;
    bool exampleByItself;  // it opened by itself, so it folds away once it has played
    Rectangle shelfChips[NOVELTY_MAX];  // the chips as drawn, to tell a click on one from a click outside
    int shelfChipCount;
    bool demo;         // set up for a --shot: nothing is saved
    bool demoBack;     // --shot: leave the level as if Back had been clicked
} ls;

static bool Cpp(void) { return ls.lesson->track == TRACK_CPP; }
static const Step *CurrentStep(void) { return &ls.lesson->steps[ls.step]; }

static void DraftKey(char *out, size_t size) { snprintf(out, size, "%s_step%d", ls.lesson->id, ls.step + 1); }

static void SaveDraft(void)
{
    if (ls.demo || ls.practice) return;
    char key[64];
    DraftKey(key, sizeof key);
    DraftSave(ls.draftFolder, key, Cpp(), ls.code.text, (size_t)ls.code.len);
}

static void FreeReviewCode(void)
{
    for (int i = 0; i < MAX_STEPS; i++) {
        free(ls.reviewCode[i]);
        ls.reviewCode[i] = NULL;
    }
}

// Shows the level's review page. fresh: the level has just been finished, so it celebrates.
// Each stage's tile shows the code saved for it (its solution if there's no draft, as in a
// --shot).
static void OpenReview(bool fresh)
{
    SaveDraft();  // the stage on show, as it is now
    FreeReviewCode();
    for (int i = 0; i < ls.lesson->stepCount; i++) {
        char key[64];
        snprintf(key, sizeof key, "%s_step%d", ls.lesson->id, i + 1);
        char *draft = ls.draftFolder[0] ? DraftLoad(ls.draftFolder, key, Cpp()) : NULL;
        ls.reviewCode[i] = draft ? draft : CopyText(ls.lesson->steps[i].solution);
    }
    memset(ls.reviewHover, 0, sizeof ls.reviewHover);
    ls.reviewOpen = true;
    ls.examplePiece = -1;
    ls.reviewFresh = fresh;
    ls.reviewOpenedAt = GetTime();
    ls.autoNextAt = -1;
    UiFocus(NULL);
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
// pointers to show, on the test case on show. The learner isn't kept waiting for it.
static void StartProbe(void)
{
    RunnerCancel(&ls.probe);
    UpdateScan();
    if (!ls.scan.hasPointers || ls.code.version != ls.ranVersion) return;
    char *instrumented = ProbeInstrument(ls.code.text);
    if (!instrumented) return;
    ls.probingVersion = ls.code.version;
    RunnerStart(&ls.probe, instrumented, Cpp(), CurrentStep()->cases[ls.shownCase].input);
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
    const Step *step = CurrentStep();
    const char *inputs[MAX_STEP_CASES];
    for (int i = 0; i < step->caseCount; i++) inputs[i] = step->cases[i].input;
    CaseRunnerStart(&ls.runner, ls.code.text, Cpp(), inputs, step->caseCount);
    for (int i = 0; i < MAX_STEP_CASES; i++) ls.caseResultAt[i] = -1;
}

// Whether the last run printed test case index's target.
static bool CaseMatches(int index)
{
    const CaseOutcome *outcome = &ls.runner.outcomes[index];
    return outcome->result == RESULT_OK && !outcome->truncated
        && RunnerOutputMatches(outcome->output, CurrentStep()->cases[index].expected, NULL);
}

// Shows test case index in the panels, its lines typed out afresh.
static void ShowCase(int index)
{
    ls.shownCase = index;
    ls.outputScroll = ls.targetScroll = 0;
    ls.shownAt = GetTime();
    ls.linesRevealed = 0;
}

// Whether Reset was pressed on this opening of the step and not undone since (undoing it
// takes the undo depth below where it left it): the step is being worked afresh.
static bool ResetStands(void) { return ls.resetDepth > 0 && ls.code.undoCount >= ls.resetDepth; }

// Whether the learner is looking back over a finished level: every step is passed, and this
// one isn't being worked afresh after a Reset. Only then is there a Next button; otherwise a
// pass moves on to the next step by itself.
static bool Reviewing(void) { return RoadmapLevelComplete(ls.lesson) && !ResetStands(); }

// Whether this pass earns XP. A first pass always does. A step passed before earns again
// only for new work since the code it started from: the starter if Reset was pressed (and
// not undone since), otherwise the code it opened with.
static bool EarnsXp(bool passedBefore)
{
    if (!passedBefore) return true;
    const char *before = ResetStands() ? CurrentStep()->starter : ls.openedCode ? ls.openedCode : "";
    return StepReplayEarns(CurrentStep(), before, ls.code.text);
}

// The learner's first pass of this opening of the step: progress, XP in the skills their code
// used (less for hints) with the reward animation, and the celebration if the level is done.
// Replaying a passed step earns XP only for new work (see EarnsXp).
static void RecordPass(void)
{
    if (ls.demo || ls.practice) return;  // revision and shots never mutate ordinary progress
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
    if (!levelBefore && RoadmapLevelComplete(ls.lesson)) OpenReview(true);
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
    // On to the next step once the pass has been celebrated: the last step's pass ends the
    // level instead, and a finished level being reviewed waits for its Next button.
    if (ls.step + 1 < ls.lesson->stepCount && !ls.reviewOpen && !Reviewing()) {
        ls.autoNextAt = ls.passedAt + AUTO_NEXT_SECONDS;
        ls.autoNextVersion = ls.code.version;
    }
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
        bool useful = out && !RunnerOutputMatches(out, CurrentStep()->cases[0].expected, &badLine);
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
            ShowCase(0);  // the second run changes the main case: its output is typed out in its turn
            CheckFailed(CHECK_VALUES_FAILED);
        }
    }
}

// Called once when a run finishes: error marks, or every test case's output checked against
// its target. The panels move to the first case that went wrong.
static void RunFinished(void)
{
    Runner *r = &ls.runner.runner;  // the build: its diagnostics
    ls.ran = true;
    int firstWrong = -1;
    for (int i = 0; i < CurrentStep()->caseCount && firstWrong < 0; i++)
        if (!CaseMatches(i)) firstWrong = i;
    ShowCase(firstWrong >= 0 ? firstWrong : ls.shownCase);
    bool compileFailed = ls.runner.buildFailed && ls.runner.outcomes[0].result == RESULT_COMPILE_ERROR;
    if (compileFailed) {
        int lines[RUNNER_MAX_DIAGS], n = 0;
        for (int i = 0; i < r->diagCount; i++) if (r->diags[i].error) lines[n++] = r->diags[i].line;
        EditorSetMarks(&ls.code, lines, n);
        ls.markedVersion = ls.code.version;
        if (ls.learnerRun) {  // lightning from the first faulty line
            if (n > 0) LevelFxLightning(EditorTextPoint(&ls.code, ls.fxLayout.editor, EditorPosOf(&ls.code, lines[0] - 1, 0)));
            LevelFxEmit(FX_COMPILE_FAILED);
        }
    }
    if (ls.runner.outcomes[ls.shownCase].result == RESULT_OK) StartProbe();
    bool wasPassed = ls.passed;
    ls.passed = firstWrong < 0;
    if (ls.passed) {
        if (!wasPassed) ls.passedAt = GetTime();
        if (ls.learnerRun) BeginCheck();
    } else if (ls.learnerRun) {
        ls.failedAt = GetTime();
        MascotReact(FOX_EDITOR, FOX_INCORRECT);
        if (!compileFailed) LevelFxEmit(FX_OUTPUT_WRONG);
    }
}

// ---------------------------------------------------------------------------
// Opening steps

// Plays step index. When leaving another step of this level, saves its draft first.
static void OpenStep(int index, bool leavingStep)
{
    if (leavingStep) SaveDraft();
    CaseRunnerCancel(&ls.runner);
    RunnerCancel(&ls.checker);
    RunnerCancel(&ls.probe);
    memset(&ls.snapshot, 0, sizeof ls.snapshot);
    MemoryViewReset();
    ls.step = index;
    const Step *step = CurrentStep();
    StepVariationFree(&ls.variation);
    StepVariationMake(step, &ls.variation);
    StepNoveltyFind(ls.set, ls.lessonIndex, index, &ls.novelty);
    ls.noveltyAt = GetTime();
    ls.noveltyFocus = ls.noveltyDemoHover = -1;
    ls.examplePiece = -1;
    ls.exampleAutoAt = ProgressStepDone(ls.lesson->id, index) || ls.demo ? -1 : GetTime() + EXAMPLE_DELAY;
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
    ls.shownCase = 0;
    ls.chipHighlightX = -1;  // straight to the first chip
    ls.stepButtonAppear = 0;
    ls.passedAt = ls.failedAt = -10;
    ls.autoNextAt = -1;
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
    ls.reviewOpen = ls.demo = false;
    ls.practice=false;
    LevelFxReset();
    OpenStep(RoadmapResumeStep(ls.lesson), false);
    ls.open = true;
}

void LevelScreenPractice(const Lesson *lesson) {
    static LessonSet practice;
    practice.items=(Lesson *)lesson;practice.count=1;
    LevelScreenOpen(&practice,0,NULL);ls.practice=true;
    ls.novelty.count=0;  // a lone exam task: nothing to measure "new" against
}

void LevelScreenClose(void)
{
    if (!ls.open) return;
    SaveDraft();
    CaseRunnerCancel(&ls.runner);
    RunnerCancel(&ls.checker);
    RunnerCancel(&ls.probe);
    ls.open = false;
    UiFocus(NULL);
}

void LevelScreenShutdown(void)
{
    LevelScreenClose();
    if (!ls.ready) return;
    CaseRunnerFree(&ls.runner);
    RunnerFree(&ls.checker);
    RunnerFree(&ls.probe);
    StepVariationFree(&ls.variation);
    free(ls.variedExpected);
    free(ls.variedCode);
    free(ls.openedCode);
    ls.variedExpected = ls.variedCode = ls.openedCode = NULL;
    FreeReviewCode();
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
static float PopScale(float age) { return PopOvershoot(age / 0.22f); }

// Whether the checks refused the code itself (step_check.h, step_cheats.h): it prints the
// target, but by writing the answer in, skipping what the step is about or editing a given.
// Then none of that run's output counts as right, however much of it matches.
static bool CodeRefused(void)
{
    return ls.check == CHECK_GIVEN_CHANGED || ls.check == CHECK_CONSTRUCT_MISSING || ls.check == CHECK_ANSWER_WRITTEN
        || ls.check == CHECK_FUNCTION_UNTOUCHED || ls.check == CHECK_INDEXED;
}

// Lines of output in area, scrolled by *scroll (the wheel scrolls it). When compareWith is
// set, each line is ticked where it matches the same line there, or crossed and tinted red,
// and lines missing from the end show as empty red slots; the lines then appear one after
// another from ls.shownAt, typed out, each sparking as its tick or cross pops in. allWrong
// crosses every line, matching or not (the code was refused: CodeRefused).
static void OutputLinesView(Rectangle area, const char *text, const char *compareWith, bool allWrong, float *scroll)
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
            bool same = !allWrong && SameLine(text, compareWith, i);
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

// What the program did on the test case on show, when it didn't print anything to compare:
// compile errors (line and message), a crash or a timeout.
static void ProblemView(Rectangle area)
{
    const Runner *r = &ls.runner.runner;  // the build: its diagnostics
    const CaseOutcome *outcome = &ls.runner.outcomes[ls.shownCase];
    float size = 17 * UiTextScale(), y = area.y;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    if (outcome->result == RESULT_COMPILE_ERROR && r->errorCount > 0) {
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
        y += UiWrappedText(gFonts.text, outcome->summary, (Vector2){ area.x + 30, y }, area.width - 34, size, COL_BAD,
                           true) + 10;
        // A failed build with no line to point at (a link error, say): the compiler's own words.
        if (outcome->result == RESULT_COMPILE_ERROR && r->compileOutput)
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

// A step with several test cases: a chip per case above the panels, numbered, the one on
// show filled. Once a run is over each gets a tick or a cross; during it, the cases still to
// run are dim. Clicking a chip shows that case. Returns the height used.
static float CaseChips(Rectangle row)
{
    const Step *step = CurrentStep();
    if (step->caseCount < 2) return 0;
    const CaseRunner *c = &ls.runner;
    float chipW = 74, chipH = 34, gap = 8, x = row.x;
    // The accent highlight glides from chip to chip.
    float shownX = row.x + ls.shownCase * (chipW + gap);
    if (ls.chipHighlightX < 0) ls.chipHighlightX = shownX;
    EaseToward(&ls.chipHighlightX, shownX, 16, GetFrameTime());
    for (int i = 0; i < step->caseCount; i++) {
        Rectangle chip = { x + i * (chipW + gap), row.y, chipW, chipH };
        bool over = i != ls.shownCase && UiMouseOver(chip);
        UiRoundRect(chip, 8, over ? COL_SECONDARY_HOVER : COL_PANEL);
    }
    UiRoundRect((Rectangle){ ls.chipHighlightX, row.y, chipW, chipH }, 8, COL_ACCENT);
    for (int i = 0; i < step->caseCount; i++, x += chipW + gap) {
        Rectangle chip = { x, row.y, chipW, chipH };
        // A chip's ink turns as the highlight passes under it.
        float under = Clamp01(1 - fabsf(ls.chipHighlightX - chip.x) / chipW);
        bool shown = i == ls.shownCase, over = !shown && UiMouseOver(chip);
        if (over) UiWantHand();
        Color ink = ColorLerp(COL_TEXT, COL_ON_ACCENT, under);
        IconDraw(ICON_USER, (Vector2){ chip.x + 16, chip.y + chipH / 2 }, 15, ink);
        UiText(gFonts.bold, TextFormat("%d", i + 1), chip.x + 29, chip.y + 7, 18, ink);
        // How the case went: a tick or a cross popping in as its result arrives, a dim dot
        // while it waits.
        Vector2 mark = { chip.x + chipW - 17, chip.y + chipH / 2 };
        if (c->outcomes[i].result != RESULT_NONE) {
            if (ls.caseResultAt[i] < 0) ls.caseResultAt[i] = GetTime();
            float pop = PopScale((float)(GetTime() - ls.caseResultAt[i]));
            bool right = CaseMatches(i) && !CodeRefused() && !(i == 0 && ls.check == CHECK_VALUES_FAILED);
            Color sign = right ? COL_GOOD : COL_BAD;
            DrawCircleV(mark, 10 * pop, ColorLerp(sign, COL_ON_ACCENT, under));
            IconDraw(right ? ICON_CHECK : ICON_CROSS, mark, 12 * pop, ColorLerp(COL_ON_ACCENT, sign, under));
        } else {
            DrawCircleV(mark, 4, Fade(ink, 0.35f));
        }
        if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ShowCase(i);
            if (!CaseRunnerBusy(c) && c->outcomes[i].result == RESULT_OK) StartProbe();  // its picture of memory
        }
    }
    return chipH + 10;
}

// The right-hand column: the test case chips (if the step has several), then the target, the
// input if the case has one, and what the program printed, for the case on show. After a
// failed second run it shows that run instead (of the main case): the target, input and
// output for the changed values.
static void OutputColumn(Rectangle column)
{
    const Step *step = CurrentStep();
    const CaseOutcome *outcome = &ls.runner.outcomes[ls.shownCase];
    bool runBusy = CaseRunnerBusy(&ls.runner);
    double now = GetTime();
    float gap = 12;
    float chipsH = CaseChips(column);
    column.y += chipsH;
    column.height -= chipsH;
    float targetH = (column.height - gap) * 0.42f;
    float y = column.y;
    bool varied = ls.check == CHECK_VALUES_FAILED && ls.shownCase == 0;
    bool checking = ls.check == CHECK_EXPECTING || ls.check == CHECK_RUNNING;
    bool refused = ls.check == CHECK_VALUES_FAILED || CodeRefused();
    const StepCase *shownCase = &step->cases[ls.shownCase];
    const char *expected = varied ? ls.variedExpected : shownCase->expected;
    const char *input = varied ? ls.variation.input : shownCase->input;

    Rectangle target = { column.x, y, column.width, targetH };
    OutputLinesView(Panel(target, ICON_TARGET, "Target", BLANK, LevelFxTargetBackdrop), expected, NULL, false,
                    &ls.targetScroll);
    y += targetH + gap;

    // Between the panels: how much of the output matches the target so far (none of it, when
    // the code was refused).
    const char *shown = CodeRefused() ? NULL : varied ? ls.checker.programOutput
                      : outcome->result == RESULT_OK ? outcome->output : NULL;
    int wanted = OutputLines(expected), matching = 0;
    for (int i = 1; shown && ls.ran && !runBusy && i <= wanted && i <= ls.linesRevealed; i++)
        matching += SameLine(shown, expected, i);
    int rows = shown && OutputLines(shown) > wanted ? OutputLines(shown) : wanted;
    LevelFxMatchBar((Rectangle){ column.x + 4, y - gap + 4, column.width - 8, 4 }, rows ? (float)matching / rows : 0);

    if (input) {
        float inputH = 44 + 18 * UiTextScale() * 1.45f * (float)(OutputLines(input) > 3 ? 3 : OutputLines(input)) + 10;
        float scroll = 0;
        OutputLinesView(Panel((Rectangle){ column.x, y, column.width, inputH }, ICON_USER, "Input", BLANK, NULL), input,
                        NULL, false, &scroll);
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
    bool wrong = refused || (!ls.passed && ls.ran && ls.learnerRun && !runBusy);
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
    if (runBusy) {
        UiText(gFonts.mono, TextFormat("%.*s", dots + 1, "...."), area.x + 6, area.y, 22, COL_DIM);
    } else if (varied) {
        const char *out = ls.checker.result == RESULT_OK && ls.checker.programOutput ? ls.checker.programOutput : "";
        OutputLinesView(area, out, expected, false, &ls.outputScroll);
    } else if (ls.ran && outcome->result == RESULT_OK) {
        OutputLinesView(area, outcome->output, shownCase->expected, CodeRefused(), &ls.outputScroll);
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
// What the step shows for the first time

// The index in ls.novelty of a piece's key, or -1 if it isn't new.
static int NovelIndex(const char *key)
{
    for (int i = 0; i < ls.novelty.count; i++)
        if (strcmp(ls.novelty.pieces[i].key, key) == 0) return i;
    return -1;
}

// How strongly a glow that started at `since` seconds ago shows now: fully, then fading out
// over its last second.
static float GlowStrength(float since)
{
    if (since < 0 || since > NEW_GLOW_SECONDS) return 0;
    return since > NEW_GLOW_SECONDS - 1 ? (float)NEW_GLOW_SECONDS - since : 1;
}

// Over the editor: every place a new piece appears in the code glows gold, after a burst of
// rings when the step opens (or when its chip is hovered).
static void NoveltySpotlight(Rectangle editor)
{
    if (!ls.novelty.count) return;
    float now = (float)GetTime(), sinceOpen = now - (float)ls.noveltyAt, sinceFocus = now - (float)ls.noveltyFocusAt;
    if (GlowStrength(sinceOpen) <= 0 && (ls.noveltyFocus < 0 || GlowStrength(sinceFocus) <= 0)) return;
    static CodePiece pieces[512];
    int count = CodePiecesFind(ls.code.text, pieces, 512);
    float lineH = ls.code.fontSize * 1.3f, pulse = (sinf(now * 5) + 1) / 2;
    BeginScissorMode((int)editor.x, (int)editor.y, (int)editor.width, (int)editor.height);
    for (int i = 0; i < count; i++) {
        int novel = NovelIndex(pieces[i].key);
        if (novel < 0) continue;
        float since = novel == ls.noveltyFocus ? sinceFocus : sinceOpen;
        float strength = GlowStrength(since);
        if (strength <= 0) continue;
        Vector2 from = EditorTextPoint(&ls.code, editor, pieces[i].start);
        Vector2 to = EditorTextPoint(&ls.code, editor, pieces[i].start + pieces[i].length);
        float width = to.y == from.y ? to.x - from.x : pieces[i].length * ls.code.fontSize * 0.55f;
        Rectangle box = { from.x - 3, from.y - lineH / 2, width + 6, lineH };
        UiRoundRect(box, 5, Fade(COL_GOLD, (0.14f + 0.1f * pulse) * strength));
        DrawRectangleRoundedLinesEx(box, 0.4f, 6, 2, Fade(COL_GOLD, 0.85f * strength));
        if (since < NEW_BURST_SECONDS) {  // rings bursting out, then a star landing on the corner
            float u = since / (float)NEW_BURST_SECONDS;
            Vector2 middle = { box.x + box.width / 2, box.y + box.height / 2 };
            DrawRing(middle, 8 + 70 * u, 10 + 70 * u, 0, 360, 48, Fade(COL_GOLD, 0.7f * (1 - u)));
            DrawRing(middle, 4 + 40 * u, 5.5f + 40 * u, 0, 360, 48, Fade(COL_GOLD, 0.5f * (1 - u)));
        }
        IconDraw(ICON_STAR, (Vector2){ box.x + box.width, box.y }, 13 * PopOvershoot(since / 0.4f) * strength, COL_GOLD);
    }
    EndScissorMode();
}

// The model line for a piece: the first line of the step's solution that uses it, as its
// shape (names and values blanked, so it shows how the piece is written, not the answer).
static void NoveltyModel(const char *key)
{
    ls.noveltyModel[0] = '\0';
    const char *solution = CurrentStep()->solution;
    char line[STEP_HINT_TEXT];
    for (const char *p = solution; *p && !ls.noveltyModel[0];) {
        const char *end = strchr(p, '\n');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        while (length && (*p == ' ' || *p == '\t')) p++, length--;
        snprintf(line, sizeof line, "%.*s", (int)(length < sizeof line - 1 ? length : sizeof line - 1), p);
        CodePiece pieces[64];
        int count = CodePiecesFind(line, pieces, 64);
        for (int i = 0; i < count; i++)
            if (strcmp(pieces[i].key, key) == 0) {
                StepHintShape(line, ls.noveltyModel, sizeof ls.noveltyModel);
                break;
            }
        p = end ? end + 1 : p + length;
    }
}

// The shelf of new pieces in the toolbar, from x to right: a gold star, then a chip per piece
// popping in one after another, its code in syntax colours. A piece the learner will write
// (it's only in the solution) carries a plus. Hovering a chip lights its places up again and
// readies its model line (drawn last, over the editor: NoveltyModelPopup).
static void NoveltyShelf(float x, float y, float right, float height)
{
    ls.noveltyModelAt.x = -1;
    ls.shelfChipCount = 0;
    if (!ls.novelty.count || x + 60 > right) return;
    float since = (float)(GetTime() - ls.noveltyAt), size = 18;
    IconDraw(ICON_STAR, (Vector2){ x + 14, y + height / 2 }, 22 * PopOvershoot(since / 0.35f), COL_GOLD);
    float chipX = x + 32;
    int hovered = -1;
    for (int i = 0; i < ls.novelty.count; i++) {
        const NovelPiece *piece = &ls.novelty.pieces[i];
        float plus = piece->inStarter ? 0 : 16;
        bool hasExample = PieceDemoFind(piece->key) != NULL;
        float play = hasExample ? 16 : 0;
        float width = UiTextWidth(gFonts.mono, piece->key, size) + 22 + plus + play;
        if (chipX + width > right) break;
        float pop = PopOvershoot((since - 0.15f - i * 0.09f) / 0.25f);
        if (pop <= 0) break;
        Rectangle chip = { chipX, y, width, height };
        ls.shelfChips[ls.shelfChipCount++] = chip;
        bool over = UiMouseOver(chip) || i == ls.noveltyDemoHover;
        bool playing = i == ls.examplePiece;
        if (over) {
            UiWantHand();
            hovered = i;
            // A click plays its example (from the start), or closes the one playing.
            if (hasExample && UiMouseOver(chip) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ls.examplePiece = playing ? -1 : i;
                ls.exampleAt = GetTime();
                ls.exampleAutoAt = -1;
                ls.exampleByItself = false;
            }
        }
        Rectangle drawn = { chip.x + chip.width * (1 - pop) / 2, chip.y + chip.height * (1 - pop) / 2, chip.width * pop,
                            chip.height * pop };
        UiRoundRect(drawn, 8, playing ? COL_CODE_BG : over ? COL_SECONDARY_HOVER : COL_PANEL);
        DrawRectangleRoundedLinesEx(drawn, 0.3f, 8, playing ? 2.5f : 1.5f, Fade(COL_GOLD, over || playing ? 1 : 0.6f));
        if (pop > 0.9f) {
            if (plus) IconDraw(ICON_PLUS, (Vector2){ chip.x + 14, chip.y + height / 2 }, 12, COL_GOLD);
            UiCodeLine(piece->key, chip.x + 11 + plus, chip.y + height / 2 - size * 0.5f, size);
            if (play) IconDraw(ICON_PLAY, (Vector2){ chip.x + width - 14, chip.y + height / 2 }, 11,
                               Fade(COL_GOLD, playing || over ? 1 : 0.7f));
        }
        if (over) ls.noveltyModelAt = (Vector2){ chip.x, chip.y + height + 6 };
        chipX += width + 6;
    }
    if (hovered >= 0 && hovered != ls.noveltyFocus) {
        ls.noveltyFocus = hovered;
        ls.noveltyFocusAt = GetTime();
        NoveltyModel(ls.novelty.pieces[hovered].key);
    } else if (hovered < 0 && ls.noveltyFocus >= 0 && GetTime() - ls.noveltyFocusAt > NEW_BURST_SECONDS) {
        ls.noveltyFocus = -1;  // let go: hovering it again bursts again
    }
}

// The hovered chip's model line, in a code panel under it.
static void NoveltyModelPopup(void)
{
    if (ls.noveltyModelAt.x < 0 || !ls.noveltyModel[0] || ls.examplePiece >= 0) return;
    float size = 18 * UiTextScale();
    float width = UiTextWidth(gFonts.mono, ls.noveltyModel, size) + 28;
    Rectangle box = { ls.noveltyModelAt.x, ls.noveltyModelAt.y, width, size * 1.6f + 16 };
    if (box.x + box.width > GetScreenWidth() - 12) box.x = GetScreenWidth() - 12 - box.width;
    UiRoundRect((Rectangle){ box.x, box.y + 4, box.width, box.height }, 10, Fade(BLACK, 0.18f));
    UiRoundRect(box, 10, COL_CODE_BG);
    DrawRectangleRoundedLinesEx(box, 0.25f, 8, 2, COL_GOLD);
    UiCodeLine(ls.noveltyModel, box.x + 14, box.y + box.height / 2 - size * 0.6f, size);
}

// A new piece's example (piece_demo_view.h), in a panel under its chip, over the editor.
// It closes with its button, Esc, its chip, or a click anywhere else.
static void NoveltyExample(float top)
{
    ls.exampleBox = (Rectangle){ 0, 0, 0, 0 };
    // A step not yet passed plays its first new piece's example once the shelf has popped in.
    if (ls.exampleAutoAt >= 0 && GetTime() >= ls.exampleAutoAt) {
        ls.exampleAutoAt = -1;
        for (int i = 0; i < ls.shelfChipCount && ls.examplePiece < 0; i++)
            if (PieceDemoFind(ls.novelty.pieces[i].key)) {
                ls.examplePiece = i;
                ls.exampleAt = GetTime();
                ls.exampleByItself = true;
            }
    }
    if (ls.examplePiece < 0 || ls.examplePiece >= ls.shelfChipCount) return;
    const char *piece = ls.novelty.pieces[ls.examplePiece].key;
    const PieceDemo *example = PieceDemoFind(piece);
    if (!example) return;
    Vector2 size = PieceDemoViewSize(example);
    float W = (float)GetScreenWidth();
    Rectangle chip = ls.shelfChips[ls.examplePiece];
    float x = fmaxf(16, fminf(chip.x - 24, W - 16 - size.x));
    Rectangle box = { x, top, size.x, size.y };
    float since = (float)(GetTime() - ls.exampleAt);
    float open = EaseOutCubic(Clamp01(since / 0.25f));  // it drops open from the chip
    UiClaimEscape();  // Esc closes it rather than leaving the level
    BeginScissorMode((int)box.x - 10, (int)box.y - 10, (int)box.width + 20, (int)((box.height + 20) * open));
    PieceDemoViewAction action = PieceDemoViewDraw(example, piece, box, since);
    EndScissorMode();
    // A thread of gold from the chip to the panel.
    DrawLineEx((Vector2){ chip.x + chip.width / 2, chip.y + chip.height }, (Vector2){ chip.x + chip.width / 2, box.y },
               2, Fade(COL_GOLD, 0.8f));
    ls.exampleBox = box;

    bool clickedOutside = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), box);
    for (int i = 0; i < ls.shelfChipCount && clickedOutside; i++)
        if (CheckCollisionPointRec(GetMousePosition(), ls.shelfChips[i])) clickedOutside = false;  // the chip decides
    if (action == PIECE_DEMO_VIEW_REPLAY) {
        ls.exampleAt = GetTime();
        ls.exampleByItself = false;  // asked for again: it stays
    }
    if (ls.exampleByItself && since > PieceDemoViewLength(example) + 2.5f) ls.examplePiece = -1;  // played through
    if (action == PIECE_DEMO_VIEW_CLOSE || IsKeyPressed(KEY_ESCAPE) || clickedOutside) ls.examplePiece = -1;
}

// ---------------------------------------------------------------------------
// The step pips and the toolbar

// A circle per step in the top bar: passed ones ticked, the current one ringed with its
// number. Every step can be selected, regardless of earlier completions. While the next step
// is about to open by itself, a ring fills round its circle as the countdown runs.
static void StepPips(void)
{
    int count = ls.lesson->stepCount;
    float gap = 8, width = count * (PIP_RADIUS * 2 + gap) - gap;
    float x = GetScreenWidth() - 24 - width + PIP_RADIUS, y = TOPBAR_H / 2;
    for (int i = 0; i < count; i++, x += PIP_RADIUS * 2 + gap) {
        Vector2 c = { x, y };
        bool done = ProgressStepDone(ls.lesson->id, i), current = i == ls.step;
        bool over = !current && CheckCollisionPointCircle(GetMousePosition(), c, PIP_RADIUS + 3)
                 && UiMouseOver((Rectangle){ c.x - PIP_RADIUS, c.y - PIP_RADIUS, PIP_RADIUS * 2, PIP_RADIUS * 2 });
        if (over) UiWantHand();
        if (current) {  // ringed, with a dot in orbit
            DrawRing(c, PIP_RADIUS + 3, PIP_RADIUS + 5.5f, 0, 360, 48, COL_ACCENT);
            float orbit = (float)GetTime() * 2.2f;
            DrawCircleV((Vector2){ c.x + cosf(orbit) * (PIP_RADIUS + 4.25f), c.y + sinf(orbit) * (PIP_RADIUS + 4.25f) }, 3,
                        COL_ON_ACCENT);
            ls.fxLayout.pip = c;
        }
        float swell = 1;
        if (i == ls.step + 1 && ls.autoNextAt >= 0) {
            float share = Clamp01(1 - (float)((ls.autoNextAt - GetTime()) / AUTO_NEXT_SECONDS));
            DrawRing(c, PIP_RADIUS + 3, PIP_RADIUS + 5.5f, -90, -90 + 360 * share, 48, COL_ACCENT);
            float head = (-90 + 360 * share) * DEG2RAD;
            Vector2 spark = { c.x + cosf(head) * (PIP_RADIUS + 4.25f), c.y + sinf(head) * (PIP_RADIUS + 4.25f) };
            DrawCircleV(spark, 6, Fade(COL_ACCENT, 0.25f));
            DrawCircleV(spark, 3, COL_ON_ACCENT);
            swell = 1 + 0.12f * share * share;
        }
        DrawCircleV(c, PIP_RADIUS * swell, done ? COL_ACCENT : over ? COL_SECONDARY_HOVER : COL_PANEL_HI);
        if (done) IconDraw(ICON_CHECK, c, 14, COL_ON_ACCENT);
        else UiTextCentered(gFonts.bold, TextFormat("%d", i + 1), c.x, c.y - 9, 15,
                                                 current ? COL_ACCENT : COL_TEXT);
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

// One stage's tile on the review page: its number (ticked once it has appeared), its
// difficulty and the first lines of the code written for it. Returns whether it was clicked.
static bool ReviewTile(Rectangle tile, int index, float appear)
{
    bool over = UiMouseOver(tile);
    if (over) UiWantHand();
    float hover = EaseToward(&ls.reviewHover[index], over ? 1.0f : 0.0f, 14, GetFrameTime());
    float lift = 4 * hover;
    tile.y -= lift + (1 - appear) * 16;
    UiRoundRect((Rectangle){ tile.x, tile.y + 4 + lift, tile.width, tile.height }, 12, COL_PANEL_HI);
    UiRoundRect(tile, 12, ColorLerp(COL_PANEL, COL_SECONDARY_HOVER, hover));
    if (hover > 0.01f) DrawRectangleRoundedLinesEx(tile, 0.15f, 8, 2, Fade(COL_ACCENT, hover));

    Vector2 badge = { tile.x + 22, tile.y + 22 };
    DrawCircleV(badge, PIP_RADIUS, appear >= 1 ? COL_ACCENT : COL_PANEL_HI);
    if (appear >= 1) IconDraw(ICON_CHECK, badge, 14, COL_ON_ACCENT);
    UiText(gFonts.bold, TextFormat("%d", index + 1), tile.x + 44, tile.y + 12, 19, COL_TEXT);
    for (int bar = 0; bar < SKILL_MAX_DIFFICULTY; bar++) {
        float h = 6 + bar * 4;
        UiRoundRect((Rectangle){ tile.x + tile.width - 14 - (SKILL_MAX_DIFFICULTY - bar) * 8, tile.y + 30 - h, 5, h }, 2,
                    bar < ls.lesson->steps[index].difficulty ? COL_ACCENT : COL_PANEL_HI);
    }

    // The code, from main on (headers and blank lines say little), clipped to the tile.
    const char *code = ls.reviewCode[index] ? ls.reviewCode[index] : "";
    const char *from = strstr(code, "main(");
    if (from) while (from > code && from[-1] != '\n') from--;
    else from = code;
    float size = 12.5f, lineH = size * 1.35f, y = tile.y + 42;
    BeginScissorMode((int)tile.x + 10, (int)y, (int)tile.width - 20, (int)(tile.y + tile.height - 8 - y));
    char line[200];
    for (const char *p = from; *p && y < tile.y + tile.height - 8;) {
        const char *endOfLine = strchr(p, '\n');
        size_t length = endOfLine ? (size_t)(endOfLine - p) : strlen(p);
        if (length && p[length - 1] == '\r') length--;  // a draft saved with Windows line ends
        snprintf(line, sizeof line, "%.*s", (int)(length < sizeof line - 1 ? length : sizeof line - 1), p);
        UiCodeLine(line, tile.x + 14, y, size);
        y += lineH;
        p = endOfLine ? endOfLine + 1 : p + length;
    }
    EndScissorMode();
    return over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// The level's review page: a trophy, then a tile per stage with the code written for it
// (click one to go back to that stage), then back to the journey or on to the next level,
// which opens at its first stage not yet passed. Fresh from finishing the level, confetti
// falls and the stages tick in one by one.
static LevelAction LevelReview(void)
{
    LevelAction action = { LEVEL_STAY, -1 };
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    double age = GetTime() - ls.reviewOpenedAt;
    if (!ls.reviewFresh) age += 2;  // reopened: everything is in place at once
    DrawRectangle(0, 0, (int)W, (int)H, Fade(COL_BG, 0.86f));
    if (ls.reviewFresh) MascotConfetti((Rectangle){ 0, 0, W, H }, age, false);

    // As many tile columns as fit; the tiles shrink in height if the rows wouldn't fit.
    int count = ls.lesson->stepCount;
    float gap = 14, cardW = fminf(W - 60, 1100), inner = cardW - 48;
    int columns = (int)((inner + gap) / (230 + gap));
    if (columns < 1) columns = 1;
    if (columns > count) columns = count;
    int rows = (count + columns - 1) / columns;
    float tileW = (inner - gap * (columns - 1)) / columns;
    float header = 150, footer = 96;
    float tileH = fminf(150, (H - 40 - header - footer - gap * (rows - 1)) / rows);
    if (tileH < 70) tileH = 70;
    float cardH = header + rows * tileH + (rows - 1) * gap + footer;
    float x = (W - cardW) / 2, y = fmaxf(20, (H - cardH) / 2);
    float pop = 1 - powf(1 - fminf(1, (float)age / 0.35f), 3);
    y += (1 - pop) * 40;
    UiRoundRect((Rectangle){ x, y + 6, cardW, cardH }, 18, COL_PANEL_HI);
    UiRoundRect((Rectangle){ x, y, cardW, cardH }, 18, COL_BG);
    Vector2 trophy = { W / 2, y + 52 };
    float shine = (float)GetTime() * 0.4f;
    for (int ray = 0; ray < 12; ray++) {  // rays turning slowly behind the trophy
        float a = shine + ray * PI / 6, b = a + PI / 18;
        DrawTriangle(trophy, (Vector2){ trophy.x + cosf(b) * 52, trophy.y + sinf(b) * 52 },
                     (Vector2){ trophy.x + cosf(a) * 52, trophy.y + sinf(a) * 52 }, Fade(COL_GOLD, 0.12f));
    }
    float bounce = ls.reviewFresh ? PopOvershoot((float)(age - 0.15) / 0.45f) : 1;  // it lands with a bounce
    IconDraw(ICON_TROPHY, trophy, 58 * bounce, COL_GOLD);
    UiTextCentered(gFonts.bold, ls.lesson->title, W / 2, y + 94, 26, COL_TEXT);

    int clicked = -1;
    for (int i = 0; i < count; i++) {
        int row = i / columns, column = i % columns;
        Rectangle tile = { x + 24 + column * (tileW + gap), y + header + row * (tileH + gap), tileW, tileH };
        float appear = fminf(1, fmaxf(0, (float)(age - 0.3 - i * 0.06) / 0.2f));  // they tick in one by one
        if (ReviewTile(tile, i, appear)) clicked = i;
    }

    int next = NextLevel();
    float buttonY = y + cardH - 76, buttonW = next >= 0 ? 220 : 300;
    Rectangle journey = { next >= 0 ? W / 2 - buttonW - 10 : W / 2 - buttonW / 2, buttonY, buttonW, 54 };
    if (UiButtonIconEx(journey, ICON_BACK, "Journey", 21, COL_PANEL_HI, COL_SECONDARY_HOVER, COL_TEXT, true))
        action.kind = LEVEL_BACK;
    if (next >= 0 && (UiButtonIcon((Rectangle){ W / 2 + 10, buttonY, buttonW, 54 }, ICON_NEXT, "Next level", true)
                      || IsKeyPressed(KEY_ENTER))) {
        action.kind = LEVEL_OPEN_LEVEL;  // main.c opens it at its first stage not yet passed (RoadmapResumeStep)
        action.lessonIndex = next;
    }
    if (IsKeyPressed(KEY_ESCAPE)) action.kind = LEVEL_BACK;
    if (clicked >= 0 && action.kind == LEVEL_STAY) {  // back to that stage
        ls.reviewOpen = false;
        OpenStep(clicked, true);
    }
    return action;
}

LevelAction LevelScreen(void)
{
    LevelAction action = { LEVEL_STAY, -1 };
    if (!ls.open) return (LevelAction){ LEVEL_BACK, -1 };

    // Finish runs (the learner's, then the check's second run); drop error marks once the
    // code has been edited.
    CaseRunnerUpdate(&ls.runner);
    bool running = CaseRunnerBusy(&ls.runner);
    if (ls.wasBusy && !running) RunFinished();
    ls.wasBusy = running;
    RunnerUpdate(&ls.checker);
    bool checking = RunnerBusy(&ls.checker);
    if (ls.checkerWasBusy && !checking) CheckerFinished();
    ls.checkerWasBusy = checking = RunnerBusy(&ls.checker);  // one check run may start the next
    bool busy = CaseRunnerBusy(&ls.runner) || checking;
    RunnerUpdate(&ls.probe);  // in the background: it never holds the learner up
    if (ls.probeWasBusy && !RunnerBusy(&ls.probe)) ProbeFinished();
    ls.probeWasBusy = RunnerBusy(&ls.probe);
    UpdateScan();
    LevelFxActivity(CaseRunnerBusy(&ls.runner), checking);
    LevelFxUpdate(GetFrameTime());
    if (ls.code.markCount && ls.code.version != ls.markedVersion) EditorSetMarks(&ls.code, NULL, 0);
    UpdateHint();

    // While the review page is open, the step underneath is only a picture: nothing in it reacts.
    if (ls.reviewOpen) UiSetClip((Rectangle){ 0, 0, 0, 0 });
    // Likewise under a new piece's example (where it was drawn last frame).
    bool overExample = ls.examplePiece >= 0 && CheckCollisionPointRec(GetMousePosition(), ls.exampleBox);
    if (overExample) UiSetClip((Rectangle){ 0, 0, 0, 0 });

    bool back = TopBar(ls.lesson->title);
    StepPips();
    LevelFxProgressLine(TOPBAR_H - 3, (float)RoadmapStepsDone(ls.lesson) / (float)ls.lesson->stepCount);

    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pad = 16, barY = TOPBAR_H + 12, barH = 42;
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Toolbar: run (or stop), reset, hint, and while reviewing a finished level, the next step
    // (or, on its last step, the level's review page).
    float x = pad;
    bool run = !busy && !ls.reviewOpen && (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_ENTER)));
    if (busy) {
        if (UiButtonIconEx((Rectangle){ x, barY, 130, barH }, ICON_CROSS, "Stop", 20, COL_BAD,
                           ColorBrightness(COL_BAD, 0.15f), COL_ON_ACCENT, true)) {
            CaseRunnerCancel(&ls.runner);
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
    // A pending move on to the next step (its pip shows the countdown): editing the code
    // (more work on this step) or running it again calls it off.
    if (ls.autoNextAt >= 0 && (ls.code.version != ls.autoNextVersion || busy || ls.reviewOpen)) ls.autoNextAt = -1;
    bool go = ls.autoNextAt >= 0 && GetTime() >= ls.autoNextAt;
    bool showNext = hasNext && Reviewing(), showReview = !hasNext && Reviewing() && !ls.reviewOpen;
    float appear = EaseToward(&ls.stepButtonAppear, showNext || showReview ? 1.0f : 0.0f, 12, GetFrameTime());
    Rectangle stepButton = { x - (1 - appear) * 24, barY, 150, barH };  // it slides in from the Hint button
    if (showNext) {  // looking back over a finished level: on to the next step when asked
        float pulse = (sinf((float)GetTime() * 5) + 1) / 2;
        DrawRectangleRoundedLinesEx((Rectangle){ stepButton.x - 3, stepButton.y - 3, stepButton.width + 6,
                                                 stepButton.height + 6 }, 0.3f, 8, 2,
                                    Fade(COL_ACCENT, (0.3f + 0.4f * pulse) * appear));
        if (UiButtonIconEx(stepButton, ICON_NEXT, "Next", 20, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, !busy)) go = true;
    } else if (showReview) {  // the last stage: back to the level's review page
        if (UiButtonIconEx(stepButton, ICON_TROPHY, "Review", 20, COL_ACCENT, COL_ACCENT_HI, COL_ON_ACCENT, !busy))
            OpenReview(false);
    }
    if (go && hasNext && !busy) {
        OpenStep(ls.step + 1, true);
        busy = true;  // the new step's first run has just started
    }
    float right = W - pad;
    DifficultyMeter(right, barY);
    const char *place = TextFormat("%s  %d/%d", TrackName(ls.lesson->track), ls.step + 1, ls.lesson->stepCount);
    float placeX = right - 46 - UiTextWidth(gFonts.mono, place, 19);
    UiText(gFonts.mono, place, placeX, barY + 10, 19, COL_DIM);
    float shelfX = x + (showNext || showReview ? 162 : 0);
    NoveltyShelf(shelfX, barY + 4, placeX - (MascotAvailable() ? 90 : 14), barH - 8);
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
    NoveltySpotlight(editor);
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
    NoveltyModelPopup();
    if (overExample && !ls.reviewOpen) UiClearClip();
    if (!ls.reviewOpen) NoveltyExample(barY + barH + 8);

    if (ls.reviewOpen) {
        UiClearClip();
        action = LevelReview();
        if (action.kind != LEVEL_STAY) {
            ls.reviewOpen = false;
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

void LevelScreenDemoNovelty(int piece)
{
    ls.noveltyDemoHover = piece;
}

void LevelScreenDemoExample(int piece, float seconds)
{
    if (!ls.open || piece < 0 || piece >= ls.novelty.count) return;
    ls.noveltyAt = GetTime() - 2;  // the shelf has popped in
    ls.examplePiece = piece;
    ls.exampleAt = GetTime() - seconds;
    ls.exampleAutoAt = -1;
    ls.exampleByItself = false;
}

void LevelScreenDemoCelebration(void)
{
    OpenReview(true);
    ls.reviewOpenedAt = GetTime() - 2;  // the stages have all ticked in
}

bool LevelScreenBusy(void)
{
    return ls.open && (CaseRunnerBusy(&ls.runner) || RunnerBusy(&ls.checker) || RunnerBusy(&ls.probe));
}
