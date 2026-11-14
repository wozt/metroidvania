/* SPDX-License-Identifier: GPL-3.0-only */
/* Skills/souls/guardian cluster implementation — see aria_skills.h.
 * Moved verbatim from aria_runtime.c; the only edits are the linkage
 * changes (entry points and the shared homing/camera state are no
 * longer static) noted at each definition. */
#include "aria_skills.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defined in aria_runtime.c (also declared in aria_draw.h for
 * blade_frame): the still-frame lookup the cast guard and the
 * frame loop's skill draw share. */
int static_index(const int *ids, int count, int id);

/* sub_08031EA4(entity, source, dx, dy): the source's facing and whole
 * position (only + 0x42 and + 0x46 are written; a fresh entity's fractions
 * are 0), dx mirrored when it faces left. */
static void place(AriaSkill *skill, int32_t x, int32_t y, bool facing_left, int dx, int dy) {
    skill->facing_left = facing_left;
    skill->x = (int32_t)((uint32_t)(int16_t)((x >> 16) + (facing_left ? -dx : dx)) << 16);
    skill->y = (int32_t)((uint32_t)(int16_t)((y >> 16) + dy) << 16);
}

static AriaSkill *spawn_child(AriaSouls *souls, int kind);

/* Soma's cast hook: sub_08019478's guards and bookkeeping, then the skill
 * entity; souls whose routine is not ported are refused rather than
 * spending MP on nothing. */
int cast_red_soul(void *context) {
    AriaSouls *souls = context;
    if (!souls->records || souls->stats->red_soul >= AOS_RED_SOUL_RECORDS) return -1;
    const AosRedSoulRecord *record = &souls->records[souls->stats->red_soul];
    uint32_t routine = record->skill;
    /* The shared throw routine is followed when all of the selected record's
     * flags and its resource-specific still frame are available. */
    const AosThrowRecord *throw_record = souls->throws && record->parameter < AOS_THROW_RECORDS
                                             ? &souls->throws[record->parameter]
                                             : NULL;
    bool throw_art = throw_record &&
                     ((throw_record->resource == 0 &&
                       static_index(souls->shared_ids, souls->shared_count,
                                    throw_record->frame) >= 0) ||
                      (throw_record->resource == 1 &&
                       static_index(souls->ripper_ids, souls->ripper_count,
                                    throw_record->frame) >= 0) ||
                      (throw_record->resource == 7 && souls->bone &&
                       static_index(souls->bone_ids, souls->bone_count,
                                    throw_record->frame) >= 0) ||
                      (throw_record->resource == 18 && souls->archer_ids &&
                       static_index(souls->archer_ids, souls->archer_count,
                                    throw_record->frame) >= 0));
    bool throwable = routine == SKILL_THROW_ROUTINE && throw_record &&
                     aos_throw_supported(throw_record) && throw_art;
    int kind = routine == SKILL_BAT_ROUTINE && souls->bat      ? SKILL_BAT
               : routine == SKILL_ARCHER_ROUTINE && souls->bow ? SKILL_BOW
               : throwable                                     ? SKILL_THROW
               : routine == SKILL_MERMAN_ROUTINE && souls->merman ? SKILL_SPOUT
               : routine == SKILL_AXE_ROUTINE &&
                       static_index(souls->shared_ids, souls->shared_count, record->parameter) >= 0
                   ? SKILL_AXE
               : routine == SKILL_SPEAR_ROUTINE &&
                       static_index(souls->shared_ids, souls->shared_count, 0) >= 0
                   ? SKILL_SPEAR
               : routine == SKILL_DURGA_ROUTINE && souls->durga ? SKILL_SHOT
               : routine == SKILL_HARPY_ROUTINE && souls->harpy ? SKILL_HARPY
               : routine == SKILL_DEVIL_ROUTINE && souls->devil ? SKILL_DEVIL
               : routine == SKILL_CROW_ROUTINE && souls->crow   ? SKILL_CROW
               : routine == SKILL_FLEA_ROUTINE && souls->flea   ? SKILL_FLEA
               : routine == SKILL_GHOST_ROUTINE && souls->devil &&
                         souls->devil->count > AOS_GHOST_SEQUENCE
                   ? SKILL_GHOST
               : routine == SKILL_SIREN_ROUTINE &&
                         static_index(souls->shared_ids, souls->shared_count, 2) >= 0 &&
                         static_index(souls->shared_ids, souls->shared_count, 3) >= 0
                   ? SKILL_SIREN
               : routine == SKILL_SKULL_ROUTINE && souls->skull ? SKILL_SKULL
               : routine == SKILL_LIGHTNING_ROUTINE && souls->lightning ? SKILL_LIGHTNING
               : routine == SKILL_KILLER_DOLL_ROUTINE && souls->killer_doll ? SKILL_DECOY
               : routine == SKILL_NEMESIS_ROUTINE && souls->devil &&
                         souls->devil->count > AOS_NEMESIS_SOUL_SEQUENCE
                   ? SKILL_NEMESIS
               : routine == SKILL_VALKYRIE_ROUTINE && souls->valkyrie ? SKILL_VALKYRIE
               : routine == SKILL_NEEDLES_ROUTINE && souls->needles ? SKILL_NEEDLES
               : routine == SKILL_RIPPER_ROUTINE && souls->ripper && record->parameter < 2
                   ? SKILL_RIPPER
               : routine == SKILL_MILLIONE_ROUTINE && souls->millione
                   ? SKILL_MILLIONE_EMITTER
               : routine == SKILL_FISH_HEAD_ROUTINE && souls->fish_head
                   ? SKILL_FISH_HEAD
               : routine == SKILL_KILLER_FISH_ROUTINE && souls->killer_fish
                   ? SKILL_KILLER_FISH
               : routine == SKILL_GLADIATOR_ROUTINE && souls->gladiator &&
                         souls->gladiator_kind && souls->gladiator_kind->probes.count
                   ? SKILL_GLADIATOR
               : routine == SKILL_WEREWOLF_ROUTINE && souls->werewolf &&
                         souls->werewolf_kind && souls->werewolf_kind->probes.count
                   ? SKILL_WEREWOLF
               : routine == SKILL_KILLER_MANTLE_ROUTINE && souls->killer_mantle
                   ? SKILL_KILLER_MANTLE
               : routine == SKILL_WERE_BEAST_ROUTINE && souls->were_beast
                   ? SKILL_WERE_BEAST
               : routine == SKILL_FLAME_DEMON_ROUTINE && souls->fish_head
                   ? SKILL_FLAME_DEMON
               : routine == SKILL_DEMON_LORD_ROUTINE && souls->demon_lord
                   ? SKILL_DEMON_LORD
               : routine == SKILL_BIPHRON_ROUTINE && souls->biphron &&
                         souls->biphron_kind && souls->biphron_kind->probes.count
                   ? SKILL_BIPHRON
               : routine == SKILL_BALORE_ROUTINE && souls->balore
                   ? SKILL_BALORE
               : routine == SKILL_KYOMA_DEMON_ROUTINE && souls->kyoma_demon
                   ? SKILL_KYOMA_DEMON
               : routine == SKILL_LEGION_ROUTINE && souls->legion
                   ? SKILL_LEGION
               : routine == SKILL_STUDENT_WITCH_ROUTINE && souls->student_witch &&
                         souls->witch_kind && souls->witch_kind->probes.count
                   ? SKILL_STUDENT_WITCH
               : routine == SKILL_SLIME_ROUTINE && souls->slime
                   ? SKILL_SLIME
               : routine == SKILL_UKOBACK_ROUTINE && souls->ukoback
                   ? SKILL_UKOBACK
               : routine == SKILL_ALTAIR_ROUTINE && souls->altair
                   ? SKILL_ALTAIR
               : routine == SKILL_UNE_ROUTINE && souls->une && souls->une_kind &&
                         souls->une_kind->probes.count
                   ? SKILL_UNE
               : routine == SKILL_MANDRAGORA_ROUTINE && souls->mandragora
                   ? SKILL_MANDRAGORA
               : routine == SKILL_CHRONOMAGE_ROUTINE && souls->chronomage
                   ? SKILL_CHRONOMAGE
               : routine == SKILL_DISC_ARMOR_ROUTINE && souls->disc_armor
                   ? SKILL_DISC_ARMOR
               : routine == SKILL_COCKATRICE_ROUTINE && souls->cockatrice
                   ? SKILL_COCKATRICE
               : routine == SKILL_DRYAD_ROUTINE && souls->dryad
                   ? SKILL_DRYAD
               : routine == SKILL_ARACHNE_ROUTINE && record->parameter == 6 &&
                         souls->arachne
                   ? SKILL_ARACHNE
               : routine == SKILL_ARACHNE_ROUTINE && record->parameter == 7 &&
                         souls->fish_head
                   ? SKILL_MUDMAN
               : routine == SKILL_RED_MINOTAUR_ROUTINE && souls->red_minotaur
                   ? SKILL_RED_MINOTAUR
               : routine == SKILL_NIGHTMARE_ROUTINE && souls->nightmare
                   ? SKILL_NIGHTMARE
               : routine == SKILL_MAN_EATER_ROUTINE && souls->man_eater
                   ? SKILL_MAN_EATER
                                                                : -1;
    if (kind < 0) return -1;
    int slot = aos_red_soul_try(&souls->caster, souls->stats, souls->records,
                                (souls->stats->challenge_flags & 0x200000u) != 0);
    if (slot < 0) return -1;
    AriaSkill *skill = &souls->skills[slot];
    *skill = (AriaSkill){.alive = true, .kind = kind, .slot = slot,
                         .soul = souls->stats->red_soul};
    /* SkillBatUse: (0, -0x1A); SkillSkullArcherUse: (-0xC, -0x18). */
    if (kind == SKILL_BAT)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x1A);
    else if (kind == SKILL_NEEDLES) {
        skill->needles = aos_needles_create(souls->soma->x, souls->soma->y,
                                            souls->soma->facing_left);
        skill->x = skill->needles.x;
        skill->y = skill->needles.y;
        skill->facing_left = skill->needles.facing_left;
    } else if (kind == SKILL_RIPPER) {
        skill->parameter = (uint8_t)record->parameter;
        /* Both configuration entries have byte +15 clear and therefore
         * enter the rebound branch after a wall correction. */
        skill->ripper = aos_ripper_soul_create(souls->soma->x, souls->soma->y,
                                               souls->soma->facing_left,
                                               skill->parameter, true);
        skill->x = skill->ripper.x;
        skill->y = skill->ripper.y;
        skill->facing_left = skill->ripper.facing_left;
    } else if (kind == SKILL_MILLIONE_EMITTER) {
        skill->millione = aos_millione_soul_create();
    } else if (kind == SKILL_FISH_HEAD) {
        skill->fish_head = aos_fish_head_soul_create(souls->soma->x, souls->soma->y,
                                                     souls->soma->facing_left);
        skill->x = skill->fish_head.x;
        skill->y = skill->fish_head.y;
        skill->facing_left = skill->fish_head.facing_left;
    } else if (kind == SKILL_KILLER_FISH) {
        skill->killer_fish = aos_killer_fish_soul_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->killer_fish.x;
        skill->y = skill->killer_fish.y;
        skill->facing_left = skill->killer_fish.facing_left;
    } else if (kind == SKILL_GLADIATOR) {
        skill->gladiator = aos_gladiator_soul_create(souls->soma->x, souls->soma->y,
                                                     souls->soma->facing_left);
        skill->x = skill->gladiator.body.x;
        skill->y = skill->gladiator.body.y;
        skill->facing_left = skill->gladiator.facing_left;
    } else if (kind == SKILL_VALKYRIE) {
        skill->valkyrie = aos_valkyrie_create(souls->soma->x, souls->soma->y,
                                              souls->soma->facing_left);
        skill->x = skill->valkyrie.x;
        skill->y = skill->valkyrie.y;
        skill->facing_left = skill->valkyrie.facing_left;
    } else if (kind == SKILL_SKULL) {
        skill->skull = aos_skull_create(souls->soma->facing_left);
        skill->facing_left = skill->skull.facing_left;
    } else if (kind == SKILL_DECOY) {
        skill->decoy = aos_decoy_create(souls->soma->x, souls->soma->y,
                                        souls->soma->facing_left);
        skill->x = skill->decoy.x;
        skill->y = skill->decoy.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_NEMESIS) {
        skill->nemesis = aos_nemesis_soul_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -8);
    } else if (kind == SKILL_KILLER_MANTLE) {
        skill->killer_mantle = aos_killer_mantle_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_WERE_BEAST) {
        skill->were_beast = aos_were_beast_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_FLAME_DEMON) {
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
              -8, -0x10);    /* sub_08031EA4(sb, 8, -0x10) */
    } else if (kind == SKILL_DEMON_LORD) {
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
              8, -0x10);     /* sub_08031EA4(sb, 8, -0x10) */
    } else if (kind == SKILL_FLAME_BOLT) {
        /* The parent's fan sets the velocity; see update_skills. */
    } else if (kind == SKILL_FLAME_SPIRIT) {
        skill->flame = aos_flame_spirit_create(skill->x, skill->y,
                                               skill->facing_left);
    } else if (kind == SKILL_WEREWOLF) {
        skill->werewolf = aos_werewolf_create(souls->soma->x, souls->soma->y,
                                              souls->soma->facing_left);
        skill->x = skill->werewolf.body.x;
        skill->y = skill->werewolf.body.y;
        skill->facing_left = skill->werewolf.body.mirrored;
    } else if (kind == SKILL_BIPHRON) {
        skill->biphron = aos_biphron_create(souls->soma->x, souls->soma->y,
                                            souls->soma->facing_left);
        skill->x = skill->biphron.body.x;
        skill->y = skill->biphron.body.y;
        skill->facing_left = skill->biphron.body.mirrored;
    } else if (kind == SKILL_BALORE) {
        skill->were_beast = aos_were_beast_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_KYOMA_DEMON) {
        skill->kyoma_demon = aos_kyoma_demon_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_LEGION) {
        skill->were_beast = aos_were_beast_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_STUDENT_WITCH) {
        skill->student_witch = aos_student_witch_create(souls->soma->x, souls->soma->y,
                                                        souls->soma->facing_left);
        skill->x = skill->student_witch.body.x;
        skill->y = skill->student_witch.body.y;
        skill->facing_left = skill->student_witch.body.mirrored;
    } else if (kind == SKILL_COCKATRICE) {
        /* No sub_08031EA4 in the native init: the + 0x4A = -+8 /
         * + 0x4E = -+0x1A fields fed the partner registration, so the
         * entity keeps the cast position offset like the other souls. */
        skill->cockatrice = aos_cockatrice_create(
            souls->soma->x + (souls->soma->facing_left ? -8 : 8) * 0x10000,
            souls->soma->y - 0x1A * 0x10000, souls->soma->facing_left);
        skill->x = skill->cockatrice.x;
        skill->y = skill->cockatrice.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_DRYAD) {
        /* SkillDryadUse's state 0 places it through sub_08031EA4 at
         * (0x14, -0x1A) with vx -+0x10000, vy -0x20000 and gravity
         * 0x2000 already loaded. */
        skill->dryad = aos_dryad_create(souls->soma->x, souls->soma->y,
                                        souls->soma->facing_left);
        skill->x = skill->dryad.x;
        skill->y = skill->dryad.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_ARACHNE || kind == SKILL_MUDMAN) {
        /* SkillArachneUse state 0: sub_08031EA4(source, 8, -0x1A), the
         * record's vx/vy/gravity, box preset 5 + sub_0802DB84, song
         * record +0x10 when nonzero (0xD1 arachne, none mudman). */
        const AosThrowRecord *t = &souls->throws[record->parameter];
        skill->arachne = aos_arachne_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left,
            (uint8_t)record->parameter, (t->flags & 0x100) != 0,
            t->vx, t->vy, t->gravity);
        skill->x = skill->arachne.x;
        skill->y = skill->arachne.y;
        skill->facing_left = souls->soma->facing_left;
        skill->shard_parent = -1;
    } else if (kind == SKILL_RED_MINOTAUR) {
        /* SkillRedMinotaurUse state 0: sub_08031EA4(source, 0, -0x1A),
         * facing-signed spin fields, charge 0, timer 0x50, songs
         * 0x1A0 and 0xCF; the sub_08023530 partner registration is the
         * blade child spawned below. */
        skill->red_minotaur = aos_red_minotaur_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->red_minotaur.x;
        skill->y = skill->red_minotaur.y;
        skill->facing_left = souls->soma->facing_left;
        AriaSkill *blade = spawn_child(souls, SKILL_MINOTAUR_BLADE);
        if (blade) {
            blade->soul = skill->soul;
            blade->shard_parent = skill->slot;
            blade->facing_left = skill->facing_left;
            blade->minotaur_blade = (AosMinotaurBlade){0};
            aos_minotaur_blade_init(&blade->minotaur_blade);
        }
    } else if (kind == SKILL_NIGHTMARE) {
        /* SkillNightmareUse state 0: sub_08031EA4(source, 8, 0),
         * facing-signed vx -+0x4000 / ax -+0x2000 with the + 0x58 bit 6
         * inverted, charge 0, state 1 on the same frame. */
        skill->nightmare = aos_nightmare_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->nightmare.x;
        skill->y = skill->nightmare.y;
        skill->facing_left = skill->nightmare.facing_left;
    } else if (kind == SKILL_MAN_EATER) {
        /* SkillManEaterUse state 0: sub_08031EA4(source, 8, -0x1A),
         * the rng % 3 variant, vx -+0x12000 by the facing, state 1;
         * song 0x15E is played on the first update. */
        skill->man_eater = aos_man_eater_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left, aos_random);
        skill->x = skill->man_eater.x;
        skill->y = skill->man_eater.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_DISC_ARMOR) {
        skill->disc_armor = aos_disc_armor_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->disc_armor.x;
        skill->y = skill->disc_armor.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_CHRONOMAGE) {
        /* The native copies the source + 0x1E into the global + 0x4BE
         * hold field; the soul record's value is not decoded here, so
         * the runtime passes a documented 0x100-frame hold. */
        skill->chronomage = aos_chronomage_create(0x100);
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
              0, -0x30);
    } else if (kind == SKILL_MANDRAGORA) {
        skill->mandragora = aos_mandragora_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
              0x10, -0x1A);
    } else if (kind == SKILL_UNE) {
        skill->une = aos_une_create(souls->soma->x, souls->soma->y,
                                    souls->soma->facing_left);
        skill->x = skill->une.body.x;
        skill->y = skill->une.body.y;
        skill->facing_left = skill->une.body.mirrored;
    } else if (kind == SKILL_ALTAIR) {
        skill->altair = aos_altair_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
              0x14, -0x1A);
    } else if (kind == SKILL_UKOBACK) {
        skill->ukoback = aos_ukoback_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
    } else if (kind == SKILL_SLIME) {
        skill->slime = aos_slime_create(souls->soma->x, souls->soma->y,
                                        souls->soma->facing_left, aos_random());
        skill->x = skill->slime.body.x;
        skill->y = skill->slime.body.y;
        skill->facing_left = skill->slime.body.mirrored;
    } else if (kind == SKILL_LIGHTNING) {
        skill->lightning = aos_lightning_create(souls->soma->x, souls->soma->y,
                                                souls->soma->facing_left, aos_random() & 1);
        skill->x = skill->lightning.x;
        skill->y = skill->lightning.y;
        skill->facing_left = skill->lightning.facing_left;
    }
    else if (kind == SKILL_GHOST)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x18);
    else if (kind == SKILL_SIREN) {
        /* SkillSirenUse consumes its two random draws on the first update. */
    }
    else if (kind == SKILL_FLEA) {
        skill->flea = aos_flea_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->flea.body.x;
        skill->y = skill->flea.body.y;
        skill->facing_left = skill->flea.body.mirrored;
    } else if (kind == SKILL_CROW) {
        skill->crow = aos_crow_create(souls->soma->facing_left);
        skill->facing_left = skill->crow.facing_left;
    } else if (kind == SKILL_DEVIL)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 8, -0x1A);
    else if (kind == SKILL_HARPY)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0x14, -0x1A);
    else if (kind == SKILL_SHOT) {
        skill->shot = aos_shot_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->shot.x;
        skill->y = skill->shot.y;
        skill->facing_left = skill->shot.facing_left;
    } else if (kind == SKILL_SPEAR) {
        skill->spear = aos_spear_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->spear.x;
        skill->y = skill->spear.y;
        skill->facing_left = skill->spear.facing_left;
    } else if (kind == SKILL_AXE) {
        skill->parameter = (uint8_t)record->parameter;
        skill->axe = aos_axe_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->axe.x;
        skill->y = skill->axe.y;
        skill->facing_left = false;
    } else if (kind == SKILL_SPOUT)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 6, -0x1A);
    else if (kind == SKILL_THROW) {
        skill->parameter = (uint8_t)record->parameter;
        skill->thrown = aos_throw_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        /* Flag 0x200: the still frame's own attack box (resource 18's
         * static_attack rows, indexed by the record's frame id). */
        if (souls->throws && record->parameter < AOS_THROW_RECORDS &&
            souls->throws[record->parameter].resource == 18 &&
            souls->throws[record->parameter].frame < 64 && souls->archer_attacks)
            skill->thrown.frame_attack =
                souls->archer_attacks[souls->throws[record->parameter].frame];
    } else
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, -0xC, -0x18);
    printf("Red soul %u cast (MP %d)\n", souls->stats->red_soul, souls->stats->mp);
    return souls->records[souls->stats->red_soul].pose;
}

