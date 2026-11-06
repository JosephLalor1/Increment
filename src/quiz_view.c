// Quiz widgets shared by lesson quizzes and custom practice; no score saving here.
#include "quiz_view.h"
#include "ui.h"

float QuizViewChoice(const Question *q, int choice, int selected, float x, float y, float w, float scale, bool *chosen)
{
    const char *label = TextFormat("%d.  %s", choice + 1, q->choices[choice]);
    float h = UiWrappedText(gFonts.text, label, (Vector2){ 0, 0 }, w - 48, 22 * scale, COL_TEXT, false) + 28;
    Rectangle r = { x, y, w, h };
    bool answered = selected >= 0;
    bool over = !answered && UiMouseOver(r);
    if (over) UiWantHand();
    Color fill = over ? COL_PANEL_HI : COL_PANEL;
    Color textColor = COL_TEXT;
    if (answered) {
        if (choice == q->correct) fill = COL_GOOD_BG;
        else if (choice == selected) fill = COL_BAD_BG;
        else textColor = COL_DIM;
    }
    UiRoundRect(r, 10, fill);
    UiWrappedText(gFonts.text, label, (Vector2){ x + 24, y + 14 }, w - 48, 22 * scale, textColor, true);
    *chosen = (over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || (!answered && IsKeyPressed(KEY_ONE + choice));
    return h;
}

float QuizViewReview(const QuizReviewItem *item, int number, float x, float y, float w, float scale, bool draw)
{
    const Question *q = item->question;
    float top = y;
    Color state = item->correct ? COL_GOOD : COL_BAD;
    const char *status = !item->answered ? "Not answered" : item->correct ? "Correct" : "Incorrect";
    if (draw) UiIconText(item->correct ? ICON_CHECK : ICON_CROSS, gFonts.bold,
                        TextFormat("Question %d - %s", number, status), x, y, 22 * scale, state);
    y += 38 * scale;
    y += UiWrappedText(gFonts.text, q->prompt, (Vector2){ x, y }, w, 24 * scale, COL_TEXT, draw) + 16;
    if (q->code) y += UiCodeBlock(q->code, (Vector2){ x, y }, w, 20 * scale, draw) + 16;
    y += UiWrappedText(gFonts.text, "Your answer:", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
    y += UiWrappedText(gFonts.text, item->selectedAnswer ? item->selectedAnswer : "Not answered",
                       (Vector2){ x, y }, w, 22 * scale, state, draw) + 16;
    y += UiWrappedText(gFonts.text, "Correct answer:", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
    y += UiWrappedText(gFonts.text, item->correctAnswer ? item->correctAnswer : "Not available",
                       (Vector2){ x, y }, w, 22 * scale, COL_GOOD, draw) + 16;
    if (item->explanation) {
        y += UiWrappedText(gFonts.text, "Explanation", (Vector2){ x, y }, w, 18 * scale, COL_DIM, draw) + 6;
        y += UiWrappedText(gFonts.text, item->explanation, (Vector2){ x, y }, w, 21 * scale, COL_TEXT, draw) + 12;
    }
    return y - top;
}
