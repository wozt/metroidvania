/* SPDX-License-Identifier: GPL-3.0-only */
/* Skills/souls/guardian cluster of the Aria of Sorrow room runtime,
 * extracted from aria_runtime.c (AGENTS.md rule 8): the red-soul cast
 * hook and skill-entity update (spawn_child and the per-kind helpers),
 * the attack-box collection for the enemies' type-5 tests, and the
 * blue-soul guardian entity (start/update/attack hooks). The AriaSkill,
 * AriaSouls, AriaGuardian and the SKILL_/GUARDIAN_ declarations live here
 * because the runtime's frame loop and world draw still read them;
 * AriaCombat/AriaPlayerStats stay in aria_runtime.c (entity cluster). */
#ifndef ARIA_SKILLS_H
#define ARIA_SKILLS_H

/* aos_opening_event.h first: aria_runtime_types.h's AriaObjects uses
 * AOS_OPENING_ACTOR_COUNT from it (same include order as
 * aria_draw.h / aria_runtime.c). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"

#include "aos/core/aos_audio.h"
#include "aos/core/aos_collision.h"
#include "aos/core/aos_combat.h"
#include "aos/core/aos_soma.h"
#include "aos/core/aos_stats.h"
#include "aos/souls/aos_soul.h"
#include "aos/red_souls/aos_soul_red_axe.h"
#include "aos/red_souls/aos_soul_red_arrow.h"
#include "aos/red_souls/aos_soul_red_crow.h"
#include "aos/red_souls/aos_soul_red_dagger.h"
#include "aos/red_souls/aos_soul_red_flea.h"
#include "aos/red_souls/aos_soul_red_fish_head.h"
#include "aos/red_souls/aos_soul_red_killer_fish.h"
#include "aos/red_souls/aos_soul_red_gladiator.h"
#include "aos/red_souls/aos_soul_red_werewolf.h"
#include "aos/red_souls/aos_soul_red_killer_mantle.h"
#include "aos/red_souls/aos_soul_red_were_beast.h"
#include "aos/red_souls/aos_soul_red_flame_demon.h"
#include "aos/red_souls/aos_soul_red_devil.h"
#include "aos/red_souls/aos_soul_red_ghost.h"
#include "aos/red_souls/aos_soul_red_siren.h"
#include "aos/red_souls/aos_soul_red_merman.h"
#include "aos/red_souls/aos_soul_red_needles.h"
#include "aos/red_souls/aos_soul_red_lightning.h"
#include "aos/red_souls/aos_soul_red_killer_doll.h"
#include "aos/red_souls/aos_soul_red_nemesis.h"
#include "aos/red_souls/aos_soul_red_ripper.h"
#include "aos/red_souls/aos_soul_red_millione.h"
#include "aos/red_souls/aos_soul_red_shot.h"
#include "aos/red_souls/aos_soul_red_skull.h"
#include "aos/red_souls/aos_soul_red_valkyrie.h"
#include "aos/red_souls/aos_soul_red_spear.h"
#include "aos/red_souls/aos_soul_red_throw.h"
#include "aos/red_souls/aos_soul_red_biphron.h"
#include "aos/red_souls/aos_soul_red_kyoma_demon.h"
#include "aos/red_souls/aos_soul_red_student_witch.h"
#include "aos/red_souls/aos_soul_red_slime.h"
#include "aos/red_souls/aos_soul_red_ukoback.h"
#include "aos/red_souls/aos_soul_red_altair.h"
#include "aos/red_souls/aos_soul_red_une.h"
#include "aos/red_souls/aos_soul_red_mandragora.h"
#include "aos/red_souls/aos_soul_red_chronomage.h"
#include "aos/red_souls/aos_soul_red_disc_armor.h"
#include "aos/red_souls/aos_soul_red_cockatrice.h"
#include "aos/red_souls/aos_soul_red_dryad.h"
#include "aos/red_souls/aos_soul_red_arachne.h"
#include "aos/red_souls/aos_soul_red_minotaur.h"
#include "aos/red_souls/aos_soul_red_nightmare.h"
#include "aos/red_souls/aos_soul_red_man_eater.h"
#include "aos/blue_souls/aos_soul_blue_imp.h"
#include "aos/blue_souls/aos_soul_blue_death.h"
#include "aos/blue_souls/aos_soul_blue_ghost.h"
#include "aos/blue_souls/aos_soul_blue_sky_fish.h"
#include "aos/blue_souls/aos_soul_blue_creaking_skull.h"
#include "aos/blue_souls/aos_soul_blue_alastor.h"
#include "aos/blue_souls/aos_soul_blue_giant_bat.h"
#include "aos/blue_souls/aos_soul_blue_final_guard.h"

/* A red soul skill entity (EntityCreateInRange(2, 0x10)): SkillBatUse,
 * SkillSkullArcherUse's bow and its arrow sub_08028BC4 are ported. */