/* sub_08023530: a child skill entity in a free slot, counted (0x131BE). */
static AriaSkill *spawn_child(AriaSouls *souls, int kind) {
    for (int i = 0; i < MAX_SKILLS; ++i) {
        if (souls->caster.slots[i]) continue;
        souls->caster.slots[i] = true;
        souls->caster.active++;
        AriaSkill *child = &souls->skills[i];
        *child = (AriaSkill){.alive = true, .started = false, .kind = kind, .slot = i};
        return child;
    }
    return NULL;
}

static void end_skill(AriaSouls *souls, AriaSkill *skill) {
    aos_red_soul_release(&souls->caster, skill->slot);
    skill->alive = false;
}

/* sub_0806D128(e, n) on the camera. */
static bool skill_outside(const AriaSouls *souls, const AriaSkill *skill, int n) {
    int mx = souls->margins ? souls->margins[n][0] : 16, my = souls->margins ? souls->margins[n][1] : 16;
    int sx = (skill->x >> 16) - souls->cam_x, sy = (skill->y >> 16) - souls->cam_y;
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}

/* Soma's guardian hook: sub_0802E0C4 for the souls whose routine is
 * ported; the others are refused rather than draining MP for nothing. */
void start_guardian(void *context) {
    AriaGuardian *guardian = context;
    const AosStats *stats = guardian->stats;
    if (stats->blue_soul >= AOS_BLUE_SOUL_RECORDS) return;
    uint32_t routine = guardian->records[stats->blue_soul].routine;
    if (routine != GUARDIAN_MEDUSA_ROUTINE && routine != GUARDIAN_PANTHER_ROUTINE &&
        routine != GUARDIAN_GIANT_BAT_ROUTINE &&
        !(routine == GUARDIAN_CAGNAZZO_ROUTINE && guardian->cagnazzo_art) &&
        !(routine == GUARDIAN_FLYING_ARMOR_ROUTINE && guardian->flying_armor_art) &&
        !(routine == GUARDIAN_DEATH_ROUTINE && guardian->death_art) &&
        !(routine == GUARDIAN_PERSEPHONE_ROUTINE && guardian->persephone_art) &&
        !(routine == GUARDIAN_GREAT_ARMOR_ROUTINE && guardian->great_armor_art) &&
        !(routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE && guardian->shadow_knight_art) &&
        !(routine == GUARDIAN_ALURA_UNE_ROUTINE && guardian->alura_une_art) &&
        !(routine == GUARDIAN_BIG_GOLEM_ROUTINE && guardian->big_golem_art) &&
        !(routine == GUARDIAN_SKY_FISH_ROUTINE && guardian->sky_fish_art) &&
        !(routine == GUARDIAN_CREAKING_SKULL_ROUTINE && guardian->creaking_skull_art) &&
        !(routine == GUARDIAN_ALASTOR_ROUTINE && guardian->alastor_art) &&
        !(routine == GUARDIAN_FINAL_GUARD_ROUTINE && guardian->final_guard_art) &&
        !(routine == GUARDIAN_BUER_ROUTINE && guardian->buer_art) &&
        !(routine == GUARDIAN_CATOBLEPAS_ROUTINE && guardian->catoblepas_art) &&
        !(routine == GUARDIAN_PILLAR_ROUTINE && guardian->pillar_art) &&
        !(routine == GUARDIAN_IMP_ROUTINE && guardian->imp_art) &&
        !(routine == GUARDIAN_GHOST_ROUTINE &&
          (guardian->records[stats->blue_soul].argument ? guardian->ghost_art
                                                        : guardian->witch_art)) &&
        !(routine == GUARDIAN_TRANSFORM_ROUTINE &&
          (guardian->records[stats->blue_soul].argument == AOS_CURLY_VARIANT
               ? guardian->curly_art != NULL
               : guardian->records[stats->blue_soul].argument < AOS_TRANSFORM_VARIANTS &&
                     guardian->spark_art != NULL)))
        return;
    /* sub_0802E0C4: NOSOUL (unk_33C[2] & 0x200000) limits the guardian to
     * blue souls 1-3 (+0x1326A <= 3). */
    if (aos_guardian_try(&guardian->core, stats, guardian->records, guardian->soma->held,
                         guardian->soma->pressed,
                         (stats->challenge_flags & 0x200000u) && stats->blue_soul > 3,
                         &guardian->soma->flags)) {
        guardian->routine = routine;
        guardian->medusa = (AosMedusaGuardian){0};
        guardian->cagnazzo = (AosCagnazzoGuardian){0};
        guardian->buer = (AosBuerGuardian){0};
        guardian->catoblepas = (AosCatoblepasGuardian){0};
        guardian->imp = (AosImpGuardian){.target = -1};
        guardian->curly = (AosCurlyGuardian){0};
        for (int k = 0; k < 7; ++k) guardian->sparks[k].alive = false;
        guardian->ghost = (AosGhostGuardian){0};
        guardian->panther = (AosPantherGuardian){0};
        guardian->flying_armor = (AosFlyingArmorGuardian){0};
        guardian->death = (AosDeathGuardian){0};
        guardian->persephone = (AosPersephoneGuardian){0};
        guardian->great_armor = (AosGreatArmorGuardian){0};
        guardian->shadow_knight = (AosShadowKnightGuardian){0};
        guardian->alura_une = (AosAluraUneGuardian){0};
        guardian->sky_fish = (AosSkyFishGuardian){0};
        guardian->creaking_skull = (AosCreakingSkullGuardian){0};
        guardian->alastor = (AosAlastorGuardian){.target = -1};
        guardian->giant_bat = (AosGiantBatGuardian){0};
        guardian->final_guard = (AosFinalGuardGuardian){0};
        guardian->anim_ended = false;
        guardian->imp_target = -1;
    }
}

/* sub_0802DFF0: the transformed body met a ceiling; the guardian, its
 * child and 0x13260 bits 8-13 go, and the held sound 0x9C is released. */
void cancel_transform(void *context) {
    AriaGuardian *guardian = context;
    if (!guardian || !guardian->core.active || guardian->routine != GUARDIAN_TRANSFORM_ROUTINE)
        return;
    aos_audio_release(0x9C);
    aos_curly_guardian_end(&guardian->curly, &guardian->core, &guardian->soma->flags,
                           &guardian->stats->flags);
}

/* The camera, for the ghost's spirits' screen margin. */
int guardian_cam_x, guardian_cam_y;

