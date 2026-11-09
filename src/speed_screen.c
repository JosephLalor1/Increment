// The speed round screen: intro, the timed run with its effects, and the failed and gold
// pages. See speed_screen.h; the questions and the clock are in speed_round.c.
#include "speed_screen.h"
#include "code_rain.h"
#include "layout.h"
#include "mascot.h"
#include "progress.h"
#include "speed_round.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLASH_SECONDS  0.28   // the green flash after a right answer
#define PUNCH_SECONDS  0.18   // a new question's zoom-in "punch"
#define IMPACT_SECONDS 0.6    // the red flash and shake when the run ends badly

typedef enum { STAGE_INTRO, STAGE_RUN } Stage;

static struct {
    const Lesson *lesson;
    SpeedSkill skills[SPEED_SKILL_COUNT];
    int skillCount;
    Stage stage;
    SpeedRound round;
    SpeedPhase lastPhase;     // to notice the moment the run fails or is won
    double rightAt;           // when the last right answer was given (for the flash)
    bool newGold;             // this run earned gold for the first time
    float scroll, contentHeight;
    double frozenTime;        // --shot: the time to draw at; < 0 runs live
} speed = { .frozenTime = -1 };

static double Now(void) { return speed.frozenTime >= 0 ? speed.frozenTime : GetTime(); }

void SpeedScreenOpen(const Lesson *lesson)
{
    speed.lesson = lesson;
    speed.skillCount = SpeedSkillsParse(lesson->speedSkills, speed.skills, SPEED_SKILL_COUNT);
    speed.stage = STAGE_INTRO;
    speed.scroll = 0;
    CodeRainClear();
}

static void StartRun(void)
{
    double now = Now();
    uint32_t seed = (uint32_t)(now * 1000003.0) ^ 0xa5a5a5a5u;  // a different run each time
    SpeedRoundStart(&speed.round, speed.skills, speed.skillCount, seed, now);
    speed.lastPhase = speed.round.phase;
    speed.rightAt = -10;
    speed.newGold = false;
    speed.stage = STAGE_RUN;
    speed.scroll = 0;
    CodeRainClear();
}

static float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

static Color Mix(Color a, Color b, float t)
{
    t = Clamp01(t);
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}

