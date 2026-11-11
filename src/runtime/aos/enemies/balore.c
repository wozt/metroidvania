/* Balore (EnemyBaloreCreate 0x080B5C38 / EnemyBaloreUpdate
 * 0x080B5DC0, id 0x6D): the winged knight boss of the Arena whose
 * body is drawn by three linked part entities orbiting and copying
 * its position while it hovers, rises and throws blade volleys.
 *
 * Native layout folded into AosEnemy:
 * - the three sub_0806DFF8 children of the create — the orbiting
 *   wing of sub_080B41E0, the flap limb of sub_080B4370 and the
 *   parent-locked limb of sub_080B44C4 — become the persistent
 *   AOS_ROLE_BALO_WING/LIMB0/LIMB1 children emitted on the first
 *   updates under the 8-child cap, chained through their + 0x18
 *   parent pointers;
 * - the sub_080B5094 blade (five states, random + 0x58 variant)
 *   and the sub_080B53F8 shot of the state-3 attack become
 *   AOS_ROLE_BALO_BLADE/SHOT; the sub_080B4FB0 screen effects
 *   (sub_080B4C7C/sub_080B6524) become AOS_ROLE_BALO_FX;
 * - the + 0x12 animation index feeding the 0x085287C4/0x085287CD
 *   tables, the + 0x1C/+ 0x1D limb frame bytes, the + 0x18 hover
 *   flag and the wing orbit angle fold into bl_* fields.
 *
 * Documented approximations: the state-1 rise keeps its substep
 * order, the two sub_080B4FB0 effect spawns at screen x 0x40/0x1C0
 * and song 0x1D4 but the gEwramData + 0x3E0/+ 0x3F0 OAM slot
 * writes collapse into the FX children; the state-2 hover keeps
 * the sine lift, the player-delta animation index and the
 * hp <= 0x7CF phase gate; the state-3 attack keeps the two-blade
 * + shot spawn order, song 0x1D5 and the hp-scaled timers; the
 * state-4 seven-case death keeps songs 0x1D6/0x77/0x7C, the
 * sub_08045CEC gore and the delete; the blade/shot flight
 * machines and the sub_080B67A8 param0 cutscene spawn, the
 * sub_08021654/sub_08021530 emits, the sub_080428F0 box presets,
 * the + 0x5A palette merge and the sub_08013CF0/sub_08034498
 * event calls are approximated or not ported. */

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

static bool bl_is_balore(uint8_t id) { return id == 0x6D; }

static int32_t bl_sin(uint32_t angle) { return aos_sine(angle & 0xFFFF); }
static int32_t bl_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
static int32_t bl_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* Emit the three body parts of the create through the first
 * updates: the orbit wing first, then the two parent-locked
 * limbs, each carrying its native frame byte in static_frame. */
static void bl_emit_parts(AosEnemy *e, AosHitReport *hit) {
    static const uint8_t roles[3] = {
        AOS_ROLE_BALO_WING, AOS_ROLE_BALO_LIMB0, AOS_ROLE_BALO_LIMB1
    };
    while (e->spawned_count < 3 && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = roles[i];
        c.parent = e;
        c.x = e->x;
        c.y = e->y;
        c.hp = 1;
        c.static_frame = i == 0 ? 0 : (int16_t)(0x1B + i);
        hit->children[hit->child_count++] = c;
    }
}

/* sub_080B4FB0: a screen-space effect entity at (cam + offset),
 * life through the timer; the two rise spawns use offsets
 * 0x40/0x1C0 with flags 1/0. */
static void bl_spawn_fx(AosEnemy *e, AosHitReport *hit, int32_t sx,
                        int32_t sy, uint8_t life) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_BALO_FX;
    c.parent = e;
    c.x = e->x + sx * 0x10000;
    c.y = e->y + sy * 0x10000;
    c.timer = life;
    c.hp = 1;
    c.static_frame = 0;
    hit->children[hit->child_count++] = c;
}

/* sub_080B5094: the state-3 blade — random + 0x58 variant, a
 * five-state flight approximated by a sine dive toward the
 * player with contact and a life. */
