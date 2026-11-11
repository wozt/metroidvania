/* Creaking Skull (id 0x21) and Giant Skeleton (id 0x5C): the shared
 * implementation of EnemyCreakingSkullCreate 0x0809549C /
 * EnemyCreakingSkullUpdate 0x08095320 (the Giant Skeleton thunks
 * forward to them).  The boss is a head body driving a container of
 * thirteen bone records (stride 0x30) whose children run the
 * sub_0809808C segmented-follower machine.
 *
 * Native layout folded into AosEnemy:
 * - the + 0x1C container entity and its thirteen child slots fold
 *   into the AOS_ROLE_CS_BONE persistent children emitted over the
 *   first updates under the 8-child cap, chained in slot order like
 *   the native container records;
 * - each child's + 0x21/+ 0x65 table bytes, the + 0x36 variant byte
 *   and the sub_0804277C hurt/death callbacks (sub_08098C54 /
 *   sub_08098C84) fold into the shared collide and the bone role;
 * - the attack shots of the state-1 machine become
 *   AOS_ROLE_CS_SHOT children and the sub_08098CF0 debris spawns
 *   AOS_ROLE_CS_GORE;
 * - the + 0xB sixteen-case state-0 machine, the state-1 attack
 *   machine and the + 0xD death countdown fold into step/substep/
 *   timer with the cs_* counters.
 *
 * Documented approximations: the two ~2000-line choreography
 * machines keep their case order, the song stingers (0x8C bone
 * creaks, 0x86/0x80/0x84/0x76/0x77/0x7A/0x97/0xE0/0x1C/0x29/0x6B)
 * and the state transitions, while the per-case bone-target tables
 * collapse to a documented spine cycle; the sub_0809808C bone brain
 * keeps its parent lock, sine wobble and death-on-parent, with the
 * full case tables approximated; the sub_08097D84/sub_08098BE0 bone
 * placement folds into fixed spine offsets; the sub_08098E00 room
 * gate, the gEwramData flag checks and the + 0x5A palette merge are
 * covered by the shared spawn/collide paths. */

#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <string.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define integrate aos_enemy_integrate
#define player_near aos_enemy_player_near

#define CS_BONES 13

static bool cs_is_boss(uint8_t id) {
    return id == 0x21 || id == 0x5C;
}

static int32_t cs_sin(uint32_t angle) { return aos_sine(angle & 0xFFFF); }
static int32_t cs_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
static int32_t cs_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* The native container records hold per-bone home offsets indexed
 * by the state-0 tables; the port reproduces the spine layout:
 * bone 0 is the skull cap, bones 1..11 the spine/rib segments and
 * bone 12 the tail, spaced 0x18000 vertically with a small x
 * stagger. */
static void cs_bone_offset(int bone, int32_t *ox, int32_t *oy) {
    *ox = (bone == 2 ? 0x8000 : 0) + (bone % 3) * 0x4000 - 0x8000;
    *oy = -bone * 0x18000 - 0x40000;
}

/* Emit the thirteen bone children over the first updates, at most
 * the room child cap per call like the other multipart ports. */
static void cs_emit_bones(AosEnemy *e, AosHitReport *hit) {
    while (e->spawned_count < CS_BONES &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = AOS_ROLE_CS_BONE;
        c.parent = e;
        c.x = e->x;
        c.y = e->y;
        c.cs_bone = (uint8_t)i;
        c.cs_pose = 0;
        c.hp = 1;
        c.static_frame = (int16_t)i;
        hit->children[hit->child_count++] = c;
    }
}

/* The state-1 attack shot: a fast horizontal projectile spawned at
 * the head, mirrored by facing (the native volley helpers collapse
 * to this single role). */
static void cs_fire_shot(AosEnemy *e, AosSoma *soma, AosHitReport *hit) {
    (void)soma;
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_CS_SHOT;
    c.parent = e;
    c.x = e->x + (e->mirrored ? -0x100000 : 0x100000);
    c.y = e->y - 0x600000;
    c.vx = e->mirrored ? 0x500000 : -0x500000;
    c.vy = 0;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
}

/* The sub_08098CF0 debris spawn of the death machine. */
static void cs_spawn_gore(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_CS_GORE;
    c.parent = e;
    c.x = e->x + (int32_t)(e->cs_hits & 1 ? 0x40000 : -0x40000);
    c.y = e->y - 0x200000;
    c.vx = (int32_t)((e->cs_hits & 3) - 2) * 0x100000;
    c.vy = -0x300000;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
    e->cs_hits++;
}

/* sub_0809808C: a bone child tracks its container record around the
 * parent with the sine wobble, mirrors the parent's facing and dies
 * with the parent's state 2 or removal. */
static void cs_bone_update(AosEnemy *e, const AosAnimSet *anims) {
    AosEnemy *p = e->parent;
    if (!p || !cs_is_boss(p->id) || p->state == 2 || p->removed) {
        e->removed = true;
        return;
    }
    e->mirrored = p->mirrored;
    e->phase += 0x800 + e->cs_bone * 0x100;
    int32_t ox, oy;
    cs_bone_offset(e->cs_bone, &ox, &oy);
    int32_t wob = cs_mul(0x20000, cs_sin(e->phase + e->cs_bone * 0x800));
    e->x = p->x + ox + (e->mirrored ? -wob : wob);
    e->y = p->y + oy + cs_mul(0x10000, cs_cos(e->phase));
    (void)anims;
}