static bool PressedStart(void)
{
    return (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && !UiAnyFocus();
}

// ---------------------------------------------------------------------------
// Intro

static SpeedScreenAction IntroPage(void)
{
    if (TopBar("Speed round")) return SPEED_SCREEN_BACK;
    Rectangle view = ScrollView();
    UpdateScroll(&speed.scroll, speed.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 40 - speed.scroll, y = top;

    IconDraw(ICON_BOLT, (Vector2){ x + 36, y + 36 }, 64, COL_ACCENT);
    UiText(gFonts.bold, "SPEED ROUND", x + 90, y + 2, 40, COL_ACCENT);
    y += UiWrappedText(gFonts.text, speed.lesson->title, (Vector2){ x + 90, y + 50 }, w - 90, 24 * scale, COL_TEXT, true) + 70;

    char skills[160] = "";
    for (int i = 0; i < speed.skillCount; i++) {
        strncat(skills, i ? ", " : "Tests: ", sizeof skills - strlen(skills) - 1);
        strncat(skills, SpeedSkillTitle(speed.skills[i]), sizeof skills - strlen(skills) - 1);
    }
    y += UiWrappedText(gFonts.text, skills, (Vector2){ x, y }, w, 20 * scale, COL_DIM, true) + 24;

    static const char *rules[] = {
        "Work out the value of a variable after a few lines of code.",
        "10 questions, straight after one another: no breaks.",
        "10 seconds each. Answer with the keys 1 to 4, or click.",
        "One wrong answer or a timeout ends the run.",
        "Answer all 10 for gold mastery of this lesson.",
    };
    for (int i = 0; i < 5; i++) {
        float size = 22 * scale;
        DrawCircleV((Vector2){ x + 9, y + size * 0.62f }, 3.5f * scale, i == 4 ? COL_GOLD : COL_ACCENT);
        y += UiWrappedText(gFonts.text, rules[i], (Vector2){ x + 28, y }, w - 28, size, COL_TEXT, true) + 10;
    }
    y += 20;

    if (ProgressGold(speed.lesson->id)) {
        IconDraw(ICON_STAR, (Vector2){ x + 16, y + 16 }, 30, COL_GOLD);
        y += UiWrappedText(gFonts.bold, "You have gold mastery of this lesson. Play again any time.",
                           (Vector2){ x + 44, y + 2 }, w - 44, 21 * scale, COL_GOLD, true) + 28;
    }
    bool start = UiButtonIcon((Rectangle){ x, y, 260, 56 }, ICON_BOLT, "Start", true) || PressedStart();
    UiText(gFonts.text, "or press Enter", x + 280, y + 18, 18, COL_DIM);
    y += 56;

    speed.contentHeight = y - top + 40;
    EndView();
    PageScrollBar(view, &speed.scroll, speed.contentHeight);
    if (start) StartRun();
    return SPEED_SCREEN_STAY;
}

// ---------------------------------------------------------------------------
// The run

// The camera for this frame: a slow zoom as time runs out, a punch-in as each question
// arrives, and shake in the last couple of seconds (or as the run fails).
static Camera2D TensionCamera(float tension, double now)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float sinceQuestion = (float)(now - speed.round.phaseStart);
    float punch = speed.round.phase == SPEED_PLAYING ? 1 - Clamp01(sinceQuestion / (float)PUNCH_SECONDS) : 0;
    float zoom = 1 + 0.075f * tension * tension + 0.035f * punch * punch;

    float shake = tension > 0.5f ? 10 * powf((tension - 0.5f) / 0.5f, 2) : 0;
    if (speed.round.phase == SPEED_FAILED) {  // one hard jolt as it ends
        float sinceEnd = (float)(now - speed.round.endedAt);
        shake = 16 * (1 - Clamp01(sinceEnd / (float)IMPACT_SECONDS));
    }
    float t = (float)now;
    Vector2 jitter = { shake * (sinf(t * 71.0f) * 0.6f + sinf(t * 113.0f + 1.3f) * 0.4f),
                       shake * (sinf(t * 89.0f + 0.7f) * 0.6f + sinf(t * 131.0f) * 0.4f) };
    Camera2D camera = { 0 };
    camera.target = (Vector2){ W / 2, H / 2 };
    camera.offset = (Vector2){ W / 2 + jitter.x, H / 2 + jitter.y };
    camera.zoom = zoom;
    return camera;
}

// A red glow creeping in from the screen's edges as time runs out.
static void Vignette(float tension)
{
    float alpha = 0.5f * tension * tension * tension;
    if (alpha < 0.01f) return;
    int W = GetScreenWidth(), H = GetScreenHeight(), band = H / 4;
    Color edge = Fade(COL_BAD, alpha), clear = Fade(COL_BAD, 0);
    DrawRectangleGradientV(0, 0, W, band, edge, clear);
    DrawRectangleGradientV(0, H - band, W, band, clear, edge);
    DrawRectangleGradientH(0, 0, band, H, edge, clear);
    DrawRectangleGradientH(W - band, 0, band, H, clear, edge);
}

static int PressedChoice(void)
{
    for (int i = 0; i < SPEED_CHOICES; i++) {
        if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i)) return i;
    }
    return -1;
}

// The ten questions as pips: done in gold, the current one in the accent colour.
static void Pips(float cx, float y, int done)
{
    float gap = 22;
    float x0 = cx - gap * (SPEED_QUESTIONS - 1) / 2;
    for (int i = 0; i < SPEED_QUESTIONS; i++) {
        Vector2 at = { x0 + i * gap, y };
        if (i < done) DrawCircleV(at, 6, COL_GOLD);
        else if (i == done) DrawCircleV(at, 6, COL_ACCENT);
        else DrawRing(at, 4.5f, 6, 0, 360, 24, COL_PANEL_HI);
    }
}

