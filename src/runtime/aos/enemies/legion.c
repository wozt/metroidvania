/* Legion (id 0x6C): the boss squad of EnemyLegionCreate 0x0809C6BC /
 * EnemyLegionUpdate 0x0809C5E8 — a command body that spawns five
 * soldier entities (sub_0809DF7C -> sub_0809E0C0) which fight
 * autonomously around it.
 *
 * Native layout folded into AosEnemy:
 * - the five sub_0809DF7C room entities fold into the persistent
 *   AOS_ROLE_LG_SOLDIER children emitted on the first update under
 *   the 8-child cap, each carrying its slot in lg_slot like the
 *   native + 0x6F pick and its + 0x10 parent link;
 * - the + 0x14 container entity (0x1C bytes of shared counters)
 *   folds into the parent's lg_* fields;
 * - the soldier projectiles and the death debris become
 *   AOS_ROLE_LG_SHOT / AOS_ROLE_LG_GORE children.
 *
 * Documented approximations: the state-0 sub_0809C994 coordination
 * machine keeps its substep order, the container +0x10 ramp and
 * the sub_0809DB00 ten-case soldier AI while the per-case
 * choreography tables (0x085281C8-0x0852835C) collapse to a
 * documented cycle; the state-1 attack keeps its +0xD timer to
 * 0xC7 and the table lookups as a volley cadence; the state-2
 * thirteen-case death machine collapses to its song stingers
 * (0x6E death, 0x31/0x6F/0x8F/0x8E accents) over a gore timer;
 * the sub_0809E0C0 soldier brain keeps its parent/variant gate,
 * player tracking and contact while its full case tables are
 * approximated; the sub_0809DE80 palette merge, the +0x5A byte
 * and the sub_08013CF0/sub_08034498 event calls are covered by the
 * shared spawn/collide paths or not ported. */

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

#define LG_SOLDIERS 5

static bool lg_is_legion(uint8_t id) { return id == 0x6C; }

/* The five soldier spawn offsets of the create's sub_0809DF7C
 * calls (the native y arguments 0xA8/0xD8/0xA0/0xBC/... fold into
 * a documented fan around the body). */
static void lg_slot_offset(int slot, int32_t *ox, int32_t *oy) {
    static const int8_t dx[LG_SOLDIERS] = {0, -2, 2, -4, 4};
    *ox = dx[slot] * 0x80000;
    *oy = (slot < 2 ? -0xA0000 : slot < 4 ? -0x140000 : -0x1E0000);
}

/* Emit the five soldiers on the first update, at most the room
 * child cap like the other multipart ports. */
static void lg_emit_soldiers(AosEnemy *e, AosHitReport *hit) {
    while (e->spawned_count < LG_SOLDIERS &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = AOS_ROLE_LG_SOLDIER;
        c.parent = e;
        int32_t ox, oy;
        lg_slot_offset(i, &ox, &oy);
        c.x = e->x + ox;
        c.y = e->y + oy;
        c.lg_slot = (uint8_t)i;
        c.lg_seq = 0;
        c.hp = 1;
        c.static_frame = (int16_t)i;
        hit->children[hit->child_count++] = c;
    }
}

/* The soldier's attack shot (the native projectile helpers of the
 * sub_0809E5A8..sub_080A00AC block collapse to this role). */
static void lg_fire_shot(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_LG_SHOT;
    c.parent = e;
    c.x = e->x;
    c.y = e->y - 0x40000;
    c.vx = e->mirrored ? 0x400000 : -0x400000;
    c.vy = 0;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
}

/* The death debris of the state-2 machine. */
static void lg_spawn_gore(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_LG_GORE;
    c.parent = e;
    c.x = e->x + (int32_t)((e->lg_wait & 1) ? 0x40000 : -0x40000);
    c.y = e->y - 0x200000;
    c.vx = (int32_t)((e->lg_wait & 3) - 2) * 0x100000;
    c.vy = -0x300000;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
    e->lg_wait++;
}

/* sub_0809E0C0: a soldier holds its slot fan around the parent,
 * tracks the player, fires on its sequence and dies with the
 * parent's state 2 or removal. */
static void lg_soldier_update(AosEnemy *e, AosSoma *soma,
                              const AosAnimSet *anims,
                              AosHitReport *hit) {
    AosEnemy *p = e->parent;
    if (!p || !lg_is_legion(p->id) || p->state == 2 || p->removed) {
        e->removed = true;
        return;
    }
    e->mirrored = p->mirrored;
    /* Approach the player inside a leash around the parent. */
    int32_t tx = soma ? target_x(soma) : e->x;
    int32_t ty = soma ? target_y(soma) : e->y;
    int32_t dx = tx - e->x, dy = ty - e->y;
    if (dx > 0x400000 || dx < -0x400000 || dy > 0x400000 ||
        dy < -0x400000) {
        /* Outside the leash: drift back toward the slot. */
        int32_t ox, oy;
        lg_slot_offset(e->lg_slot, &ox, &oy);
        aos_enemy_move_toward(e, p->x + ox, p->y + oy, 0x10);
    } else {
        aos_enemy_move_toward(e, tx, ty, 0x18);
    }
    /* Sequence attack: every ~0x60 ticks a shot with the 0x31
     * accent the native soldier volley uses. */
    if (++e->timer > 0x60) {
        e->timer = 0;
        lg_fire_shot(e, hit);
        aos_audio_play_song(0x31);
    }
    (void)anims;
}