enum { SKILL_BAT, SKILL_BOW, SKILL_ARROW, SKILL_THROW, SKILL_SPOUT, SKILL_DROP, SKILL_AXE,
       SKILL_AXE_TRAIL, SKILL_SPEAR, SKILL_SPEAR_TIP, SKILL_SHOT, SKILL_HARPY, SKILL_DAGGER,
       SKILL_DEVIL, SKILL_DEVIL_FIRE, SKILL_CROW, SKILL_FLEA, SKILL_GHOST,
       SKILL_GHOST_SOUL, SKILL_SIREN, SKILL_SKULL, SKILL_VALKYRIE, SKILL_NEEDLES,
       SKILL_RIPPER, SKILL_MILLIONE_EMITTER, SKILL_MILLIONE_CHILD, SKILL_FISH_HEAD,
       SKILL_KILLER_FISH, SKILL_GLADIATOR, SKILL_LIGHTNING, SKILL_DECOY, SKILL_NEMESIS,
       SKILL_WEREWOLF, SKILL_KILLER_MANTLE, SKILL_WERE_BEAST,
       SKILL_FLAME_DEMON, SKILL_FLAME_BOLT, SKILL_DEMON_LORD, SKILL_FLAME_SPIRIT,
       SKILL_BIPHRON, SKILL_BIPHRON_TOTEM, SKILL_BALORE, SKILL_KYOMA_DEMON,
       SKILL_LEGION, SKILL_STUDENT_WITCH, SKILL_SLIME, SKILL_SLIME_TRAIL, SKILL_UKOBACK, SKILL_ALTAIR, SKILL_UNE, SKILL_MANDRAGORA, SKILL_CHRONOMAGE, SKILL_DISC_ARMOR, SKILL_COCKATRICE, SKILL_DRYAD, SKILL_ARACHNE, SKILL_MUDMAN, SKILL_ARACHNE_SHARD, SKILL_RED_MINOTAUR, SKILL_MINOTAUR_BLADE, SKILL_NIGHTMARE, SKILL_MAN_EATER };

