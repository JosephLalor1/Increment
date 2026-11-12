// The rankings screen: the accounts on this computer ranked by progress (levels and steps
// passed) or by the concepts found in their saved programs. See ranking_screen.h.
#include "rankings/ranking_screen.h"
#include "rankings/ranking.h"
#include "rankings/ranking_skills.h"
#include "ui/layout.h"
#include "ui/ui.h"
#include <stdlib.h>
#include <string.h>

static struct {
    const LessonSet *lessons;
    RankingEntry *entries;          // one per local account, best first
    int count;
    int totalLessons, totalSteps;   // what there is to complete in the chosen track
    Track track;                    // TRACK_COUNT means both
    RankingMode mode;
    int expanded;                   // the row showing its skills, -1 for none
    bool error;                     // some account couldn't be read
    float scroll, height;
} page;

void RankingScreenClear(void)
{
    free(page.entries);
    memset(&page, 0, sizeof page);
}

// Reads every account's saved results again and ranks them.
static void Refresh(void)
{
    free(page.entries);
    page.entries = NULL;
    page.count = page.totalLessons = page.totalSteps = 0;
    page.error = false;
    page.scroll = page.height = 0;
    page.expanded = -1;
    if (!page.lessons) return;
    for (int i = 0; i < page.lessons->count; i++) {
        const Lesson *lesson = &page.lessons->items[i];
        if (page.track != TRACK_COUNT && lesson->track != page.track) continue;
        if (!lesson->stepCount) continue; // not written yet
        page.totalLessons++;
        page.totalSteps += lesson->stepCount;
    }
    int count = AccountCount();
    page.entries = calloc(count ? count : 1, sizeof *page.entries);
    if (!page.entries) {
        page.error = true;
        return;
    }
    for (int i = 0; i < count; i++) {
        char path[1100];
        RankingEntry *entry = &page.entries[page.count];
        if (!AccountRankingProfile(i, entry->name, sizeof entry->name, path, sizeof path)) {
            page.error = true;
            continue;
        }
        if (page.mode == RANKING_SKILLS) {
            char folder[1100];
            if (!AccountRankingCodeFolder(i, folder, sizeof folder)) {
                page.error = true;
                continue;
            }
            RankingReadSkills(entry, page.track, folder);
        } else
            RankingRead(entry, page.lessons, page.track, path);
        page.count++;
    }
    RankingSortMode(page.entries, page.count, page.mode);
}

void RankingScreenSelect(RankingMode mode, Track track)
{
    if (mode < 0 || mode >= RANKING_MODE_COUNT || track < 0 || track > TRACK_COUNT) return;
    page.mode = mode;
    page.track = track;
    Refresh();
}

void RankingScreenOpen(const LessonSet *lessons)
{
    RankingScreenClear();
    page.lessons = lessons;
    page.track = TRACK_COUNT;
    Refresh();
}

// A tab: accent-filled when it is the open one. Returns whether another tab was clicked.
static bool Tab(Rectangle r, const char *label, bool active)
{
    return UiButtonEx(r, label, 20, active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                      active ? COL_ON_ACCENT : COL_TEXT, true) &&
           !active;
}