/* The guardian entity's update, after Soma's in the entity order. */
void update_guardian(AriaGuardian *guardian) {
    if (!guardian->core.active) return;
    AosSoma *soma = guardian->soma;
    if (guardian->routine == GUARDIAN_MEDUSA_ROUTINE) {
        aos_medusa_guardian_update(&guardian->medusa, &guardian->core, guardian->stats,
                                   guardian->records, soma->held, soma->pressed, &soma->flags,
                                   &soma->vy, &soma->gravity_mod, &soma->hold_still);
    } else if (guardian->routine == GUARDIAN_FLYING_ARMOR_ROUTINE) {
        /* Its looping sequence 0 starts on the first update and steps. */
        bool first = guardian->core.state == 0;
        if (!aos_flying_armor_guardian_update(&guardian->flying_armor, &guardian->core,
                                              guardian->stats, guardian->records, soma->held,
                                              soma->pressed, &soma->flags, soma->x >> 16,
                                              soma->y >> 16, soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->flying_armor_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->flying_armor_art);
    } else if (guardian->routine == GUARDIAN_ALURA_UNE_ROUTINE) {
        bool first = guardian->core.state == 0;
        if (!aos_alura_une_guardian_update(&guardian->alura_une, &guardian->core,
                                           guardian->stats, guardian->records, soma->held,
                                           soma->pressed, &soma->flags, soma->x >> 16,
                                           soma->y >> 16, soma->facing_left, &soma->hp,
                                           soma->max_hp, aos_random))
            return;
        /* Its looping sequence 0, started on creation (no step: it is drawn
         * through its affine slot only). */
        if (first) aos_anim_start(&guardian->anim, guardian->alura_une_art, 0, true);
    } else if (guardian->routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE ||
               guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE) {
        bool golem = guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE;
        const AosAnimSet *art = golem ? guardian->big_golem_art : guardian->shadow_knight_art;
        if (golem ? !aos_big_golem_guardian_update(&guardian->shadow_knight, &guardian->core,
                                                   guardian->stats, guardian->records,
                                                   soma->held, soma->pressed, &soma->flags,
                                                   soma->x >> 16, soma->y >> 16,
                                                   soma->facing_left, soma->weapon_active,
                                                   guardian->anim_ended, guardian->anim.frame,
                                                   guardian->anim.tick)
                  : !aos_shadow_knight_guardian_update(&guardian->shadow_knight, &guardian->core,
                                                       guardian->stats, guardian->records,
                                                       soma->held, soma->pressed, &soma->flags,
                                                       soma->x >> 16, soma->y >> 16,
                                                       soma->facing_left, soma->weapon_active,
                                                       guardian->anim_ended))
            return;
        int seq = guardian->shadow_knight.sequence;
        /* The idle sequence loops (1 for the knight, 0 for the golem). */
        if (seq >= 0) aos_anim_start(&guardian->anim, art, (unsigned)seq, seq == (golem ? 0 : 1));
        /* sub_0802E1D0: the step (and the frame's attack box). */
        guardian->anim_ended = aos_anim_step(&guardian->anim, art) == 3;
    } else if (guardian->routine == GUARDIAN_GREAT_ARMOR_ROUTINE) {
        /* Its looping sequence 1 starts on the first update and steps. */
        bool first = guardian->core.state == 0;
        if (!aos_great_armor_guardian_update(&guardian->great_armor, &guardian->core,
                                             guardian->stats, guardian->records, soma->held,
                                             soma->pressed, &soma->flags, soma->x >> 16,
                                             soma->y >> 16, soma->facing_left, aos_random))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->great_armor_art, 1, true);
        aos_anim_step(&guardian->anim, guardian->great_armor_art);
        if (guardian->great_armor.spark && guardian->effect_count < 2)
            guardian->effects[guardian->effect_count++] = (AosEffect){
                .kind = AOS_EFFECT_SPARK_ROW7, .drift = true, .x = guardian->great_armor.spark_x,
                .y = guardian->great_armor.spark_y};
    } else if (guardian->routine == GUARDIAN_PERSEPHONE_ROUTINE) {
        bool first = guardian->core.state == 0;
        if (!aos_persephone_guardian_update(&guardian->persephone, &guardian->core,
                                            guardian->stats, guardian->records, soma->held,
                                            soma->pressed, &soma->flags, soma->x >> 16,
                                            soma->y >> 16, soma->facing_left))
            return;
        /* sub_0802E1D0 (not while leaving): the looping sequence 0. */
        if (first) aos_anim_start(&guardian->anim, guardian->persephone_art, 0, true);
        if (guardian->core.state != AOS_PERSEPHONE_LEAVING)
            aos_anim_step(&guardian->anim, guardian->persephone_art);
    } else if (guardian->routine == GUARDIAN_DEATH_ROUTINE) {
        AosSkillTarget targets[AOS_DEATH_TARGETS] = {{0}};
        for (int slot = 0; slot < AOS_DEATH_TARGETS; ++slot) {
            int index = homing.index[slot];
            if (index < 0 || !homing_room || (size_t)index >= homing_room->entity_count) continue;
            const AriaEntity *entity = &homing_room->entities[index];
            targets[slot] = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                             entity->enemy.x, entity->enemy.y, entity->enemy.hp};
        }
        int mx = guardian->margins ? guardian->margins[1][0] : 16;
        int my = guardian->margins ? guardian->margins[1][1] : 16;
        aos_death_guardian_update(&guardian->death, &guardian->core, guardian->stats,
                                  guardian->records, soma->held, soma->pressed, &soma->flags,
                                  soma->x >> 16, soma->y >> 16, soma->facing_left, targets,
                                  guardian->death_art, guardian_cam_x, guardian_cam_y, mx, my,
                                  aos_random);
    } else if (guardian->routine == GUARDIAN_PANTHER_ROUTINE) {
        aos_panther_guardian_update(&guardian->panther, &guardian->core, guardian->stats,
                                    guardian->records, soma->held, soma->pressed, &soma->flags,
                                    soma->x >> 16, soma->y >> 16, soma->facing_left,
                                    soma->anim.id);
    } else if (guardian->routine == GUARDIAN_CAGNAZZO_ROUTINE) {
        /* Its looping sequence 0 starts on the first update and steps at
         * its end (sub_0803F17C); sub_08042A54(2, 0x14) is not run. */
        bool first = guardian->core.state == 0;
        if (!aos_cagnazzo_guardian_update(&guardian->cagnazzo, &guardian->core, guardian->stats,
                                          guardian->records, soma->held, soma->pressed,
                                          &soma->flags, soma->x >> 16, soma->y >> 16,
                                          soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->cagnazzo_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->cagnazzo_art);
    } else if (guardian->routine == GUARDIAN_BUER_ROUTINE) {
        /* The flames start looping sequence 0 together and step at the end
         * of each update (sub_08042A54(2, 0x18) is not run). */
        bool first = guardian->core.state == 0;
        if (!aos_buer_guardian_update(&guardian->buer, &guardian->core, guardian->stats,
                                      guardian->records, soma->held, soma->pressed,
                                      &soma->flags, soma->x >> 16, soma->y >> 16,
                                      soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->buer_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->buer_art);
    } else if (guardian->routine == GUARDIAN_CATOBLEPAS_ROUTINE ||
               guardian->routine == GUARDIAN_PILLAR_ROUTINE) {
        bool pose = false;
        bool pillar = guardian->routine == GUARDIAN_PILLAR_ROUTINE;
        if (pillar ? !aos_pillar_guardian_update(&guardian->catoblepas, &guardian->core,
                                                 guardian->stats, guardian->records,
                                                 soma->held, soma->pressed, &soma->flags,
                                                 soma->x >> 16, soma->y >> 16,
                                                 soma->facing_left, soma->vx,
                                                 &soma->hold_still, &pose, aos_random)
                   : !aos_catoblepas_guardian_update(&guardian->catoblepas, &guardian->core,
                                                     guardian->stats, guardian->records,
                                                     soma->held, soma->pressed, &soma->flags,
                                                     soma->x >> 16, soma->y >> 16,
                                                     soma->facing_left, soma->vx,
                                                     &soma->hold_still, &pose))
            return;
        if (pose) aos_soma_guardian_pose(soma, AOS_CATOBLEPAS_POSE);
        const AosAnimSet *gas_art = pillar ? guardian->pillar_art : guardian->catoblepas_art;
        /* Each cloud plays sequence 0 once from its first update. */
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
            AosCatoblepasGas *gas = &guardian->catoblepas.gases[k];
            if (!gas->alive) continue;
            if (!gas->started) {
                gas->started = true;
                aos_anim_start(&guardian->gas_anims[k], gas_art, 0, false);
            }
            aos_anim_step(&guardian->gas_anims[k], gas_art);
        }
    } else if (guardian->routine == GUARDIAN_TRANSFORM_ROUTINE) {
        bool had_child = guardian->curly.child;
        AosTransformTarget target = {&soma->flags, &guardian->stats->flags, &soma->state,
                                     &soma->transform_step, &soma->transform_variant,
                                     &soma->friction, &soma->gravity_mod};
        bool alive = aos_curly_guardian_update(&guardian->curly, &guardian->core, guardian->stats,
                                               guardian->records, soma->held, soma->pressed,
                                               soma->x, soma->y, soma->facing_left, target);
        /* sub_0802ECF4: a translucent spark at Soma with his facing, its
         * sequence r & 3 played once, deleted at its end; the guardian's
         * end deletes them (sub_0802DD84). */
        for (int k = 0; k < 7; ++k)
            if (guardian->sparks[k].alive &&
                (!alive || aos_anim_step(&guardian->sparks[k].anim, guardian->spark_art) == 3))
                guardian->sparks[k].alive = false;
        if (alive && guardian->curly.spawn_spark && guardian->spark_art)
            for (int k = 0; k < 7; ++k) {
                if (guardian->sparks[k].alive) continue;
                uint8_t seq = (uint8_t)(aos_random() & 3);
                guardian->sparks[k].alive = true;
                guardian->sparks[k].x = soma->x;
                guardian->sparks[k].y = soma->y;
                guardian->sparks[k].facing_left = soma->facing_left;
                guardian->sparks[k].sequence = seq;
                aos_anim_start(&guardian->sparks[k].anim, guardian->spark_art, seq, false);
                break;
            }
        if (!alive) return;
        if (aos_transform_variants[guardian->curly.variant].sparks) return;
        /* sub_0802EE40: the shell's looping sequence 0. */
        if (guardian->curly.child && !had_child)
            aos_anim_start(&guardian->anim, guardian->curly_art, 0, true);
        if (guardian->curly.child) aos_anim_step(&guardian->anim, guardian->curly_art);
    } else if (guardian->routine == GUARDIAN_GHOST_ROUTINE) {
        /* Its looping sequence (0 the Witch, 1 the Ghost), stepped at the
         * end of each update (sub_0803F17C); sub_08042A54(2, 0xA) is not
         * run. */
        bool first = guardian->core.state == 0;
        if (!aos_ghost_guardian_update(&guardian->ghost, &guardian->core, guardian->stats,
                                       guardian->records, soma->held, soma->pressed,
                                       &soma->flags, soma->x, soma->y, soma->facing_left,
                                       guardian_cam_x, guardian_cam_y))
            return;
        const AosAnimSet *art = guardian->ghost.giant ? guardian->ghost_art : guardian->witch_art;
        if (first) aos_anim_start(&guardian->anim, art, guardian->ghost.giant ? 1 : 0, true);
        aos_anim_step(&guardian->anim, art);
    } else if (guardian->routine == GUARDIAN_IMP_ROUTINE) {
        /* The homing list's slots; a chased slot keeps its entity. */
        AosSkillTarget targets[AOS_IMP_TARGETS] = {{0}};
        for (int slot = 0; slot < AOS_IMP_TARGETS; ++slot) {
            int index = homing.index[slot];
            if (guardian->core.state == AOS_IMP_DIVING && guardian->imp.target == slot &&
                guardian->imp_target >= 0)
                index = guardian->imp_target;
            if (index < 0 || !homing_room || (size_t)index >= homing_room->entity_count)
                continue;
            const AriaEntity *entity = &homing_room->entities[index];
            targets[slot] = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                             entity->enemy.x, entity->enemy.y, entity->enemy.hp};
        }
        bool first = guardian->core.state == 0;
        int chased = guardian->imp.target;
        if (!aos_imp_guardian_update(&guardian->imp, &guardian->core, guardian->stats,
                                     guardian->records, soma->held, soma->pressed, &soma->flags,
                                     soma->x, soma->y, soma->facing_left, targets, aos_random))
            return;
        if (guardian->imp.target != chased || guardian->imp_target < 0)
            guardian->imp_target = guardian->imp.target >= 0
                                       ? homing.index[guardian->imp.target]
                                       : -1;
        /* sub_0802E1D0 (not in state 6): its looping sequence 0 and the
         * frame's attack box (sub_08042A54(2, 0x3C) is not run). */
        if (first) aos_anim_start(&guardian->anim, guardian->imp_art, 0, true);
        if (guardian->core.state != AOS_IMP_LEAVING)
            aos_anim_step(&guardian->anim, guardian->imp_art);
    } else if (guardian->routine == GUARDIAN_SKY_FISH_ROUTINE) {
        /* SkillSkyFishUse: the shoulder pet copies Soma's position and
         * facing every update; states 1 and 5 spawn their sparkle
         * bursts (sub_08045B44), the songs 0x99/0x1A8/0x95/0x1A2 fire on
         * their native edges; its looping sequence steps each update. */
        bool first = guardian->core.state == 0;
        if (!aos_sky_fish_guardian_update(&guardian->sky_fish, &guardian->core,
                                          guardian->stats, guardian->records,
                                          soma->held, soma->pressed, &soma->flags,
                                          soma->x, soma->y, soma->facing_left,
                                          aos_random))
            return;
        if (guardian->sky_fish.song_99) aos_audio_play_song(0x99);
        if (guardian->sky_fish.song_1a8) aos_audio_play_song(0x1A8);
        if (guardian->sky_fish.song_95) aos_audio_play_song(0x95);
        if (guardian->sky_fish.song_1a2) aos_audio_play_song(0x1A2);
        if (first) aos_anim_start(&guardian->anim, guardian->sky_fish_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->sky_fish_art);
        /* sub_08045B44(x +- 16, y - 0x28 +- 16, 4, 8): the native spawn
         * issues it twice per update; both requests carry the same box. */
        for (int k = 0; guardian->sky_fish.fx && k < 2 &&
                       guardian->effect_count < 2; ++k)
            guardian->effects[guardian->effect_count++] = (AosEffect){
                .kind = AOS_EFFECT_SPARKLE, .drift = true,
                .x = (guardian->sky_fish.x >> 16) +
                     (int)(aos_random() & 0x1F) - 0x10,
                .y = (guardian->sky_fish.y >> 16) - 0x28 +
                     (int)(aos_random() & 0x1F) - 0x10};
    } else if (guardian->routine == GUARDIAN_CREAKING_SKULL_ROUTINE) {
        /* SkillCreakingSkullUse: the anchor rides 0x14 px above Soma
         * with his inverse facing; the three orbiting skulls are
         * embedded (the native uses the shared + 0x13144 slot
         * table). empowered is the + 0x1311C slot read — not
         * modelled, so table A only. */
        bool first = guardian->core.state == 0;
        if (!aos_creaking_skull_guardian_update(&guardian->creaking_skull, &guardian->core,
                                                guardian->stats, guardian->records,
                                                soma->held, soma->pressed, &soma->flags,
                                                soma->x, soma->y, soma->facing_left,
                                                false, aos_random))
            return;
        if (guardian->creaking_skull.song_9b) aos_audio_play_song(0x9B);
        if (first) aos_anim_start(&guardian->anim, guardian->creaking_skull_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->creaking_skull_art);
    } else if (guardian->routine == GUARDIAN_ALASTOR_ROUTINE) {
        /* SkillAlastorUse: the spinning guardian parks at the cast
         * spot and whirls toward the homing list; the aim point is
         * the focus entity (Soma) 0x28 px up — sub_0806CD38/78's
         * +0x13118 approximation. */
        AosSkillTarget targets[AOS_ALASTOR_TARGETS] = {{0}};
        for (int slot = 0; slot < AOS_ALASTOR_TARGETS; ++slot) {
            int index = homing.index[slot];
            if (index < 0 || !homing_room || (size_t)index >= homing_room->entity_count)
                continue;
            const AriaEntity *entity = &homing_room->entities[index];
            targets[slot] = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                             entity->enemy.x, entity->enemy.y,
                                             entity->enemy.hp};
        }
        bool first = guardian->core.state == 0;
        if (!aos_alastor_guardian_update(&guardian->alastor, &guardian->core,
                                         guardian->stats, guardian->records,
                                         soma->held, soma->pressed, &soma->flags,
                                         soma->x, soma->y,
                                         soma->x, soma->y - 0x280000,
                                         soma->facing_left, targets, aos_random))
            return;
        if (guardian->alastor.song_19e) aos_audio_play_song(0x19E);
        if (first) aos_anim_start(&guardian->anim, guardian->alastor_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->alastor_art);
    } else if (guardian->routine == GUARDIAN_GIANT_BAT_ROUTINE) {
        /* SkillGiantBatUse: no sprite — the first update damps Soma
         * (velocities clamped to +-4 px/frame, + 0x50/+ 0x54 zeroed,
         * + 0x10 bits 0x21 cleared, + 0xB zeroed) and plays 0x8F;
         * sixteen frames of orbiting sparkles follow, one a frame.
         * The upkeep or the + 0x10 mask 0x010001A0 deletes outright. */
        if (!aos_giant_bat_guardian_update(&guardian->giant_bat, &guardian->core,
                                           guardian->stats, guardian->records,
                                           soma->held, soma->pressed, &soma->flags,
                                           soma, aos_random))
            return;
        if (guardian->giant_bat.song_8f) aos_audio_play_song(0x8F);
        if (guardian->giant_bat.fx && guardian->effect_count < 2)
            guardian->effects[guardian->effect_count++] = (AosEffect){
                .kind = AOS_EFFECT_SPARKLE, .drift = true,
                .x = guardian->giant_bat.fx_x >> 16,
                .y = guardian->giant_bat.fx_y >> 16,
                .vx = guardian->giant_bat.fx_vx, .vy = guardian->giant_bat.fx_vy};
    } else if (guardian->routine == GUARDIAN_FINAL_GUARD_ROUTINE) {
        /* SkillFinalGuardUse: while the source stays grounded the
         * guard trails him at his pixel position with y + 4; the
         * mask miss or the upkeep end deletes outright. Its looping
         * sequence steps every update (sub_0803F17C). */
        bool first = guardian->core.state == 0;
        if (!aos_final_guard_guardian_update(&guardian->final_guard, &guardian->core,
                                             guardian->stats, guardian->records,
                                             soma->held, soma->pressed, &soma->flags,
                                             soma, aos_random))
            return;
        if (guardian->final_guard.song_1a7) aos_audio_play_song(0x1A7);
        if (first) aos_anim_start(&guardian->anim, guardian->final_guard_art, 0, true);
        if (guardian->final_guard.phase == 2)
            aos_anim_step(&guardian->anim, guardian->final_guard_art);
    }
}