typedef struct {
    bool alive, started;
    int kind;
    int slot;               /* + 0x1A */
    uint8_t soul;           /* + 0x1C: the soul it was cast with */
    int32_t x, y;           /* room pixels, 16.16 */
    int32_t vx;             /* + 0x48 */
    bool facing_left;
    uint8_t state, timer;   /* + 0x0A, + 0x0D */
    bool struck;            /* + 0x20 set by the arrow's hit callback */
    AosAnimState anim;
    AosThrow thrown;        /* SKILL_THROW: the whole entity */
    AosMermanDrop drop;     /* SKILL_DROP: the whole entity */
    AosAxe axe;             /* SKILL_AXE */
    AosAxeTrail trail;      /* SKILL_AXE_TRAIL */
    AosSpear spear;         /* SKILL_SPEAR */
    AosShot shot;           /* SKILL_SHOT */
    AosArrow arrow;         /* SKILL_ARROW */
    AosDagger dagger;       /* SKILL_DAGGER, with its trail entity */
    AosDevilFire fire;      /* SKILL_DEVIL_FIRE */
    AosCrow crow;           /* SKILL_CROW */
    AosFlea flea;           /* SKILL_FLEA */
    AosGhostSoul ghost;     /* SKILL_GHOST_SOUL */
    AosSirenSoul siren;     /* SKILL_SIREN */
    AosSkull skull;         /* SKILL_SKULL */
    AosValkyrie valkyrie;   /* SKILL_VALKYRIE */
    AosNeedles needles;     /* SKILL_NEEDLES */
    AosRipperSoul ripper;   /* SKILL_RIPPER */
    AosMillioneSoul millione; /* SKILL_MILLIONE_EMITTER */
    AosFishHeadSoul fish_head; /* SKILL_FISH_HEAD */
    AosKillerFishSoul killer_fish; /* SKILL_KILLER_FISH */
    AosGladiatorSoul gladiator; /* SKILL_GLADIATOR */
    AosLightning lightning; /* SKILL_LIGHTNING */
    AosDecoy decoy;         /* SKILL_DECOY */
    AosNemesisSoul nemesis; /* SKILL_NEMESIS */
    AosWerewolf werewolf;   /* SKILL_WEREWOLF */
    AosKillerMantle killer_mantle; /* SKILL_KILLER_MANTLE */
    AosWereBeast were_beast;   /* SKILL_WERE_BEAST */
    AosFlameSpirit flame;     /* SKILL_FLAME_BOLT / SKILL_FLAME_SPIRIT */
    AosBiphron biphron;       /* SKILL_BIPHRON */
    AosKyomaDemon kyoma_demon; /* SKILL_KYOMA_DEMON */
    AosStudentWitch student_witch; /* SKILL_STUDENT_WITCH */
    AosSlime slime;           /* SKILL_SLIME */
    AosSlimeTrail slime_trail; /* SKILL_SLIME_TRAIL */
    AosUkoback ukoback;       /* SKILL_UKOBACK */
    AosAltair altair;         /* SKILL_ALTAIR */
    AosUne une;             /* SKILL_UNE */
    AosMandragora mandragora; /* SKILL_MANDRAGORA */
    AosChronomage chronomage; /* SKILL_CHRONOMAGE */
    AosDiscArmor disc_armor;   /* SKILL_DISC_ARMOR */
    AosCockatrice cockatrice; /* SKILL_COCKATRICE */
    AosDryad dryad;           /* SKILL_DRYAD */
    AosEnemy *dryad_host;     /* its + 0x20 child's + 0x20: the struck enemy */
    AosArachneSoul arachne;   /* SKILL_ARACHNE / SKILL_MUDMAN */
    AosArachneShard shard;    /* SKILL_ARACHNE_SHARD */
    AosRedMinotaur red_minotaur; /* SKILL_RED_MINOTAUR */
    AosMinotaurBlade minotaur_blade; /* SKILL_MINOTAUR_BLADE */
    AosNightmare nightmare;   /* SKILL_NIGHTMARE */
    AosManEater man_eater;     /* SKILL_MAN_EATER */
    AosEnemy *arachne_host;   /* the sub_0802DB84 child's + 0x20 target */
    int shard_parent;         /* the releasing skill's slot (-1 none) */
    uint8_t volley_count;      /* SKILL_DEMON_LORD spawn gate */
    AosEnemy *ripper_target; /* sub_080239E4's +0x20 */
    int fire_target;        /* its + 0x20: the room entity chased, -1 for none */
    AosDaggerTrail dagger_trail;
    int parent;             /* + 0x14: a child's parent slot */
    uint8_t parameter;      /* + 0x1E: the record's parameter */
} AriaSkill;

