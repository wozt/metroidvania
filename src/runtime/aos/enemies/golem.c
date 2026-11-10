/* Golem family (Aria of Sorrow enemy ids 0x22 Wooden Golem, 0x35 Golem,
 * 0x50 Flesh Golem, 0x65 Iron Golem), ported from the shared
 * EnemyGolemCreate (0x0807E508) / EnemyGolemUpdate (0x0807E35C) of
 * code_080709D8.s and the cluster sub_0807E8EC (state 0),
 * sub_0807F494 (state 1), sub_080809C0 (state 3 dormant) of
 * code_080809C0.s plus the limb machine sub_080812CC.
 *
 * The native actor is a multi-part boss: the body plus a 0x58-byte
 * container entity holding per-limb anchor records (the offset pairs
 * of 0x08527048 and the scale words of 0x08527354) and ten limb
 * children spawned by sub_0806DFF8 (callback sub_080812CC).  Each
 * child rides its anchor record around the parent, mirrors the
 * parent's facing, aims its + 0x48 angle (the {0,2,7,9} pieces track
 * the player through ArcTan2, the others hold their record angle)
 * and detaches into a thrown state when the parent dies.  The + 0x36
 * variant byte selects the palette/animation set and the + 0x24
 * index {0x35->0, 0x65->1, 0x22->2, 0x50->3} picks the attack tables
 * at 0x08118ED8/0x08118EDC and 0x08118EE0..0x08118EF7.
 *
 * State 0 (sub_0807E8EC) is a 42-case substate machine: intro cases
 * run sub_0808197C animation steps with gravity sub_0806D288 and the
 * song 0x7C attack call, attack cases count the + 0xD timer, pick a
 * branch with RandomNumberGenerator & 3, probe the player distance
 * (sub_0806CDB8 against the + 0x42 anchor, thresholds 0x3B/0x77) and
 * the walls (sub_0806D0B8 at the anchor +/- 0x20) before advancing.
 * State 1 (sub_0807F494) is the ~57-case follow-up machine (songs
 * 0x80 and 0x87) returning to state 0 step 6.  State 2 is the death
 * handled inside EnemyGolemUpdate: every fourth tick a sub_0806D7F0
 * debris spawns at a random offset, + 0x59 bit 8 deletes the
 * container.  State 3 (sub_080809C0) is the dormant machine the
 * gEwramData + 0x8E & 0x40 create flag parks the body in.
 *
 * Head fields: + 0xA state, + 0xB substate, + 0xC substep, + 0xD
 * timer/selector, + 0x20 attack mode, + 0x24 variant index, + 0x42/
 * + 0x46 anchor (go_ax/go_ay), + 0x48 approach speed (vx), + 0x4C
 * (vy), + 0x54 (ay).  The ten limb children carry + 0x20 piece,
 * + 0x21 aim-target piece, + 0x48 angle, + 0xD throw timer and the
 * + 0x65 static frame byte. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <stdlib.h>
#include <string.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define player_in aos_enemy_player_in
#define player_near aos_enemy_player_near
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define random_centred(r, n) aos_enemy_random_centred((r), (n))
#define move_toward aos_enemy_move_toward
#define walk_collide aos_enemy_walk_collide
#define integrate aos_enemy_integrate
#define at_room_edge aos_enemy_at_room_edge
#define blocked_ahead aos_enemy_blocked_ahead
#define generic_death aos_enemy_generic_death

static bool go_is_golem(uint8_t id) {
    return id == AOS_ENEMY_WOODEN_GOLEM || id == AOS_ENEMY_GOLEM ||
           id == AOS_ENEMY_FLESH_GOLEM || id == AOS_ENEMY_IRON_GOLEM;
}

/* The four variant bytes of + 0x36 and their + 0x24 index. */
static int go_variant_index(uint8_t id) {
    switch (id) {
    case AOS_ENEMY_GOLEM: return 0;        /* 0x35 */
    case AOS_ENEMY_IRON_GOLEM: return 1;   /* 0x65 */
    case AOS_ENEMY_WOODEN_GOLEM: return 2; /* 0x22 */
    case AOS_ENEMY_FLESH_GOLEM: return 3;  /* 0x50 */
    default: return 0;
    }
}