// Draws the question (inside the camera). Returns the choice clicked, or -1. mouse is in
// the camera's coordinates.
static int QuestionView(float tension, double now, Vector2 mouse)
{
    const SpeedQuestion *q = &speed.round.question;
    float W = (float)GetScreenWidth();
    float w = W - 120 < 900 ? W - 120 : 900, x = (W - w) / 2, y = 30;
    Color hot = Mix(COL_ACCENT, COL_BAD, tension * 1.2f);

    UiText(gFonts.bold, TextFormat("SPEED ROUND  /  %s", SpeedSkillTitle(q->skill)), x, y, 18, COL_DIM);
    const char *counter = TextFormat("QUESTION %d / %d", speed.round.number + 1, SPEED_QUESTIONS);
    UiText(gFonts.bold, counter, x + w - UiTextWidth(gFonts.bold, counter, 18), y, 18, COL_TEXT);
    Pips(W / 2, y + 10, speed.round.number);
    y += 40;

    double left = SpeedRoundTimeLeft(&speed.round, now);
    ProgressBar((Rectangle){ x, y, w, 12 }, (float)(left / SPEED_SECONDS), COL_PANEL_HI, hot);
    y += 26;

    // The countdown: each new second lands with a thump, bigger and redder near the end.
    int seconds = (int)ceil(left);
    float intoSecond = (float)(ceil(left) - left);
    float beat = 1 + (0.18f + 0.3f * tension) * powf(1 - Clamp01(intoSecond / 0.35f), 3);
    float size = 76 * beat;
    UiTextCentered(gFonts.bold, TextFormat("%d", seconds), W / 2, y + 40 - size / 2, size, seconds <= 2 ? COL_BAD : hot);
    y += 96;

    UiTextCentered(gFonts.bold, TextFormat("What is %s?", q->asks), W / 2, y, 34, COL_TEXT);
    y += 58;

    // The code shrinks on a short window so the answers always fit below it.
    int clicked = -1;
    float gap = 16, boxW = (w - gap * (SPEED_CHOICES - 1)) / SPEED_CHOICES, boxH = 86;
    float room = (float)GetScreenHeight() - y - 26 - boxH - 40;
    float codeSize = room / (q->lineCount * 1.5f + 1.5f);
    codeSize = codeSize > 22 ? 22 : codeSize < 13 ? 13 : codeSize;
    char code[SPEED_MAX_LINES * SPEED_LINE_SIZE];
    float codeW = w < 760 ? w : 760;
    y += UiCodeBlock(SpeedQuestionCode(q, 0, code, sizeof code), (Vector2){ (W - codeW) / 2, y }, codeW, codeSize, true) + 26;

    for (int i = 0; i < SPEED_CHOICES; i++) {
        Rectangle box = { x + i * (boxW + gap), y, boxW, boxH };
        bool over = CheckCollisionPointRec(mouse, box);
        if (over) UiWantHand();
        UiRoundRect(box, 12, over ? COL_PANEL_HI : COL_PANEL);
        DrawRectangleRoundedLinesEx(box, 0.2f, 8, 2, Fade(hot, 0.35f + 0.5f * tension));
        UiRoundRect((Rectangle){ box.x + 10, box.y + 10, 28, 28 }, 6, COL_BG);
        UiTextCentered(gFonts.bold, TextFormat("%d", i + 1), box.x + 24, box.y + 13, 18, COL_DIM);
        UiTextCentered(gFonts.mono, TextFormat("%d", q->choices[i]), box.x + boxW / 2, box.y + 22, 40, COL_TEXT);
        if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
    }
    return clicked;
}

// "3, 2, 1" before the first question, under calm rain.
static void CountIn(double now)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    double left = SPEED_COUNT_IN - (now - speed.round.phaseStart);
    int number = (int)ceil(left);
    float intoSecond = (float)(ceil(left) - left);
    float size = 150 * (1 + 0.4f * powf(1 - Clamp01(intoSecond / 0.3f), 3));
    UiTextCentered(gFonts.bold, "GET READY", W / 2, H / 2 - 170, 30, COL_DIM);
    UiTextCentered(gFonts.bold, TextFormat("%d", number < 1 ? 1 : number), W / 2, H / 2 - size / 2, size, COL_ACCENT);
    UiTextCentered(gFonts.text, "Answer with the keys 1 to 4", W / 2, H / 2 + 120, 22, COL_DIM);
}

// The run in progress: count-in or a question, under the effects. Esc gives up.
static void RunPage(double now)
{
    float tension = SpeedRoundTension(&speed.round, now);
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    CodeRainUpdate((Rectangle){ 0, 0, W, H }, GetFrameTime(), speed.round.phase == SPEED_COUNTING_IN ? 0.05f : tension);
    CodeRainDraw(tension);
    Vignette(tension);

    Camera2D camera = TensionCamera(tension, now);
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    BeginMode2D(camera);
    int clicked = -1;
    if (speed.round.phase == SPEED_COUNTING_IN) CountIn(now);
    else clicked = QuestionView(tension, now, mouse);
    EndMode2D();

    // A right answer flashes green as the next question lands.
    float flash = 1 - Clamp01((float)((now - speed.rightAt) / FLASH_SECONDS));
    if (flash > 0) DrawRectangle(0, 0, (int)W, (int)H, Fade(COL_GOOD, 0.22f * flash));
    UiText(gFonts.text, "Esc: give up", 16, H - 30, 16, Fade(COL_DIM, 0.8f));

    if (IsKeyPressed(KEY_ESCAPE)) {
        speed.stage = STAGE_INTRO;
        return;
    }
    int choice = clicked >= 0 ? clicked : PressedChoice();
    if (choice >= 0 && SpeedRoundAnswer(&speed.round, choice, now) == SPEED_RIGHT) speed.rightAt = now;
}