/* The guardian's attacks this frame (type 6, sub_08021530(6) with the
 * 0x131C2 power and the 0x131C4 elements). Returns their count. */
int guardian_attacks(const AriaGuardian *guardian,
                            AosSoulAttack attacks[GUARDIAN_ATTACKS]) {
    if (!guardian->core.active || guardian->core.state == 0) return 0;
    if (guardian->routine == GUARDIAN_GHOST_ROUTINE) {
        /* Its body (type 7: power 0, its touches throwing the spirits),
         * then each spirit (type 6, box 0x0808FCFC: -4, -4, 8 x 8). */
        const AosGhostGuardian *ghost = &guardian->ghost;
        int count = 0;
        if (ghost->attacking) {
            int8_t b[4];
            aos_ghost_guardian_box(ghost, b);
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){b[0], b[1], (uint8_t)b[2], (uint8_t)b[3]},
                                ghost->x >> 16, ghost->y >> 16, ghost->facing_left, false),
                0, 0, false, NULL, AOS_GHOST_ATTACK_TYPE};
        }
        for (int k = 0; k < AOS_GHOST_SHOTS; ++k)
            if (ghost->shots[k].alive && ghost->shots[k].state == 1)
                attacks[count++] = (AosSoulAttack){
                    aos_entity_rect((AosBox){-4, -4, 8, 8}, ghost->shots[k].x >> 16,
                                    ghost->shots[k].y >> 16, false, false),
                    aos_guardian_power(guardian->stats, AOS_GHOST_POWER, false),
                    AOS_GHOST_ELEMENTS, false, NULL, AOS_TYPE_GUARDIAN};
        return count;
    }
    if (guardian->routine == GUARDIAN_CAGNAZZO_ROUTINE) {
        const AosCagnazzoGuardian *cagnazzo = &guardian->cagnazzo;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){10, 5, 20, 20}, cagnazzo->x, cagnazzo->y, /* 0x1414050A */
                            cagnazzo->facing_left, false),
            aos_guardian_power(guardian->stats, AOS_CAGNAZZO_POWER, false), 0, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE ||
        guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box in the current sequence. */
        bool golem = guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE;
        const AosEnemyKind *kind = golem ? guardian->big_golem_kind : guardian->shadow_knight_kind;
        if (!kind || guardian->anim.id >= AOS_ENEMY_MAX_ANIMS ||
            guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES)
            return 0;
        AosBox box = kind->attack[guardian->anim.id][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->shadow_knight.x, guardian->shadow_knight.y,
                            guardian->shadow_knight.mirrored, false),
            aos_guardian_power(guardian->stats,
                               golem ? AOS_BIG_GOLEM_POWER : AOS_SHADOW_KNIGHT_POWER, false),
            golem ? AOS_BIG_GOLEM_ELEMENTS : AOS_SHADOW_KNIGHT_ELEMENTS, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_PERSEPHONE_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box, none without one or while
         * leaving. */
        if (guardian->core.state == AOS_PERSEPHONE_LEAVING || !guardian->persephone_kind ||
            guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES)
            return 0;
        AosBox box = guardian->persephone_kind->attack[0][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->persephone.x, guardian->persephone.y,
                            guardian->persephone.facing_left, false),
            aos_guardian_power(guardian->stats, AOS_PERSEPHONE_POWER, false),
            AOS_PERSEPHONE_ELEMENTS, false, NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_DEATH_ROUTINE) {
        int count = 0;
        for (int k = 0; k < AOS_DEATH_SICKLES; ++k) {
            const AosDeathSickle *s = &guardian->death.sickles[k];
            if (!s->alive || !s->attacking || s->state > 3) continue;
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){-8, -8, 16, 16}, s->x >> 16, s->y >> 16,  /* 0x1010F8F8 */
                                s->mirrored, false),
                aos_guardian_power(guardian->stats, AOS_DEATH_POWER, false), AOS_DEATH_ELEMENTS,
                false, NULL, AOS_TYPE_GUARDIAN};
        }
        return count;
    }
    if (guardian->routine == GUARDIAN_PANTHER_ROUTINE) {
        if (!guardian->panther.attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-12, -40, 48, 48},                     /* 0x3030D8F4 */
                            guardian->panther.x, guardian->panther.y,
                            guardian->panther.facing_left, false),
            aos_guardian_power(guardian->stats, AOS_PANTHER_POWER, false), AOS_PANTHER_ELEMENTS,
            false, NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_BUER_ROUTINE) {
        for (int k = 0; k < AOS_BUER_FLAMES; ++k)
            attacks[k] = (AosSoulAttack){
                aos_entity_rect((AosBox){-10, -10, 20, 20},                  /* 0x1414F6F6 */
                                guardian->buer.flames[k].x >> 16,
                                guardian->buer.flames[k].y >> 16, false, false),
                aos_guardian_power(guardian->stats, AOS_BUER_POWER, false), AOS_BUER_ELEMENTS,
                false, NULL, AOS_TYPE_GUARDIAN};
        return AOS_BUER_FLAMES;
    }
    if (guardian->routine == GUARDIAN_TRANSFORM_ROUTINE) {
        if (!guardian->curly.attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-24, -64, 48, 68}, guardian->curly.x >> 16,
                            guardian->curly.y >> 16, guardian->curly.facing_left,
                            false),                                          /* 0x4430C0E8 */
            aos_guardian_power(guardian->stats,
                               aos_transform_variants[guardian->curly.variant].power, false),
            aos_transform_variants[guardian->curly.variant].elements, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_IMP_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box, none without one. */
        if (!guardian->imp_kind || guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES) return 0;
        AosBox box = guardian->imp_kind->attack[0][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->imp.x >> 16, guardian->imp.y >> 16,
                            guardian->imp.mirrored, false),
            aos_guardian_power(guardian->stats, AOS_IMP_POWER, false), AOS_IMP_ELEMENTS, false,
            NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_CATOBLEPAS_ROUTINE ||
        guardian->routine == GUARDIAN_PILLAR_ROUTINE) {
        bool pillar = guardian->routine == GUARDIAN_PILLAR_ROUTINE;
        int count = 0;
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
            const AosCatoblepasGas *gas = &guardian->catoblepas.gases[k];
            if (!gas->alive) continue;
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){-10, -10, 20, 20}, gas->x >> 16, gas->y >> 16,
                                gas->facing_left, false),                    /* 0x1414F6F6 */
                aos_guardian_power(guardian->stats,
                                   pillar ? AOS_PILLAR_SOUL_POWER : AOS_CATOBLEPAS_POWER, false),
                pillar ? AOS_PILLAR_SOUL_ELEMENTS : AOS_CATOBLEPAS_ELEMENTS, false, NULL,
                AOS_TYPE_GUARDIAN};
        }
        return count;
    }
    if (guardian->routine == GUARDIAN_CREAKING_SKULL_ROUTINE) {
        /* Only child 2 registers a contact box natively
         * (sub_08042884 0x2020F0F0: -16, -16, 32 x 32); the parent's
         * + 0x131C2 power 0x28 with no elements feeds it. Children
         * 0/1's contact path is not decoded — documented. */
        const AosCreakingChild *child = &guardian->creaking_skull.children[2];
        if (child->dead || !child->attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-16, -16, 32, 32}, child->x >> 16,
                            child->y >> 16, child->facing_left, false),
            aos_guardian_power(guardian->stats, 0x28, false), 0, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_ALASTOR_ROUTINE) {
        /* The type-6 contact armed by + 0x72 bit 1 in states 2-4
         * (sub_0804277C(6, 0, 0) + the sub_08042A54(2, 0x3C) preset);
         * the state-5 rotating box is unreachable natively. The
         * frame box is approximated by {-16,-16,32,32}. */
        if (!guardian->alastor.attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-16, -16, 32, 32}, guardian->alastor.x >> 16,
                            guardian->alastor.y >> 16, guardian->alastor.facing_left,
                            false),
            aos_guardian_power(guardian->stats, AOS_ALASTOR_POWER, false),
            AOS_ALASTOR_ELEMENTS, false, NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    return 0;
}

