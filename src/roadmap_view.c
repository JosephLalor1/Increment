// Optional lesson preparation notes; course navigation lives in track_screen.c.
#include "roadmap_view.h"
#include "roadmap.h"
#include "ui.h"

float RoadmapViewPreparation(const LessonSet *set, const Lesson *lesson, float x, float y, float w, float size,
                             bool draw)
{
    char text[512];
    if (!RoadmapPreparation(set, lesson, text, sizeof text)) return 0;
    return UiWrappedText(gFonts.text, text, (Vector2){ x, y }, w, size, COL_DIM, draw);
}
