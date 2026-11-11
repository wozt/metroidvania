/* Julius Belmont (id 0x6E): the boss of EnemyBelmontCreate 0x080CFAB0 /
 * EnemyBelmontUpdate 0x080CFED0 — a single-body fighter with a
 * thirteen-state machine and thrown projectiles.
 *
 * Native layout folded into AosEnemy:
 * - the + 0x20 juggle counter of the sub_080D1A44 hurt callback
 *   folds into bel_juggle;
 * - the sub_080D1BB4/sub_080D1ADC projectile entities (callback
 *   sub_080D2368, resources palette 0x0820C04C / frames 0x08252A18 /
 *   tiles 0x081F82B4) become AOS_ROLE_BEL_SHOT children emitted
 *   under the 8-child cap;
 * - the death debris becomes AOS_ROLE_BEL_GORE.
 *
 * Documented approximations: the state-2 sub_080D0044 machine keeps
 * its substep order, facing-change tracking and the decoded random
 * attack pick (mod 12 == 0 -> state 9, & 7 == 0 -> state 4, & 3 ->
 * state 8 else state 6/10) while the per-case wall probes
 * (sub_0806DF40), the 0x080E9644 HP-gate table and the Object20
 * (+ 0x13110) checks collapse to the shared probes; each attack
 * state keeps its entry song accents (0xF6/0x93 state 4, 0xFC state
 * 7, 0x94 state 9, 0x80/0xF0 state 10) and its return to state 2
 * while the per-case choreography tables collapse to a documented
 * windup/strike/recover cycle; the state-0xB hurt path keeps the
 * lethal song 0x1E1 and the juggle songs 0x1E2/0x1E3 with the
 * + 0x20 > 2 gate, and the state-0xF0 death machine keeps its
 * role as the post-0xB exit with a gore timer; the param0 == 1
 * cutscene variant (sub_080CFC04 Object20 dialogue entities,
 * states 1/0xD) is not ported — the room spawns the boss variant;
 * the sub_08013CF0/sub_08034498/sub_0806E1B8 event calls are
 * covered by the shared spawn/collide paths or not ported. */

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

static bool bel_is_belmont(uint8_t id) { return id == 0x6E; }

/* sub_080D1ADC: a thrown projectile — the native entity copies the
 * parent's facing (+ 0x10 -> + 0x11) and velocity (+ 0x40/+ 0x44);
 * here it flies toward the player with the native lifetime. */
static void bel_fire_shot(AosEnemy *e, AosHitReport *hit, int32_t vy,
                          int spread) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_BEL_SHOT;
    c.parent = e;
    c.x = e->x + (e->mirrored ? -0x20000 : 0x20000);
    c.y = e->y - 0x30000;
    c.vx = e->mirrored ? -0x380000 : 0x380000;
    c.vy = vy;
    c.ax = 0;
    c.ay = 0;
    c.hp = 1;
    c.timer = 0x50;
    c.combat.type = e->combat.type;
    if (spread)
        c.vx += e->mirrored ? -spread : spread;
    hit->children[hit->child_count++] = c;
}

/* The death debris of the state-0xF0 machine. */
static void bel_spawn_gore(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_BEL_GORE;
    c.parent = e;
    c.x = e->x + (int32_t)((e->bel_juggle & 1) ? 0x30000 : -0x30000);
    c.y = e->y - 0x20000;
    c.vx = (int32_t)((e->bel_juggle & 3) - 2) * 0x80000;
    c.vy = -0x200000;
    c.hp = 1;
    c.timer = 0xA0;
    hit->children[hit->child_count++] = c;
    e->bel_juggle++;
}

/* AOS_ROLE_BEL_SHOT: the sub_080D2368 projectile — flies, reports
 * its frame boxes through the shared collide and dies on a hit or
 * at the end of its native lifetime. */
static void bel_shot_update(AosEnemy *e, AosSoma *soma,
                            const AosEnemyKind *kind,
                            const AosWeaponEntity *weapon,
                            const AosWeaponFrames *weapon_frames,
                            int soma_atk, int soma_def,
                            int cam_x, int cam_y, AosHitReport *hit) {
    (void)hit;
    if (e->state == 2) {
        if (e->timer && (uint8_t)--e->timer == 0)
            e->removed = true;
        return;
    }
    e->x += e->vx;
    e->y += e->vy;
    step_anim(e, kind->anims);
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) {
        e->removed = true;
        return;
    }
    AosHitReport report =
        collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                soma_def);
    *hit = report;
    if ((report.enemy_hit || report.soma_hit) && !e->removed) {
        e->state = 2;
        e->timer = 40;
        e->combat.attack_off = e->combat.hurt_off = true;
    }
    if (e->timer && (uint8_t)--e->timer == 0)
        e->removed = true;
}