void update_skills(AriaSouls *souls) {
    for (int i = 0; i < MAX_SKILLS; ++i) {
        AriaSkill *skill = &souls->skills[i];
        if (!skill->alive) continue;
        switch (skill->kind) {
        case SKILL_BAT:
            /* SkillBatUse: on its first update it counts itself and starts
             * sequence 0 (sound 0xE2 and sub_08042A54(1, 0xF) are not
             * played); it stays where it was created and ends with it. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->bat, 0, false);
            }
            if (aos_anim_step(&skill->anim, souls->bat) == 3) end_skill(souls, skill);
            break;
        case SKILL_BOW:
            /* SkillSkullArcherUse: counted on its first update, sequence 0;
             * on frame 14 it shoots the arrow from its own position; state
             * 2 (animation over or off screen) deletes it on the next
             * update. */
            if (skill->state == 2) {
                end_skill(souls, skill);
                break;
            }
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->bow, 0, false);
                skill->state = 1;
            }
            if (skill->anim.frame == 14) {
                AriaSkill *arrow = spawn_child(souls, SKILL_ARROW);
                if (arrow) {
                    arrow->soul = skill->soul;
                    place(arrow, skill->x, skill->y, skill->facing_left, 0, 0);
                }
            }
            if (aos_anim_step(&skill->anim, souls->bow) == 3) skill->state = 2;
            if (skill->state != 2 && skill_outside(souls, skill, 4)) skill->state = 2;
            break;
        case SKILL_THROW: {
            /* The shared throw routine: counted on its first update
             * (sub_08042A54(1, 0x1E) is not run). */
            const AosThrowRecord *record = &souls->throws[skill->parameter];
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            AosThrow *thrown = &skill->thrown;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            const AosEnemyKind *throw_kind = record->resource == 1 ? souls->ripper_kind
                                                                   : souls->bone_kind;
            if (!aos_throw_update_world(thrown, record, souls->layer,
                                        throw_kind ? &throw_kind->probes : NULL,
                                        souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045A98(x, y, 0): burst list 0 at its whole position. */
            for (size_t k = 0; thrown->exploded && souls->effects && k < souls->burst_count &&
                               *souls->effect_count < MAX_EFFECTS;
                 ++k) {
                AosEffect piece = souls->burst[k];
                piece.x = (int32_t)((uint32_t)(int16_t)((thrown->x >> 16) + (piece.x >> 16)) << 16);
                piece.y = (int32_t)((uint32_t)(int16_t)((thrown->y >> 16) + (piece.y >> 16)) << 16);
                souls->effects[(*souls->effect_count)++] = piece;
            }
            /* sub_08025540: four randomly selected static fragments. */
            for (int k = 0; thrown->landing_burst && souls->effects && k < 4 &&
                            *souls->effect_count < MAX_EFFECTS;
                 ++k) {
                AosEffect piece = {
                    .kind = (uint8_t)(AOS_EFFECT_ROCK_FRAGMENT_0 + (aos_random() & 1)),
                    .ballistic = true,
                    .x = thrown->x + (int32_t)((aos_random() & 7) - 4) * 0x10000,
                    .y = thrown->y + (int32_t)((aos_random() & 7) - 4) * 0x10000,
                    .vx = (int32_t)(aos_random() & 0xFF) << 8,
                    .ay = 0x2000};
                if (aos_random() & 1) piece.vx = -piece.vx;
                piece.vy = -0x10000 - ((int32_t)(aos_random() & 0xFF) << 8);
                souls->effects[(*souls->effect_count)++] = piece;
            }
            /* Flag 0x400's last eight even frames call the standard
             * animation-4, row-6 drifting dust helper. */
            if (thrown->timeout_dust && souls->effects &&
                *souls->effect_count < MAX_EFFECTS) {
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_DUST,
                    .drift = true,
                    .x = thrown->x + (int32_t)(aos_random() & 0x7FFFF) - 0x40000,
                    .y = thrown->y + (int32_t)(aos_random() & 0x7FFFF) - 0x40000,
                    .ay = (int32_t)(aos_random() & 0x3FF) - 0x1A00};
            }
            skill->x = thrown->x;
            skill->y = thrown->y;
            skill->facing_left = thrown->facing_left;
            break;
        }
        case SKILL_SPOUT:
            /* SkillMermanUse: on its first update it starts sequence 0
             * (sound 0xED and sub_08042A54(1, 0x1E) are not played), counts
             * itself and drops five sub_08026DB8 children from (6, -0x1A);
             * every update it then stays at Soma's (0, -0x1A) and it ends
             * with its animation. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->merman, AOS_MERMAN_SPOUT_SEQUENCE, false);
                souls->caster.active++;
                for (int k = 0; k < AOS_MERMAN_DROPS; ++k) {
                    AriaSkill *drop = spawn_child(souls, SKILL_DROP);
                    if (!drop) continue;
                    drop->soul = skill->soul;
                    drop->drop = aos_merman_drop_create(k, skill->x, skill->y, skill->facing_left);
                    drop->x = skill->x;
                    drop->y = skill->y;
                    drop->facing_left = skill->facing_left;
                }
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x1A);
            if (aos_anim_step(&skill->anim, souls->merman) == 3) end_skill(souls, skill);
            break;
        case SKILL_DROP: {
            /* sub_08026DB8: its splash (sub_0802DAD4) is not created. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->merman, AOS_MERMAN_DROP_SEQUENCE, true);
            }
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool splash;
            if (!aos_merman_drop_update(&skill->drop, souls->cam_x, souls->cam_y, mx, my, &splash)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->drop.x;
            skill->y = skill->drop.y;
            aos_anim_step(&skill->anim, souls->merman);
            break;
        }
        case SKILL_AXE: {
            /* SkillAxeArmorUse: counted and its two sub_080240A0
             * afterimages made on its first update (sound 0xE3 and
             * sub_08042A54 are not played). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                for (int k = 0; k < AOS_AXE_TRAILS; ++k) {
                    AriaSkill *trail = spawn_child(souls, SKILL_AXE_TRAIL);
                    if (!trail) continue;
                    trail->soul = skill->soul;
                    trail->parameter = skill->parameter;
                    trail->parent = skill->slot;
                    trail->trail = aos_axe_trail_create(k, souls->soma->facing_left);
                    trail->x = skill->x;
                    trail->y = skill->y;
                }
            }
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            if (!aos_axe_update(&skill->axe, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->axe.x;
            skill->y = skill->axe.y;
            break;
        }
        case SKILL_SPEAR:
            /* SkillWingedSkeletonUse: counted, with its tip child
             * sub_08025F70, on its first update (sound 0xE4 is not
             * played). */
            if (!skill->started) {
                skill->started = true;
                AriaSkill *tip = spawn_child(souls, SKILL_SPEAR_TIP);
                if (tip) {
                    tip->soul = skill->soul;
                    tip->parent = skill->slot;
                    tip->x = skill->x;
                    tip->y = skill->y;
                }
                souls->caster.active++;
            }
            {
                int mx = souls->margins ? souls->margins[4][0] : 16;
                int my = souls->margins ? souls->margins[4][1] : 16;
                if (!aos_spear_update(&skill->spear, souls->layer, souls->cam_x, souls->cam_y, mx,
                                      my)) {
                    end_skill(souls, skill);
                    break;
                }
            }
            skill->x = skill->spear.x;
            skill->y = skill->spear.y;
            break;
        case SKILL_FLEA: {
            /* SkillFleamanUse: counted with sound 0x19C on its first update
             * (sub_08042A54(1, 0x14) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x19C);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            bool ending;
            if (!aos_flea_update(&skill->flea, souls->layer, &souls->flea_kind->probes, aos_random,
                                 souls->cam_x, souls->cam_y, mx, my, &ending)) {
                end_skill(souls, skill);
                break;
            }
            if (ending) aos_anim_start(&skill->anim, souls->flea, AOS_FLEA_END_SEQUENCE, false);
            else if (skill->flea.state == 2) aos_anim_step(&skill->anim, souls->flea);
            skill->x = skill->flea.body.x;
            skill->y = skill->flea.body.y;
            skill->facing_left = skill->flea.body.mirrored;
            break;
        }
        case SKILL_WEREWOLF:
            /* SkillWerewolfUse: sub_0802393C must find its floor on the
             * first update or the cast deletes itself; on success it is
             * counted, plays song 0x1B2 and sequence 0 once while
             * sub_0806CAF8 walks it at -0x4000 with the 0x084F13B4 probes
             * (mode 4); its animation end deletes it. */
            if (!skill->started) {
                if (!aos_werewolf_ground_ok(&skill->werewolf, souls->layer)) {
                    end_skill(souls, skill);
                    break;
                }
                aos_werewolf_land(&skill->werewolf, souls->layer,
                                  &souls->werewolf_kind->probes);
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1B2);
                aos_anim_start(&skill->anim, souls->werewolf, 0, false);
            }
            aos_werewolf_update(&skill->werewolf, souls->layer,
                                &souls->werewolf_kind->probes);
            if (aos_anim_step(&skill->anim, souls->werewolf) == 3)
                end_skill(souls, skill);
            skill->x = skill->werewolf.body.x;
            skill->y = skill->werewolf.body.y;
            skill->facing_left = skill->werewolf.body.mirrored;
            break;
        case SKILL_KILLER_MANTLE:
            /* SkillKillerMantleUse: counted on its first update, song 0xD1,
             * sequence 0; sub_08031EA4(entity, source, 0, 0) re-places it on
             * Soma every frame, the animation end (sub_0803F17C == 3) or
             * the sub_0806D128(e, 4) margin probe ends it, and state 2
             * deletes it. The + 0x59 bit-4 tail gate is not traced. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xD1);
                aos_anim_start(&skill->anim, souls->killer_mantle, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            bool mantle_anim_done = aos_anim_step(&skill->anim, souls->killer_mantle) == 3;
            if (!aos_killer_mantle_update(&skill->killer_mantle, mantle_anim_done,
                                          skill_outside(souls, skill, 4)))
                end_skill(souls, skill);
            break;
        case SKILL_WERE_BEAST:
            /* Skill_Werejaguar_Weretiger_Use (shared by souls 35 and 36):
             * counted on its first update, song 0xEB, sequence 0;
             * sub_08031EA4(entity, source, 0, 0) re-places it on Soma every
             * frame; it ends on the animation end or the native
             * (source + 0x10 & 0x180) != 0x100 check, approximated as
             * never-gone while the cast lives. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xEB);
                aos_anim_start(&skill->anim, souls->were_beast, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            bool beast_anim_done = aos_anim_step(&skill->anim, souls->were_beast) == 3;
            if (!aos_were_beast_update(&skill->were_beast, beast_anim_done, false))
                end_skill(souls, skill);
            break;
        case SKILL_BALORE:
            /* SkillBaloreUse: counted on its first update, attack box
             * preset 5, song 0x1A1 — the sub_080213F4 bonus branch (shared
             * with SkillLightningDollUse and Skill_Devil_Manticore_Curly_Use)
             * is approximated as false because its + 0x13220 record is
             * never populated; sub_08031EA4(entity, source, 0, 0) re-places
             * it on Soma every frame, and it ends on the animation end or
             * the (source + 0x10 & 0x180) != 0x100 check. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A1);
                aos_anim_start(&skill->anim, souls->balore, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            bool balore_anim_done = aos_anim_step(&skill->anim, souls->balore) == 3;
            if (!aos_were_beast_update(&skill->were_beast, balore_anim_done, false))
                end_skill(souls, skill);
            break;
        case SKILL_KYOMA_DEMON:
            /* SkillKyomaDemonUse: counted with song 0x97; state 1 glues it
             * to Soma through sub_08031EA4(entity, source, 0, 0) for the
             * 0x20-frame wobble (+ 0x24 = sub_080009E4(+ 0x0D << 10) feeds
             * the visual sub_0803E058), then state 2 lingers 0xB4 frames
             * before deleting. The sub_0803E654 palette-variant pick and
             * its 0x8000 sentinel are not tracked (cast always proceeds),
             * and sub_08022134/sub_0803E6BC bookkeeping is skipped. No
             * attack box is preset natively. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x97);
                aos_anim_start(&skill->anim, souls->kyoma_demon, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            if (aos_anim_step(&skill->anim, souls->kyoma_demon) == 3)
                skill->anim.id = 0;    /* the native entity has no anim-end gate */
            if (!aos_kyoma_demon_update(&skill->kyoma_demon))
                end_skill(souls, skill);
            break;
        case SKILL_LEGION:
            /* SkillLegionUse: counted with song 0x1A9, attack box preset 5,
             * sub_08031EA4(entity, source, 0, 0) glue and the source facing
             * bit mirrored onto + 0x58 bit 6 each frame (the place() call
             * already follows Soma's facing). At animation frame 0xC it
             * requests song 0xCE through the sub_08013D60 history dedup
             * (approximated by a plain play); the delete paths run
             * sub_08013E18(0xCE), a history removal with no sound. It ends
             * on the animation end or the (source + 0x10 & 0x80) != 0
             * check, approximated as never-gone while the cast lives. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A9);
                aos_anim_start(&skill->anim, souls->legion, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            if (!skill->volley_count && skill->anim.frame == 0xC) {
                skill->volley_count = 1;
                aos_audio_play_song(0xCE);
            }
            bool legion_anim_done = aos_anim_step(&skill->anim, souls->legion) == 3;
            if (!aos_were_beast_update(&skill->were_beast, legion_anim_done, false))
                end_skill(souls, skill);
            break;
        case SKILL_STUDENT_WITCH: {
            /* SkillStudentWitchUse (red soul 15, shared by the Student
             * Witch and the Basilisk drops): state 0 places it through
             * sub_08031EA4(entity, source, 8, -0x1A), presets attack box
             * 5 plus the 0x1018F0F4 box, and enters state 1; state 1 walks
             * at -0x20000 with the 0x084F12BC probes (mode 8) — result&8
             * plays song 0x1A0 once, result&3 lingers 0x10 frames; the
             * sub_0806D128(e, 4) margin probe forces state 3 and state 3
             * deletes. The rng&7 variant pick, + 0x54/+ 0x5C fields and
             * the + 0x58 bit-5 margin flag are not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->student_witch, 0, false);
            }
            bool contact_song = false;
            bool alive = aos_student_witch_update(&skill->student_witch,
                                                  souls->layer,
                                                  &souls->witch_kind->probes,
                                                  skill_outside(souls, skill, 4),
                                                  &contact_song);
            if (contact_song) aos_audio_play_song(0x1A0);
            if (aos_anim_step(&skill->anim, souls->student_witch) == 3)
                skill->anim.id = 0;    /* no native anim-end gate here */
            if (!alive) end_skill(souls, skill);
            skill->x = skill->student_witch.body.x;
            skill->y = skill->student_witch.body.y;
            skill->facing_left = skill->student_witch.body.mirrored;
            break;
        }
        case SKILL_SLIME: {
            /* SkillSlimeUse: state 0 places it through
             * sub_08031EA4(entity, source, 0xC, -0x1A), draws
             * (rng & 0x3FF) + 0x1000 into + 0x2C (mirrored: -0x8000 -
             * angle), presets + 0x34 = 0x300000 and + 0x24 = 0x100, attack
             * box 5 plus the 0x0808FCFC box, and spawns the sub_0802C9E8
             * trail child; state 1 recomputes + 0x48/+ 0x4C from the angle
             * each frame, integrates with the sub_0802CE20 floor/ceiling
             * snap (approximated), swaps the animation variant at 0x20
             * remaining and deletes at life 0 or the sub_0806D128(e, 6)
             * margin probe. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->slime, 0, false);
                AriaSkill *trail = spawn_child(souls, SKILL_SLIME_TRAIL);
                if (trail) {
                    trail->soul = skill->soul;
                    trail->parent = skill->slot;
                    trail->started = true;
                    trail->x = skill->x;
                    trail->y = skill->y;
                }
            }
            bool variant = false;
            bool alive = aos_slime_update(&skill->slime, souls->layer, &variant);
            if (variant) aos_anim_start(&skill->anim, souls->slime, 1, false);
            if (skill_outside(souls, skill, 6) || !alive)
                end_skill(souls, skill);
            skill->x = skill->slime.body.x;
            skill->y = skill->slime.body.y;
            skill->facing_left = skill->slime.body.mirrored;
            break;
        }
        case SKILL_SLIME_TRAIL: {
            /* sub_0802C9E8's child (update sub_0802DCB0, orphan check
             * sub_0802CD6C): appends the parent's room position to its
             * nine-entry ring each frame and draws at the oldest entry;
             * sub_0802CD6C deletes it once the parent's slot empties. */
            AriaSkill *parent = skill->parent >= 0 && skill->parent < MAX_SKILLS &&
                                       souls->skills[skill->parent].alive
                                   ? &souls->skills[skill->parent]
                                   : NULL;
            if (!parent || parent->kind != SKILL_SLIME) {
                end_skill(souls, skill);
                break;
            }
            aos_slime_trail_push(&skill->slime_trail, parent->x, parent->y);
            int32_t tx, ty;
            aos_slime_trail_pos(&skill->slime_trail, &tx, &ty);
            skill->x = tx;
            skill->y = ty;
            break;
        }
        case SKILL_UKOBACK:
            /* SkillUkobackUse: counted with song 0xDC, attack box preset 5
             * (with the sub_0802D9B4 callback, not ported) plus the
             * 0x1010F8F8 box, sub_08031EA4(entity, source, 8, -0x1A) glue;
             * + 0x24 ramps 0x1000/frame to 0xFFFF through state 1's 0xF0
             * timer (+ 0x18 force-end not traced), then state 2 decays
             * -0x2000/frame until <= 0 deletes; the sub_0806D128(e, 2)
             * margin probe deletes from any state. The sub_0803E654
             * variant pick, + 0x58 bits 0-4 and the sub_0803E058/
             * sub_0803E6BC bookkeeping are not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xDC);
                aos_anim_start(&skill->anim, souls->ukoback, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, 0);
            if (!aos_ukoback_update(&skill->ukoback, false,
                                    skill_outside(souls, skill, 2)))
                end_skill(souls, skill);
            break;
        case SKILL_ALTAIR: {
            /* SkillAltairUse: counted with song 0xD6, attack box preset 5
             * plus the 0x1020F8F0 box {-16,-8,32,16}, + 0x48 = -0x28000,
             * + 0x72 |= 6, sub_08031EA4(entity, source, 0x14, -0x1A);
             * states 1->2->3 advance on the + 0x6C bit-4 hit flag which
             * is not produced in this model (the soul then only leaves
             * through the sub_0806D128(e, 2) margin probe); state 3's
             * + 0x6E == 2 plays song 0xEE once; the sub_0806D430
             * physics and the + 0x5A variant/sequence swaps are not
             * ported. */
            static const uint8_t seqs[4] = {1, 1, 2, 0};
            bool song_ee = false;
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xD6);
                aos_anim_start(&skill->anim, souls->altair,
                               seqs[skill->altair.state], false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
                  0x14, -0x1A);
            const uint8_t before = skill->altair.state;
            if (!aos_altair_update(&skill->altair, false,
                                   skill_outside(souls, skill, 2), &song_ee))
                end_skill(souls, skill);
            else {
                if (skill->altair.state != before)
                    aos_anim_start(&skill->anim, souls->altair,
                                   seqs[skill->altair.state], false);
                if (song_ee) aos_audio_play_song(0xEE);
            }
            break;
        }
        case SKILL_UNE:
            /* SkillUneUse: entry-0 resources, sequence 3, counted on its
             * first update; sub_08031EA4(entity, source, 8, -0x1A),
             * + 0x24 = 0xA0000, + 0x4C = -0x18000, + 0x54 = 0x2000,
             * + 0x59 |= 2, + 0x72 |= 6, attack box preset 5 with the
             * sub_0802D9B4 callback; state 1 walks at -+ 0x24 with the
             * 0x084F13AA probes (mode 0): walls negate and halve the
             * speed, the first floor contact (result & 8) plays song
             * 0xE0 and enters the 0x80-frame linger, whose end plays
             * song 0xE8 in state 3; the table-driven sub_08042848 shots
             * (gEwramData + 0x1017C via + 0x64/+ 0x65) and the + 0x6C
             * bit-4 hit flag are not ported; the sub_0806D128(e, 4)
             * margin probe forces state 4. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->une, 3, false);
            }
            if (!aos_une_update(&skill->une, souls->layer,
                                &souls->une_kind->probes, false,
                                skill_outside(souls, skill, 4))) {
                end_skill(souls, skill);
                break;
            }
            if (skill->une.shot1) {
                skill->une.shot1 = false;
                aos_audio_play_song(0xE0);
                aos_anim_start(&skill->anim, souls->une, 4, false);
            }
            if (skill->une.shot2) {
                skill->une.shot2 = false;
                aos_audio_play_song(0xE8);
            }
            skill->x = skill->une.body.x;
            skill->y = skill->une.body.y;
            skill->facing_left = skill->une.body.mirrored;
            break;
        case SKILL_MANDRAGORA: {
            /* SkillMandragoraUse: counted on its first update, sequence 0,
             * song 0xDF, attack box preset 5 plus the 0x7F7FC0C0 box,
             * + 0x72 |= 4, + 0x59 |= 2, + 0x5A bits 0-2 = 2,
             * sub_08031EA4(entity, source, 0x10, -0x1A); state 1 emits a
             * leaf every fourth + 0x0D frame at a sine-offset position
             * (the leaf entity is approximated by an AOS_EFFECT_DUST
             * drift, its + 0x50/+ 0x54 velocities and + 0x58 flip bits
             * not ported); the + 0x6C bit-4 hit flag (not produced in
             * this model) starts the 0x20 + 8 frame death sequence;
             * the sub_0803D18C/sub_0803D270/sub_0803CED4 palette
             * effects are not ported; the sub_0806D128(e, 4) margin
             * probe deletes from any state. */
            static const uint8_t seqs[4] = {0, 0, 1, 1};
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xDF);
                aos_anim_start(&skill->anim, souls->mandragora,
                               seqs[skill->mandragora.state], false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
                  0x10, -0x1A);
            const uint8_t before = skill->mandragora.state;
            if (!aos_mandragora_update(&skill->mandragora, (int32_t)aos_random(),
                                       skill->x, skill->y, false,
                                       skill_outside(souls, skill, 4))) {
                end_skill(souls, skill);
                break;
            }
            if (skill->mandragora.leaf && souls->effects &&
                *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_DUST, .drift = true,
                    .x = skill->mandragora.leaf_x, .y = skill->mandragora.leaf_y};
            if (skill->mandragora.state != before)
                aos_anim_start(&skill->anim, souls->mandragora,
                               seqs[skill->mandragora.state], false);
            break;
        }
        case SKILL_CHRONOMAGE:
            /* SkillChronomageUse: counted on its first update, sequence
             * 0, songs 0x1030, 0x19D and 0xEC at init (the later calls
             * queue over the earlier ones natively; all three are
             * requested here), + 0x5C = 0xD, + 0x59 |= 0x12, the
             * source + 0x46 - 0x30 anchor; state 1 ramps + 0x20/+ 0x24
             * toward 0x10000 while the + 0x4BE hold counts down with a
             * 0x16E tick every sixteenth frame, state 2 releases over
             * 0x20 frames and deletes; the published + 0x12FE4 time
             * value, the sub_0803E654 variant pick, the sub_0803E058
             * visual and the conditional release songs are not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1030);
                aos_audio_play_song(0x19D);
                aos_audio_play_song(0xEC);
                aos_anim_start(&skill->anim, souls->chronomage, 0, false);
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left,
                  0, -0x30);
            if (!aos_chronomage_update(&skill->chronomage)) {
                end_skill(souls, skill);
                break;
            }
            if (skill->chronomage.tick) aos_audio_play_song(0x16E);
            break;
        case SKILL_DISC_ARMOR:
            /* SkillDiscArmorUse: counted on its first update, sequence
             * 0, song 0xD4, + 0x65 = 1, + 0x5C = 0xD, + 0x59 |= 0x12,
             * + 0x58 = variant | source facing bit 6, + 0x2C = 0x2000
             * (0x6000 mirrored) with + 0x2E = -+0x276, attack box preset
             * 5 plus the 0x2020F0F0 box {-16,-16,32,32}; state 1 orbits
             * the source anchor (+ 0x50/+ 0x54 = source +0x40/+0x44 -
             * 0x18000) at radius |sin(t * 1260)| * 60 for 0x1A frames,
             * then state 2 deletes; the sub_08023530(e,
             * sub_08027140) partner registration, the + 0x30 spin
             * timer, the sub_0803E654 variant pick and the sub_0803E058
             * visual are not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xD4);
                aos_anim_start(&skill->anim, souls->disc_armor, 0, false);
            }
            if (!aos_disc_armor_update(&skill->disc_armor)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->disc_armor.x;
            skill->y = skill->disc_armor.y;
            skill->facing_left = souls->soma->facing_left;
            break;
        case SKILL_COCKATRICE:
            /* SkillCockatriceUse: counted on its first update, sequence
             * 0, + 0x65 = 1, + 0x5C = 0xD, + 0x59 |= 0x12, + 0x58 =
             * variant | facing bit 6, + 0x2C = 0x2800 (mirrored
             * -0x8000 - 0x2800) with + 0x2E = -+0x40, + 0x4A = -+8,
             * + 0x4E = -+0x1A, attack box preset 5, + 0x72 |= 4, song
             * history 0xCD requested once; state 1 spins the angle for
             * 0x20 frames while + 0x50/+ 0x54 = sine(angle +/- 0x4000)
             * * 32 shape the dynamic box, then deletes; the
             * sub_08023530(e, sub_08029568) partner registration, the
             * + 0x48/+ 0x4C drift fields and the sub_0803E654 variant
             * pick are not ported; the sub_0806D128(e, 2) margin probe
             * deletes from any state. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xCD);
                aos_anim_start(&skill->anim, souls->cockatrice, 0, false);
            }
            if (!aos_cockatrice_update(&skill->cockatrice,
                                       skill_outside(souls, skill, 2))) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->cockatrice.x;
            skill->y = skill->cockatrice.y;
            break;
        case SKILL_DRYAD:
            /* SkillDryadUse: counted on its first update with box
             * preset 5 plus 0x0202FFFF, song 0xE5 and the + 0x131BE
             * count; state 1 arcs under sub_0806D3D8 (cap 0x40000),
             * the sub_0802D9BC strike plants the sub_080239E4 child
             * (the runtime sets planted and resets + 0x26), state 2
             * counts to 0x14 with song 0xD5 and the sub_08021FEC(8)
             * bonus once while the host's + 0x34/+ 0x72 bit 2 end it,
             * state 3 lingers 0x10 frames blinking, state 4 deletes;
             * the sub_0806D128(e, 4) margin probe and the + 0x59 bit-4
             * mark enter state 4 from any state. The child entity, the
             * enemy-side charm fields and the sub_08042A7C popup are
             * not ported (the host fights on normally). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xE5);
            }
            skill->dryad.host_gone = skill->dryad_host &&
                                    (skill->dryad_host->hp <= 0 ||
                                     skill->dryad_host->removed);
            skill->dryad.host_flag = skill->dryad_host &&
                                    skill->dryad_host->defeated;
            skill->dryad.outside = skill_outside(souls, skill, 4);
            if (!aos_dryad_update(&skill->dryad)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->dryad.x;
            skill->y = skill->dryad.y;
            break;
        case SKILL_ARACHNE:
        case SKILL_MUDMAN: {
            /* SkillArachneUse: counted on its first update with song
             * 0xD1 (record 6 only), sequence record +0x0E, box preset 5
             * upgraded by the +0x64/+0x65 frame lookup; state 1 flies
             * under sub_0806D3D8 cap 0x80000 until the sub_0802DB84
             * strike plants the sub_080239E4 child — Arachne then
             * lingers 0x10 frames blinking on +0x64 = 5, Mudman plays
             * 0xD2, releases three sub_0802D880 shards and plays
             * sequence 2 until its end or the host's +0x24; states
             * 2/3 are stationary and the sub_0806D128(e, 2) margin
             * probe deletes from any state. The child entity, the
             * +0x59 bit-4 mark and the palette row +0x0F are not
             * ported. */
            const AosAnimSet *art = skill->kind == SKILL_ARACHNE
                                        ? souls->arachne
                                        : souls->fish_head;
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                if (skill->parameter == 6) aos_audio_play_song(0xD1);
                aos_anim_start(&skill->anim, art,
                               souls->throws[skill->parameter].sequence,
                               false);
            }
            skill->arachne.host_gone = skill->arachne_host &&
                                       (skill->arachne_host->hp <= 0 ||
                                        skill->arachne_host->removed);
            /* sub_0803F17C advances the animation in states 1 and 3
             * only; state 3 reads its == 3 end. */
            bool anim_done = false;
            if (skill->arachne.state == 3)
                anim_done = aos_anim_step(&skill->anim, art) == 3;
            else if (skill->arachne.state == 1)
                aos_anim_step(&skill->anim, art);
            if (!aos_arachne_update(&skill->arachne,
                                    skill_outside(souls, skill, 2),
                                    anim_done)) {
                end_skill(souls, skill);
                break;
            }
            if (skill->arachne.released && skill->shard_parent < 0) {
                /* PlaySong 0xD2, sequence 2 and the three-shard fan. */
                aos_audio_play_song(0xD2);
                aos_anim_start(&skill->anim, art, 2, false);
                for (int k = 0; k < 3; ++k) {
                    AriaSkill *shard = spawn_child(souls, SKILL_ARACHNE_SHARD);
                    if (!shard) break;
                    shard->soul = skill->soul;
                    shard->shard_parent = skill->slot;
                    shard->shard = aos_arachne_shard_create(
                        skill->arachne.x, skill->arachne.y,
                        skill->arachne.vx, aos_random(), aos_random(),
                        aos_random());
                    shard->x = shard->shard.x;
                    shard->y = shard->shard.y;
                }
                skill->shard_parent = skill->slot;
            }
            skill->x = skill->arachne.x;
            skill->y = skill->arachne.y;
            skill->facing_left = skill->arachne.facing_left;
            break;
        }
        case SKILL_ARACHNE_SHARD:
            /* sub_0802D880: integrates +0x48/+0x4C with the 0x4000
             * gravity, deletes on the sub_08068AD4 window probe or when
             * the parent's count drained (orphan). No attack box. */
            if (!skill->started) {
                skill->started = true;
            }
            if (skill->shard_parent >= 0 &&
                !souls->skills[skill->shard_parent].alive) {
                end_skill(souls, skill);
                break;
            }
            if (!aos_arachne_shard_update(&skill->shard,
                                          skill_outside(souls, skill, 4))) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->shard.x;
            skill->y = skill->shard.y;
            break;
        case SKILL_RED_MINOTAUR:
            /* SkillRedMinotaurUse: counted on its first update with
             * songs 0x1A0 and 0xCF; state 1 ramps + 0x24 to 0x1FFFF,
             * state 2 spins + 0x2C by the accelerating + 0x2E for 0x50
             * frames of + 0x0D, state 3 spins down while + 0x24 drains,
             * state 4 deletes; the sub_0806D128(e, 4) margin probe and
             * the + 0x59 bit-4 mark enter state 4 from any state. The
             * sub_0803E058 affine spin visual is drawn as a rotation,
             * the + 0x131BE count and the sub_0803E654 variant pick are
             * not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A0);
                aos_audio_play_song(0xCF);
                aos_anim_start(&skill->anim, souls->red_minotaur, 0, false);
            }
            aos_anim_step(&skill->anim, souls->red_minotaur);
            skill->red_minotaur.outside = skill_outside(souls, skill, 4);
            if (!aos_red_minotaur_update(&skill->red_minotaur)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->red_minotaur.x;
            skill->y = skill->red_minotaur.y;
            break;
        case SKILL_MINOTAUR_BLADE:
            /* sub_08026858: the partner blade orbits the anchor at
             * radius 80 px on the shared + 0x2C angle, installs the
             * {-32,-32,64,64} box (+ 0x72 bits 1|2) at init, disarms
             * while the anchor spins up (state 2) and rearms in state
             * 3; it dies with the anchor's state 4. */
            if (!skill->started) {
                skill->started = true;
            }
            {
                const AriaSkill *anchor = skill->shard_parent >= 0
                                              ? &souls->skills[skill->shard_parent]
                                              : NULL;
                if (!aos_minotaur_blade_update(
                        &skill->minotaur_blade,
                        anchor && anchor->alive && anchor->kind == SKILL_RED_MINOTAUR
                            ? &anchor->red_minotaur
                            : NULL)) {
                    end_skill(souls, skill);
                    break;
                }
            }
            skill->x = skill->minotaur_blade.x;
            skill->y = skill->minotaur_blade.y;
            break;
        case SKILL_NIGHTMARE:
            /* SkillNightmareUse: counted on its first update with songs
             * 0x19A and 0xE1; state 1 jitters x +-4 px and y upward
             * 12-20 px per frame while spawning the sub_08045CEC
             * trail and ramping + 0x24 by 0x1000; past 0xFFFF the
             * state-2 init runs and both states integrate the
             * facing-signed + 0x48/+ 0x50 drift under the 0x80000 cap
             * until the sub_0806D128(e, 3) margin probe or the + 0x59
             * bit-4 mark deletes. The + 0x64/+ 0x65 frame attack box
             * stands in with the preset-5 approximation, the trail
             * entity's own velocities and the sub_0802383C partner
             * touches are not ported. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x19A);
                aos_audio_play_song(0xE1);
                aos_anim_start(&skill->anim, souls->nightmare, 0, false);
            }
            aos_anim_step(&skill->anim, souls->nightmare);
            skill->nightmare.outside = skill_outside(souls, skill, 3);
            if (!aos_nightmare_update(&skill->nightmare, aos_random)) {
                end_skill(souls, skill);
                break;
            }
            if (skill->nightmare.trail && souls->effects &&
                *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_SOUL_STORM0, .drift = true,
                    .x = skill->nightmare.x >> 16, .y = skill->nightmare.y >> 16};
            skill->x = skill->nightmare.x;
            skill->y = skill->nightmare.y;
            break;
        case SKILL_MAN_EATER:
            /* SkillManEaterUse: one counted cast plays song 0x15E;
             * state 1 flies straight with + 0x65 = variant * 5 +
             * tick / 10 under the 0x80000 cap until + 0x24 passes
             * 0x1D, then lingers 0x30 frames into the + 0xF = 1 fade
             * of 0x10 frames into the delete bookkeeping. A type-5
             * contact (+ 0x18 via sub_0802D9B4) while the state is
             * <= 2 sticks the bite: vx 0, frame base + 4, state 4
             * for 0x10 frames back into state 3. The + 0x13124 slot
             * table and + 0x131BE counter are global bookkeeping with
             * no modelled reader. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x15E);
            }
            skill->man_eater.outside = skill_outside(souls, skill, 2);
            if (!aos_man_eater_update(&skill->man_eater, aos_random)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->man_eater.x;
            skill->y = skill->man_eater.y;
            break;
        case SKILL_FLAME_DEMON:
            /* SkillFlameDemonUse: one counted cast plays song 0xD8, emits
             * a sub_08045CEC(0xC, 6) burst and a fan of 3 or 5
             * sub_080292B4 bolts (5 when rng & 7 == 0), then deletes itself
             * on the next update. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->fish_head, 0, false);
                int count = (aos_random() & 7) == 0 ? 5 : 3;
                int32_t step = 0x1000 / count;
                int32_t angle = -step * (count >> 1);
                for (int k = 0; k < count; ++k) {
                    AriaSkill *bolt = spawn_child(souls, SKILL_FLAME_BOLT);
                    if (!bolt) break;
                    bolt->soul = skill->soul;
                    bolt->x = skill->x;
                    bolt->y = skill->y;
                    bolt->facing_left = skill->facing_left;
                    bolt->flame = aos_flame_bolt_create(skill->x, skill->y,
                                                        (uint32_t)angle,
                                                        skill->facing_left);
                    bolt->x = bolt->flame.x;
                    bolt->y = bolt->flame.y;
                    angle += step;
                }
                aos_audio_play_song(0xD8);
                if (souls->effects && *souls->effect_count < MAX_EFFECTS)
                    souls->effects[(*souls->effect_count)++] = (AosEffect){
                        .kind = AOS_EFFECT_BURST_15, .drift = true,
                        .x = skill->x >> 16, .y = skill->y >> 16};
                skill->state = 1;
            } else {
                end_skill(souls, skill);
            }
            break;
        case SKILL_FLAME_BOLT: {
            /* sub_080292B4: fish-head art bolt drifting on its fan velocity,
             * deleted on the animation end or the sub_0806D128(e, 2)
             * margin probe. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->fish_head, 0, false);
            }
            bool bolt_done = aos_anim_step(&skill->anim, souls->fish_head) == 3;
            skill->flame.x = skill->x;
            skill->flame.y = skill->y;
            if (!aos_flame_spirit_update(&skill->flame, bolt_done,
                                         skill_outside(souls, skill, 2), NULL)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->flame.x;
            skill->y = skill->flame.y;
            skill->facing_left = skill->flame.mirrored;
            break;
        }
        case SKILL_DEMON_LORD:
            /* SkillDemonLordUse: counted with song 0x1A4; at + 0x0D == 0x1D
             * it emits one SkillFlameDemonUpdate wisp (song 0x12A) and the
             * animation end deletes the caster. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A4);
                aos_anim_start(&skill->anim, souls->demon_lord, 0, false);
            }
            if (!skill->volley_count) {
                ++skill->timer;
                if (skill->timer >= 0x1D) {
                    skill->volley_count = 1;
                    AriaSkill *wisp = spawn_child(souls, SKILL_FLAME_SPIRIT);
                    if (wisp) {
                        wisp->soul = skill->soul;
                        place(wisp, skill->x, skill->y, skill->facing_left, 0, 0);
                        wisp->flame = aos_flame_spirit_create(skill->x, skill->y,
                                                            skill->facing_left);
                        wisp->x = wisp->flame.x;
                        wisp->y = wisp->flame.y;
                        wisp->facing_left = wisp->flame.mirrored;
                        aos_audio_play_song(0x12A);
                    }
                }
            }
            if (aos_anim_step(&skill->anim, souls->demon_lord) == 3)
                end_skill(souls, skill);
            break;
        case SKILL_BIPHRON: {
            /* SkillBiphronUse: state 0 places it at (+-0x14, -0xA) from
             * Soma's facing; sub_0802393C must find its floor or the cast
             * deletes itself, else + 0x24 = 0x8000, song 0x1A4 and
             * + 0x0D = 0x40. Each update ramps + 0x24 toward 0x1FFFF,
             * sub_0806CAF8 walks it at -+ 0x24 (0x084F13B4 probes, mode 4),
             * a wall result&3 flips + 0x58 bit 6, every eighth + 0x0D tick
             * (including the delete frame) emits a sub_080286BC totem
             * (song 0xD9 each), and + 0x0D == 0 deletes it. */
            if (!skill->started) {
                if (!aos_biphron_ground_ok(&skill->biphron, souls->layer)) {
                    end_skill(souls, skill);
                    break;
                }
                aos_biphron_land(&skill->biphron, souls->layer,
                                 &souls->biphron_kind->probes);
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A4);
                aos_anim_start(&skill->anim, souls->biphron, 0, false);
            }
            bool totem = false;
            bool alive = aos_biphron_update(&skill->biphron, souls->layer,
                                            &souls->biphron_kind->probes, &totem);
            if (totem) {
                AriaSkill *tot = spawn_child(souls, SKILL_BIPHRON_TOTEM);
                if (tot) {
                    tot->soul = skill->soul;
                    place(tot, skill->biphron.body.x, skill->biphron.body.y,
                          skill->biphron.body.mirrored, 0, 0);
                    tot->x = skill->biphron.body.x;
                    tot->y = skill->biphron.body.y;
                    tot->facing_left = skill->biphron.body.mirrored;
                    aos_audio_play_song(0xD9);
                }
            }
            if (aos_anim_step(&skill->anim, souls->biphron) == 3 || !alive)
                end_skill(souls, skill);
            skill->x = skill->biphron.body.x;
            skill->y = skill->biphron.body.y;
            skill->facing_left = skill->biphron.body.mirrored;
            break;
        }
        case SKILL_BIPHRON_TOTEM:
            /* sub_080286BC: stationary totem on the Biphron art, attack box
             * preset 5, song 0xD9 at spawn, deleted on the animation end.
             * Its + 0x5C = 0xF field has no exported sequence equivalent. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->biphron, 0, false);
            }
            if (aos_anim_step(&skill->anim, souls->biphron) == 3)
                end_skill(souls, skill);
            break;
        case SKILL_FLAME_SPIRIT: {
            /* SkillFlameDemonUpdate: the rotating wisp — velocity drift
             * capped at 0x80000, + 0x2C spin, a fire puff every eighth
             * frame, deleted on the animation end or the
             * sub_0806D128(e, 3) margin probe. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->demon_lord, 0, false);
            }
            skill->flame.x = skill->x;
            skill->flame.y = skill->y;
            bool puff = false;
            bool wisp_done = aos_anim_step(&skill->anim, souls->demon_lord) == 3;
            if (!aos_flame_spirit_update(&skill->flame, wisp_done,
                                         skill_outside(souls, skill, 3), &puff)) {
                end_skill(souls, skill);
                break;
            }
            if (puff && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_FLAME, .drift = true,
                    .x = (int16_t)((skill->flame.x >> 16) +
                                   (int16_t)(aos_random() % 0x100 - 0x80)),
                    .y = (int16_t)((skill->flame.y >> 16) +
                                   (int16_t)(aos_random() % 0x80 - 0x40))};
            skill->x = skill->flame.x;
            skill->y = skill->flame.y;
            skill->facing_left = skill->flame.mirrored;
            break;
        }
        case SKILL_GHOST:
            /* SkillGhostUse: one counted, invisible owner plays 0xD6 and
             * emits two sub_0802A49C spirits, then releases itself. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            aos_audio_play_song(0xD6);
            for (int k = 0; k < AOS_GHOST_SOULS; ++k) {
                AriaSkill *ghost = spawn_child(souls, SKILL_GHOST_SOUL);
                if (!ghost) break;
                ghost->soul = skill->soul;
                ghost->ghost = aos_ghost_soul_create(k, skill->x, skill->y, aos_random());
                ghost->x = ghost->ghost.x;
                ghost->y = ghost->ghost.y;
            }
            break;
        case SKILL_GHOST_SOUL: {
            AosGhostSoul *ghost = &skill->ghost;
            ghost->struck = skill->struck;
            if (ghost->state == 0) {
                aos_anim_start(&skill->anim, souls->devil, AOS_GHOST_SEQUENCE, false);
                ghost->state = 1;
            }
            if (ghost->state == 1) {
                if (aos_anim_step(&skill->anim, souls->devil) == 3) {
                    AosSkillTarget targets[4] = {{0}};
                    for (int slot = 0; slot < 4; ++slot) {
                        int index = homing.index[slot];
                        if (index < 0 || !homing_room ||
                            (size_t)index >= homing_room->entity_count)
                            continue;
                        const AriaEntity *entity = &homing_room->entities[index];
                        targets[slot] = (AosSkillTarget){
                            entity->spawned && !entity->enemy.removed,
                            entity->enemy.x, entity->enemy.y, entity->enemy.hp};
                    }
                    aos_ghost_soul_launch(ghost, targets);
                    skill->fire_target = ghost->target >= 0 ? homing.index[ghost->target] : -1;
                    skill->started = true;
                }
            } else {
                AosSkillTarget target = {0};
                if (skill->fire_target >= 0 && homing_room &&
                    (size_t)skill->fire_target < homing_room->entity_count) {
                    const AriaEntity *entity = &homing_room->entities[skill->fire_target];
                    target = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                              entity->enemy.x, entity->enemy.y,
                                              entity->enemy.hp};
                }
                aos_ghost_soul_update(ghost, &target, aos_random);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (aos_ghost_soul_outside(ghost, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = ghost->x;
            skill->y = ghost->y;
            break;
        }
        case SKILL_SIREN: {
            /* SkillSirenUse parameter 0: one counted note, sound 0xDD or
             * 0xDE, then the native horizontal and sine-wave motion.
             * sub_08042A54(1, 0xA) is not run. */
            AosSirenSoul *note = &skill->siren;
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                uint32_t variant_random = aos_random();
                uint32_t phase_random = aos_random();
                *note = aos_siren_soul_create(souls->soma->x, souls->soma->y,
                                              souls->soma->facing_left, variant_random,
                                              phase_random);
                aos_audio_play_song(note->song);
            }
            note->struck = skill->struck;
            int mx = souls->margins ? souls->margins[1][0] : 16;
            int my = souls->margins ? souls->margins[1][1] : 16;
            if (!aos_siren_soul_update(note, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = note->x;
            skill->y = note->y;
            skill->facing_left = false;
            break;
        }
        case SKILL_NEEDLES: {
            /* SkillNeedlesUse: counted with sound 0xE0 and looping sequence
             * 0 on its first update (sub_08042A54(1, 0xF) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xE0);
                aos_anim_start(&skill->anim, souls->needles, 0, true);
            }
            if (skill->needles.state == 1) aos_anim_step(&skill->anim, souls->needles);
            skill->needles.struck |= skill->struck;
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool burst;
            if (!aos_needles_update(&skill->needles, aos_random, souls->cam_x, souls->cam_y, mx,
                                    my, &burst)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045CEC(x, y, 15, 7): its dust. */
            if (burst && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_BURST_15, .drift = true, .x = skill->needles.x,
                    .y = skill->needles.y};
            skill->x = skill->needles.x;
            skill->y = skill->needles.y;
            break;
        }
        case SKILL_RIPPER: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            int16_t wall = 0;
            bool outside = false;
            if (skill->ripper.state == AOS_RIPPER_FLYING) {
                int32_t tested_x = skill->ripper.x;
                if (!skill->ripper.attached) {
                    tested_x = (int32_t)((uint32_t)tested_x + (uint32_t)skill->ripper.vx);
                    int x = tested_x >> 16, y = skill->ripper.y >> 16;
                    if (souls->layer)
                        wall = skill->ripper.facing_left
                                   ? (int16_t)aos_wall_push_right(souls->layer, x - 6, y)
                                   : (int16_t)aos_wall_push_left(souls->layer, x + 6, y);
                }
                int mx = souls->margins ? souls->margins[4][0] : 16;
                int my = souls->margins ? souls->margins[4][1] : 16;
                int sx = (tested_x >> 16) - souls->cam_x;
                int sy = (skill->ripper.y >> 16) - souls->cam_y;
                outside = sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
            }
            AosRipperTarget target = {0};
            const AosRipperTarget *target_ptr = NULL;
            if (skill->ripper_target) {
                target = (AosRipperTarget){.x = skill->ripper_target->x,
                                           .y = skill->ripper_target->y,
                                           .active = !skill->ripper_target->removed,
                                           .defeated = skill->ripper_target->defeated ||
                                                       skill->ripper_target->hp <= 0};
                target_ptr = &target;
            }
            if (!aos_ripper_soul_update(&skill->ripper, wall, outside, target_ptr)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->ripper.x;
            skill->y = skill->ripper.y;
            break;
        }
        case SKILL_MILLIONE_EMITTER: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            AosMillioneSpawn spawn;
            bool alive = aos_millione_soul_update(&skill->millione, souls->soma->x,
                                                   souls->soma->y,
                                                   souls->soma->facing_left, &spawn);
            skill->x = skill->millione.x;
            skill->y = skill->millione.y;
            skill->facing_left = skill->millione.facing_left;
            if (spawn.created) {
                AriaSkill *child = spawn_child(souls, SKILL_MILLIONE_CHILD);
                if (child) {
                    child->soul = skill->soul;
                    child->x = spawn.x;
                    child->y = spawn.y;
                    child->facing_left = spawn.facing_left;
                }
            }
            if (!alive || skill_outside(souls, skill, 3)) end_skill(souls, skill);
            break;
        }
        case SKILL_MILLIONE_CHILD:
            /* sub_0802B028: sequence 0 supplies both the visible frames
             * and its changing type-5 attack box. sub_08023530 already
             * counted this child when it was allocated. */
            if (skill->state == 2) {
                end_skill(souls, skill);
                break;
            }
            if (!skill->started) {
                skill->started = true;
                skill->state = 1;
                aos_anim_start(&skill->anim, souls->millione, 0, false);
            }
            if (aos_anim_step(&skill->anim, souls->millione) == 3) skill->state = 2;
            if (skill_outside(souls, skill, 2)) end_skill(souls, skill);
            break;
        case SKILL_FISH_HEAD: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->fish_head, 0, true);
            }
            skill->fish_head.struck |= skill->struck;
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (!aos_fish_head_soul_update(&skill->fish_head, souls->cam_x, souls->cam_y,
                                           mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->fish_head.x;
            skill->y = skill->fish_head.y;
            aos_anim_step(&skill->anim, souls->fish_head);
            break;
        }
        case SKILL_KILLER_FISH: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->killer_fish, 1, false);
            }
            AosKillerFishTarget targets[4] = {{0}};
            for (int slot = 0; slot < 4; ++slot) {
                int index = homing.index[slot];
                if (index < 0 || !homing_room ||
                    (size_t)index >= homing_room->entity_count)
                    continue;
                const AriaEntity *entity = &homing_room->entities[index];
                targets[slot] = (AosKillerFishTarget){
                    entity->spawned && !entity->enemy.removed,
                    entity->enemy.x, entity->enemy.y};
            }
            uint8_t old_state = skill->killer_fish.state;
            AosKillerFishEffect effect;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            bool alive = aos_killer_fish_soul_update(
                &skill->killer_fish, souls->layer, targets, 4,
                (skill->anim.flags & AOS_ANIM_ENDED) != 0, aos_random,
                souls->cam_x, souls->cam_y, mx, my, &effect);
            if (old_state == AOS_KILLER_FISH_APPEARING &&
                skill->killer_fish.state != AOS_KILLER_FISH_APPEARING)
                aos_anim_start(&skill->anim, souls->killer_fish, 0, true);
            if (effect.spawn && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_IMPACT, .drift = true,
                    .x = effect.x, .y = effect.y};
            if (!alive) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->killer_fish.x;
            skill->y = skill->killer_fish.y;
            skill->facing_left = skill->killer_fish.facing_left;
            aos_anim_step(&skill->anim, souls->killer_fish);
            break;
        }
        case SKILL_GLADIATOR: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->gladiator, 0, true);
            }
            AosGladiatorDust dust;
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool alive = aos_gladiator_soul_update(
                &skill->gladiator, souls->layer, &souls->gladiator_kind->probes,
                aos_random, souls->cam_x, souls->cam_y, mx, my, &dust);
            if (dust.spawn && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_FLAME, .drift = true, .x = dust.x, .y = dust.y};
            if (!alive) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->gladiator.body.x;
            skill->y = skill->gladiator.body.y;
            aos_anim_step(&skill->anim, souls->gladiator);
            break;
        }
        case SKILL_VALKYRIE: {
            /* SkillValkyrieUse: counted with sound 0x1AF and sequence 0 on
             * its first update (sub_08042A54(1, 0x3C) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1AF);
                aos_anim_start(&skill->anim, souls->valkyrie, 0, false);
            }
            AosValkyrieSpark spark;
            if (!aos_valkyrie_update(&skill->valkyrie, souls->cam_x, souls->cam_y, aos_random,
                                     &spark)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045B44(x, y, 0, ay, 0, 6), shown twice by its draw. */
            for (int copy = 0; spark.spawn && souls->effects && copy < 2 &&
                               *souls->effect_count < MAX_EFFECTS;
                 ++copy)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .timed = true,
                    .x = spark.x + (copy ? spark.offset_x : 0),
                    .y = spark.y + (copy ? spark.offset_y : 0), .vy = -0x10000, .ay = spark.ay,
                    .timer = 12};
            if (aos_anim_step(&skill->anim, souls->valkyrie) == 3)
                aos_valkyrie_animation_ended(&skill->valkyrie);
            break;
        }
        case SKILL_SKULL:
            /* SkillBeamSkeletonUse: counted with sound 0x1A9 and sequence 0
             * on its first update (sub_08042A54(1, 0x10) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A9);
                aos_anim_start(&skill->anim, souls->skull, 0, false);
            }
            if (!aos_skull_update(&skill->skull, souls->soma->x, souls->soma->y,
                                  skill->anim.frame, skill->anim.tick)) {
                end_skill(souls, skill);
                break;
            }
            if (aos_anim_step(&skill->anim, souls->skull) == 3) aos_skull_end(&skill->skull);
            skill->x = skill->skull.x;
            skill->y = skill->skull.y;
            break;
        case SKILL_DECOY:
            /* SkillKillerDollUse: counted with sound 0xE5 and looping
             * sequence 0 or 1 at random on its first update, the enemies'
             * aim (0x13168) while it exists; deleted when its time is out
             * or beyond the margins of entry 2. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xE5);
                aos_anim_start(&skill->anim, souls->killer_doll, aos_random() & 1, true);
            }
            if (!aos_decoy_update(&skill->decoy, souls->layer)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->decoy.x;
            skill->y = skill->decoy.y;
            if (skill_outside(souls, skill, 2)) {
                end_skill(souls, skill);
                break;
            }
            aos_anim_step(&skill->anim, souls->killer_doll);
            break;
        case SKILL_NEMESIS:
            /* SkillNemesisUse: counted with sound 0x97 and looping
             * sequence 2 of resource 21 on its first update, the enemies
             * without target (0x13168 = 0) while it exists; it rises over
             * Soma (sub_08031EA4) and is deleted after its blink. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x97);
                aos_anim_start(&skill->anim, souls->devil, AOS_NEMESIS_SOUL_SEQUENCE, true);
            }
            if (!aos_nemesis_soul_update(&skill->nemesis)) {
                end_skill(souls, skill);
                break;
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0,
                  aos_nemesis_soul_dy(&skill->nemesis));
            aos_anim_step(&skill->anim, souls->devil);
            break;
        case SKILL_LIGHTNING:
            /* SkillLightningDollUse: counted with sounds 0x1A9 (sub_080213F4
             * is false: 0x13220 is never set) and 0xE9 and sequence 0 once
             * on its first update; deleted when it ends or beyond the
             * screen margins of entry 2 (sub_0806D128). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A9);
                aos_audio_play_song(0xE9);
                aos_anim_start(&skill->anim, souls->lightning, 0, false);
            }
            aos_lightning_update(&skill->lightning, (uint8_t)skill->anim.frame,
                                 souls->soma->flags, souls->soma->x, souls->soma->y,
                                 souls->soma->facing_left);
            skill->x = skill->lightning.x;
            skill->y = skill->lightning.y;
            skill->facing_left = skill->lightning.facing_left;
            if (aos_anim_step(&skill->anim, souls->lightning) == 3 ||
                skill_outside(souls, skill, 2))
                end_skill(souls, skill);
            break;
        case SKILL_CROW:
            /* sub_SkillBlueCrowUse: counted with sound 0xEE on its first
             * update, looping sequence 2 (sub_08042A54(1, 0x14) is not
             * run); it circles Soma for 50 frames. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xEE);
                aos_anim_start(&skill->anim, souls->crow, AOS_CROW_SEQUENCE, true);
            }
            if (!aos_crow_update(&skill->crow, souls->soma->x, souls->soma->y)) {
                end_skill(souls, skill);
                break;
            }
            aos_anim_step(&skill->anim, souls->crow);
            skill->x = skill->crow.x;
            skill->y = skill->crow.y;
            break;
        case SKILL_DEVIL:
            /* SkillTinyDevilUse: counted, sound 0x1A3 and three
             * SkillTinyDevilUpdate fires from its position (sub_08042A54(1,
             * 0x14) is not run); deleted on its next update. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            aos_audio_play_song(0x1A3);
            for (int k = 0; k < AOS_DEVIL_FIRES; ++k) {
                AriaSkill *fire = spawn_child(souls, SKILL_DEVIL_FIRE);
                if (!fire) break;
                fire->soul = skill->soul;
                fire->fire = aos_devil_fire_create(k, skill->x, skill->y, skill->facing_left);
                fire->fire_target = -1;
                fire->x = skill->x;
                fire->y = skill->y;
            }
            break;
        case SKILL_DEVIL_FIRE: {
            AosDevilFire *fire = &skill->fire;
            fire->struck = skill->struck;
            if (fire->state == 0) {
                aos_anim_start(&skill->anim, souls->devil, 0, false);
                fire->state = 1;
            }
            if (fire->state == 1) {
                if (aos_anim_step(&skill->anim, souls->devil) == 3) {
                    /* The homing list's filled slots (0x13220 is never set
                     * by the runtime), the 0x134 sound; the two afterimage
                     * trails of sub_0802383C are not ported. */
                    bool slots[4];
                    for (int slot = 0; slot < 4; ++slot) slots[slot] = homing.index[slot] >= 0;
                    aos_devil_fire_launch(fire, aos_random(), slots);
                    skill->fire_target = fire->target >= 0 ? homing.index[fire->target] : -1;
                    aos_audio_play_song(0x134);
                    skill->started = true;
                }
            } else {
                AosSkillTarget target = {false, 0, 0, 0};
                if (skill->fire_target >= 0 && homing_room &&
                    (size_t)skill->fire_target < homing_room->entity_count) {
                    const AriaEntity *entity = &homing_room->entities[skill->fire_target];
                    target = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                              entity->enemy.x, entity->enemy.y, entity->enemy.hp};
                }
                aos_devil_fire_update(fire, &target);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (aos_devil_fire_outside(fire, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = fire->x;
            skill->y = fire->y;
            break;
        }
        case SKILL_HARPY:
            /* SkillHarpyUse: counted, then (same update) two sub_0802CAC0
             * daggers from its position (sound 0xE6 and sub_08042A54(1,
             * 0x14) are not played); deleted on its next update. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            for (int k = 0; k < AOS_HARPY_DAGGERS; ++k) {
                AriaSkill *dagger = spawn_child(souls, SKILL_DAGGER);
                if (!dagger) break;
                dagger->soul = skill->soul;
                dagger->dagger = aos_dagger_create(k, skill->x, skill->y);
                dagger->dagger_trail = aos_dagger_trail_create(&dagger->dagger);
                dagger->x = skill->x;
                dagger->y = skill->y;
                dagger->facing_left = dagger->dagger.facing_left;
            }
            break;
        case SKILL_DAGGER: {
            /* sub_0802CAC0, then its trail entity's sub_0802C944. */
            skill->started = true;
            skill->dagger.struck = skill->struck;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            if (!aos_dagger_update(&skill->dagger, souls->layer, souls->cam_x, souls->cam_y, mx,
                                   my)) {
                end_skill(souls, skill);
                break;
            }
            aos_dagger_trail_update(&skill->dagger_trail, &skill->dagger);
            skill->x = skill->dagger.x;
            break;
        }
        case SKILL_SHOT: {
            /* SkillDurgaUse: counted on its first update (sound 0xC7 and
             * sub_08042A54(1, 0x1E) are not played). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            if (!aos_shot_update(&skill->shot, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->shot.x;
            break;
        }
        case SKILL_SPEAR_TIP: {
            /* sub_08025F70: the spear's type-5 box at its tip, until the
             * spear reaches state 3 (sub_08042A54(1, 0x14) is not run). */
            const AriaSkill *parent = &souls->skills[skill->parent];
            if (!parent->alive || parent->kind != SKILL_SPEAR || parent->spear.state == 3) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            skill->x = parent->spear.x + parent->spear.tip_x;
            skill->y = parent->spear.y + parent->spear.tip_y;
            break;
        }
        case SKILL_AXE_TRAIL: {
            const AriaSkill *parent = &souls->skills[skill->parent];
            if (!skill->started) skill->started = true;
            /* The parent's slot may be taken again only after it was freed,
             * which happens after its state 2: a trail sees it first. */
            if (!parent->alive || parent->kind != SKILL_AXE ||
                !aos_axe_trail_update(&skill->trail, &parent->axe)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->trail.x;
            skill->y = skill->trail.y;
            break;
        }
        case SKILL_ARROW:
            /* sub_08028BC4 (aos_soul_red_arrow); the state and position are
             * copied back for the collision pass and the blink. */
            if (!skill->started) {
                skill->started = true;
                skill->arrow = aos_arrow_start(skill->x, skill->facing_left);
            }
            if (!aos_arrow_update(&skill->arrow, skill->struck)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->arrow.x;
            skill->vx = skill->arrow.vx;
            skill->state = skill->arrow.state;
            skill->timer = skill->arrow.timer;
            if (skill_outside(souls, skill, 2)) end_skill(souls, skill);
            break;
        default: break;
        }
    }
}

