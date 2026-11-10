// Scrollable tracing predictions with retryable feedback and separate completion.
#include "tracing_screen.h"
#include "tracing.h"
#include "progress.h"
#include "layout.h"
#include "ui.h"
#include <string.h>

static struct {
    const Lesson *lesson;
    int index;
    char answer[512];
    bool submitted, correct, saveFailed;
    float scroll, height;
} state;

void TracingScreenClear(void)
{
    memset(&state, 0, sizeof state);
    UiFocus(NULL);
}

void TracingScreenOpen(const Lesson *lesson, int index)
{
    TracingScreenClear();
    if (lesson && index >= 0 && index < lesson->traceCount) {
        state.lesson = lesson;
        state.index = index;
    }
}

bool TracingScreen(void)
{
    if (TopBar("Code Tracing - Predict before running")) { UiFocus(NULL); return true; }
    if (!state.lesson) return true;
    const TraceExercise *trace = &state.lesson->traces[state.index];
    Rectangle view = ScrollView();
    UpdateScroll(&state.scroll, state.height, view.height);
    BeginView(view);
    float x = ContentX(), w = ContentWidth(), scale = UiTextScale();
    float top = view.y + 24 - state.scroll, y = top;
    y += UiWrappedText(gFonts.bold, trace->title, (Vector2){x,y}, w, 32 * scale, COL_TEXT, true) + 12;
    y += UiWrappedText(gFonts.text, state.lesson->title, (Vector2){x,y}, w, 19 * scale, COL_DIM, true) + 20;
    if (!TracingValid(trace)) {
        y += UiWrappedText(gFonts.text, "This tracing task is incomplete and cannot be submitted.", (Vector2){x,y}, w, 22 * scale, COL_DIM, true);
    } else {
        y += UiCodeBlock(trace->code, (Vector2){x,y}, w, 20 * scale, true) + 20;
        y += UiWrappedText(gFonts.bold, trace->prompt, (Vector2){x,y}, w, 24 * scale, COL_TEXT, true) + 12;
        y += UiWrappedText(gFonts.text, "Enter output or values in order, separated by spaces. Case matters; extra whitespace is ignored.",
                           (Vector2){x,y}, w, 18 * scale, COL_DIM, true) + 14;
        char before[sizeof state.answer];
        memcpy(before, state.answer, sizeof before);
        bool enter = UiTextBox((Rectangle){x,y,w,48}, state.answer, sizeof state.answer, "Your prediction", false);
        if (strcmp(before, state.answer)) { state.submitted = state.correct = state.saveFailed = false; }
        y += 62;
        bool submit = UiButtonIcon((Rectangle){x,y,250,48}, ICON_CHECK, "Check prediction", state.answer[0] != 0);
        if ((submit || enter) && state.answer[0]) {
            state.submitted = true;
            state.correct = TracingAnswer(trace, state.answer);
            state.saveFailed = state.correct && !ProgressRecordTrace(state.lesson->id, trace->id);
            UiFocus(NULL);
        }
        y += 66;
        if (state.submitted) {
            y += UiWrappedText(gFonts.bold, state.correct ? "Correct! You traced it successfully." : "Not quite. Follow each statement and try again.",
                               (Vector2){x,y}, w, 23 * scale, state.correct ? COL_GOOD : COL_BAD, true) + 12;
            y += UiWrappedText(gFonts.text, trace->explain, (Vector2){x,y}, w, 22 * scale, COL_TEXT, true) + 14;
            if (!state.correct && UiButtonIcon((Rectangle){x,y,200,44}, ICON_RETRY, "Try again", true)) {
                state.submitted = false;
                UiFocus(state.answer);
            }
            if (!state.correct) y += 58;
        }
        const char *status = state.saveFailed ? "Could not save completion. Check your data folder and submit again."
            : ProgressTraceDone(state.lesson->id, trace->id) ? "Tracing complete - saved to your account. Your quiz score is unchanged."
            : "Tracing completion is separate from your lesson quiz score.";
        y += UiWrappedText(gFonts.text, status, (Vector2){x,y}, w, 18 * scale, state.saveFailed ? COL_BAD : COL_DIM, true);
    }
    state.height = y - top + 40;
    EndView();
    PageScrollBar(view, &state.scroll, state.height);
    return false;
}
