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
 * its + 0xB case order (approach / facing tracker / turn delay),
 * the + 0x22 settle, the + 0x34-vs-record-hp/3 HP gate, the
 * sub_0806DF40 wall probes (shared cell read, wall bit), the + 0x14
 * cornered latch with its state-8 one-shot and + 0x42C |= 0x1000
 * write, the sub_0806C1B0 proximity picks (close 0x20x0x30: %12->9,
 * &7->4, &3->10; wide 0x80x0x40 behind the Soma state-1/facing check:
 * &7->9, %6->4, %10->7, %50->10; cornered 0x100x0x80 %30->6 with the
 * + 0x22 rng&3 reload) and the facing-change advance to the turn
 * case; the sub_0806E140 walk/integrate, the sub_0803F2C8 per-frame
 * helper, the sub_080212C8 player + 0x4F4 action word (its two gated
 * picks stay collapsed) and the + 0x6E byte stay approximated or
 * unported; each attack state keeps its entry song accents
 * (0xF6/0x93 state 4, 0xFC state 7, 0x94 state 9, 0x80/0xF0 state
 * 10) and its return to state 2 while the per-case attack
 * choreography tables collapse to a documented windup/strike/
 * recover cycle; the state-0xB hurt path keeps the lethal song
 * 0x1E1 and the juggle songs 0x1E2/0x1E3 with the + 0x20 > 2 gate,
 * and the state-0xF0 death machine keeps its role as the post-0xB
 * exit with a gore timer; the param0 == 1 room record
 * (area_05_room_006) runs the sub_080CFD0C five-case intro —
 * SetPlayerInput(1, 0x20) LEFT for cases 0-3, the 0x38/0x20/0x20
 * countdowns, the hidden stateless entity — while the case-2
 * sub_080CFC04 Object20 dialogue spawn and the gEwramData + 0x4E4
 * record poll stay unported and case 4 folds into the state-3
 * fight entry (the param0 = 0 re-create); the
 * sub_08013CF0/sub_08034498/sub_0806E1B8 event calls are covered by
 * the shared spawn/collide paths or not ported. */

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

/* sub_0806DF40(e, dx, dy): the collision attribute at the point
 * dx/dy pixels from the entity's + 0x42/+ 0x46 anchor; the map
 * edge reads as a wall natively. The port probes the same pixel
 * offset from the body position and tests the wall bit (2); the
 * + 0x42/+ 0x46 anchor and the raw attribute byte fold into the
 * shared cell read (documented). */
static int bel_wall(const AosEnemy *e, const AosCollision *layer,
                    int dx, int dy) {
    if (!layer || !layer->cells)
        return 0;
    int32_t px = (e->x >> 16) + dx, py = (e->y >> 16) + dy;
    if (px < 0 || py < 0 || px >= layer->width_cells * 8 ||
        py >= layer->height_cells * 8)
        return 1;
    return (aos_collision_cell(layer, px, py) & 2) != 0;
}

/* sub_0806C1B0(e, w, h): the player inside the w x h box centred
 * on the entity's anchor — the shared centred probe. */
#define player_box(e, soma, w, h) \
    aos_enemy_player_in_centred((e), (soma), (w), (h))

/* The state-2 attack pick's shared tail (_080D024E): state + 0xA,
 * case + 0xB and substep + 0xC — the RNG remainder r1 is always 0
 * on the decoded picks (the & / % tests only pass on zero), so
 * every entry lands on case 0 substep 0. */
static void bel_enter(AosEnemy *e, uint8_t state, uint8_t step) {
    e->state = state;
    e->step = step;
    e->substep = 0;
}

/* sub_080D0044 state 2: the main combat machine. + 0xB is the case
 * (0 approach, 1 facing tracker, 2 turn delay — and it carries
 * into the attack states as their own case byte), + 0xC the
 * substep, + 0xD the u8 timer and + 0x22 the u16 settle.
 *
 * Case 0/1 body: the + 0x22 settle ticks down, the per-frame
 * sub_0803F2C8(e,1,1,1) and the sub_0806E140 walk/integrate stay
 * approximated by the shared move/face helpers; below the
 * 0x080E9644[+ 0x36].unk_C / 3 HP gate (the enemy record's hp
 * field, e->stats.hp) the wall probes and the + 0x14 cornered
 * latch run; then the sub_0806C1B0 close pick (0x20 x 0x30:
 * rng % 12 == 0 -> 9, rng & 7 == 0 -> 4, rng & 3 == 0 -> 10),
 * the facing tracker, the 0x80 x 0x40 wide pick behind the
 * gEwramData + 0x13110 Soma check (state 1, same facing, + 0x6E
 * == 0 — + 0x6E is not modeled and reads clear: rng & 7 -> 9,
 * rng % 6 -> 4, rng % 10 -> 7, rng % 50 -> 10) and the cornered
 * state-6 volley pick (0x100 x 0x80, rng % 30, + 0x22 reload).
 * The sub_080212C8 player + 0x4F4 action word is not modeled, so
 * its two gated picks (the far state-4 and the case-2 state-4)
 * stay collapsed. */
