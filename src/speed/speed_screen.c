// The speed round screen: intro, the timed run with its effects, and the pages after it (a
// failed lesson round, gold, a short challenge's results). See speed_screen.h; the questions
// and the clock are in speed_round.c, and what goes into a run in speed_plan.c.
#include "speed/speed_screen.h"
#include "ui/easing.h"
#include "learning/answers.h"
#include "app/skill_reward.h"
#include "speed/code_rain.h"
#include "ui/layout.h"
#include "ui/mascot.h"
#include "learning/progress.h"
#include "learning/review.h"
#include "speed/speed_plan.h"
#include "speed/speed_round.h"
#include "ui/ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLASH_SECONDS  0.28   // the green (or red) flash after an answer
#define PUNCH_SECONDS  0.18   // a new question's zoom-in "punch"
#define IMPACT_SECONDS 0.6    // the red flash and shake when the run ends badly

typedef enum { STAGE_INTRO, STAGE_RUN } Stage;

static struct {
    const Lesson *lesson;     // a lesson's round; NULL for a short challenge
    const LessonSet *set;     // a short challenge's lessons
    SpeedSkill skills[SPEED_SKILL_COUNT];
    int skillCount;
    Stage stage;
    SpeedRound round;
    SpeedPhase lastPhase;     // to notice the moment the run fails or ends
    double rightAt, wrongAt;  // when the last right / wrong answer was given (for the flashes)
    bool newGold;             // this run earned gold for the first time
    bool recording;           // answers go into the review history (not in --shot demos)
    float scroll, contentHeight;
    double frozenTime;        // --shot: the time to draw at; < 0 runs live
} speed = { .frozenTime = -1 };

static double Now(void) { return speed.frozenTime >= 0 ? speed.frozenTime : GetTime(); }
static bool IsChallenge(void) { return speed.lesson == NULL; }

static void Open(const Lesson *lesson, const LessonSet *set)
{
    speed.lesson = lesson;
    speed.set = set;
    speed.skillCount = lesson ? SpeedSkillsParse(lesson->speedSkills, speed.skills, SPEED_SKILL_COUNT) : 0;
    speed.stage = STAGE_INTRO;
    speed.scroll = 0;
    speed.recording = true;
    speed.frozenTime = -1;
    CodeRainClear();
}

void SpeedScreenOpen(const Lesson *lesson) { Open(lesson, NULL); }
void SpeedScreenOpenChallenge(const LessonSet *set) { Open(NULL, set); }

// The learner's history, in the terms speed_round.c times questions by.
static SpeedFamiliarity Familiarity(const char *reviewId)
{
    switch (ReviewFamiliarityOf(reviewId)) {
    case REVIEW_NEW:        return SPEED_NEW;
    case REVIEW_STRUGGLING: return SPEED_STRUGGLING;
    default:                return SPEED_KNOWN;
    }
}

static void StartRun(void)
{
    double now = Now();
    uint32_t seed = (uint32_t)(now * 1000003.0) ^ 0xa5a5a5a5u;  // a different run each time
    SpeedPlanItem plan[SPEED_QUESTIONS];
    int count = IsChallenge() ? SpeedPlanChallenge(speed.set, ReviewToday(), &seed, plan)
                              : SpeedPlanLesson(speed.lesson, &seed, plan);
    SpeedRoundStart(&speed.round, plan, count, !IsChallenge(), Familiarity, seed, now);
    speed.lastPhase = speed.round.phase;
    speed.rightAt = speed.wrongAt = -10;
    speed.newGold = false;
    speed.stage = STAGE_RUN;
    speed.scroll = 0;
    CodeRainClear();
}

// Puts an answer (or a timeout) into the learner's review history and skill XP. timeLeft is
// the share of the question's time that was left when it was answered.
static void Record(SpeedOutcome outcome, float timeLeft)
{
    if (outcome == SPEED_IGNORED || !speed.recording || speed.round.lastAnswered < 0) return;
    int index = speed.round.lastAnswered;
    const SpeedPlanItem *item = &speed.round.plan[index];
    bool right = outcome == SPEED_RIGHT;
    SkillWeight skills[MAX_QUESTION_SKILLS];
    int count = SpeedSkillWeights(item->skill, skills);
    SkillGains gains = AnswerRecordTimed(item->reviewId, skills, count,
                                         SpeedQuestionDifficulty(&speed.round.generated[index]), right, timeLeft);
    // The XP streams off to the skill badges (a timeout's from the middle of the screen).
    SkillRewardShow(SkillRewardOrigin(), &gains);
}

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