/* AOS_ROLE_BEL_GORE: ballistic debris with the native + 0xD life. */
static void bel_gore_update(AosEnemy *e) {
    e->vy += 0x8000;
    integrate(e);
    if (e->timer && (uint8_t)--e->timer == 0)
        e->removed = true;
}

/* sub_080D0044 state 2: the main combat machine. substep 0 is the
 * approach/idle case (the + 0x22 settle timer, the wall probes and
 * the decoded attack pick), substep 1 the facing-change tracker and
 * substep 2 the post-pick settle. */
static void bel_state2(AosEnemy *e, AosSoma *soma,
                       const AosAnimSet *anims,
                       uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if (soma)
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        if (player_near(soma, e->x >> 16, e->y >> 16, 0xA0, 0x80)) {
            /* The decoded pick: 1/12 -> state 9, 1/8 -> state 4,
             * then & 3 -> state 8 or the state 6/10 pair. */
            uint32_t r = random();
            if (r % 0xC == 0) {
                e->state = 9;
            } else if ((random() & 7) == 0) {
                e->state = 4;
            } else if (random() & 3) {
                e->state = 8;
            } else {
                e->state = (random() & 1) ? 6 : 10;
            }
            e->step = e->substep = 0;
            e->timer = 0;
        }
        break;
    case 1:
        /* Facing-change tracker: the native + 0xB increments when
         * the player crosses the body; approximated as a short
         * tracking beat before the next pick. */
        if (soma)
            face_player(e, soma);
        if (++e->timer > 0x20) {
            e->step = 0;
            e->timer = 0;
        }
        break;
    default:
        e->step = 0;
        break;
    }
    (void)anims;
}

/* The shared attack envelope: windup (song accent), strike
 * (projectile or attack window), recover, back to state 2. The
 * per-state songs and projectile counts follow the decoded calls:
 * state 4 sub_080D03F8 (0xF6/0x93), state 6 sub_080D0B8C (three
 * sub_080D1BB4 shots), state 7 sub_080D0334 (0xFC), state 8
 * sub_080D0E60 (sub_080D1ADC throw), state 9 sub_080D10B0 (0x94),
 * state 10 sub_080D1240 (0x80/0xF0). */
static void bel_attack(AosEnemy *e, AosSoma *soma,
                       const AosAnimSet *anims, AosHitReport *hit,
                       uint8_t accent1, uint8_t accent2,
                       int shots, int32_t vy) {
    switch (e->substep) {
    case 0:
        play(e, anims, 1);
        if (accent1 != 0xFF)
            aos_audio_play_song(accent1);
        e->substep = 1;
        e->timer = 0;
        break;
    case 1:
        if (soma)
            face_player(e, soma);
        if (++e->timer > 0x18) {
            e->substep = 2;
            e->timer = 0;
            if (accent2 != 0xFF)
                aos_audio_play_song(accent2);
        }
        break;
    case 2:
        /* Strike: the native projectile helpers fire here. */
        for (int i = 0; i < shots && hit->child_count <
             AOS_ENEMY_MAX_CHILDREN; ++i)
            bel_fire_shot(e, hit, vy, i * 0x80000 - shots * 0x40000);
        e->substep = 3;
        e->timer = 0;
        break;
    case 3:
        if (soma)
            face_player(e, soma);
        if (++e->timer > 0x28) {
            e->state = 2;
            e->substep = 0;
            e->step = 0;
            e->timer = 0;
            play_loop(e, anims, 0, true);
        }
        break;
    default:
        e->substep = 0;
        break;
    }
}