// One line of the explained snippet: the code, and the variables' values after it.
// The notes sit in a column after the longest line of code; when that leaves too little
// room, each note goes on its own row under its line instead.
static float ExplainedCode(const SpeedQuestion *q, float x, float y, float w, float scale)
{
    float size = 20 * scale, noteSize = size * 0.85f, lineH = size * 1.45f, pad = 16;
    float codeW = 0, noteW = 0;
    for (int i = 0; i < q->lineCount; i++) {
        codeW = fmaxf(codeW, UiTextWidth(gFonts.mono, q->lines[i], size));
        if (q->notes[i][0]) noteW = fmaxf(noteW, UiTextWidth(gFonts.text, TextFormat("// %s", q->notes[i]), noteSize));
    }
    float noteX = fmaxf(x + w * 0.52f, x + pad + codeW + 24);
    bool beside = noteX + noteW <= x + w - pad;
    if (!beside) noteX = x + pad + 24;

    int rows = q->lineCount;
    for (int i = 0; i < q->lineCount && !beside; i++) rows += q->notes[i][0] != 0;
    float height = pad * 2 + rows * lineH;
    UiRoundRect((Rectangle){ x, y, w, height }, 10, COL_CODE_BG);
    float rowY = y + pad;
    for (int i = 0; i < q->lineCount; i++) {
        UiCodeLine(q->lines[i], x + pad, rowY, size);
        if (q->notes[i][0]) {
            if (!beside) rowY += lineH;
            UiText(gFonts.text, TextFormat("// %s", q->notes[i]), noteX, rowY + size * 0.08f, noteSize, COL_GOOD);
        }
        rowY += lineH;
    }
    return height;
}

