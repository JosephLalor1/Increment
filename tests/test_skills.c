// Tests for skills.c: the catalogue, XP for right and wrong answers and what weighs it,
// levels, recording and saving progress, and weakness.
//
// usage: test_skills <throwaway folder>
#include "../src/skills.h"
#include "../src/skill_progress.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static SkillAnswer Untimed(int difficulty, int rightBefore, int wrongBefore)
{
    return (SkillAnswer){ difficulty, rightBefore, wrongBefore, -1 };
}

static void TestCatalogue(void)
{
    for (int s = 0; s < SKILL_COUNT; s++) {
        Skill back;
        assert(SkillName((Skill)s)[0] && SkillTitle((Skill)s)[0]);
        assert(SkillFromName(SkillName((Skill)s), -1, &back) && back == (Skill)s);
        assert(SkillGroupOf((Skill)s) < SKILL_GROUP_COUNT);
        for (int t = s + 1; t < SKILL_COUNT; t++) assert(strcmp(SkillName((Skill)s), SkillName((Skill)t)) != 0);
    }
    Skill skill;
    assert(SkillFromName("tracing:1", 7, &skill) && skill == SKILL_TRACING);  // a name inside longer text
    assert(!SkillFromName("pointer", -1, &skill));
    assert(SKILL_COUNT >= 25);
}

static void TestXp(void)
{
    // Right answers: weight x difficulty.
    float easy = SkillXpChange(true, 1, Untimed(1, 0, 0)), hard = SkillXpChange(true, 1, Untimed(3, 0, 0));
    assert(easy == 10 && hard == 30);
    assert(SkillXpChange(true, 0.3f, Untimed(1, 0, 0)) < easy);  // a marginal skill gets less
    // Beating a question you used to miss is worth more; repeating an easy win, less.
    assert(SkillXpChange(true, 1, Untimed(2, 0, 3)) > SkillXpChange(true, 1, Untimed(2, 0, 0)));
    assert(SkillXpChange(true, 1, Untimed(2, 4, 0)) < SkillXpChange(true, 1, Untimed(2, 0, 0)));
    // Against the clock: more, and more again for a quick answer.
    float quiz = SkillXpChange(true, 1, Untimed(2, 0, 0));
    float slow = SkillXpChange(true, 1, (SkillAnswer){ 2, 0, 0, 0 });
    float fast = SkillXpChange(true, 1, (SkillAnswer){ 2, 0, 0, 1 });
    assert(slow == quiz * 1.5f && fast == quiz * 2 && fast > slow);

    // Wrong answers lose XP: more on easy questions, more when you'd known it, less the first time.
    float wrongEasy = SkillXpChange(false, 1, Untimed(1, 1, 1)), wrongHard = SkillXpChange(false, 1, Untimed(3, 1, 1));
    assert(wrongEasy < wrongHard && wrongHard < 0);
    assert(SkillXpChange(false, 1, Untimed(2, 3, 0)) < SkillXpChange(false, 1, Untimed(2, 0, 1)));
    assert(SkillXpChange(false, 1, Untimed(2, 0, 0)) > SkillXpChange(false, 1, Untimed(2, 0, 1)));
    assert(SkillXpChange(false, 1, (SkillAnswer){ 2, 1, 0, 0.5f }) > SkillXpChange(false, 1, Untimed(2, 1, 0)));
    // Difficulty outside 1..3 is clamped.
    assert(SkillXpChange(true, 1, Untimed(9, 0, 0)) == hard);

    // Levels: 50, 150, 300, 500 XP.
    assert(SkillLevel(0) == 0 && SkillLevel(49.9f) == 0 && SkillLevel(50) == 1);
    assert(SkillLevel(149) == 1 && SkillLevel(150) == 2 && SkillLevel(300) == 3 && SkillLevel(499) == 3);
    assert(SkillXpForLevel(4) == 500 && SkillXpForLevel(0) == 0);
}

static void TestProgress(const char *folder)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/skills_test.txt", folder);
    remove(path);

    SkillsLoad("");  // signed out: nothing recorded
    SkillWeight both[] = { { SKILL_TRACING, 1 }, { SKILL_POINTERS, 0.3f } };
    assert(!SkillsRecord(both, 2, true, Untimed(2, 0, 0)));
    assert(SkillProgressOf(SKILL_TRACING)->xp == 0);

    SkillsLoad(path);
    assert(SkillWeakness(SKILL_TRACING) == 0.5f);  // untried
    assert(SkillsRecord(both, 2, true, Untimed(2, 0, 0)));
    const SkillProgress *tracing = SkillProgressOf(SKILL_TRACING), *pointers = SkillProgressOf(SKILL_POINTERS);
    assert(tracing->xp == 20 && tracing->right == 1 && pointers->xp > 5.9f && pointers->xp < 6.1f);
    assert(tracing->accuracy > 0.5f && pointers->accuracy > 0.5f && pointers->accuracy < tracing->accuracy);

    // XP never drops below zero.
    for (int i = 0; i < 20; i++) SkillsRecord(both, 2, false, Untimed(1, 2, 2));
    assert(pointers->xp == 0 && pointers->wrong == 20);
    assert(SkillWeakness(SKILL_POINTERS) > 0.8f);

    // A strong skill is less weak than a struggling one.
    SkillWeight strong[] = { { SKILL_LOOPS, 1 } };
    for (int i = 0; i < 30; i++) SkillsRecord(strong, 1, true, Untimed(3, 0, 1));
    assert(SkillLevel(SkillProgressOf(SKILL_LOOPS)->xp) >= 5);
    assert(SkillWeakness(SKILL_LOOPS) < 0.2f && SkillWeakness(SKILL_LOOPS) < SkillWeakness(SKILL_POINTERS));

    // Saved and loaded back.
    float loopsXp = SkillProgressOf(SKILL_LOOPS)->xp;
    SkillsLoad(path);
    assert(SkillProgressOf(SKILL_LOOPS)->xp > loopsXp - 0.01f && SkillProgressOf(SKILL_LOOPS)->right == 30);
    assert(SkillProgressOf(SKILL_POINTERS)->wrong == 20 && SkillProgressOf(SKILL_ARRAYS)->right == 0);

    SkillsLoad("");
    remove(path);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    TestCatalogue();
    TestXp();
    TestProgress(argv[1]);
    puts("skills: all tests passed");
    return 0;
}
