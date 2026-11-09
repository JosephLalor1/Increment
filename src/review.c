// The user's answer history and SM-2 review schedule. See review.h.
#include "review.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static ReviewItem items[REVIEW_MAX];
static int itemCount;
static char savePath[600];  // "" while signed out

// Days from 1970-01-01 to a calendar date (Howard Hinnant's days_from_civil).
static long DaysFromCivil(int year, int month, int day)
{
    year -= month <= 2;
    long era = (year >= 0 ? year : year - 399) / 400;
    long yearOfEra = year - era * 400;
    long dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

long ReviewToday(void)
{
    // The local calendar date, so "due today" turns over at the learner's own midnight.
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (!local) return (long)(now / 86400);
    return DaysFromCivil(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

static bool Save(void)
{
    if (!savePath[0]) return false;
    char temp[620];
    snprintf(temp, sizeof temp, "%s.tmp", savePath);
    FILE *f = fopen(temp, "w");
    if (!f) return false;
    for (int i = 0; i < itemCount; i++) {
        const ReviewItem *r = &items[i];
        fprintf(f, "%s %d %d %d %d %.3f %d %ld %ld\n", r->id, r->right, r->wrong, r->repetitions, r->lapses, r->ease,
                r->interval, r->due, r->last);
    }
    bool ok = fclose(f) == 0;
    return ok && PlatformReplaceFile(temp, savePath);  // the old file stays until the new one is complete
}

void ReviewLoad(const char *path)
{
    itemCount = 0;
    snprintf(savePath, sizeof savePath, "%s", path ? path : "");
    if (!savePath[0]) return;
    FILE *f = fopen(savePath, "r");
    if (!f) return;
    ReviewItem r;
    while (itemCount < REVIEW_MAX
           && fscanf(f, "%63s %d %d %d %d %f %d %ld %ld", r.id, &r.right, &r.wrong, &r.repetitions, &r.lapses, &r.ease,
                     &r.interval, &r.due, &r.last) == 9) {
        if (r.ease < 1.3f) r.ease = 1.3f;
        items[itemCount++] = r;
    }
    fclose(f);
}

static ReviewItem *Find(const char *id)
{
    for (int i = 0; i < itemCount; i++) if (strcmp(items[i].id, id) == 0) return &items[i];
    return NULL;
}

const ReviewItem *ReviewFind(const char *id) { return Find(id); }
int ReviewCount(void) { return itemCount; }
const ReviewItem *ReviewAt(int index) { return index >= 0 && index < itemCount ? &items[index] : NULL; }

bool ReviewRecord(const char *id, bool correct, long today)
{
    if (!savePath[0] || !id || !id[0] || strlen(id) >= REVIEW_ID_SIZE) return false;
    ReviewItem *r = Find(id);
    if (!r) {
        if (itemCount == REVIEW_MAX) return false;
        r = &items[itemCount++];
        memset(r, 0, sizeof *r);
        snprintf(r->id, sizeof r->id, "%s", id);
        r->ease = 2.5f;
    }
    // SM-2 with two grades: 4 for a right answer, 1 for a wrong one.
    int quality = correct ? 4 : 1;
    if (correct) {
        r->right++;
        r->repetitions++;
        r->interval = r->repetitions == 1 ? 1 : r->repetitions == 2 ? 3 : (int)(r->interval * r->ease + 0.5f);
        r->due = today + r->interval;
    } else {
        r->wrong++;
        if (r->repetitions > 0) r->lapses++;  // it had been learnt, and slipped
        r->repetitions = 0;
        r->interval = 1;
        r->due = today;  // practise it again today
    }
    r->ease += 0.1f - (5 - quality) * (0.08f + (5 - quality) * 0.02f);
    if (r->ease < 1.3f) r->ease = 1.3f;
    r->last = today;
    return Save();
}

ReviewFamiliarity ReviewFamiliarityOf(const char *id)
{
    const ReviewItem *r = Find(id);
    if (!r) return REVIEW_NEW;
    bool lastWasWrong = r->repetitions == 0;
    bool oftenWrong = r->wrong * 2 >= r->right + r->wrong && r->wrong > 0;
    return lastWasWrong || oftenWrong || r->ease < 2.0f ? REVIEW_STRUGGLING : REVIEW_KNOWN;
}

int ReviewDueCount(long today)
{
    int due = 0;
    for (int i = 0; i < itemCount; i++) due += items[i].due <= today;
    return due;
}

float ReviewNeed(const ReviewItem *item, long today)
{
    if (!item) return 30;  // new: worth seeing, after anything due
    float struggle = item->lapses * 5 + (2.5f - item->ease) * 20 + item->wrong * 2;
    if (item->due <= today) return 100 + (today - item->due) * 2 + struggle;
    if (item->repetitions == 0) return 50 + struggle;  // missed last time, not yet due again
    return 10 - (float)(item->due - today) + struggle * 0.5f;
}

float ReviewWeakness(const ReviewItem *item)
{
    if (!item) return 0.5f;
    // Laplace smoothing: one imagined right and one wrong answer, so a single miss isn't 100%.
    float missRate = (item->wrong + 1.0f) / (item->right + item->wrong + 2.0f);
    return missRate + (2.5f - item->ease) * 0.2f;
}
