// Local users ranked by completed levels and passed steps, using existing themes.
#include "ranking_screen.h"
#include "ranking.h"
#include "ranking_skills.h"
#include "layout.h"
#include "ui.h"
#include <stdlib.h>
#include <string.h>

static struct {
    const LessonSet *lessons;
    RankingEntry *entries;
    int count, totalLessons, totalSteps;
    Track track;
    RankingMode mode;
    int expanded;
    bool error;
    float scroll, height;
} page;

void RankingScreenClear(void)
{
    free(page.entries);
    memset(&page, 0, sizeof page);
}

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
        if (!lesson->stepCount) continue;  // not written yet
        page.totalLessons++;
        page.totalSteps += lesson->stepCount;
    }
    int count = AccountCount();
    page.entries = calloc(count ? count : 1, sizeof *page.entries);
    if (!page.entries) { page.error = true; return; }
    for (int i = 0; i < count; i++) {
        char path[1100];
        RankingEntry *entry = &page.entries[page.count];
        if (!AccountRankingProfile(i, entry->name, sizeof entry->name, path, sizeof path)) { page.error = true; continue; }
        if (page.mode == RANKING_SKILLS) {
            char folder[1100];
            if (!AccountRankingCodeFolder(i, folder, sizeof folder)) { page.error = true; continue; }
            RankingReadSkills(entry, page.track, folder);
        } else RankingRead(entry, page.lessons, page.track, path);
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

void RankingScreenDemo(int row, float scroll)
{
    page.expanded = row >= 0 && row < page.count ? row : -1;
    page.scroll = scroll > 0 ? scroll : 0;
    page.height = page.scroll + GetScreenHeight();
}

static float Text(const char *text, float x, float y, float w, float size, Color color)
{
    return UiWrappedText(gFonts.text, text, (Vector2){x,y}, w, size, color, true);
}

bool RankingScreen(void)
{
    if (TopBar("User Rankings")) return true;
    float x = ContentX(), w = ContentWidth();
    const char *modes[] = { "Progress Ranking", "Skills Ranking" };
    float modeW = (w - 12) / 2;
    for (int i = 0; i < RANKING_MODE_COUNT; i++) {
        bool active = page.mode == (RankingMode)i;
        if (UiButtonEx((Rectangle){x+i*(modeW+12),78,modeW,42}, modes[i], 20,
                       active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                       active ? COL_ON_ACCENT : COL_TEXT, true) && !active) RankingScreenSelect((RankingMode)i, page.track);
    }
    if (UiButtonIcon((Rectangle){x+w-132,132,132,42}, ICON_RETRY, "Refresh", true)) Refresh();
    const Track tracks[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    const char *labels[] = { "Overall", "C", "C++" };
    float tabW = (w - 168) / 3;
    for (int i = 0; i < 3; i++) {
        bool active = page.track == tracks[i];
        if (UiButtonEx((Rectangle){x+i*(tabW+8),132,tabW,42}, labels[i], 20,
                       active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                       active ? COL_ON_ACCENT : COL_TEXT, true) && !active) RankingScreenSelect(page.mode, tracks[i]);
    }
    Rectangle view = {0,190,(float)GetScreenWidth(),GetScreenHeight()-190.0f};
    UpdateScroll(&page.scroll, page.height, view.height);
    BeginView(view);
    float scale = UiTextScale(), top = view.y+12-page.scroll, y = top;
    y += Text("Learn together, grow together", x,y,w,30*scale,COL_TEXT)+12;
    bool skillsMode = page.mode == RANKING_SKILLS;
    y += Text(skillsMode
        ? "Local accounts ranked by unique detected concepts, then saved programs with recognised concepts. Equal results share a rank."
        : "Local accounts on this computer. Ranked by completed levels, then passed coding steps. Equal results share a rank.",x,y,w,19*scale,COL_DIM)+18;
    const char *current = AccountCurrent();
    for (int i = 0; i < page.count; i++) {
        const RankingEntry *entry = &page.entries[i];
        if (current && !strcmp(entry->name,current)) {
            y += Text(TextFormat("Your rank: #%d of %d learners",entry->rank,page.count),x,y,w,24*scale,COL_ACCENT)+18;
            break;
        }
    }
    bool warning = page.error;
    for (int i = 0; i < page.count; i++) if (page.entries[i].warning) warning = true;
    if (warning) y += Text("Some saved records could not be read. Rankings may be incomplete; try Refresh after checking the data folder.",x,y,w,18*scale,COL_BAD)+18;
    if (!skillsMode && !page.totalLessons) y += Text("No lessons are available yet. Rankings will reflect progress once lessons are loaded.",x,y,w,20*scale,COL_DIM)+18;
    bool anySkills = false;
    for (int i = 0; i < page.count; i++) if (page.entries[i].uniqueSkills) anySkills = true;
    if (skillsMode && !anySkills) y += Text("No programming skills detected yet. Save programs in the sandbox to start building your skills ranking!",x,y,w,20*scale,COL_ACCENT)+18;
    if (!page.count && !page.error) y += Text("No local accounts yet. Create an account to join the leaderboard.",x,y,w,22*scale,COL_DIM)+18;
    for (int i = 0; i < page.count; i++) {
        const RankingEntry *entry = &page.entries[i];
        bool me = current && !strcmp(entry->name,current);
        const char *summary = skillsMode
            ? TextFormat("%d unique concept%s    |    %d contributing program%s",entry->uniqueSkills,entry->uniqueSkills == 1 ? "" : "s",
                         entry->skillPrograms,entry->skillPrograms == 1 ? "" : "s")
            : TextFormat("%d / %d levels completed    |    Steps passed: %d / %d",entry->completed,page.totalLessons,entry->stepsPassed,page.totalSteps);
        float bodyH = UiWrappedText(gFonts.text,summary,(Vector2){0,0},w-116,18*scale,COL_DIM,false);
        float h = 62*scale+bodyH;
        float detailsY = y+h;
        bool expanded = skillsMode && page.expanded == i;
        if (skillsMode) h += 32*scale;
        if (expanded) {
            for (int s = 0; s < SKILL_COUNT; s++) if (entry->skills[s])
                h += UiWrappedText(gFonts.text,SkillTitle((Skill)s),(Vector2){0,0},w-148,19*scale,COL_TEXT,false)+8;
            if (!entry->uniqueSkills) h += 30*scale;
            h += 14;
        }
        Rectangle row = {x,y,w,h};
        bool over = skillsMode && UiMouseOver(row);
        if (over) UiWantHand();
        UiRoundRect(row,12,me ? COL_GOOD_BG : over ? COL_PANEL_HI : COL_PANEL);
        UiTextCentered(gFonts.bold,TextFormat("#%d",entry->rank),x+42,y+24*scale,24*scale,entry->rank<=3 ? COL_ACCENT : COL_DIM);
        Text(TextFormat("%s%s",entry->name,me ? " (you)" : ""),x+84,y+14,w-104,23*scale,me ? COL_GOOD : COL_TEXT);
        Text(summary,x+84,y+48*scale,w-116,18*scale,COL_DIM);
        if (skillsMode) {
            Text(expanded ? "Hide skills" : "Click to view skills",x+84,detailsY-2,w-116,17*scale,COL_ACCENT);
            float dy = detailsY+30*scale;
            if (expanded) {
                for (int s = 0; s < SKILL_COUNT; s++) if (entry->skills[s]) {
                    IconDraw(ICON_CHECK,(Vector2){x+94,dy+11*scale},16*scale,COL_ACCENT);
                    dy += Text(SkillTitle((Skill)s),x+116,dy,w-148,19*scale,COL_TEXT)+8;
                }
                if (!entry->uniqueSkills) Text("No recognised concepts in this track yet.",x+84,dy,w-116,18*scale,COL_DIM);
            }
            if (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) page.expanded = expanded ? -1 : i;
        }
        y += h+12;
    }
    y += Text(skillsMode
        ? "Concept hints come from currently saved sandbox source, not compiler-verified mastery. Repeated concepts count once. Click a learner to view names only; source code stays private. Refresh after changing saved programs."
        : "Every passed coding step counts once; replaying a step adds nothing. Speed-round gold does not add ranking points.",x,y+8,w,17*scale,COL_DIM)+28;
    page.height = y-top+24;
    EndView();
    PageScrollBar(view,&page.scroll,page.height);
    return false;
}