static SpeedScreenAction FailedPage(double now)
{
    if (TopBar(TextFormat("Speed round: %s", speed.lesson->title))) return SPEED_SCREEN_BACK;
    const SpeedQuestion *q = &speed.round.question;
    Rectangle view = ScrollView();
    UpdateScroll(&speed.scroll, speed.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 32 - speed.scroll, y = top;

    bool timedOut = speed.round.picked < 0;
    UiIconText(timedOut ? ICON_RETRY : ICON_CROSS, gFonts.bold, timedOut ? "TIME'S UP!" : "WRONG!", x, y, 44, COL_BAD);
    y += 64;
    y += UiWrappedText(gFonts.text, TextFormat("You reached question %d of %d. %s is %d:", speed.round.number + 1,
                       SPEED_QUESTIONS, q->asks, q->choices[q->correct]), (Vector2){ x, y }, w, 24 * scale, COL_TEXT, true) + 6;
    const char *picked = timedOut ? "You ran out of time." : TextFormat("You picked %d.", q->choices[speed.round.picked]);
    y += UiWrappedText(gFonts.text, TextFormat("%s Here is what each line does:", picked), (Vector2){ x, y }, w,
                       20 * scale, COL_DIM, true) + 16;
    y += ExplainedCode(q, x, y, w, scale) + 28;

    bool retry = UiButtonIcon((Rectangle){ x, y, 240, 54 }, ICON_RETRY, "Try again", true) || PressedStart();
    bool back = UiButtonIconEx((Rectangle){ x + 256, y, 240, 54 }, ICON_BACK, "Back to lesson", 22, COL_PANEL,
                               COL_SECONDARY_HOVER, COL_TEXT, true);
    UiText(gFonts.text, "Enter: try again", x + 516, y + 18, 18, COL_DIM);
    y += 54;
    speed.contentHeight = y - top + 40;
    EndView();
    PageScrollBar(view, &speed.scroll, speed.contentHeight);

    float impact = 1 - Clamp01((float)((now - speed.round.endedAt) / IMPACT_SECONDS));
    if (impact > 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(COL_BAD, 0.35f * impact));
    if (retry) StartRun();
    return back ? SPEED_SCREEN_BACK : SPEED_SCREEN_STAY;
}

static SpeedScreenAction GoldPage(double now)
{
    if (TopBar(TextFormat("Speed round: %s", speed.lesson->title))) return SPEED_SCREEN_BACK;
    Rectangle view = ScrollView();
    float age = (float)(now - speed.round.endedAt);
    MascotConfetti(view, age, true);
    float cx = GetScreenWidth() / 2.0f, y = view.y + 50;

    // A gold star with rings pulsing out of it.
    Vector2 star = { cx, y + 90 };
    for (int i = 0; i < 3; i++) {
        float phase = fmodf(age * 0.8f + i / 3.0f, 1.0f);
        DrawRing(star, 70 + phase * 90, 74 + phase * 90, 0, 360, 64, Fade(COL_GOLD, 0.45f * (1 - phase)));
    }
    float pop = 1 + 0.5f * powf(1 - Clamp01(age / 0.5f), 3);
    IconDraw(ICON_STAR, star, 150 * pop, COL_GOLD);
    y += 210;
    UiTextCentered(gFonts.bold, "GOLD MASTERY", cx, y, 56, COL_GOLD);
    y += 76;
    UiTextCentered(gFonts.text, speed.lesson->title, cx, y, 26, COL_TEXT);
    y += 44;
    UiTextCentered(gFonts.text, speed.newGold ? "10 out of 10 against the clock. This lesson is now gold."
                                              : "10 out of 10 against the clock, again!", cx, y, 20, COL_DIM);
    y += 60;
    bool back = UiButtonIcon((Rectangle){ cx - 250, y, 240, 54 }, ICON_BACK, "Back to lesson", true)
             || PressedStart();
    bool again = UiButtonIconEx((Rectangle){ cx + 10, y, 240, 54 }, ICON_RETRY, "Play again", 22, COL_PANEL,
                                COL_SECONDARY_HOVER, COL_TEXT, true);
    if (again) StartRun();
    return back ? SPEED_SCREEN_BACK : SPEED_SCREEN_STAY;
}

SpeedScreenAction SpeedScreen(void)
{
    if (speed.stage == STAGE_INTRO) return IntroPage();
    double now = Now();
    SpeedRoundUpdate(&speed.round, now);
    if (speed.round.phase != speed.lastPhase) {  // the moment the run ends
        if (speed.round.phase == SPEED_WON) {
            speed.newGold = !ProgressGold(speed.lesson->id);
            ProgressRecordGold(speed.lesson->id);
        }
        speed.scroll = 0;
        speed.lastPhase = speed.round.phase;
    }
    switch (speed.round.phase) {
    case SPEED_FAILED: return FailedPage(now);
    case SPEED_WON:    return GoldPage(now);
    default:
        RunPage(now);
        return SPEED_SCREEN_STAY;
    }
}

void SpeedScreenDemo(int stage, float elapsedMs)
{
    speed.frozenTime = 1000.0;
    if (stage <= 0) {
        speed.stage = STAGE_INTRO;
        return;
    }
    StartRun();  // uses the frozen time, so the questions are the same every shot
    SpeedRound *round = &speed.round;
    if (stage == 1) {  // part way through the count-in
        speed.frozenTime += 1.4;
        return;
    }
    // Answer six questions right to reach the seventh, then wind the clock on.
    double now = speed.frozenTime + SPEED_COUNT_IN;
    SpeedRoundUpdate(round, now);
    for (int i = 0; i < 6; i++) SpeedRoundAnswer(round, round->question.correct, now);
    float elapsed = elapsedMs / 1000.0f;
    if (stage == 2) {
        // Let the rain build up as it would have over the question so far.
        float tension = Clamp01(elapsed / (float)SPEED_SECONDS);
        Rectangle screen = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        for (int frame = 0; frame < 150; frame++) CodeRainUpdate(screen, 1 / 60.0f, tension);
        speed.frozenTime = now + elapsed;
        return;
    }
    if (stage == 3) {
        SpeedRoundAnswer(round, (round->question.correct + 1) % SPEED_CHOICES, now + 2);
        speed.lastPhase = round->phase;
        speed.frozenTime = now + 2 + IMPACT_SECONDS;  // after the impact flash
        return;
    }
    while (round->phase == SPEED_PLAYING) SpeedRoundAnswer(round, round->question.correct, now);
    speed.lastPhase = round->phase;  // a demo never records gold
    speed.newGold = true;
    speed.frozenTime = now + 1.5;
}