/* The ten limb piece indices of the create loop: non-0x50 variants
 * read table 0x08118EC2 (0..10 in order), Flesh Golem reads
 * 0x08118ECD (the 4/5 swap). */
static const uint8_t go_pieces[2][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    {0, 1, 2, 3, 5, 4, 6, 7, 8, 9},
};

/* The + 0x65 static frame bytes, one table per variant
 * (0x08118E80/8B/96/A1/AC indexed by the piece). */
static const uint8_t go_frames[5][11] = {
    {7, 8, 9, 11, 4, 5, 6, 7, 8, 9, 11},   /* 0x35 base 0x08118E80 */
    {7, 8, 9, 11, 14, 5, 6, 7, 8, 9, 11},  /* 0x65 base 0x08118E8B */
    {7, 8, 9, 11, 13, 12, 6, 7, 8, 9, 11}, /* 0x22 t3 0x08118EA1 */
    {7, 8, 9, 11, 15, 12, 6, 7, 8, 9, 11}, /* 0x22 t2 0x08118E96 */
    {19, 20, 22, 24, 21, 17, 18, 19, 20, 22, 24}, /* 0x50 0x08118EAC */
};

/* The state-0/state-1 attack timer bytes the native + 0x24-indexed
 * tables 0x08118ED8/EDC and 0x08118EE0..F7 hand out; approximated by
 * the per-variant durations below (documented divergence: the full
 * 42/57-case choreography collapses to this cycle). */
static const uint8_t go_attack_ticks[4] = {0x28, 0x1E, 0x24, 0x20};

/* Anchor offsets of the ten limbs around the body (16.16).  The
 * native container records pair a shared (0, -16..-28) offset with
 * per-piece angles; the spread below reproduces the silhouette order
 * (head, shoulders, arms, legs) — documented approximation of the
 * 0x08527048/0x08527354 record fill. */
static const int16_t go_anchor_x[10] = {0, -24, 24, -40, 40, -16, 16, -48, 48, 0};
static const int16_t go_anchor_y[10] = {-48, -40, -40, -24, -24, -8, -8, 8, 8, 16};
/* Fixed aim angles (+ 0x48) of the non-tracking pieces, 16.16 turns. */
static const uint32_t go_piece_angle[10] = {
    0x0000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000, 0x0000, 0x2000};

/* The native create spawns all ten limbs through sub_0806DFF8; the
 * room's per-update child cap (AOS_ENEMY_MAX_CHILDREN = 8) spreads the
 * emission over the first two updates — spawned_count tracks the
 * emitted prefix. */
static void go_spawn_limbs(AosHitReport *hit, AosEnemy *e) {
    if (e->boss_parts_spawned && e->spawned_count >= 10)
        return;
    const uint8_t *pieces = go_pieces[e->id == AOS_ENEMY_FLESH_GOLEM ? 1 : 0];
    while (e->spawned_count < 10 &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = (uint8_t)(AOS_ROLE_GOLEM_PART0 + i);
        c.parent = e;
        c.state = 0;
        c.step = 0;
        c.timer = 0;
        c.hp = 1;
        c.combat.hurt_off = true;
        c.go_piece = pieces[i];
        c.static_frame = (int16_t)(e->id == AOS_ENEMY_FLESH_GOLEM
                                       ? go_frames[4][c.go_piece]
                                       : go_frames[go_variant_index(e->id) == 1 ? 1 :
                                                   go_variant_index(e->id) == 2 ? (aos_random() & 0xF ? 3 : 2) :
                                                   go_variant_index(e->id) == 0 ? 0 : 0][c.go_piece]);
        c.x = e->x;
        c.y = e->y;
        hit->children[hit->child_count++] = c;
    }
    if (e->spawned_count >= 10)
        e->boss_parts_spawned = true;
}