static void bl_spawn_blade(AosEnemy *e, AosHitReport *hit, bool alt) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_BALO_BLADE;
    c.parent = e;
    c.x = e->x + (alt ? 0x10 : -0x10) * 0x10000;
    c.y = e->y - 0x20 * 0x10000;
    c.vx = e->mirrored ? 0x30000 : -0x30000;
    c.vy = -0x20000;
    c.timer = 0x60;
    c.hp = 1;
    c.mt_slot = alt ? 1 : 0;
    hit->children[hit->child_count++] = c;
}

/* sub_080B53F8: the state-3 follow-up shot. */
static void bl_spawn_shot(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_BALO_SHOT;
    c.parent = e;
    c.x = e->x;
    c.y = e->y - 0x30 * 0x10000;
    c.vx = e->mirrored ? 0x40000 : -0x40000;
    c.vy = 0;
    c.timer = 0x50;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
}

/* sub_080B41E0: the wing orbits the parent at radius + 0x20
 * (0x10000) on an ArcTan2 aim at the player with sine offsets;
 * its six states mirror the parent's phase flags and delete on
 * the parent's + 0x1E & 4 (the death flag). */
static void bl_wing_update(AosEnemy *e, const AosAnimSet *anims,
                           AosSoma *soma) {
    AosEnemy *p = e->parent;
    if (!p || !bl_is_balore(p->id) || p->state == 4 || p->removed ||
        (p->phase & 4)) {
        e->removed = true;
        return;
    }
    if (e->state == 0) {
        e->state = p->state == 5 ? 3 : 1;
    }
    /* Aim: ArcTan2 of the player delta from the parent's (+ 0x42,
     * + 0x46) anchor minus (0x20, 0x10), then the orbit position
     * parent + radius * (sin, cos) of angle + 0x4000. */
    int32_t tx = soma ? target_x(soma) : p->x;
    int32_t ty = soma ? target_y(soma) : p->y;
    int32_t dx = ((tx - p->x) >> 16) - 0x20;
    int32_t dy = ((ty - p->y) >> 16) - 0x10;
    if (dx > 0x7FFF) dx = 0x7FFF;
    if (dx < -0x8000) dx = -0x8000;
    if (dy > 0x7FFF) dy = 0x7FFF;
    if (dy < -0x8000) dy = -0x8000;
    if (dx || dy) {
        uint32_t ang = (uint32_t)aos_arctan2((int16_t)dy, (int16_t)dx);
        e->bl_ang = (int32_t)(ang << 16 >> 16);
        p->bl_ang = e->bl_ang;
    }
    int32_t r = 0x10000;
    e->x = p->x + bl_mul(r, bl_sin((uint32_t)e->bl_ang + 0x4000));
    e->y = p->y + bl_mul(r, bl_cos((uint32_t)e->bl_ang + 0x4000));
    e->phase += 0x400;
    /* Parent state 3 hides the wing, state 4 / + 0x1E & 2 flaps it
     * to state 4, parent + 0x1E & 4 deletes (handled above). */
    if (p->state == 3)
        e->state = 2;
    if (p->state == 4 || (p->phase & 2))
        e->state = 4;
    play_loop(e, anims, 2, true);
    step_anim(e, anims);
}

/* sub_080B4370: the flap limb copies the parent position, runs a
 * 0x20-tick flap cycle (frames 0x1B/0x12 of the 0x085287CD table)
 * and deletes on the parent's + 0x1E & 2. */
