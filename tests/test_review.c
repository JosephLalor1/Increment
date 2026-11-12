// Tests for review.c: recording answers, the SM-2 schedule, saving and loading, and the
// need, weakness and familiarity measures used to choose and time practice.
//
// usage: test_review <throwaway folder>
#include "learning/review.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[1024];
    snprintf(path, sizeof path, "%s/review_test.txt", argv[1]);
    remove(path);

    // Signed out: nothing is recorded.
    ReviewLoad("");
    assert(!ReviewRecord("c_01?abc", true, 100) && ReviewCount() == 0);

    ReviewLoad(path);
    long today = 20000;
    assert(ReviewFind("q1") == NULL && ReviewFamiliarityOf("q1") == REVIEW_NEW);

    // Right answers: due in 1 day, then 3, then interval x ease.
    assert(ReviewRecord("q1", true, today));
    const ReviewItem *r = ReviewFind("q1");
    assert(r && r->right == 1 && r->repetitions == 1 && r->interval == 1 && r->due == today + 1);
    assert(r->ease > 2.49f && r->ease < 2.51f);  // grade 4 leaves the ease unchanged
    assert(ReviewRecord("q1", true, today + 1) && r->interval == 3 && r->due == today + 4);
    assert(ReviewRecord("q1", true, today + 4) && r->interval == 8 && r->due == today + 12);  // 3 x 2.5, rounded
    assert(ReviewFamiliarityOf("q1") == REVIEW_KNOWN);

    // A wrong answer: a lapse, due again today, a lower ease, and it counts as struggling.
    assert(ReviewRecord("q1", false, today + 12));
    assert(r->wrong == 1 && r->lapses == 1 && r->repetitions == 0 && r->due == today + 12 && r->ease < 2.0f);
    assert(ReviewFamiliarityOf("q1") == REVIEW_STRUGGLING);

    // The ease never drops below 1.3, and a miss on a new question isn't a lapse.
    for (int i = 0; i < 20; i++) ReviewRecord("q2", false, today);
    const ReviewItem *weak = ReviewFind("q2");
    assert(weak->ease >= 1.3f && weak->lapses == 0 && weak->wrong == 20);

    // Need: due and often-missed questions first, then new ones, then ones not yet due.
    assert(ReviewRecord("q3", true, today));  // due tomorrow
    float dueWeak = ReviewNeed(ReviewFind("q2"), today), fresh = ReviewNeed(NULL, today);
    float notDue = ReviewNeed(ReviewFind("q3"), today);
    assert(dueWeak > fresh && fresh > notDue);
    assert(ReviewNeed(ReviewFind("q3"), today + 5) > notDue);  // overdue later on
    // Weakness: always wrong is weaker than always right, and both stay in a sensible range.
    assert(ReviewWeakness(ReviewFind("q2")) > ReviewWeakness(ReviewFind("q3")));
    assert(ReviewWeakness(NULL) == 0.5f);

    assert(ReviewDueCount(today) == 1 && ReviewDueCount(today + 1) == 2);  // q2 now; q3 tomorrow (q1 later)

    // Saved straight away, and loaded back exactly.
    ReviewLoad(path);
    assert(ReviewCount() == 3);
    r = ReviewFind("q1");
    assert(r && r->right == 3 && r->wrong == 1 && r->lapses == 1 && r->due == today + 12);
    assert(ReviewAt(0) && !ReviewAt(3));

    // Ids that can't be stored are refused.
    char tooLong[REVIEW_ID_SIZE + 8];
    memset(tooLong, 'a', sizeof tooLong - 1);
    tooLong[sizeof tooLong - 1] = '\0';
    assert(!ReviewRecord(tooLong, true, today) && !ReviewRecord("", true, today));

    // Today is a plausible date (after 2024).
    assert(ReviewToday() > 19723);

    ReviewLoad("");
    remove(path);
    puts("review: all tests passed");
    return 0;
}
