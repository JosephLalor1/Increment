// The statistics page's "Your skills" section. See skills_view.h.
#include "skills_view.h"
#include "layout.h"
#include "skill_progress.h"
#include "ui.h"

#define WEAKEST_MARKED 3  // tried skills marked "Needs work"

static bool Tried(Skill skill)
{
    const SkillProgress *p = SkillProgressOf(skill);
    return p->right + p->wrong > 0;
}

// The tried skills, weakest first; returns how many.
static int WeakestTried(Skill *out)
{
    int count = 0;
    for (int s = 0; s < SKILL_COUNT; s++) if (Tried((Skill)s)) out[count++] = (Skill)s;
    for (int i = 1; i < count; i++) {  // insertion sort: at most SKILL_COUNT items
        Skill key = out[i];
        int j = i - 1;
        while (j >= 0 && SkillWeakness(out[j]) < SkillWeakness(key)) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return count;
}

// One skill's tile: title, level, an XP bar towards the next level, and accuracy.
static void SkillTile(Skill skill, bool needsWork, Rectangle r, float scale)
{
    const SkillProgress *p = SkillProgressOf(skill);
    bool tried = Tried(skill);
    int level = SkillLevel(p->xp);
    float from = SkillXpForLevel(level), to = SkillXpForLevel(level + 1);
    UiRoundRect(r, 10, COL_PANEL);
    if (needsWork) DrawRectangleRoundedLinesEx(r, 16 / (r.height < r.width ? r.height : r.width), 8, 1.5f, COL_BAD);

    UiText(gFonts.text, SkillTitle(skill), r.x + 16, r.y + 12, 18 * scale, tried ? COL_TEXT : COL_DIM);
    const char *badge = TextFormat("Level %d", level);
    float badgeW = UiTextWidth(gFonts.bold, badge, 15 * scale) + 18;
    UiRoundRect((Rectangle){ r.x + r.width - badgeW - 12, r.y + 10, badgeW, 24 * scale }, 12,
                tried ? COL_NOTE_BG : COL_BG);
    UiText(gFonts.bold, badge, r.x + r.width - badgeW - 3, r.y + 13, 15 * scale, tried ? COL_ACCENT : COL_DIM);

    float barY = r.y + 20 + 24 * scale;
    ProgressBar((Rectangle){ r.x + 16, barY, r.width - 32, 7 }, tried ? (p->xp - from) / (to - from) : 0, COL_BG,
                COL_ACCENT);
    const char *detail = !tried ? "Not tried yet"
        : TextFormat("%.0f XP  |  %.0f to level %d  |  %d right, %d wrong  |  %d%% lately", p->xp, to - p->xp, level + 1,
                     p->right, p->wrong, (int)(p->accuracy * 100 + 0.5f));
    UiText(gFonts.text, detail, r.x + 16, barY + 14, 14 * scale, COL_DIM);
    if (needsWork) UiText(gFonts.bold, "Needs work", r.x + r.width - 96 * scale, barY + 14, 14 * scale, COL_BAD);
}

float SkillsViewDraw(float x, float y, float w, float scale, bool *practise)
{
    *practise = false;
    float top = y;
    UiWrappedText(gFonts.text, "Your skills", (Vector2){ x, y }, w, 28 * scale, COL_TEXT, true);
    y += 28 * scale * 1.4f + 12;
    y += UiWrappedText(gFonts.text, "Every question works on one or more skills. Right answers earn XP (more for hard "
                       "questions, ones you used to miss, and answers against the clock); wrong answers lose some.",
                       (Vector2){ x, y }, w, 18 * scale, COL_DIM, true) + 14;

    Skill weakest[SKILL_COUNT];
    int tried = WeakestTried(weakest);
    float totalXp = 0;
    for (int s = 0; s < SKILL_COUNT; s++) totalXp += SkillProgressOf((Skill)s)->xp;
    const char *summary = tried == 0
        ? "No skills tried yet: answer a quiz or a speed round to start earning XP."
        : TextFormat("Total: %.0f XP across %d skills  |  Strongest: %s  |  To work on: %s", totalXp, tried,
                     SkillTitle(weakest[tried - 1]), SkillTitle(weakest[0]));
    y += UiWrappedText(gFonts.text, summary, (Vector2){ x, y }, w, 20 * scale, COL_TEXT, true) + 14;
    if (tried > 0 && UiButtonIcon((Rectangle){ x, y, 340, 50 }, ICON_BOLT, "Practise your weakest skills", true))
        *practise = true;
    if (tried > 0) y += 50 + 20;

    // Only skills with at least one wrong answer can need work.
    bool marked[SKILL_COUNT] = { false };
    for (int i = 0, n = 0; i < tried && n < WEAKEST_MARKED; i++) {
        if (SkillProgressOf(weakest[i])->wrong == 0) continue;
        marked[weakest[i]] = true;
        n++;
    }

    int columns = w / scale >= 720 ? 2 : 1;
    float gap = 12, tileW = (w - gap * (columns - 1)) / columns, tileH = 30 + 24 * scale + 32 * scale;
    for (int g = 0; g < SKILL_GROUP_COUNT; g++) {
        y += UiWrappedText(gFonts.text, SkillGroupTitle((SkillGroup)g), (Vector2){ x, y }, w, 20 * scale, COL_ACCENT,
                           true) + 10;
        int column = 0;
        for (int s = 0; s < SKILL_COUNT; s++) {
            if (SkillGroupOf((Skill)s) != (SkillGroup)g) continue;
            SkillTile((Skill)s, marked[s], (Rectangle){ x + column * (tileW + gap), y, tileW, tileH }, scale);
            if (++column == columns) {
                column = 0;
                y += tileH + gap;
            }
        }
        if (column != 0) y += tileH + gap;
        y += 10;
    }
    return y - top + 12;
}