static void bl_limb0_update(AosEnemy *e, const AosAnimSet *anims) {
    AosEnemy *p = e->parent;
    if (!p || !bl_is_balore(p->id) || p->removed || (p->phase & 2)) {
        e->removed = true;
        return;
    }
    e->x = p->x;
    e->y = p->y;
    e->mirrored = p->mirrored;
    if (e->state == 0) {
        e->state = 1;
        e->timer = 0x20;
        e->bl_f1c = 0x1B;
    } else if (e->state == 1) {
        if (p->state == 3)
            e->state = 2;
    } else if (e->state == 2) {
        e->state = 3;
        e->timer = 0x20;
    } else if (e->state == 3) {
        e->bl_f1c = (e->timer & 2) ? 0x1B : 0x12;
        if (--e->timer == 0) {
            e->bl_f1c = 0x12;
            e->timer = 8;
            e->state = 4;
        }
    } else if (e->state == 4) {
        e->bl_f1c = (e->timer & 1) ? 0x1B : 0x12;
        if (--e->timer == 0) {
            e->state = 5;
        }
    }
    play_loop(e, anims, e->bl_f1c, true);
    step_anim(e, anims);
}

/* sub_080B44C4: the second limb copies the parent position and
 * mirrors the parent's state flags, deleting on + 0x1E & 2. */
static void bl_limb1_update(AosEnemy *e, const AosAnimSet *anims) {
    AosEnemy *p = e->parent;
    if (!p || !bl_is_balore(p->id) || p->removed || (p->phase & 2)) {
        e->removed = true;
        return;
    }
    e->x = p->x;
    e->y = p->y;
    e->mirrored = p->mirrored;
    if (e->state == 0)
        e->state = 1;
    if (p->state == 3)
        e->state = 0;
    play_loop(e, anims, 0, true);
    step_anim(e, anims);
}

/* The blade and shot attacks: sine flight toward the player,
 * contact through the shared collide handled by the caller, a
 * life and deletion with the parent. */
static void bl_projectile_update(AosEnemy *e, const AosAnimSet *anims,
                                 AosSoma *soma) {
    AosEnemy *p = e->parent;
    if (!p || !bl_is_balore(p->id) || p->state == 4 || p->removed ||
        --e->timer == 0) {
        e->removed = true;
        return;
    }
    if (e->role == AOS_ROLE_BALO_BLADE) {
        e->phase += 0x800;
        e->vy += bl_mul(bl_sin(e->phase), 0x8000);
    }
    integrate(e);
    if (soma && (e->timer & 0x10) == 0) {
        int32_t dx = (target_x(soma) - e->x) >> 16;
        int32_t dy = (target_y(soma) - e->y) >> 16;
        if (dx > 0x7FFF) dx = 0x7FFF;
        if (dx < -0x8000) dx = -0x8000;
        if (dy > 0x7FFF) dy = 0x7FFF;
        if (dy < -0x8000) dy = -0x8000;
        if (dx || dy) {
            uint32_t ang = (uint32_t)aos_arctan2((int16_t)dy, (int16_t)dx);
            int32_t sp = e->role == AOS_ROLE_BALO_SHOT ? 0x38000 : 0x28000;
            e->vx = bl_mul(sp, bl_cos(ang));
            e->vy = bl_mul(sp, bl_sin(ang));
        }
    }
    play_loop(e, anims, 5, true);
    step_anim(e, anims);
}

/* sub_080B5E9C/B52C family: the short-lived screen effect — a
 * fade/timer entity deleted when its life ends or the parent
 * dies. */
static void bl_fx_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || !bl_is_balore(p->id) || p->removed ||
        --e->timer == 0) {
        e->removed = true;
        return;
    }
}

/* sub_080B569C state 1: the rise — substep 0 arms timer 0x40;
 * substep 1 fires the two sub_080B4FB0 effects at x 0x40 (flag 1)
 * and 0x1C0 (flag 0) on the 0x38/0x18 ticks, then substep 2 waits
 * 0x40 with song 0x1D4 and the wing OAM writes; the end clears
 * + 0x72 bit 2 and enters state 2. */