#define MAX_SKILLS AOS_RED_SOUL_SLOTS
#define SKILL_BAT_ROUTINE 0x08026C55u
#define SKILL_ARCHER_ROUTINE 0x08028D69u
#define SKILL_THROW_ROUTINE 0x08025785u
#define SKILL_MERMAN_ROUTINE 0x08026F75u
#define SKILL_AXE_ROUTINE 0x08024275u
#define SKILL_SPEAR_ROUTINE 0x08026009u
#define SKILL_DURGA_ROUTINE 0x0802B809u
#define SKILL_HARPY_ROUTINE 0x0802CC85u
#define SKILL_DEVIL_ROUTINE 0x08029FB1u
#define SKILL_CROW_ROUTINE 0x0802C489u
#define SKILL_FLEA_ROUTINE 0x080299A1u
#define SKILL_GHOST_ROUTINE 0x0802A8D5u
#define SKILL_SIREN_ROUTINE 0x08026369u
#define SKILL_SKULL_ROUTINE 0x080289B5u
#define SKILL_VALKYRIE_ROUTINE 0x08024CBDu
#define SKILL_NEEDLES_ROUTINE 0x080280B1u
#define SKILL_RIPPER_ROUTINE 0x08023A71u
#define SKILL_MILLIONE_ROUTINE 0x0802B1B9u
#define SKILL_FISH_HEAD_ROUTINE 0x0802B351u
#define SKILL_KILLER_FISH_ROUTINE 0x08027C59u
#define SKILL_GLADIATOR_ROUTINE 0x0802B4C9u
#define SKILL_LIGHTNING_ROUTINE 0x080277B1u
#define SKILL_KILLER_DOLL_ROUTINE 0x08024F39u
#define SKILL_NEMESIS_ROUTINE 0x080251F1u
#define SKILL_WEREWOLF_ROUTINE 0x080284FCu
#define SKILL_KILLER_MANTLE_ROUTINE 0x0802B9A8u
#define SKILL_WERE_BEAST_ROUTINE 0x08027608u
#define SKILL_FLAME_DEMON_ROUTINE 0x080293C4u
#define SKILL_DEMON_LORD_ROUTINE 0x0802BF68u
#define SKILL_BIPHRON_ROUTINE 0x08028800u
#define SKILL_BALORE_ROUTINE 0x0802C2C4u
#define SKILL_KYOMA_DEMON_ROUTINE 0x0802BB44u
#define SKILL_LEGION_ROUTINE 0x0802C0D4u
#define SKILL_STUDENT_WITCH_ROUTINE 0x08025360u
#define SKILL_SLIME_ROUTINE 0x0802CFC4u
#define SKILL_UKOBACK_ROUTINE 0x0802A0DCu
#define SKILL_ALTAIR_ROUTINE 0x08027A04u
#define SKILL_UNE_ROUTINE 0x080265C0u
#define SKILL_MANDRAGORA_ROUTINE 0x0802AD78u
#define SKILL_CHRONOMAGE_ROUTINE 0x08024950u
#define SKILL_DISC_ARMOR_ROUTINE 0x08027340u
#define SKILL_COCKATRICE_ROUTINE 0x0802968Cu
#define SKILL_DRYAD_ROUTINE 0x080282DDu
#define SKILL_ARACHNE_ROUTINE 0x08028EC1u
#define SKILL_RED_MINOTAUR_ROUTINE 0x080269BDu
#define SKILL_NIGHTMARE_ROUTINE 0x0802A9DCu
#define SKILL_MAN_EATER_ROUTINE 0x0802C6EDu

