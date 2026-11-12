// The statistics page's "Your speed round questions" section. See review_view.h.
#include "home/review_view.h"
#include "ui/layout.h"
#include "learning/review.h"
#include "speed/speed_plan.h"
#include "ui/ui.h"

#include <stdlib.h>
#include <string.h>

#define WEAKEST_SHOWN 8  // rows before "Show all"

// One answered skill's speed round questions, with how weak the learner is at them.
typedef struct {
    const ReviewItem *item;
    SpeedSkill skill;
    float weakness;
} Row;

static int ByWeakness(const void *a, const void *b)
{
    float wa = ((const Row *)a)->weakness, wb = ((const Row *)b)->weakness;
    return wa < wb ? 1 : wa > wb ? -1 : 0;
}

// The skills the review history has answers for, weakest first. (Older histories also hold
// answers to quiz questions, which no longer exist; they're left out.) Returns the count;
// *rows is malloc'd.
static int BuildRows(Row **rows)
{
    int count = 0;
    *rows = malloc(sizeof **rows * SPEED_SKILL_COUNT);
    if (!*rows) return 0;
    for (int s = 0; s < SPEED_SKILL_COUNT; s++) {
        char id[64];
        SpeedSkillReviewId((SpeedSkill)s, id, sizeof id);
        const ReviewItem *item = ReviewFind(id);
        if (item) (*rows)[count++] = (Row){ item, (SpeedSkill)s, ReviewWeakness(item) };
    }
    qsort(*rows, (size_t)count, sizeof **rows, ByWeakness);
    return count;
}

// "Due today", "Due in 3 days", "Due tomorrow".
static const char *DueText(const ReviewItem *item, long today)
{
    long days = item->due - today;
    if (days <= 0) return "Due for review today";
    if (days == 1) return "Due tomorrow";
    return TextFormat("Due in %ld days", days);
}

// One skill's row: its speed round questions, then right / wrong, accuracy and when they're
// next due. Returns the height.
static float SkillRow(const Row *row, long today, float x, float y, float w, float scale)
{
    const char *title = TextFormat("%s (generated questions)", SpeedSkillTitle(row->skill));
    float textW = w - 260, size = 19 * scale;
    float titleH = UiWrappedText(gFonts.text, title, (Vector2){ 0, 0 }, textW, size, COL_TEXT, false);
    float h = titleH + 52;
    if (h < 78) h = 78;
    UiRoundRect((Rectangle){ x, y, w, h }, 10, COL_PANEL);
    UiWrappedText(gFonts.text, title, (Vector2){ x + 18, y + 12 }, textW, size, COL_TEXT, true);
    UiText(gFonts.text, "Speed rounds", x + 18, y + titleH + 20, 15 * scale, COL_DIM);

    const ReviewItem *item = row->item;
    int answers = item->right + item->wrong;
    int percent = answers ? item->right * 100 / answers : 0;
    float sx = x + w - 230;
    UiText(gFonts.bold, TextFormat("%d right  |  %d wrong", item->right, item->wrong), sx, y + 12, 17 * scale, COL_TEXT);
    Color accuracy = percent >= 70 ? COL_ACCENT : COL_BAD;
    ProgressBar((Rectangle){ sx, y + 38, 150, 6 }, answers ? (float)item->right / answers : 0, COL_BG, accuracy);
    UiText(gFonts.text, TextFormat("%d%%", percent), sx + 160, y + 30, 16 * scale, accuracy);
    UiText(gFonts.text, DueText(item, today), sx, y + 52, 15 * scale, item->due <= today ? COL_ACCENT : COL_DIM);
    return h + 10;
}

float ReviewViewDraw(ReviewView *view, float x, float y, float w, float scale, bool *challenge)
{
    *challenge = false;
    float top = y;
    long today = ReviewToday();
    UiWrappedText(gFonts.text, "Your speed round questions", (Vector2){ x, y }, w, 28 * scale, COL_TEXT, true);
    y += 28 * scale * 1.4f + 12;
    y += UiWrappedText(gFonts.text, "Every answer you give is remembered. Skills you miss come back sooner; ones you "
                       "know wait longer, like flashcards. A short challenge practises the ones that need it most.",
                       (Vector2){ x, y }, w, 18 * scale, COL_DIM, true) + 14;

    Row *rows;
    int count = BuildRows(&rows);
    int due = 0, learnt = 0, right = 0, answers = 0;
    for (int i = 0; i < count; i++) {
        due += rows[i].item->due <= today;
        learnt += rows[i].item->interval >= 21;  // three weeks between reviews: well known
        right += rows[i].item->right;
        answers += rows[i].item->right + rows[i].item->wrong;
    }
    const char *summary = count == 0
        ? "No answers yet. Play a speed round and your skills will appear here."
        : TextFormat("Skills answered: %d  |  Due today: %d  |  Learnt: %d  |  Overall: %d%% right", count, due, learnt,
                     answers ? right * 100 / answers : 0);
    y += UiWrappedText(gFonts.text, summary, (Vector2){ x, y }, w, 20 * scale, COL_TEXT, true) + 14;
    if (UiButtonIcon((Rectangle){ x, y, 300, 50 }, ICON_BOLT, "Start short challenge", true)) *challenge = true;
    y += 50 + 22;

    if (count > 0) {
        y += UiWrappedText(gFonts.text, view->showAll ? "Every skill you've answered, weakest first"
                                                      : "Your weakest skills",
                           (Vector2){ x, y }, w, 21 * scale, COL_ACCENT, true) + 12;
        int shown = view->showAll || count < WEAKEST_SHOWN ? count : WEAKEST_SHOWN;
        for (int i = 0; i < shown; i++) y += SkillRow(&rows[i], today, x, y, w, scale);
        if (count > WEAKEST_SHOWN) {
            const char *label = view->showAll ? "Show only the weakest" : TextFormat("Show all %d skills", count);
            if (UiButtonEx((Rectangle){ x, y + 4, 260, 40 }, label, 17, COL_PANEL, COL_SECONDARY_HOVER, COL_TEXT, true))
                view->showAll = !view->showAll;
            y += 56;
        }
    }
    free(rows);
    return y - top + 16;
}