/* The skills' attack boxes this frame for the enemies' type-5 tests:
 * SkillBatUse enables its frame's box (frame record + 8) when it has one;
 * the arrow keeps 0x0202FF08 (8, -1, 2 x 2) until it has struck. The power
 * and elements come from the last used soul's record (sub_0802D244). */
int soul_attacks(AriaSouls *souls, const AosEnemyKind *bat, AosSoulAttack attacks[MAX_SKILLS],
                        AriaSkill *owners[MAX_SKILLS]) {
    int count = 0;
    if (!souls->records || souls->caster.last_used >= AOS_RED_SOUL_RECORDS) return 0;
    const AosRedSoulRecord *record = &souls->records[souls->caster.last_used];
    for (int i = 0; i < MAX_SKILLS; ++i) {
        AriaSkill *skill = &souls->skills[i];
        if (!skill->alive || !skill->started) continue;
        AosBox box = {0, 0, 0, 0};
        if (skill->kind == SKILL_BAT && bat && skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = bat->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_ARROW && skill->state == 1 && !skill->struck)
            box = (AosBox){8, -1, 2, 2};
        else if (skill->kind == SKILL_THROW && skill->thrown.attacking &&
                 skill->thrown.state != 3)
            /* 0x1010F8F8, the fused throws' 0x2838E2E4 once exploded, or
             * flag 0x200's still-frame box. */
            box = souls->throws[skill->parameter].flags & 0x60
                      ? (AosBox){-28, -30, 56, 40}
                      : souls->throws[skill->parameter].flags & 0x200
                            ? skill->thrown.frame_attack
                            : (AosBox){-8, -8, 16, 16};
        else if (skill->kind == SKILL_DROP && skill->drop.attacking)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SHOT)
            box = souls->durga_attack;          /* the still frame's, sub_08042848 */
        else if (skill->kind == SKILL_DAGGER)
            box = (AosBox){-1, -1, 2, 2};      /* 0x0202FFFF */
        else if (skill->kind == SKILL_FLEA && souls->flea_kind)
            box = skill->flea.state == 1
                      ? souls->flea_still_attack[skill->flea.frame]
                      : (skill->anim.frame < AOS_ENEMY_MAX_FRAMES
                             ? souls->flea_kind->attack[AOS_FLEA_END_SEQUENCE][skill->anim.frame]
                             : (AosBox){0, 0, 0, 0});
        else if (skill->kind == SKILL_NEEDLES && skill->needles.state != 3)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_RIPPER && skill->ripper.state == AOS_RIPPER_FLYING)
            box = skill->ripper.attached ? (AosBox){-10, -8, 20, 16} /* 0x1014F8F6 */
                                         : (AosBox){-1, -1, 2, 2};   /* 0x0202FFFF */
        else if (skill->kind == SKILL_MILLIONE_CHILD && souls->bone_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->bone_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_FISH_HEAD)
            box = (AosBox){-6, -6, 12, 12};    /* 0x0C0CFAFA */
        else if (skill->kind == SKILL_KILLER_FISH &&
                 skill->killer_fish.state != AOS_KILLER_FISH_ENDING)
            box = (AosBox){-24, -16, 48, 32};  /* 0x2030F0E8 */
        else if (skill->kind == SKILL_GLADIATOR)
            box = (AosBox){-16, -16, 32, 32};  /* 0x2020F0F0 */
        else if (skill->kind == SKILL_VALKYRIE && souls->valkyrie_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->valkyrie_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_WEREWOLF && souls->werewolf_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->werewolf_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_KILLER_MANTLE)
            box = (AosBox){-8, -8, 16, 16};    /* preset 5, approximated */
        else if (skill->kind == SKILL_WERE_BEAST || skill->kind == SKILL_BALORE ||
                 skill->kind == SKILL_LEGION)
            box = (AosBox){-8, -8, 16, 16};    /* preset 5, approximated */
        else if (skill->kind == SKILL_FLAME_BOLT || skill->kind == SKILL_FLAME_SPIRIT)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 / preset 5 */
        else if (skill->kind == SKILL_BIPHRON || skill->kind == SKILL_BIPHRON_TOTEM)
            box = (AosBox){-8, -8, 16, 16};    /* preset 5, approximated */
        else if (skill->kind == SKILL_LIGHTNING)
            box = aos_lightning_box(&skill->lightning);
        else if (skill->kind == SKILL_SKULL && souls->skull_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->skull_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_CROW && souls->crow_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->crow_kind->attack[AOS_CROW_SEQUENCE][skill->anim.frame];
        else if (skill->kind == SKILL_GHOST_SOUL && skill->ghost.state >= 2)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SIREN && skill->siren.state == 1)
            box = (AosBox){-4, -4, 8, 8};      /* 0x0808FCFC */
        else if (skill->kind == SKILL_DEVIL_FIRE && skill->fire.state >= 2)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SPEAR_TIP)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_DRYAD && skill->dryad.state != 4)
            /* 0x0202FFFF flying, the planted 0x1014F8F6 after the
             * strike (sub_08042884 replaces the box). */
            box = skill->dryad.planted ? (AosBox){-10, -8, 20, 16}
                                       : (AosBox){-1, -1, 2, 2};
        else if ((skill->kind == SKILL_ARACHNE || skill->kind == SKILL_MUDMAN) &&
                 skill->arachne.state <= 3)
            /* Preset 5 (sub_0804277C + sub_0802DB84); the state-1
             * +0x64/+0x65 frame lookup stands in with the kind's
             * per-frame boxes when exported. */
            box = (AosBox){-8, -8, 16, 16};
        else if (skill->kind == SKILL_MINOTAUR_BLADE &&
                 skill->minotaur_blade.attacking)
            box = (AosBox){-32, -32, 64, 64};  /* 0x4040E0E0 */
        else if (skill->kind == SKILL_NIGHTMARE)
            box = (AosBox){-8, -8, 16, 16};    /* frame box, approximated */
        else if (skill->kind == SKILL_MAN_EATER && skill->man_eater.state <= 4)
            box = (AosBox){-6, -6, 12, 12};    /* 0x0C0CFAFA */
        else if (skill->kind == SKILL_AXE && skill->axe.state == 1)
            box = (AosBox){-10, -10, 20, 20};  /* 0x1414F6F6 */
        if (!box.w || !box.h) continue;
        owners[count] = skill;
        attacks[count++] = (AosSoulAttack){
            aos_entity_rect(box, skill->x >> 16, skill->y >> 16, skill->facing_left, false),
            aos_red_soul_power(souls->stats, record, false), record->elements, false, NULL,
            AOS_TYPE_SOUL};
    }
    return count;
}

/* A room change deletes the skill entities with the room's. */
void clear_skills(AriaSouls *souls) {
    for (int i = 0; i < MAX_SKILLS; ++i) souls->skills[i].alive = false;
    homing_clear();
    souls->caster = (AosRedSoulCaster){.last_used = souls->caster.last_used};
}