/* What a cast needs: the soul state and where its entity goes. */
typedef struct {
    AosStats *stats;
    AosRedSoulCaster caster;
    const AosRedSoulRecord *records;
    AriaSkill skills[MAX_SKILLS];
    const AosSoma *soma;
    const AosAnimSet *bat, *bow, *merman; /* NULL when not exported */
    const AosThrowRecord *throws;   /* metadata/skill_throws.tsv, NULL when missing */
    bool bone;                      /* resource 7 (Enemy/skill_bone) is loaded */
    const AosEnemyKind *bone_kind;  /* its walker probes and still frames */
    const AosEnemyKind *ripper_kind; /* resource 1 and probes 0x084F139C */
    const AosAnimSet *millione;     /* resource 7, sequence 0 */
    const AosAnimSet *fish_head;    /* resource 17, sequence 0 */
    const AosAnimSet *killer_fish;  /* resource 11, sequences 0-1 */
    const AosAnimSet *gladiator;    /* resource 24, sequence/frame 0 */
    const AosAnimSet *lightning;    /* resource 9, sequence 0 */
    const AosAnimSet *werewolf;     /* resource 14, sequence 0 */
    const AosAnimSet *killer_mantle;  /* skill_killer_mantle, sequence 0 */
    const AosAnimSet *were_beast;    /* skill_were_beast, sequence 0 */
    const AosAnimSet *demon_lord;    /* skill_demon_lord, sequence 0 */
    const AosEnemyKind *werewolf_kind; /* its 0x084F13B4 walker probes */
    const AosEnemyKind *une_kind;     /* its 0x084F13AA walker probes */
    const AosAnimSet *biphron;        /* skill_biphron, sequence 0 */
    const AosEnemyKind *biphron_kind; /* its 0x084F13B4 walker probes */
    const AosAnimSet *balore;         /* skill_balore, sequence 0 */
    const AosAnimSet *kyoma_demon;   /* skill_kyoma_demon, sequence 0 */
    const AosAnimSet *legion;         /* skill_legion, sequence 0 */
    const AosAnimSet *student_witch; /* skill_student_witch, sequence 0 */
    const AosEnemyKind *witch_kind;  /* its 0x084F12BC walker probes */
    const AosAnimSet *slime;         /* skill_slime, sequence 0 */
    const AosAnimSet *ukoback;       /* skill_ukoback, sequence 0 */
    const AosAnimSet *altair;         /* skill_altair, sequences 0-2 */
    const AosAnimSet *une;           /* skill_une, sequence 0 */
    const AosAnimSet *mandragora;     /* skill_mandragora, sequences 0-1 */
    const AosAnimSet *chronomage;     /* skill_chronomage art, sequence 0 */
    const AosAnimSet *disc_armor;     /* skill_disc_armor, sequence 0 */
    const AosAnimSet *cockatrice;     /* skill_cockatrice, sequence 0 */
    const AosAnimSet *dryad;          /* skill_dryad still frame 0 */
    const AosAnimSet *arachne;        /* skill_arachne sequences */
    const AosAnimSet *red_minotaur;   /* skill_red_minotaur sequences */
    const AosAnimSet *nightmare;      /* skill_nightmare sequences */
    const AosAnimSet *man_eater;       /* skill_man_eater stills */
    const AosAnimSet *killer_doll;  /* resource 6, sequences 0-1 */
    const AosEnemyKind *gladiator_kind; /* its probes at 0x084F13C8 */
    const int *bone_ids;            /* its exported still frames */
    int bone_count;
    const int *ripper_ids;
    int ripper_count;
    AosEffect *effects;             /* the room's shared effects */
    size_t *effect_count;
    const AosEffect *burst;         /* burst list 0 of sub_08045A98 */
    size_t burst_count;
    const AosCollision *layer;      /* the room's BG1 collision */
    bool durga;                     /* resource 24's still frame 2 is loaded */
    bool harpy;                     /* and its frames 1 and 3-5 */
    const AosAnimSet *devil;        /* resource 21's sequence 0, NULL when missing */
    const AosAnimSet *crow;         /* resource 31, NULL when missing */
    const AosEnemyKind *crow_kind;  /* its frame boxes */
    const AosAnimSet *skull;        /* resource 19, NULL when missing */
    const AosEnemyKind *skull_kind; /* its frame boxes */
    const AosAnimSet *valkyrie;     /* resource 2, NULL when missing */
    const AosEnemyKind *valkyrie_kind;
    const AosAnimSet *needles;      /* resource 12 with its four piece frames */
    bool ripper;                    /* both still frames of 0x084F128C */
    const AosAnimSet *flea;         /* resource 20, NULL when missing */
    const AosEnemyKind *flea_kind;  /* its probes and frame boxes */
    const AosBox *flea_still_attack;   /* the attack boxes of its still frames */
    AosBox durga_attack;            /* that frame's attack box */
    const int *shared_ids;          /* resource 0's exported still frames */
    int shared_count;
    const int *archer_ids;          /* resource 18's exported still frames */
    int archer_count;
    const AosBox *archer_attacks;   /* their static_attack rows, by frame id */
    const int16_t (*margins)[2];    /* sub_0806D128's table */
    int cam_x, cam_y;
} AriaSouls;