// The two rows above the list: which ranking, then which track, and Refresh.
static void Filters(float x, float w)
{
    const char *modes[] = { "Progress Ranking", "Skills Ranking" };
    float modeW = (w - 12) / 2;
    for (int i = 0; i < RANKING_MODE_COUNT; i++) {
        if (Tab((Rectangle){ x + i * (modeW + 12), 78, modeW, 42 }, modes[i], page.mode == (RankingMode)i))
            RankingScreenSelect((RankingMode)i, page.track);
    }
    if (UiButtonIcon((Rectangle){ x + w - 132, 132, 132, 42 }, ICON_RETRY, "Refresh", true)) Refresh();
    const Track tracks[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    const char *labels[] = { "Overall", "C", "C++" };
    float tabW = (w - 168) / 3;
    for (int i = 0; i < 3; i++) {
        if (Tab((Rectangle){ x + i * (tabW + 8), 132, tabW, 42 }, labels[i], page.track == tracks[i]))
            RankingScreenSelect(page.mode, tracks[i]);
    }
}

static bool IsCurrentUser(const RankingEntry *entry)
{
    const char *current = AccountCurrent();
    return current && !strcmp(entry->name, current);
}

// The heading, the signed-in learner's rank and anything worth warning about, from y down.
// Returns the y below them.
static float Summary(float x, float y, float w, float scale, bool skillsMode)
{
    y += UiParagraph("Learn together, grow together", x, y, w, 30 * scale, COL_TEXT) + 12;
    y += UiParagraph(skillsMode
                         ? "Local accounts ranked by unique detected concepts, then saved programs with recognised concepts. Equal results share a rank."
                         : "Local accounts on this computer. Ranked by completed levels, then passed coding steps. Equal results share a rank.",
                     x, y, w, 19 * scale, COL_DIM) +
         18;
    for (int i = 0; i < page.count; i++) {
        const RankingEntry *entry = &page.entries[i];
        if (IsCurrentUser(entry)) {
            y += UiParagraph(TextFormat("Your rank: #%d of %d learners", entry->rank, page.count), x, y, w, 24 * scale,
                             COL_ACCENT) +
                 18;
            break;
        }
    }

    bool warning = page.error, anySkills = false;
    for (int i = 0; i < page.count; i++) {
        if (page.entries[i].warning) warning = true;
        if (page.entries[i].uniqueSkills) anySkills = true;
    }
    if (warning)
        y += UiParagraph("Some saved records could not be read. Rankings may be incomplete; try Refresh after checking the data folder.",
                         x, y, w, 18 * scale, COL_BAD) +
             18;
    if (!skillsMode && !page.totalLessons)
        y += UiParagraph("No lessons are available yet. Rankings will reflect progress once lessons are loaded.", x, y,
                         w, 20 * scale, COL_DIM) +
             18;
    if (skillsMode && !anySkills)
        y += UiParagraph("No programming skills detected yet. Save programs in the sandbox to start building your skills ranking!",
                         x, y, w, 20 * scale, COL_ACCENT) +
             18;
    if (!page.count && !page.error)
        y += UiParagraph("No local accounts yet. Create an account to join the leaderboard.", x, y, w, 22 * scale,
                         COL_DIM) +
             18;
    return y;
}

// One learner's row: rank, name and result. In the skills ranking a click opens the row
// to list the concepts found in their saved programs. Returns the row's height.
static float EntryRow(int index, float x, float y, float w, float scale, bool skillsMode)
{
    const RankingEntry *entry = &page.entries[index];
    bool me = IsCurrentUser(entry);
    const char *summary = skillsMode
        ? TextFormat("%d unique concept%s    |    %d contributing program%s", entry->uniqueSkills,
                     entry->uniqueSkills == 1 ? "" : "s", entry->skillPrograms, entry->skillPrograms == 1 ? "" : "s")
        : TextFormat("%d / %d levels completed    |    Steps passed: %d / %d", entry->completed, page.totalLessons,
                     entry->stepsPassed, page.totalSteps);

    // Measure first: the row's box is drawn before what goes in it.
    float bodyH = UiWrappedText(gFonts.text, summary, (Vector2){ 0, 0 }, w - 116, 18 * scale, COL_DIM, false);
    float h = 62 * scale + bodyH;
    float detailsY = y + h;
    bool expanded = skillsMode && page.expanded == index;
    if (skillsMode) h += 32 * scale;
    if (expanded) {
        for (int s = 0; s < SKILL_COUNT; s++) {
            if (!entry->skills[s]) continue;
            h += UiWrappedText(gFonts.text, SkillTitle((Skill)s), (Vector2){ 0, 0 }, w - 148, 19 * scale, COL_TEXT,
                               false) +
                 8;
        }
        if (!entry->uniqueSkills) h += 30 * scale;
        h += 14;
    }

    Rectangle row = { x, y, w, h };
    bool over = skillsMode && UiMouseOver(row);
    if (over) UiWantHand();
    UiRoundRect(row, 12, me ? COL_GOOD_BG : over ? COL_PANEL_HI : COL_PANEL);
    UiTextCentered(gFonts.bold, TextFormat("#%d", entry->rank), x + 42, y + 24 * scale, 24 * scale,
                   entry->rank <= 3 ? COL_ACCENT : COL_DIM);
    UiParagraph(TextFormat("%s%s", entry->name, me ? " (you)" : ""), x + 84, y + 14, w - 104, 23 * scale,
                me ? COL_GOOD : COL_TEXT);
    UiParagraph(summary, x + 84, y + 48 * scale, w - 116, 18 * scale, COL_DIM);
    if (!skillsMode) return h;

    UiParagraph(expanded ? "Hide skills" : "Click to view skills", x + 84, detailsY - 2, w - 116, 17 * scale,
                COL_ACCENT);
    if (expanded) {
        float skillY = detailsY + 30 * scale;
        for (int s = 0; s < SKILL_COUNT; s++) {
            if (!entry->skills[s]) continue;
            IconDraw(ICON_CHECK, (Vector2){ x + 94, skillY + 11 * scale }, 16 * scale, COL_ACCENT);
            skillY += UiParagraph(SkillTitle((Skill)s), x + 116, skillY, w - 148, 19 * scale, COL_TEXT) + 8;
        }
        if (!entry->uniqueSkills)
            UiParagraph("No recognised concepts in this track yet.", x + 84, skillY, w - 116, 18 * scale, COL_DIM);
    }
    if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) page.expanded = expanded ? -1 : index;
    return h;
}

bool RankingScreen(void)
{
    if (TopBar("User Rankings")) return true;
    float x = ContentX(), w = ContentWidth();
    Filters(x, w);

    Rectangle view = { 0, 190, (float)GetScreenWidth(), GetScreenHeight() - 190.0f };
    UpdateScroll(&page.scroll, page.height, view.height);
    BeginView(view);
    float scale = UiTextScale(), top = view.y + 12 - page.scroll, y = top;
    bool skillsMode = page.mode == RANKING_SKILLS;
    y = Summary(x, y, w, scale, skillsMode);
    for (int i = 0; i < page.count; i++) y += EntryRow(i, x, y, w, scale, skillsMode) + 12;
    y += UiParagraph(skillsMode
                         ? "Concept hints come from currently saved sandbox source, not compiler-verified mastery. Repeated concepts count once. Click a learner to view names only; source code stays private. Refresh after changing saved programs."
                         : "Every passed coding step counts once; replaying a step adds nothing. Speed-round gold does not add ranking points.",
                     x, y + 8, w, 17 * scale, COL_DIM) +
         28;
    page.height = y - top + 24;
    EndView();
    PageScrollBar(view, &page.scroll, page.height);
    return false;
}