static void bl_state1(AosEnemy *e, AosHitReport *hit) {
    switch (e->substep) {
    case 0:
        e->substep = 1;
        e->timer = 0x40;
        break;
    case 1:
        if (e->timer == 0x38)
            bl_spawn_fx(e, hit, 0x40, 0x80, 1);
        if (e->timer == 0x18)
            bl_spawn_fx(e, hit, 0x1C0, 0x80, 0);
        if (--e->timer == 0) {
            e->substep = 2;
            e->timer = 0x40;
            aos_audio_play_song(0x1D4);
        }
        break;
    case 2:
        /* The rise: + 0x44 gains 0x400000 per frame until the timer
         * ends, then state 2. */
        e->vy += 0x400000;
        integrate(e);
        if (--e->timer == 0) {
            e->bl_hover = 0;
            e->state = 2;
            e->substep = 0;
        }
        break;
    }
}

/* sub_080B577C state 2: the hover — sine lift on the + 0x44 axis,
 * the player-delta animation index clamped to 0..8, the + 0x18
 * hover flag and the hp <= 0x7CF phase gate into substep 2; the
 * + 0x1E & 0x20 flag then re-enters state 3 or holds substep 3. */
static void bl_state2(AosEnemy *e, AosSoma *soma) {
    switch (e->substep) {
    case 0:
        e->substep = 1;
        break;
    case 1: {
        int32_t s = bl_sin((uint32_t)e->timer << 8);
        if (s < 0) s += 7;
        e->vy -= s >> 3;
        e->timer++;
        int32_t dx = soma ? ((target_x(soma) - e->x) >> 16) : 0;
        int32_t idx = (dx >> 4) + 4;
        if (idx < 0) idx = 0;
        if (idx > 8) idx = 8;
        e->bl_idx = (uint8_t)idx;
        e->bl_hover = 5;
        if (e->hp <= 0x7CF)
            e->substep = 2;
        break;
    }
    case 2:
        e->bl_hover = 7;
        if (e->phase & 0x20) {
            e->phase &= ~0x20;
            e->bl_idx = 4;
            e->state = 3;
            e->substep = 0;
        }
        break;
    case 3:
        if (e->timer == 0) {
            e->bl_hover = 1;
            e->substep = 1;
            e->timer = 0xF0;
        } else {
            e->timer--;
        }
        break;
    }
    integrate(e);
}

/* sub_080B5860 state 3: the attack — substep 0 spawns the two
 * sub_080B5094 blades (the second flagged), plays song 0x1D5,
 * presets the attack box and arms 0x60; the timer end spawns the
 * sub_080B53F8 shot; substep 2 re-arms by hp (0x50 or 0x80). */
static void bl_state3(AosEnemy *e, AosHitReport *hit) {
    switch (e->substep) {
    case 0:
        bl_spawn_blade(e, hit, false);
        bl_spawn_blade(e, hit, true);
        aos_audio_play_song(0x1D5);
        e->attack_off = false;
        e->substep = 1;
        e->timer = 0x60;
        break;
    case 1:
        if (--e->timer == 0) {
            bl_spawn_shot(e, hit);
            e->substep = 2;
        }
        break;
    case 2:
        e->timer = e->hp <= 0x3E7 ? 0x50 : 0x80;
        e->substep = 1;
        break;
    }
}

/* sub_080B594C state 4: the seven-case death — the 0x42C & 0x10
 * screen fades, song 0x1D6 at the second case, the sub_08045CEC
 * gore every other frame with random child flags, song 0x77 every
 * eighth frame, the + 0x20 fade, song 0x7C and the sub_08011FC0
 * screen effect, then the sub_080D7308/sub_080D6EF8 gore burst
 * that returns the delete. */