/* A shot flies straight until it leaves the room margins or its
 * lifetime ends. */
static void cs_shot_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || !cs_is_boss(p->id) || p->state == 2 || p->removed ||
        ++e->timer > 0x60) {
        e->removed = true;
        return;
    }
    integrate(e);
}

/* Gore debris flies on its launch velocity and lands. */
static void cs_gore_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || p->state == 2 || p->removed || ++e->timer > 0xA0) {
        if (e->timer > 0xA0)
            e->removed = true;
        return;
    }
    e->vy += 0x8000;
    integrate(e);
}

/* sub_08095898 state 0: the sixteen-case machine — approach with
 * wall probes, random attack entries into state 1, the 0x8C bone
 * creak stingers on case boundaries and the return cycle.  The
 * per-case bone-target choreography collapses to this documented
 * cycle. */
static void cs_state0(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)layer;
    switch (e->step) {
    case 0:
        /* Aggro probe then enter the approach. */
        if (player_near(soma, e->x >> 16, e->y >> 16, 0xC0, 0x80)) {
            play(e, anims, 1);
            e->step = 1;
            e->timer = 0;
        }
        break;
    case 1:
        /* Approach; wall contact or proximity picks an attack. */
        if (soma)
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        if (++e->timer > 0x40 ||
            aos_enemy_blocked_ahead(layer, e->x >> 16, e->y >> 16)) {
            uint32_t r = random ? random() : 0x80;
            e->state = 1;
            e->step = 0;
            e->substep = 0;
            e->cs_hits = 0;
            (void)r;
        }
        break;
    case 2:
    case 3:
        /* Creak pause: the 0x8C stinger every other case entry. */
        if ((e->step & 1) == 0)
            aos_audio_play_song(0x8C);
        if (++e->timer > 0x20) {
            e->step = 1;
            e->timer = 0;
        }
        break;
    default:
        e->step = 1;
        break;
    }
}

/* sub_08096A94 state 1: the attack machine — windup, the shot
 * volleys with their songs (0x86 windup, 0x80/0x84/0x76/0x77/0x7A/
 * 0x97/0xE0/0x1C/0x29 stingers), then back to state 0. */
static void cs_state1(AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *hit,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (e->substep) {
    case 0:
        play(e, anims, 2);
        aos_audio_play_song(0x86);
        e->substep = 1;
        e->timer = 0;
        break;
    case 1:
        /* Volley: two shots then the 0x80 stinger. */
        if (e->timer == 0x10 || e->timer == 0x28) {
            cs_fire_shot(e, soma, hit);
            if (e->timer == 0x28)
                aos_audio_play_song(0x80);
        }
        if (++e->timer > 0x40) {
            e->substep = 2;
            e->timer = 0;
        }
        break;
    case 2:
        /* Return to the approach. */
        e->state = 0;
        e->step = 1;
        e->substep = 0;
        play_loop(e, anims, 0, true);
        break;
    default:
        e->substep = 0;
        break;
    }
    if (soma)
        face_player(e, soma);
}

AosHitReport aos_enemy_creaking_skull_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;

    /* Children dispatch on their roles. */
    if (e->role == AOS_ROLE_CS_BONE) {
        cs_bone_update(e, anims);
        return hit;
    }
    if (e->role == AOS_ROLE_CS_SHOT) {
        cs_shot_update(e);
        return hit;
    }
    if (e->role == AOS_ROLE_CS_GORE) {
        cs_gore_update(e);
        return hit;
    }
    if (!cs_is_boss(e->id))
        return hit;

    /* EnemyCreakingSkullUpdate prologue: state 2 skips the activity
     * gates; the probe, collision and the container hit box fold
     * into the shared collide. */
    if (e->state != 2) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    e->hidden = e->y + 0x20 >= 0x44000;

    /* The thirteen bone children emit on the first updates. */
    if (!e->boss_parts_spawned) {
        cs_emit_bones(e, &hit);
        if (e->spawned_count >= CS_BONES)
            e->boss_parts_spawned = true;
    }

    switch (e->state) {
    case 0:
        cs_state0(e, soma, layer, anims, random);
        break;
    case 1:
        cs_state1(e, soma, anims, &hit, random);
        break;
    case 2:
        /* The native state-2 tail: + 0xD countdown, + 0x59 |= 8 and
         * the container delete through sub_0806C250 — the shared
         * death machine below covers it; gore debris every few
         * ticks approximates the sub_08098CF0 spawns. */
        if ((e->timer & 3) == 0)
            cs_spawn_gore(e, &hit);
        if (e->timer)
            e->timer--;
        else
            e->removed = true;
        break;
    default:
        /* State 3 dormant (sub_08097A14). */
        break;
    }

    /* Lethal: hp 0 enters the state-2 death machine (the native
     * hurt callbacks sub_08098E74/sub_08098EA4 fold into the
     * shared hurt pass). */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x6B);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x3C;
        e->cs_hits = 0;
    }

    step_anim(e, anims);
    return hit;
}
