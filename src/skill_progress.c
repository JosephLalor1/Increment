// The learner's XP and accuracy in each skill. See skill_progress.h.
#include "skill_progress.h"
#include "platform.h"

#include <math.h>
#include <stdio.h>

static SkillProgress progress[SKILL_COUNT];
static char savePath[600];  // "" while signed out

static void Reset(void)
{
    for (int s = 0; s < SKILL_COUNT; s++) progress[s] = (SkillProgress){ 0, 0, 0, 0.5f };
}

void SkillsLoad(const char *path)
{
    Reset();
    snprintf(savePath, sizeof savePath, "%s", path ? path : "");
    if (!savePath[0]) return;
    FILE *f = fopen(savePath, "r");
    if (!f) return;
    char name[64];
    SkillProgress p;
    while (fscanf(f, "%63s %f %d %d %f", name, &p.xp, &p.right, &p.wrong, &p.accuracy) == 5) {
        Skill skill;
        if (!SkillFromName(name, -1, &skill)) continue;  // a skill since renamed or removed
        if (p.xp < 0) p.xp = 0;
        p.accuracy = p.accuracy < 0 ? 0 : p.accuracy > 1 ? 1 : p.accuracy;
        progress[skill] = p;
    }
    fclose(f);
}

static bool Save(void)
{
    char temp[620];
    snprintf(temp, sizeof temp, "%s.tmp", savePath);
    FILE *f = fopen(temp, "w");
    if (!f) return false;
    for (int s = 0; s < SKILL_COUNT; s++) {
        const SkillProgress *p = &progress[s];
        if (p->right + p->wrong == 0) continue;
        fprintf(f, "%s %.2f %d %d %.4f\n", SkillName((Skill)s), p->xp, p->right, p->wrong, p->accuracy);
    }
    bool ok = fclose(f) == 0;
    return ok && PlatformReplaceFile(temp, savePath);
}

const SkillProgress *SkillProgressOf(Skill skill) { return skill < SKILL_COUNT ? &progress[skill] : NULL; }

bool SkillsRecord(const SkillWeight *skills, int count, bool correct, SkillAnswer answer)
{
    if (!savePath[0]) return false;
    for (int i = 0; i < count; i++) {
        if (skills[i].skill >= SKILL_COUNT || skills[i].weight <= 0) continue;
        SkillProgress *p = &progress[skills[i].skill];
        p->xp += SkillXpChange(correct, skills[i].weight, answer);
        if (p->xp < 0) p->xp = 0;
        if (correct) p->right++;
        else p->wrong++;
        // Recent accuracy: each answer pulls it towards 1 or 0, more strongly when the
        // question leans heavily on this skill.
        float pull = 0.2f * skills[i].weight;
        p->accuracy += ((correct ? 1.0f : 0.0f) - p->accuracy) * pull;
    }
    return Save();
}

float SkillWeakness(Skill skill)
{
    const SkillProgress *p = SkillProgressOf(skill);
    if (!p || p->right + p->wrong == 0) return 0.5f;
    float level = (float)SkillLevel(p->xp);
    return (1 - p->accuracy) * 0.7f + (1 - fminf(level, 10) / 10) * 0.3f;
}