/* sub_080812CC state 0: the limb rides its anchor record. */
static void go_limb_update(AosEnemy *c, AosEnemy *p, AosSoma *soma,
                           const AosAnimSet *anims) {
    if (p->removed || p->state == 2 || !go_is_golem(p->id)) {
        /* Parent gone or dying: detach handled below; a removed
         * parent deletes the limb through the shared pass. */
        if (p->removed) {
            c->removed = true;
            return;
        }
    }
    if (p->state == 2 && c->state == 0) {
        /* The native detach (sub_080812CC _0808140A): thrown state 1
         * with a random vx, the aim angle carried over, throw timer
         * by piece group. */
        c->state = 1;
        c->step = 0;
        c->vx = (int32_t)((int)aos_random() & 0xFF) - 0x80;
        c->angle = (uint32_t)c->go_ang;
        c->spin = 0;
        c->ay = 0x1000;
        c->vx = c->vx; /* keep */
        c->vy = 0;
        c->ax = 0;
        c->timer = (uint8_t)((aos_random() & 3) +
            (c->go_piece == 3 || c->go_piece == 10 ? 0x32 :
             c->go_piece == 2 || c->go_piece == 9 ? 0x3C : 0x46));
        return;
    }
    if (c->state == 0) {
        int i = c->go_piece < 10 ? c->go_piece : 0;
        c->x = p->x + ((int32_t)go_anchor_x[i] << 16);
        c->y = p->y + ((int32_t)go_anchor_y[i] << 16);
        /* Aim: pieces {0,2,7,9} track the player through ArcTan2;
         * the others hold their fixed record angle. */
        if (i == 0 || i == 2 || i == 7 || i == 9) {
            int32_t dx = target_x(soma) - p->x;
            int32_t dy = target_y(soma) - p->y;
            c->go_ang = aos_arctan2(dy, dx) - 0x4000;
        } else {
            c->go_ang = (int32_t)go_piece_angle[i < 10 ? i : 0];
        }
        c->mirrored = p->mirrored;
        play_loop(c, anims, 0, true);
        return;
    }
    /* Thrown state 1: drift and fade out on the timer. */
    c->x += c->vx;
    c->y += c->vy;
    if ((uint8_t)--c->timer == 0)
        c->removed = true;
}

/* sub_080809C0 state 3: the dormant machine — the body idles in
 * place until the room flag clears it back to state 0. */
static void go_dormant(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind, const AosAnimSet *anims,
                       uint32_t (*random)(void)) {
    (void)soma; (void)layer; (void)kind; (void)random;
    play_loop(e, anims, 0, true);
    step_anim(e, anims);
    /* sub_0808197C walks the dormant step table; the native machine
     * returns to state 0 once the + 0x8E & 0x40 flag clears. */
    if (!aos_enemy_flag_8e_40) {
        e->state = 0;
        e->step = 0;
    }
}

/* sub_0807E8EC state 0: the attack machine, collapsed to the decoded
 * cycle — intro (song 0x7C, approach ramp), attack-active (probes,
 * song 0x7E on the exit), recover. */
static void go_state0(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)kind;
    switch (e->step) {
    case 0:
        /* Case 0: init the anchor at the player side, pick the
         * variant attack timer (+ 0xD = table[+ 0x24]). */
        e->go_ax = (int16_t)((target_x(soma) - e->x) >> 16);
        e->go_ay = (int16_t)((target_y(soma) - e->y) >> 16);
        e->vx = 0;
        e->ax = 0x40000;
        e->ay = 0x280000;
        e->go_mode = 1;
        e->timer = go_attack_ticks[e->go_var];
        e->step = 1;
        e->substep = 0;
        play_loop(e, anims, 0, true);
        break;
    case 1:
        /* Idle drift: face the player, slow approach. */
        face_player(e, soma);
        move_toward(e, target_x(soma), target_y(soma), 0x10000);
        if (player_near(soma, (int)(e->x >> 16), (int)(e->y >> 16),
                        0xA0, 0x78)) {
            e->step = 2;
            e->timer = go_attack_ticks[e->go_var];
            aos_audio_play_song(0x7C);
        }
        break;
    case 2:
        /* Attack active: the native cases count + 0xD, probe the
         * anchor distance (0x3B/0x77) and the walls, then advance.
         * Collapsed: hold position, swing the animation, and leave
         * when the timer ends or the player leaves the probe box. */
        face_player(e, soma);
        play_loop(e, anims, 1, true);
        if ((uint8_t)--e->timer == 0 ||
            !player_in(soma, (int)(e->x >> 16) + e->go_ax - 0x50,
                       (int)(e->y >> 16) + e->go_ay - 0x50,
                       0xA0, 0xA0)) {
            e->step = 3;
            e->timer = 8;
            aos_audio_play_song(0x7E);
        }
        break;
    case 3:
        /* Recover: drift back, then the next cycle; the native tables
         * roll into state 1 on the longer patterns. */
        if ((uint8_t)--e->timer == 0) {
            e->step = 1;
            e->timer = go_attack_ticks[e->go_var];
        }
        if (e->step == 1 && player_near(soma, (int)(e->x >> 16),
                                      (int)(e->y >> 16), 0xA0, 0x78) &&
            (random() & 0x3) == 0)
            e->state = 1;
        break;
    default:
        e->step = 1;
        break;
    }
    step_anim(e, anims);
    (void)layer;
}