static void bel_state2(AosEnemy *e, AosSoma *soma,
                       const AosAnimSet *anims,
                       const AosCollision *layer, AosHitReport *hit,
                       uint32_t (*random)(void)) {
    (void)anims;
    /* r7: the + 0x58 bit-6 facing read at entry, for the + 0xB == 1
     * tracker. */
    int facing0 = e->mirrored;
    if (e->step <= 1) {
        if (e->bel_settle)
            e->bel_settle--;
        if (soma) {
            face_player(e, soma);
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        }
        /* The HP gate: + 0x34 < record hp / 3 (unsigned div). */
        if (e->stats.hp > 0 && e->hp < (int16_t)(e->stats.hp / 3)) {
            bool wall_r = bel_wall(e, layer, 0x80, 0) != 0;
            bool wall_l = bel_wall(e, layer, -0x80, 0) != 0;
            if (wall_r || wall_l) {
                /* A wall on either side: the facing bit follows
                 * the + 0x80 probe (set when it reads clear) and
                 * the cornered attack is state 9. */
                e->mirrored = !wall_r;
                bel_enter(e, 9, 0);
                return;
            }
            if (!(e->bel_flags & 8)) {
                /* Cornered with no wall: the one-shot state 8 and
                 * the + 0x14 |= 8 latch with its + 0x42C |= 0x1000
                 * write. */
                bel_enter(e, 8, 0);
                e->bel_flags |= 8;
                hit->flags42c_set |= 0x1000;
                return;
            }
        }
        /* The close pick (sub_0806C1B0 0x20 x 0x30). */
        if (player_box(e, soma, 0x20, 0x30)) {
            if (random() % 0xC == 0) {
                bel_enter(e, 9, 0);
                return;
            }
            if ((random() & 7) == 0) {
                bel_enter(e, 4, 0);
                return;
            }
            if ((random() & 3) == 0) {
                bel_enter(e, 10, 0);
                return;
            }
        }
        /* _080D0196: the facing tracker — a player crossing the
         * body advances to the turn case, otherwise the case
         * becomes 1 and the wide pick runs the same frame. */
        if (e->step == 1) {
            if (facing0 != e->mirrored) {
                e->step = 2;
                e->substep = 0;
                return;
            }
        } else {
            e->step = 1;
        }
        /* _080D01B8: the wide pick (sub_0806C1B0 0x80 x 0x40)
         * behind the Soma state/facing check. */
        if (player_box(e, soma, 0x80, 0x40)) {
            if (soma && soma->state == 1 &&
                (int)soma->facing_left == (int)e->mirrored) {
                if ((random() & 7) == 0) {
                    bel_enter(e, 9, 0);
                    return;
                }
                if (random() % 6 == 0) {
                    bel_enter(e, 4, 0);
                    return;
                }
            }
            if (random() % 0xA == 0) {
                bel_enter(e, 7, 0);
                return;
            }
            if (random() % 0x32 == 0) {
                bel_enter(e, 10, 0);
                return;
            }
        }
        /* _080D0256: the far state-4 pick needs the sub_080212C8
         * action word — not modeled, stays collapsed. */
        /* _080D027C: the cornered state-6 volley — the + 0x14 & 8
         * gate and the 0x100 x 0x80 box run first, then the
         * rng % 30 roll (consumed even when the + 0x22 settle is
         * still ticking) and the settle-zero check gates the
         * rng & 3 reload. */
        if ((e->bel_flags & 8) &&
            player_box(e, soma, 0x100, 0x80) &&
            random() % 0x1E == 0 && e->bel_settle == 0) {
            e->bel_settle = (uint16_t)(random() & 3);
            bel_enter(e, 6, 0);
        }
        return;
    }
    if (e->step == 2) {
        /* _080D02C0: the ten-frame turn — substep 0 arms the
         * timer, then the walk, the rng & 3 gate (its player-box
         * and sub_080212C8 checks stay collapsed) and the
         * countdown back to case 1. */
        if (e->substep == 0) {
            e->timer = 10;
            e->substep = 1;
        }
        if (soma) {
            face_player(e, soma);
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        }
        (void)(random() & 3);
        if ((uint8_t)--e->timer == 0xFF) {
            e->step = 1;
            e->substep = 0;
        }
        return;
    }
    /* Other + 0xB values do nothing natively. */
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
    (void)cam_x; (void)cam_y;

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

    /* EnemyBelmontUpdate prologue: the dormant state and the
     * param0 == 1 intro's stateless entity skip the gates (the
     * native intro has no sprite object at all); the shared
     * collide covers the hurt/attack boxes. */
    if (e->state != 0xC && e->state != 1) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    e->hidden = (e->state == 1) || (e->y + 0x20 >= 0x44000);

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
        /* sub_080CFD0C: the param0 == 1 intro machine — five
         * cases behind SetPlayerInput(1, 0x20) (LEFT held) for
         * cases 0-3. Case 2's sub_080CFC04 Object20 dialogue
         * spawn and the gEwramData + 0x4E4 + slot * 0x84 record
         * poll stay unported; the case-4 exit re-runs
         * EnemyBelmontCreate with param0 = 0, which the port
         * folds into the state-3 fight entry. */
        if (e->substep < 4) {
            hit.force_input = true;
            hit.forced_held = AOS_KEY_LEFT;
        }
        switch (e->substep) {
        case 0:
            e->timer = 0x38;
            e->substep = 1;
            break;
        case 1:
        case 3:
            /* _080CFD8E: the u8 countdown wraps to 0xFF. */
            if ((uint8_t)--e->timer == 0xFF)
                e->substep++;
            break;
        case 2:
            /* _080CFD6E: + 0x58 |= 0x20 (hidden, already set) and
             * the sub_080CFC04(e, 0) Object20 dialogue entities —
             * not ported. */
            e->timer = 0x20;
            e->substep = 3;
            break;
        default:
            /* _080CFDA4: the record poll treats the dialogue as
             * finished and re-creates with param0 = 0. */
            e->state = 3;
            e->step = e->substep = 0;
            e->timer = 0;
            break;
        }
        break;
    case 2:
        bel_state2(e, soma, anims, layer, &hit, random);
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