static void bl_state4(AosEnemy *e, AosHitReport *hit,
                      uint32_t (*random)(void)) {
    switch (e->substep) {
    case 0:
        e->substep = 1;
        e->timer = 4;
        break;
    case 1:
        if (e->timer == 0) {
            e->timer = 0x20;
            e->substep = 3;
            aos_audio_play_song(0x1D6);
        } else {
            e->timer--;
        }
        break;
    case 2:
        if (e->timer == 0) {
            e->timer = 0x80;
            e->substep = 3;
        } else {
            e->timer--;
        }
        break;
    case 3:
        if (e->timer & 1) {
            /* sub_08045CEC gore at (+ 0x42 - 0x10, + 0x46 - 0x10). */
            aos_enemy_spawn_flame_at(e->x - 0x100000,
                                     e->y - 0x100000, 0x100000,
                                     0x100000, random, hit);
        }
        if (e->timer == 0) {
            e->timer = 0x80;
            e->substep = 4;
        } else {
            e->timer--;
        }
        break;
    case 4:
        /* Gore with RandomNumberGenerator2 velocities; song 0x77
         * every eighth frame. */
        if ((e->timer & 7) == 0)
            aos_audio_play_song(0x77);
        if (e->bl_fade > 0x8000)
            e->bl_fade += (int32_t)0xFFFFFF00;
        if (e->timer == 0) {
            e->phase |= 4;
            e->substep = 5;
            e->timer = 0x20;
        } else {
            e->timer--;
        }
        break;
    case 5:
        if (e->timer == 0) {
            aos_audio_play_song(0x7C);
            e->substep = 6;
            e->timer = 0x20;
        } else {
            e->timer--;
        }
        break;
    case 6:
        if (e->timer == 0) {
            /* sub_080D7308 + sub_080D6EF8 gore burst, then the
             * native return 1 deletes the entity. */
            aos_enemy_spawn_flame_at(e->x - 0x100000,
                                     e->y - 0x100000, 0x200000,
                                     0x600000, random, hit);
            e->removed = true;
        } else {
            e->timer--;
        }
        break;
    }
}

AosHitReport aos_enemy_balore_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;
    (void)layer; (void)weapon; (void)weapon_frames;

    /* Child roles dispatch first. */
    if (e->role == AOS_ROLE_BALO_WING) {
        bl_wing_update(e, anims, soma);
        return hit;
    }
    if (e->role == AOS_ROLE_BALO_LIMB0) {
        bl_limb0_update(e, anims);
        return hit;
    }
    if (e->role == AOS_ROLE_BALO_LIMB1) {
        bl_limb1_update(e, anims);
        return hit;
    }
    if (e->role == AOS_ROLE_BALO_BLADE || e->role == AOS_ROLE_BALO_SHOT) {
        bl_projectile_update(e, anims, soma);
        return hit;
    }
    if (e->role == AOS_ROLE_BALO_FX) {
        bl_fx_update(e);
        return hit;
    }
    if (!bl_is_balore(e->id))
        return hit;

    /* EnemyBaloreUpdate prologue: state 4 skips the activity gate;
     * the + 0x1E bit 0 clear and the sub_08068AD4/sub_080421AC room
     * bookkeeping fold into the shared collide/margins. */
    if (e->state != 4) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    e->phase &= ~1;

    /* The three body parts emit on the first updates. */
    if (!e->boss_parts_spawned) {
        bl_emit_parts(e, &hit);
        if (e->spawned_count >= 3)
            e->boss_parts_spawned = true;
    }

    /* State dispatch of the jump table. */
    switch (e->state) {
    case 0:
        /* sub_08013CF0(0x11) channel claim, then state 1. */
        e->state = 1;
        e->substep = 0;
        break;
    case 1:
        bl_state1(e, &hit);
        break;
    case 2:
        bl_state2(e, soma);
        break;
    case 3:
        bl_state3(e, &hit);
        break;
    case 4:
        bl_state4(e, &hit, random);
        break;
    default:
        /* State 5 dormant (sub_0803F17C tail only). */
        break;
    }

    /* Lethal: the hurt callback sub_080B6AE8 enters the state-4
     * death machine directly (its case 1 plays song 0x1D6). */
    if (!e->defeated && e->hp <= 0 && e->state != 4) {
        e->defeated = true;
        e->state = 4;
        e->substep = 0;
        e->timer = 0;
    }

    /* The anim table 0x085287C4[+ 0x12] collapses to the clamped
     * index; the dormant state holds animation 0. */
    if (e->state == 5) {
        play_loop(e, anims, 0, true);
    } else {
        play_loop(e, anims, e->bl_idx < 8 ? e->bl_idx : 7, true);
    }
    step_anim(e, anims);
    return hit;
}