/* sub_0807F494 state 1: the follow-up machine — a committed rush at
 * the player (songs 0x80 during the charge, 0x87 on the strike),
 * then back to state 0 step 6 like the native tail (_080809AA). */
static void go_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        e->timer = 0x28;
        e->vx = e->mirrored ? -(0x20000) : 0x20000;
        play_loop(e, anims, 2, true);
        aos_audio_play_song(0x80);
        e->step = 1;
        break;
    case 1:
        e->x += e->vx;
        face_player(e, soma);
        if ((uint8_t)--e->timer == 0) {
            e->step = 2;
            e->timer = 0x14;
            aos_audio_play_song(0x87);
        }
        break;
    case 2:
        play_loop(e, anims, 3, true);
        if ((uint8_t)--e->timer == 0) {
            /* _080809AA: state 0, step 6. */
            e->state = 0;
            e->step = 6;
            e->timer = 0x14;
        }
        break;
    default:
        e->step = 0;
        break;
    }
    (void)layer; (void)random; (void)kind;
    step_anim(e, anims);
}

AosHitReport aos_enemy_golem_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def,
                                    int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    if (go_is_golem(e->id) && e->role != AOS_ROLE_ENEMY &&
        e->role >= AOS_ROLE_GOLEM_PART0 && e->role <= AOS_ROLE_GOLEM_PART9) {
        go_limb_update(e, e->parent, soma, anims);
        return hit;
    }
    /* EnemyGolemUpdate 0x0807E35C: state 2 skips the activity gates. */
    if (e->state != 2) {
        /* sub_0806D128(6) activity + bounds + sub_08021924 hurt and
         * sub_0806AE54 fold into the shared collide below. */
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    /* sub_080811CC lethal: sub_080683BC defeated, song 0x71,
     * + 0xD = 0x4A, state 2, step 0 — checked here like the other
     * ports because the shared pass only sees weapon strikes. */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x71);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x4A;
    }
    if (e->state == 0)
        go_state0(e, soma, layer, kind, anims, random);
    else if (e->state == 1)
        go_state1(e, soma, layer, kind, anims, random);
    else if (e->state == 2) {
        /* Death inside the update: every fourth tick a sub_0806D7F0
         * debris spawns near the body; + 0x59 bit 8 then removes the
         * container (folded: the limbs detach through go_limb_update). */
        if ((e->timer & 3) == 0) {
            AosEnemy d;
            memset(&d, 0, sizeof d);
            d.id = e->id;
            d.role = AOS_ROLE_GOLEM_DEBRIS;
            d.parent = e;
            d.state = 2;
            d.x = e->x + ((int32_t)((int)aos_random() % 0xA0) << 16) -
                 (0x50 << 16);
            d.y = e->y - (0x20 << 16);
            d.vx = ((int32_t)((int)aos_random() & 0x3FF) - 0x200) * 0x100;
            d.vy = -((int32_t)((int)aos_random() & 0x1FF) + 0x100) * 0x100;
            d.timer = 0x28;
            if (hit.child_count < AOS_ENEMY_MAX_CHILDREN)
                hit.children[hit.child_count++] = d;
        }
        if ((uint8_t)--e->timer == 0)
            e->removed = true;
    } else if (e->state == 3) {
        go_dormant(e, soma, layer, kind, anims, random);
    }
    /* The ten limbs emit on the first update (sub_0806DFF8 x10 in
     * EnemyGolemCreate). */
    if (e->role == AOS_ROLE_ENEMY && !e->removed)
        go_spawn_limbs(&hit, e);
    integrate(e);
    return hit;
}