/* The blue soul guardian (gEwramData + 0x13144) and its routine. */
#define GUARDIAN_MEDUSA_ROUTINE 0x08030CC9u
#define GUARDIAN_CAGNAZZO_ROUTINE 0x0802E4EDu
#define GUARDIAN_BUER_ROUTINE 0x0802FF41u
#define GUARDIAN_CATOBLEPAS_ROUTINE 0x08030E65u
#define GUARDIAN_PILLAR_ROUTINE 0x0802E731u
#define GUARDIAN_IMP_ROUTINE 0x080310ADu
#define GUARDIAN_TRANSFORM_ROUTINE 0x0802EFB1u
#define GUARDIAN_GHOST_ROUTINE 0x0802F96Du
#define GUARDIAN_PANTHER_ROUTINE 0x0802E3DDu
#define GUARDIAN_FLYING_ARMOR_ROUTINE 0x0802E23Du
#define GUARDIAN_DEATH_ROUTINE 0x08030519u
#define GUARDIAN_PERSEPHONE_ROUTINE 0x080300A5u
#define GUARDIAN_SKY_FISH_ROUTINE 0x0802F1A5u
#define GUARDIAN_CREAKING_SKULL_ROUTINE 0x08030A25u
#define GUARDIAN_ALASTOR_ROUTINE 0x080314ADu
#define GUARDIAN_GIANT_BAT_ROUTINE 0x0802EB85u
#define GUARDIAN_FINAL_GUARD_ROUTINE 0x0802FC5Du
#define GUARDIAN_GREAT_ARMOR_ROUTINE 0x08031949u
#define GUARDIAN_SHADOW_KNIGHT_ROUTINE 0x0802F549u
#define GUARDIAN_ALURA_UNE_ROUTINE 0x0802E961u
#define GUARDIAN_BIG_GOLEM_ROUTINE 0x08031ACDu
enum { GUARDIAN_ATTACKS = 8 };
typedef struct {
    AosGuardian core;
    uint32_t routine;
    AosMedusaGuardian medusa;
    AosCagnazzoGuardian cagnazzo;
    AosBuerGuardian buer;
    AosCatoblepasGuardian catoblepas;
    AosImpGuardian imp;
    AosCurlyGuardian curly;
    AosGhostGuardian ghost;
    AosPantherGuardian panther;
    AosFlyingArmorGuardian flying_armor;
    AosDeathGuardian death;
    AosPersephoneGuardian persephone;
    AosGreatArmorGuardian great_armor;
    AosShadowKnightGuardian shadow_knight;
    AosAluraUneGuardian alura_une;
    AosSkyFishGuardian sky_fish;
    AosCreakingSkullGuardian creaking_skull;
    AosAlastorGuardian alastor;
    AosGiantBatGuardian giant_bat;
    AosFinalGuardGuardian final_guard;
    bool anim_ended;             /* the guardian's sequence ended (+ 0x6C bit 2) */
    struct {
        bool alive;
        int32_t x, y;            /* + 0x40, + 0x44 */
        bool facing_left;
        uint8_t sequence;        /* r & 3 */
        AosAnimState anim;
    } sparks[7];                 /* sub_0802ECF4 in the guardian slots 1-7 */
    const AosAnimSet *spark_art; /* Enemy/skill_devil_spark, NULL when missing */
    int effect_count;            /* effects of this update for the room */
    AosEffect effects[2];
    int imp_target;              /* the room entity of the chased homing slot */
    AosAnimState anim;           /* Cagnazzo, or both Buer flames (same steps) */
    AosAnimState gas_anims[AOS_CATOBLEPAS_GASES];
    AosStats *stats;
    const AosBlueSoulRecord *records;
    AosSoma *soma;
    const AosAnimSet *cagnazzo_art;  /* Enemy/skill_cagnazzo, NULL when missing */
    const AosAnimSet *buer_art;      /* Enemy/skill_buer, NULL when missing */
    const AosAnimSet *catoblepas_art; /* Enemy/skill_catoblepas, NULL when missing */
    const AosAnimSet *pillar_art;    /* Enemy/skill_bone_pillar, NULL when missing */
    const AosAnimSet *imp_art;       /* Enemy/skill_imp, NULL when missing */
    const AosAnimSet *curly_art;     /* Enemy/skill_curly, NULL when missing */
    const AosAnimSet *ghost_art;     /* Enemy/skill_giant_ghost, NULL when missing */
    const AosAnimSet *witch_art;     /* Enemy/skill_witch, NULL when missing */
    const AosAnimSet *flying_armor_art; /* Enemy/skill_flying_armor, NULL when missing */
    const AosAnimSet *death_art;     /* Enemy/skill_death, NULL when missing */
    const AosAnimSet *persephone_art; /* Enemy/skill_persephone, NULL when missing */
    const AosEnemyKind *persephone_kind; /* its frame attack boxes */
    const AosAnimSet *great_armor_art; /* Enemy/skill_great_armor, NULL when missing */
    const AosAnimSet *shadow_knight_art; /* Enemy/skill_shadow_knight, NULL when missing */
    const AosAnimSet *alura_une_art; /* Enemy/skill_alura_une, NULL when missing */
    const AosAnimSet *sky_fish_art;  /* Enemy/skill_sky_fish, NULL when missing */
    const AosAnimSet *creaking_skull_art; /* Enemy/skill_creaking_skull */
    const AosAnimSet *alastor_art;     /* Enemy/skill_alastor */
    const AosAnimSet *final_guard_art; /* Enemy/skill_final_guard */
    const AosAnimSet *big_golem_art; /* Enemy/skill_big_golem, NULL when missing */
    const AosEnemyKind *big_golem_kind; /* its frame attack boxes */
    const AosEnemyKind *shadow_knight_kind; /* its frame attack boxes */
    const int16_t (*margins)[2];     /* the screen margins of sub_0806D128 */
    const AosEnemyKind *imp_kind;    /* its frame attack boxes */
} AriaGuardian;

/* The homing list gEwramData + 0x13194 shared with the entity update
 * (homing_register/homing_age stay in aria_runtime.c): the skills and
 * the guardian read its slots for their chase targets. */
typedef struct {
    int index[4];
    uint8_t seen[4], aged[4];
} AriaHomingList;
extern AriaHomingList homing;
extern const AriaRoom *homing_room;

/* The camera the guardian's ghosts measure their screen margin against;
 * written by the frame loop each frame. */
extern int guardian_cam_x, guardian_cam_y;

/* Entry points the runtime's frame loop and Soma's hooks call. */
int cast_red_soul(void *context);
void start_guardian(void *context);
void cancel_transform(void *context);
void update_guardian(AriaGuardian *guardian);
int guardian_attacks(const AriaGuardian *guardian,
                     AosSoulAttack attacks[GUARDIAN_ATTACKS]);
void update_skills(AriaSouls *souls);
int soul_attacks(AriaSouls *souls, const AosEnemyKind *bat,
                 AosSoulAttack attacks[MAX_SKILLS], AriaSkill *owners[MAX_SKILLS]);
void clear_skills(AriaSouls *souls);
void homing_clear(void);

#endif