AosHitReport aos_enemy_belmont_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)layer; (void)cam_x; (void)cam_y;

    /* Children dispatch on their roles. */
    if (e->role == AOS_ROLE_BEL_SHOT) {
        bel_shot_update(e, soma, kind, weapon, weapon_frames,
                        soma_atk, soma_def, cam_x, cam_y, &hit);
        return hit;
    }
    if (e->role == AOS_ROLE_BEL_GORE) {
        bel_gore_update(e);
        return hit;
    }
    if (!bel_is_belmont(e->id))
        return hit;

    /* EnemyBelmontUpdate prologue: the dormant state skips the
     * gates; the shared collide covers the hurt/attack boxes. */
    if (e->state != 0xC) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    e->hidden = e->y + 0x20 >= 0x44000;

    switch (e->state) {
    case 0xC:
        /* sub_080D2334 dormant. */
        break;
    case 3:
        /* sub_080D2328: the intro handoff into the fight. */
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0;
        break;
    case 1:
        /* sub_080CFD0C: the cutscene variant's active state — not
         * spawned by the boss room; treated as an approach that
         * hands off to state 2. */
        if (soma)
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x18);
        if (++e->timer > 0x40) {
            e->state = 2;
            e->timer = 0;
        }
        break;
    case 2:
        bel_state2(e, soma, anims, random);
        break;
    case 4:
        bel_attack(e, soma, anims, &hit, 0xF6, 0x93, 0, 0);
        break;
    case 5:
        /* sub_080D0AA8: a short attack through sub_080D0998 — no
         * projectile, a melee window; approximated by the shared
         * envelope without shots. */
        bel_attack(e, soma, anims, &hit, 0xFF, 0xFF, 0, 0);
        break;
    case 6:
        /* sub_080D0B8C: the three-shot volley (3x sub_080D1BB4). */
        bel_attack(e, soma, anims, &hit, 0xFF, 0xFF, 3, 0);
        break;
    case 7:
        bel_attack(e, soma, anims, &hit, 0xFC, 0xFF, 0, 0);
        break;
    case 8:
        /* sub_080D0E60: the arcing throw (sub_080D1ADC). */
        bel_attack(e, soma, anims, &hit, 0xFF, 0xFF, 1, -0x80000);
        break;
    case 9:
        bel_attack(e, soma, anims, &hit, 0x94, 0xFF, 0, 0);
        break;
    case 10:
        bel_attack(e, soma, anims, &hit, 0x80, 0xF0, 0, 0);
        break;
    case 0xB:
        /* sub_080D1580: the hurt-stun / juggle / lethal exit —
         * defeated runs the timer into state 0xF0, a juggled body
         * recovers to state 2 (the native + 0x20 > 2 gate). */
        if (e->defeated) {
            if (++e->timer > 0x50) {
                e->state = 0xF0;
                e->substep = 0;
                e->timer = 0xA0;
            }
        } else if (++e->timer > 0x20 || e->bel_juggle > 2) {
            e->state = 2;
            e->step = e->substep = 0;
            e->timer = 0;
            e->bel_juggle = 0;
        }
        break;
    case 0xD:
        /* sub_080CFDD4: the cutscene variant's exit — approximated
         * as a short hold that removes the entity. */
        if (++e->timer > 0x40)
            e->removed = true;
        break;
    case 0xF0:
        /* sub_080D1854: the death machine — gore over the timer,
         * then the delete. */
        if ((e->timer & 3) == 0)
            bel_spawn_gore(e, &hit);
        if (e->timer)
            e->timer--;
        else
            e->removed = true;
        break;
    default:
        break;
    }

    /* sub_080D1A44: a lethal blow marks defeated, plays song
     * 0x1E1 and enters state 0xB; a non-lethal hit past the + 0x20
     * juggle gate plays 0x1E2/0x1E3 and enters the same stun. The
     * shared hurt pass reports the blow through hp <= 0 / hit
     * flags; the juggle counter increments per hit here. */
    if (!e->defeated && e->hp <= 0 && e->state != 0xB &&
        e->state != 0xF0 && e->state != 0xC) {
        e->defeated = true;
        e->hit_flash = 0xFF;
        aos_audio_play_song(0x1E1);
        e->state = 0xB;
        e->step = e->substep = 0;
        e->timer = 0;
    } else if (hit.enemy_hit && !e->defeated && e->state != 0xB &&
               e->state != 0xF0 && e->state != 0xC) {
        e->bel_juggle++;
        if (e->bel_juggle > 2) {
            e->state = 0xB;
            e->step = e->substep = 0;
            e->timer = 0;
            aos_audio_play_song((random() & 1) ? 0x1E3 : 0x1E2);
        }
    }

    step_anim(e, anims);
    return hit;
}
