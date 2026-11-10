// Local users ranked by completed lessons and best quiz scores, using existing themes.
#include "ranking_screen.h"
#include "ranking.h"
#include "layout.h"
#include "ui.h"
#include <stdlib.h>
#include <string.h>

static struct {
    const LessonSet *lessons;
    RankingEntry *entries;
    int count, totalLessons, maxPoints;
    Track track;
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
    page.count = page.totalLessons = page.maxPoints = 0;
    page.error = false;
    page.scroll = page.height = 0;
    if (!page.lessons) return;
    for (int i = 0; i < page.lessons->count; i++) {
        const Lesson *lesson = &page.lessons->items[i];
        if (page.track != TRACK_COUNT && lesson->track != page.track) continue;
        page.totalLessons++;
        if (lesson->questionCount) page.maxPoints += 100;
    }
    int count = AccountCount();
    page.entries = calloc(count ? count : 1, sizeof *page.entries);
    if (!page.entries) { page.error = true; return; }
    for (int i = 0; i < count; i++) {
        char path[1100];
        RankingEntry *entry = &page.entries[page.count];
        if (!AccountRankingProfile(i, entry->name, sizeof entry->name, path, sizeof path)) { page.error = true; continue; }
        RankingRead(entry, page.lessons, page.track, path);
        page.count++;
    }
    RankingSort(page.entries, page.count);
}

void RankingScreenOpen(const LessonSet *lessons)
{
    RankingScreenClear();
    page.lessons = lessons;
    page.track = TRACK_COUNT;
    Refresh();
}

static float Text(const char *text, float x, float y, float w, float size, Color color)
{
    return UiWrappedText(gFonts.text, text, (Vector2){x,y}, w, size, color, true);
}

bool RankingScreen(void)
{
    if (TopBar("User Rankings")) return true;
    float x = ContentX(), w = ContentWidth();
    if (UiButtonIcon((Rectangle){x+w-132,78,132,42}, ICON_RETRY, "Refresh", true)) Refresh();
    const Track tracks[] = { TRACK_COUNT, TRACK_C, TRACK_CPP };
    const char *labels[] = { "Overall", "C", "C++" };
    float tabW = (w - 168) / 3;
    for (int i = 0; i < 3; i++) {
        bool active = page.track == tracks[i];
        if (UiButtonEx((Rectangle){x+i*(tabW+8),78,tabW,42}, labels[i], 20,
                       active ? COL_ACCENT : COL_PANEL, active ? COL_ACCENT_HI : COL_SECONDARY_HOVER,
                       active ? COL_ON_ACCENT : COL_TEXT, true)) { page.track = tracks[i]; Refresh(); }
    }
    Rectangle view = {0,136,(float)GetScreenWidth(),GetScreenHeight()-136.0f};
    UpdateScroll(&page.scroll, page.height, view.height);
    BeginView(view);
    float scale = UiTextScale(), top = view.y+12-page.scroll, y = top;
    y += Text("Learn together, grow together", x,y,w,30*scale,COL_TEXT)+12;
    y += Text("Local accounts on this computer. Ranked by completed lessons, then total best quiz scores. Equal results share a rank.",x,y,w,19*scale,COL_DIM)+18;
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
    if (!page.totalLessons) y += Text("No lessons are available yet. Rankings will reflect progress once lessons are loaded.",x,y,w,20*scale,COL_DIM)+18;
    if (!page.count && !page.error) y += Text("No local accounts yet. Create an account to join the leaderboard.",x,y,w,22*scale,COL_DIM)+18;
    for (int i = 0; i < page.count; i++) {
        const RankingEntry *entry = &page.entries[i];
        bool me = current && !strcmp(entry->name,current);
        const char *summary = TextFormat("%d / %d lessons completed    |    Quiz points: %d / %d",entry->completed,page.totalLessons,entry->quizPoints,page.maxPoints);
        float bodyH = UiWrappedText(gFonts.text,summary,(Vector2){0,0},w-116,18*scale,COL_DIM,false);
        float h = 62*scale+bodyH;
        UiRoundRect((Rectangle){x,y,w,h},12,me ? COL_GOOD_BG : COL_PANEL);
        UiTextCentered(gFonts.bold,TextFormat("#%d",entry->rank),x+42,y+24*scale,24*scale,entry->rank<=3 ? COL_ACCENT : COL_DIM);
        Text(TextFormat("%s%s",entry->name,me ? " (you)" : ""),x+84,y+14,w-104,23*scale,me ? COL_GOOD : COL_TEXT);
        Text(summary,x+84,y+48*scale,w-116,18*scale,COL_DIM);
        y += h+12;
    }
    y += Text("Each quiz contributes its best saved score (0-100); unattempted quizzes contribute 0. Coding exercises, tracing tasks, practice results and speed-round gold do not add ranking points.",x,y+8,w,17*scale,COL_DIM)+28;
    page.height = y-top+24;
    EndView();
    PageScrollBar(view,&page.scroll,page.height);
    return false;
}