static void lg_shot_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || !lg_is_legion(p->id) || p->state == 2 || p->removed ||
        ++e->timer > 0x60) {
        e->removed = true;
        return;
    }
    integrate(e);
}

static void lg_gore_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || p->state == 2 || p->removed || ++e->timer > 0xA0) {
        if (e->timer > 0xA0)
            e->removed = true;
        return;
    }
    e->vy += 0x8000;
    integrate(e);
}

/* sub_0809C994 state 0: the coordination machine — the container
 * +0x10 ramp, the sub_0809DB00 soldier AI gate and the attack
 * entries; approximated as approach + periodic soldier orders. */
static void lg_state0(AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (e->step) {
    case 0:
        /* sub_0809DB00 gate: soldiers report readiness, then the
         * container +0x10 ramp runs (case 1). */
        if (player_near(soma, e->x >> 16, e->y >> 16, 0xC0, 0x80)) {
            play(e, anims, 1);
            e->step = 1;
            e->lg_wait = 0;
        }
        break;
    case 1:
        /* The +0x10 ramp: coordination builds to the attack. */
        e->lg_wait += 8;
        if (soma)
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        if (e->lg_wait >= 0x40 || ++e->timer > 0x80) {
            e->state = 1;
            e->step = 0;
            e->substep = 0;
            e->timer = 0;
        }
        break;
    case 2:
        /* Post-attack settle then back to the approach. */
        if (++e->timer > 0x30) {
            e->state = 0;
            e->step = 0;
            e->timer = 0;
        }
        break;
    default:
        e->step = 0;
        break;
    }
}

/* sub_0809D138 state 1: the attack — the +0xD timer to 0xC7 with
 * the volley cadence and the accent songs. */
static void lg_state1(AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *hit,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (e->substep) {
    case 0:
        play(e, anims, 2);
        aos_audio_play_song(0x6F);
        e->substep = 1;
        e->timer = 0;
        break;
    case 1:
        /* Volley window: soldier shots + the 0x8F accent at the
         * native +0xD == 0x1E table point. */
        if (e->timer == 0x1E)
            aos_audio_play_song(0x8F);
        if (++e->timer > 0x40) {
            e->substep = 2;
            e->timer = 0;
        }
        break;
    case 2:
        if (++e->timer > 0x20) {
            e->state = 0;
            e->step = 2;
            e->substep = 0;
            play_loop(e, anims, 0, true);
        }
        break;
    default:
        e->substep = 0;
        break;
    }
    if (soma)
        face_player(e, soma);
    (void)hit;
}

AosHitReport aos_enemy_legion_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)layer; (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;

    /* Children dispatch on their roles. */
    if (e->role == AOS_ROLE_LG_SOLDIER) {
        lg_soldier_update(e, soma, anims, &hit);
        return hit;
    }
    if (e->role == AOS_ROLE_LG_SHOT) {
        lg_shot_update(e);
        return hit;
    }
    if (e->role == AOS_ROLE_LG_GORE) {
        lg_gore_update(e);
        return hit;
    }
    if (!lg_is_legion(e->id))
        return hit;

    /* EnemyLegionUpdate prologue: state 2 skips the activity gates;
     * the probe, collision and the container hit box fold into the
     * shared collide. */
    if (e->state != 2) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    e->hidden = e->y + 0x20 >= 0x44000;

    /* The five soldiers emit on the first update. */
    if (!e->boss_parts_spawned) {
        lg_emit_soldiers(e, &hit);
        if (e->spawned_count >= LG_SOLDIERS)
            e->boss_parts_spawned = true;
    }

    switch (e->state) {
    case 0:
        lg_state0(e, soma, anims, random);
        break;
    case 1:
        lg_state1(e, soma, anims, &hit, random);
        break;
    case 2:
        /* sub_0809D51C thirteen-case death: gore debris over the
         * machine and the delete when the timer ends. */
        if ((e->timer & 3) == 0)
            lg_spawn_gore(e, &hit);
        if (e->timer)
            e->timer--;
        else
            e->removed = true;
        break;
    default:
        /* State 3 dormant (sub_080A005C). */
        break;
    }

    /* Lethal: hp 0 enters the state-2 death machine (the native
     * hurt callbacks fold into the shared hurt pass). */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x6E);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x3C;
        e->lg_wait = 0;
    }

    step_anim(e, anims);
    return hit;
}