static float Bullet(const char *text, Color dot, float x, float y, float w, float scale)
{
    float size = 22 * scale;
    DrawCircleV((Vector2){ x + 9, y + size * 0.62f }, 3.5f * scale, dot);
    return UiWrappedText(gFonts.text, text, (Vector2){ x + 28, y }, w - 28, size, COL_TEXT, true) + 10;
}

// ---------------------------------------------------------------------------
// Intro

static SpeedScreenAction IntroPage(void)
{
    if (TopBar(IsChallenge() ? "Short challenge" : "Speed round")) return SPEED_SCREEN_BACK;
    Rectangle view = ScrollView();
    UpdateScroll(&speed.scroll, speed.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 40 - speed.scroll, y = top;

    IconDraw(ICON_BOLT, (Vector2){ x + 36, y + 36 }, 64, COL_ACCENT);
    UiText(gFonts.bold, IsChallenge() ? "SHORT CHALLENGE" : "SPEED ROUND", x + 90, y + 2, 40, COL_ACCENT);
    const char *subtitle = IsChallenge() ? "Your most necessary questions, against the clock" : speed.lesson->title;
    y += UiWrappedText(gFonts.text, subtitle, (Vector2){ x + 90, y + 50 }, w - 90, 24 * scale, COL_TEXT, true) + 70;

    if (IsChallenge()) {
        int due = ReviewDueCount(ReviewToday());
        const char *summary = ReviewCount() == 0
            ? "You haven't answered any questions yet, so this starts with the first lessons' questions."
            : TextFormat("%d question%s due for review today, out of %d you've answered.", due, due == 1 ? " is" : "s are",
                         ReviewCount());
        y += UiWrappedText(gFonts.text, summary, (Vector2){ x, y }, w, 20 * scale, COL_DIM, true) + 24;
        y += Bullet("Up to 10 questions, picked like flashcards: the ones due for review, the ones you keep missing, "
                    "then new ones from lessons you've started.", COL_ACCENT, x, y, w, scale);
        y += Bullet("Each gets at least 10 seconds; new questions and ones you've struggled with get longer.",
                    COL_ACCENT, x, y, w, scale);
        y += Bullet("Answer with the number keys, or click. A miss doesn't end the run.", COL_ACCENT, x, y, w, scale);
        y += Bullet("Every answer updates when each question comes back for review.", COL_GOLD, x, y, w, scale);
    } else {
        char skills[160] = "";
        for (int i = 0; i < speed.skillCount; i++) {
            strncat(skills, i ? ", " : "Tests: ", sizeof skills - strlen(skills) - 1);
            strncat(skills, SpeedSkillTitle(speed.skills[i]), sizeof skills - strlen(skills) - 1);
        }
        y += UiWrappedText(gFonts.text, skills, (Vector2){ x, y }, w, 20 * scale, COL_DIM, true) + 24;
        y += Bullet("Work out the value of a variable after a few lines of code.", COL_ACCENT, x, y, w, scale);
        y += Bullet("10 questions, straight after one another: no breaks.", COL_ACCENT, x, y, w, scale);
        y += Bullet("At least 10 seconds each, more for longer, new or tricky questions. Answer with the number keys, "
                    "or click.", COL_ACCENT, x, y, w, scale);
        y += Bullet("One wrong answer or a timeout ends the run.", COL_ACCENT, x, y, w, scale);
        y += Bullet("Answer all 10 for gold mastery of this level.", COL_GOLD, x, y, w, scale);
    }
    y += 20;

    if (!IsChallenge() && ProgressGold(speed.lesson->id)) {
        IconDraw(ICON_STAR, (Vector2){ x + 16, y + 16 }, 30, COL_GOLD);
        y += UiWrappedText(gFonts.bold, "You have gold mastery of this level. Play again any time.",
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
// arrives, and shake in the last stretch (or as the run fails).
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

static int PressedChoice(int choiceCount)
{
    for (int i = 0; i < choiceCount && i < 9; i++) {
        if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i)) return i;
    }
    return -1;
}

// One pip per question: right answers in gold, misses in red (challenges carry on past
// them), the current one in the accent colour.
static void Pips(float cx, float y)
{
    const SpeedRound *r = &speed.round;
    float gap = 22;
    float x0 = cx - gap * (r->count - 1) / 2;
    for (int i = 0; i < r->count; i++) {
        Vector2 at = { x0 + i * gap, y };
        if (i < r->number) DrawCircleV(at, 6, r->picked[i] == SpeedRoundCorrect(r, i) ? COL_GOLD : COL_BAD);
        else if (i == r->number) DrawCircleV(at, 6, COL_ACCENT);
        else DrawRing(at, 4.5f, 6, 0, 360, 24, COL_PANEL_HI);
    }
}

// The header, time bar and countdown shared by both kinds of question. Returns the y below.
static float QuestionHeader(const char *kind, float tension, double now, float x, float w)
{
    float W = (float)GetScreenWidth(), y = 30;
    Color hot = Mix(COL_ACCENT, COL_BAD, tension * 1.2f);
    UiText(gFonts.bold, TextFormat("%s  /  %s", IsChallenge() ? "SHORT CHALLENGE" : "SPEED ROUND", kind), x, y, 18,
           COL_DIM);
    const char *counter = TextFormat("QUESTION %d / %d", speed.round.number + 1, speed.round.count);
    UiText(gFonts.bold, counter, x + w - UiTextWidth(gFonts.bold, counter, 18), y, 18, COL_TEXT);
    Pips(W / 2, y + 10);
    y += 40;

    double left = SpeedRoundTimeLeft(&speed.round, now), limit = speed.round.seconds[speed.round.number];
    ProgressBar((Rectangle){ x, y, w, 12 }, (float)(left / limit), COL_PANEL_HI, hot);
    y += 26;

    // The countdown: each new second lands with a thump, bigger and redder near the end.
    int seconds = (int)ceil(left);
    float intoSecond = (float)(ceil(left) - left);
    float beat = 1 + (0.18f + 0.3f * tension) * powf(1 - Clamp01(intoSecond / 0.35f), 3);
    float size = 76 * beat;
    UiTextCentered(gFonts.bold, TextFormat("%d", seconds), W / 2, y + 40 - size / 2, size, seconds <= 2 ? COL_BAD : hot);
    return y + 96;
}

// A generated "what is x?" question (inside the camera). Returns the choice clicked, or -1.
// mouse is in the camera's coordinates.
static int GeneratedQuestionView(const SpeedQuestion *q, float tension, double now, Vector2 mouse)
{
    float W = (float)GetScreenWidth();
    float w = W - 120 < 900 ? W - 120 : 900, x = (W - w) / 2;
    Color hot = Mix(COL_ACCENT, COL_BAD, tension * 1.2f);
    float y = QuestionHeader(SpeedSkillTitle(q->skill), tension, now, x, w);

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
    UiTextCentered(gFonts.text, "Answer with the number keys", W / 2, H / 2 + 120, 22, COL_DIM);
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
    int clicked = -1, number = speed.round.number;
    if (speed.round.phase == SPEED_COUNTING_IN) CountIn(now);
    else clicked = GeneratedQuestionView(&speed.round.generated[number], tension, now, mouse);
    EndMode2D();

    // Answers flash: green as the next question lands, red for a miss in a challenge.
    float rightFlash = 1 - Clamp01((float)((now - speed.rightAt) / FLASH_SECONDS));
    float wrongFlash = 1 - Clamp01((float)((now - speed.wrongAt) / FLASH_SECONDS));
    if (rightFlash > 0) DrawRectangle(0, 0, (int)W, (int)H, Fade(COL_GOOD, 0.22f * rightFlash));
    if (wrongFlash > 0) DrawRectangle(0, 0, (int)W, (int)H, Fade(COL_BAD, 0.25f * wrongFlash));
    UiText(gFonts.text, "Esc: give up", 16, H - 30, 16, Fade(COL_DIM, 0.8f));

    if (IsKeyPressed(KEY_ESCAPE)) {
        speed.stage = STAGE_INTRO;
        return;
    }
    int choice = clicked >= 0 ? clicked : PressedChoice(SpeedRoundChoiceCount(&speed.round, number));
    if (choice < 0) return;
    float timeLeft = (float)(SpeedRoundTimeLeft(&speed.round, now) / speed.round.seconds[number]);
    SpeedOutcome outcome = SpeedRoundAnswer(&speed.round, choice, now);
    Record(outcome, timeLeft);
    if (outcome == SPEED_RIGHT) speed.rightAt = now;
    if (outcome == SPEED_WRONG) speed.wrongAt = now;
}

// ---------------------------------------------------------------------------
// After the run

// A generated question, explained: the code with the variables' values after each line.
// The notes sit in a column after the longest line of code; when that leaves too little
// room, each note goes on its own row under its line instead.
static float ExplainedCode(const SpeedQuestion *q, float x, float y, float w, float scale, bool draw)
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
    if (!draw) return height;
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

// Question index of the run, explained: what was asked, the right answer, the learner's
// answer (or that time ran out), and why. draw = false measures the same layout.
static float ExplainedQuestion(int index, float x, float y, float w, float scale, bool draw)
{
    const SpeedRound *r = &speed.round;
    int picked = r->picked[index];
    const SpeedQuestion *q = &r->generated[index];
    float top = y;
    bool right = picked == q->correct;
    const char *status = picked < 0 ? "ran out of time" : right ? "right" : TextFormat("you picked %d", q->choices[picked]);
    const char *line = TextFormat("Question %d (%s): %s is %d, %s", index + 1, SpeedSkillTitle(q->skill), q->asks,
                                  q->choices[q->correct], status);
    if (draw) UiIconText(right ? ICON_CHECK : ICON_CROSS, gFonts.bold, line, x, y, 21 * scale, right ? COL_GOOD : COL_BAD);
    y += 36 * scale;
    y += ExplainedCode(q, x, y, w, scale, draw);
    return y - top;
}

static SpeedScreenAction FailedPage(double now)
{
    if (TopBar(TextFormat("Speed round: %s", speed.lesson->title))) return SPEED_SCREEN_BACK;
    Rectangle view = ScrollView();
    UpdateScroll(&speed.scroll, speed.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 32 - speed.scroll, y = top;

    bool timedOut = speed.round.picked[speed.round.number] < 0;
    UiIconText(timedOut ? ICON_RETRY : ICON_CROSS, gFonts.bold, timedOut ? "TIME'S UP!" : "WRONG!", x, y, 44, COL_BAD);
    y += 64;
    y += UiWrappedText(gFonts.text, TextFormat("You reached question %d of %d. Here's what it was asking:",
                                               speed.round.number + 1, speed.round.count),
                       (Vector2){ x, y }, w, 22 * scale, COL_TEXT, true) + 16;
    y += ExplainedQuestion(speed.round.number, x, y, w, scale, true) + 28;

    bool retry = UiButtonIcon((Rectangle){ x, y, 240, 54 }, ICON_RETRY, "Try again", true) || PressedStart();
    bool back = UiButtonIconEx((Rectangle){ x + 256, y, 240, 54 }, ICON_BACK, "Back", 22, COL_PANEL,
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
    UiTextCentered(gFonts.text, speed.newGold ? "10 out of 10 against the clock. This level is now gold."
                                              : "10 out of 10 against the clock, again!", cx, y, 20, COL_DIM);
    y += 60;
    bool back = UiButtonIcon((Rectangle){ cx - 250, y, 240, 54 }, ICON_BACK, "Back", true)
             || PressedStart();
    bool again = UiButtonIconEx((Rectangle){ cx + 10, y, 240, 54 }, ICON_RETRY, "Play again", 22, COL_PANEL,
                                COL_SECONDARY_HOVER, COL_TEXT, true);
    if (again) StartRun();
    return back ? SPEED_SCREEN_BACK : SPEED_SCREEN_STAY;
}

// A short challenge's results: the score, then every question with its answer explained.
static SpeedScreenAction ResultsPage(void)
{
    if (TopBar("Short challenge")) return SPEED_SCREEN_BACK;
    Rectangle view = ScrollView();
    UpdateScroll(&speed.scroll, speed.contentHeight, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 32 - speed.scroll, y = top;

    int right = SpeedRoundRight(&speed.round), count = speed.round.count;
    bool perfect = right == count;
    UiIconText(perfect ? ICON_STAR : ICON_BOLT, gFonts.bold, "CHALLENGE COMPLETE", x, y, 40, perfect ? COL_GOLD : COL_ACCENT);
    y += 60;
    y += UiWrappedText(gFonts.text, TextFormat("%d of %d right. %s", right, count,
                                               perfect ? "Every one! Those questions won't be back for a while."
                                                       : "The ones you missed will come back soon, until they stick."),
                       (Vector2){ x, y }, w, 22 * scale, COL_TEXT, true) + 20;
    bool again = UiButtonIcon((Rectangle){ x, y, 260, 54 }, ICON_BOLT, "New challenge", true) || PressedStart();
    bool back = UiButtonIconEx((Rectangle){ x + 276, y, 200, 54 }, ICON_BACK, "Back", 22, COL_PANEL,
                               COL_SECONDARY_HOVER, COL_TEXT, true);
    y += 54 + 32;

    for (int i = 0; i < count; i++) {
        float h = ExplainedQuestion(i, 0, 0, w - 40, scale, false) + 40;
        UiRoundRect((Rectangle){ x, y, w, h }, 12, COL_PANEL);
        ExplainedQuestion(i, x + 20, y + 20, w - 40, scale, true);
        y += h + 16;
    }
    speed.contentHeight = y - top + 40;
    EndView();
    PageScrollBar(view, &speed.scroll, speed.contentHeight);
    if (again) StartRun();
    return back ? SPEED_SCREEN_BACK : SPEED_SCREEN_STAY;
}

SpeedScreenAction SpeedScreen(void)
{
    if (speed.stage == STAGE_INTRO) return IntroPage();
    double now = Now();
    Record(SpeedRoundUpdate(&speed.round, now), 0);  // a timeout: no time left
    if (speed.round.phase != speed.lastPhase) {  // the moment the run ends
        if (speed.round.phase == SPEED_WON && !IsChallenge() && speed.recording) {
            speed.newGold = !ProgressGold(speed.lesson->id);
            ProgressRecordGold(speed.lesson->id);
        }
        speed.scroll = 0;
        speed.lastPhase = speed.round.phase;
    }
    switch (speed.round.phase) {
    case SPEED_FAILED: return FailedPage(now);
    case SPEED_WON:    return IsChallenge() ? ResultsPage() : GoldPage(now);
    default:
        RunPage(now);
        return SPEED_SCREEN_STAY;
    }
}

void SpeedScreenDemo(int stage, float elapsedMs)
{
    speed.recording = false;  // a demo never touches the learner's history or gold
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
    // Answer six questions to reach the seventh (right ones, or alternately for stage 5),
    // then wind the clock on.
    double now = speed.frozenTime + SPEED_COUNT_IN;
    SpeedRoundUpdate(round, now);
    for (int i = 0; i < 6 && round->phase == SPEED_PLAYING; i++) SpeedRoundAnswer(round, SpeedRoundCorrect(round, i), now);
    float elapsed = elapsedMs / 1000.0f;
    if (stage == 2) {
        // Let the rain build up as it would have over the question so far.
        float tension = Clamp01(elapsed / (float)round->seconds[round->number]);
        Rectangle screen = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        for (int frame = 0; frame < 150; frame++) CodeRainUpdate(screen, 1 / 60.0f, tension);
        speed.frozenTime = now + elapsed;
        return;
    }
    if (stage == 3) {
        int n = round->number;
        SpeedRoundAnswer(round, (SpeedRoundCorrect(round, n) + 1) % SpeedRoundChoiceCount(round, n), now + 2);
        speed.lastPhase = round->phase;
        speed.frozenTime = now + 2 + IMPACT_SECONDS;  // after the impact flash
        return;
    }
    for (int i = round->number; round->phase == SPEED_PLAYING; i++) {
        bool miss = stage == 5 && i % 3 == 1;  // results: a few misses to explain
        int answer = SpeedRoundCorrect(round, i);
        if (miss) answer = (answer + 1) % SpeedRoundChoiceCount(round, i);
        SpeedRoundAnswer(round, answer, now);
    }
    speed.lastPhase = round->phase;
    speed.newGold = true;
    speed.frozenTime = now + 1.5;
}
