/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Nemesis (enemy 0x26) and its linked helper.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#include <stdlib.h>

#define no_target aos_enemy_no_target
#define angle_to_target aos_enemy_angle_to_target
#define collide aos_enemy_collide
#define nemesis_appear aos_enemy_nemesis_appear
#define nemesis_helper_step aos_enemy_nemesis_helper_step
#define player_dx aos_enemy_player_dx
#define player_dy aos_enemy_player_dy
#define player_within aos_enemy_player_within
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_flame_at aos_enemy_spawn_flame_at
#define target_x aos_enemy_target_x
#define turn_angle aos_enemy_turn_angle

/* + 0x18 bits of Nemesis. */
#define NEM_TARGET_LEFT 0x02    /* its target lies left of it */
#define NEM_SOMA_LEFT 0x08      /* Soma lies left of it (state 6) */
#define NEM_NO_COLLIDE 0x20     /* dead: no more collision passes */

/* Sets the linked sprite's state; a new state restarts its steps. */
static void nemesis_helper_set(AosEnemy *n, uint8_t state) {
    if (n->nem_helper.state == state) return;
    n->nem_helper.state = state;
    n->nem_helper.step = 0;
}

static void helper_face(AosNemesisHelper *h, const AosSoma *soma) {
    /* With no target the linked sprite aims at itself. */
    if (no_target) return;
    if (target_x(soma) != h->x) h->mirrored = h->x < target_x(soma);
}

/* sub_080C7784, run after the body as its slot follows the body's: 0
 * hidden; 1 frame 1 on the body; 2 frame 2 nine pixels on its facing side
 * and 37 below, facing away from the target, for ten frames; 3 frame 3 ten
 * pixels behind and 4 above, facing Soma, for 40 frames; 4 a 31-frame
 * flicker (hidden unless + 0x0D % 3 = 0), then 0; 5 idle. */
void aos_enemy_nemesis_helper_step(AosEnemy *n, const AosSoma *soma) {
    AosNemesisHelper *h = &n->nem_helper;
    switch (h->state) {
    case 0: h->hidden = true; break;
    case 1:
        h->hidden = false;
        h->frame = 1;
        h->x = n->x;
        h->y = n->y;
        helper_face(h, soma);
        break;
    case 2:
        if (h->step == 0) {
            h->x = n->x;
            helper_face(h, soma);
            h->mirrored = !h->mirrored;
            h->hidden = false;
            h->frame = 2;
            h->timer = 0;
            h->step = 1;
        }
        if (h->step == 1) {
            h->x = n->x + (h->mirrored ? -AOS_FIXED(9) : AOS_FIXED(9));
            h->y = n->y + 0x250000;
            if (h->timer++ > 9) nemesis_helper_set(n, 4);
        }
        break;
    case 3:
        if (h->step == 0) {
            h->hidden = false;
            h->frame = 3;
            h->timer = 0;
            h->step = 1;
            h->mirrored = !(soma->x < n->x);
        }
        if (h->step == 1) {
            h->x = n->x + (h->mirrored ? AOS_FIXED(10) : -AOS_FIXED(10));
            h->y = n->y - AOS_FIXED(4);
            if (h->timer++ > 0x27) nemesis_helper_set(n, 4);
        }
        break;
    case 4:
        if (h->step == 0) {
            h->timer = 0;
            h->step = 1;
        }
        if (h->step == 1) {
            h->hidden = h->timer % 3 != 0;
            if (h->timer++ > 0x1E) {
                nemesis_helper_set(n, 0);
                h->timer = 0;
            }
        }
        break;
    default: break;
    }
}

/* The child entity of sub_080C76D4 (translucent, + 0x5C + 2) shows the
 * linked sprite its body runs. */
void aos_enemy_nemesis_helper_update(AosEnemy *h) {
    const AosEnemy *parent = h->parent;
    if (!parent || parent->removed) {
        h->removed = true;
        return;
    }
    const AosNemesisHelper *s = &parent->nem_helper;
    h->state = s->state;
    h->x = s->x;
    h->y = s->y;
    h->mirrored = s->mirrored;
    h->hidden = s->hidden;
    h->static_frame = s->frame;
    h->translucent = true;
}

/* The three visual slots made by sub_080C7B54 (sequence indices 9, 12 and
 * 6). sub_08045748 copies the owning Nemesis position, mirror, visibility
 * and current sprite record into each translucent child; their collision
 * callbacks are disabled. sub_080C7AB8 deletes them when the body starts
 * dying (state 0xFF). Keeping the sequence index in `variant` preserves the
 * native identity while the child scratch history selects a delayed direct
 * frame. */
void aos_enemy_nemesis_effect_update(AosEnemy *effect, const AosEnemyKind *kind) {
    const AosEnemy *parent = effect->parent;
    if (!parent || parent->removed || parent->state == 0xFF) {
        effect->removed = true;
        return;
    }
    /* sub_08045748 starts a newly allocated child in state 0, promotes it to
     * state 2 on the first callback and advances +0x0D every callback. */
    if (effect->state == 0) effect->state = 2;
    effect->timer = (uint8_t)(effect->timer + 1);
    effect->hidden = parent->hidden;
    effect->vflip = parent->vflip;
    effect->translucent = true;

    /* The child owns a zero-filled 0x31-word scratch allocation.  The native
     * callback copies records backwards so record 0 remains the newest sample
     * and records 1..23 are the preceding frames.  This is the source of the
     * Nemesis motion trail; it is not a separate animation descriptor. */
    for (int i = AOS_NEMESIS_EFFECT_HISTORY - 1; i > 0; --i) {
        effect->nemesis_history_x[i] = effect->nemesis_history_x[i - 1];
        effect->nemesis_history_y[i] = effect->nemesis_history_y[i - 1];
        effect->nemesis_history_frame[i] = effect->nemesis_history_frame[i - 1];
        effect->nemesis_history_mirror[i] = effect->nemesis_history_mirror[i - 1];
    }
    effect->nemesis_history_x[0] = (int16_t)(parent->x >> 16);
    effect->nemesis_history_y[0] = (int16_t)(parent->y >> 16);
    uint8_t frame = parent->anim.frame;
    if (kind && parent->anim.id < AOS_ENEMY_MAX_ANIMS && parent->anim.frame < AOS_ENEMY_MAX_FRAMES)
        frame = kind->sprites[parent->anim.id][parent->anim.frame];
    effect->nemesis_history_frame[0] = frame;
    effect->nemesis_history_mirror[0] = parent->mirrored ? 1 : 0;

    int index = effect->variant;
    if (index < 0 || index >= AOS_NEMESIS_EFFECT_HISTORY) index = 0;
    effect->x = AOS_FIXED(effect->nemesis_history_x[index]);
    effect->y = AOS_FIXED(effect->nemesis_history_y[index]);
    effect->mirrored = effect->nemesis_history_mirror[index] != 0;
    effect->static_frame = effect->nemesis_history_frame[index];
}

/* sub_080C68AC (state 0): it waits hidden until the target is within 80
 * pixels, flickers in translucent with its linked sprite for 41 frames,
 * waits 21, flickers the linked sprite out for 41 and after 21 more starts
 * its approach. Step 0 runs once at creation. */
void aos_enemy_nemesis_appear(AosEnemy *n, const AosSoma *soma) {
    AosNemesisHelper *h = &n->nem_helper;
    switch (n->step) {
    case 0:
        n->angle = 0x1000;
        n->hidden = h->hidden = true;
        h->state = 5;
        n->timer = 0;
        n->step = 1;
        /* fallthrough */
    case 1:
        if (!player_within(n, soma, 0x500000)) return;
        h->x = n->x + (h->mirrored ? AOS_FIXED(10) : AOS_FIXED(13));
        h->y = n->y + AOS_FIXED(16);
        n->step = 2;
        return;
    case 2:
        n->hidden = h->hidden = n->timer % 3 != 0;
        n->translucent = true;
        if (n->timer++ <= 0x28) return;
        n->hidden = h->hidden = false;
        n->translucent = false;
        break;
    case 3:
        if (n->timer++ <= 0x14) return;
        break;
    case 4:
        h->hidden = n->timer % 3 != 0;
        if (n->timer++ <= 0x28) return;
        h->hidden = true;
        nemesis_helper_set(n, 0);
        break;
    case 5:
        if (n->timer++ <= 0x14) return;
        n->timer = 0;
        n->state = 1;
        n->step = n->substep = 0;
        return;
    default: return;
    }
    n->timer = 0;
    n->step++;
}

/* sub_080C6AA8 (state 1): opaque, the attack box (-8, -8, 16, 16), it tilts
 * to 0x1000 (0xF000 with the target to its right) by 0x180 a frame, then
 * drifts half a pixel a frame on each axis until it is within 72 pixels
 * horizontally and 16 of 48 pixels above the target (state 2), bobbing
 * its tilt by sin(+ 0x1A) / 32; crossing the target restarts the tilt. */
static void nemesis_approach(AosEnemy *n, const AosSoma *soma, uint32_t (*random)(void)) {
    if (player_dx(n, soma) > 0) n->nem_flags |= NEM_TARGET_LEFT;
    else n->nem_flags &= (uint16_t)~NEM_TARGET_LEFT;
    switch (n->step) {
    case 0:
        n->translucent = false;
        n->own_attack = (AosBox){-8, -8, 16, 16};
        n->vx = n->vy = 0x8000;
        if (player_dx(n, soma) > 0) n->vx = -n->vx;
        if (player_dy(n, soma) > 0) n->vy = -n->vy;
        n->phase = 0;
        n->nem_spin = -100;
        n->step = 2;
        return;
    case 1:
    case 2:
        if (turn_angle(&n->angle, 0x180, player_dx(n, soma) > 0 ? 0x1000 : 0xF000)) n->step++;
        return;
    case 3: {
        bool near_x = false, near_y = false;
        n->vx = n->vy = 0x8000;
        if (player_dx(n, soma) > 0) n->vx = -abs(n->vx);
        if (player_dy(n, soma) + 0x20 > 0) n->vy = -abs(n->vy);
        if (abs(player_dx(n, soma)) > 0x48) n->x += n->vx;
        else near_x = true;
        if (abs(player_dy(n, soma) + 0x30) > 0x10) n->y += n->vy;
        else near_y = true;
        if (near_x && near_y && n->state != 2) {
            n->state = 2;
            n->step = n->substep = 0;
        }
        int16_t dx = player_dx(n, soma);
        if ((dx >= 0 && n->vx > 0) || (dx <= 0 && n->vx < 0)) {
            n->step = 0;
            return;
        }
        int32_t s = aos_sine(n->phase);
        if (s < 0) s += 0x1F;
        n->angle = (uint16_t)((s >> 5) + (dx > 0 ? 0x1000 : -0x1000));
        n->phase = (uint16_t)(n->phase + 0x200);
        n->nem_wait = n->nem_wait ? (int16_t)(n->nem_wait - 1) : (int16_t)(random() % 0xB4 + 10);
        return;
    }
    default: return;
    }
}

/* 0x08528A34: four rows of nine attacks (0 dash, 1 swoop, 2 dive). */
static const uint8_t nemesis_attacks[4][9] = {
    {1, 0, 0, 2, 1, 1, 0, 2, 2},
    {0, 1, 2, 1, 0, 1, 2, 1, 0},
    {0, 1, 0, 1, 0, 2, 2, 2, 2},
    {2, 2, 1, 1, 0, 0, 1, 1, 2},
};

static void nemesis_next_attack(AosEnemy *n, uint32_t (*random)(void)) {
    if (n->nem_index == 9) {
        n->nem_row = (uint8_t)(random() & 3);
        n->nem_index = 0;
    }
}

static void nemesis_enter(AosEnemy *n, uint8_t state) {
    if (n->state == state) return;
    n->state = state;
    n->step = n->substep = 0;
}

/* sub_080C6D0C (state 2): the next attack of its row; a dive (2) starts at
 * once, otherwise it slides two pixels a frame away from the target while
 * tilting to 0xD800 (0x2800) by |8 * + 0x1E|, the
 * linked sprite shows frame 2 and, once it is gone, it tilts to 0x9800
 * (0x6800) then 0x2800 (0xD800) by |16 * + 0x1E| and starts the attack. */
static void nemesis_choose(AosEnemy *n, uint32_t (*random)(void)) {
    bool left = n->nem_flags & NEM_TARGET_LEFT;
    if (n->step == 0) {
        nemesis_next_attack(n, random);
        if (nemesis_attacks[n->nem_row][n->nem_index] == 2) {
            nemesis_enter(n, 5);
            n->nem_index++;
        }
    }
    uint16_t target;
    switch (n->step) {
    case 0:
        n->x += left ? 0x20000 : -0x20000;
        if (turn_angle(&n->angle, (uint16_t)abs(n->nem_spin * 8), left ? 0xD800 : 0x2800)) {
            nemesis_helper_set(n, 2);
            n->step++;
        }
        return;
    case 1:
        if (n->nem_helper.state == 0) n->step++;
        return;
    case 2:
    case 3:
        if (n->step == 2) target = left ? 0x9800 : 0x6800;
        else target = left ? 0x2800 : 0xD800;
        if (turn_angle(&n->angle, (uint16_t)abs(n->nem_spin * 16), target)) n->step++;
        return;
    case 4: {
        if (n->nem_helper.state != 0) return;
        nemesis_next_attack(n, random);
        uint8_t attack = nemesis_attacks[n->nem_row][n->nem_index];
        nemesis_enter(n, (uint8_t)(attack + 3));
        n->nem_index++;
        return;
    }
    default: return;
    }
}

/* sub_080C6EF8: both boxes 16 x 8 at the tip its angle points to. */
static void nemesis_tip_box(AosEnemy *n) {
    int16_t a = (int16_t)n->angle;
    int8_t x = (int8_t)(aos_fixed_mul(aos_sine((uint32_t)(a + 0x8000)), 0x100000) >> 16);
    int8_t y = (int8_t)(aos_fixed_mul(aos_sine((uint32_t)(a + 0x4000)), 0x80000) >> 16);
    if (x > 0) x = (int8_t)(x - 16);
    n->own_attack = n->own_hurt = (AosBox){x, y, 16, 8};
}

/* sub_0806D2F8(e, + 0x1C + 0x4000, 1.0): the velocity along its body. */
static void nemesis_heading(AosEnemy *n) {
    uint32_t a = (uint32_t)((int16_t)n->angle + 0x4000);
    n->vx = aos_sine(a + 0x4000);
    n->vy = aos_sine(a);
}

static void nemesis_rest(AosEnemy *n) {
    if (n->timer++ <= 0x28) return;
    n->timer = 0;
    nemesis_enter(n, 1);
    n->nem_passes = 0;
}

/* sub_080C6F64 (state 3): three dashes at three times its heading for 15
 * frames (sound 0x85 at the end), the first two followed by an 8-frame
 * recoil at six times the reverse heading turning by 0x320, then -400 a
 * frame (0x081194D0); 41 frames of rest, then state 1. */
static void nemesis_dash(AosEnemy *n) {
    static const int16_t spins[3] = {0x320, -400, 0};
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->nem_passes = 3;
        n->step = 1;
        /* fallthrough */
    case 1:
        n->nem_spin = spins[3 - n->nem_passes];
        if (!(n->nem_flags & NEM_TARGET_LEFT)) n->nem_spin = (int16_t)-n->nem_spin;
        nemesis_heading(n);
        n->timer = 0;
        n->step = 2;
        /* fallthrough */
    case 2:
        if (n->timer++ > 0xE) {
            n->timer = 0;
            n->step = 3;
            aos_audio_play_song(0x85);
            return;
        }
        n->x += n->vx * 3;
        n->y += n->vy * 3;
        return;
    case 3:
        if (n->nem_passes == 1) {
            n->step = 4;
            n->timer = 0;
            return;
        }
        if (n->timer++ > 7) {
            n->timer = 0;
            n->step = --n->nem_passes == 0 ? 4 : 1;
            return;
        }
        n->angle = (uint16_t)(n->angle + n->nem_spin);
        nemesis_heading(n);
        n->x -= n->vx * 6;
        n->y -= n->vy * 6;
        return;
    case 4: nemesis_rest(n); return;
    default: return;
    }
}

/* 0x085289FC: the swoop's legs (x, y pixels, angle turned, frames), x
 * mirrored toward the target, ended by 0x7FFF. */
static const int16_t nemesis_swoop[][4] = {
    {48, 8, 0, 24},  {8, -8, 0x3400, 8},  {8, 8, -0x3400, 8},     {-24, -4, 0x3000, 8},
    {32, 0, 0, 8},   {-24, 4, -0x3000, 8}, {0x7FFF, 0, 0, 0},
};

/* sub_080C70F0 (state 4): each leg moves and turns it evenly over its
 * frames, sound 0x85 at each new leg; 41 frames of rest, then state 1. */
static void nemesis_swoop_update(AosEnemy *n) {
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->nem_passes = 0;
        aos_audio_play_song(0x85);
        n->step = 1;
        /* fallthrough */
    case 1: {
        const int16_t *leg = nemesis_swoop[n->nem_passes];
        bool left = n->nem_flags & NEM_TARGET_LEFT;
        uint8_t frames = (uint8_t)leg[3];
        n->timer = frames;
        n->vx = (int32_t)(left ? -leg[0] : leg[0]) * 0x10000 / frames;
        n->vy = (int32_t)leg[1] * 0x10000 / frames;
        n->nem_spin = (int16_t)(leg[2] / frames);
        if (!left) n->nem_spin = (int16_t)-n->nem_spin;
        n->step = 2;
    }
        /* fallthrough */
    case 2:
        n->x += n->vx;
        n->y += n->vy;
        n->angle = (uint16_t)(n->angle + n->nem_spin);
        if (--n->timer) return;
        n->nem_passes++;
        n->step = nemesis_swoop[n->nem_passes][0] == 0x7FFF ? 5 : 1;
        n->timer = 0;
        aos_audio_play_song(0x85);
        return;
    case 5: nemesis_rest(n); return;
    default: return;
    }
}

/* sub_080C722C (state 5): it spins (0x1000 a frame, sound 0x85 every 16
 * frames) in place for 65 frames, then points at the target and flies at
 * it at two pixels a frame until it reaches its x, then state 1. */
static void nemesis_dive(AosEnemy *n, const AosSoma *soma) {
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->vx = n->vy = n->ax = n->ay = 0;
        n->nem_spin = 0x1000;
        n->timer = 0;
        n->step = 1;
        /* fallthrough */
    case 1: {
        if ((n->timer & 0xF) == 0) aos_audio_play_song(0x85);
        if (n->timer++ <= 0x40) break;
        n->nem_spin = 0;
        n->angle = (uint16_t)(angle_to_target(n, soma) + 0xC000);
        int16_t a = (int16_t)n->angle;
        n->vx = aos_fixed_mul(aos_sine((uint32_t)(a + 0x8000)), 0x20000);
        n->vy = aos_fixed_mul(aos_sine((uint32_t)(a + 0x4000)), 0x20000);
        int32_t frames = n->vx ? (target_x(soma) - n->x) / n->vx : 0;
        n->timer = (uint8_t)(frames < 0 ? -frames : frames);
        n->step = 2;
        aos_audio_play_song(0x85);
        break;
    }
    case 2:
        if (--n->timer == 0) n->step = 3;
        break;
    default: nemesis_enter(n, 1); break;
    }
    n->x += n->vx;
    n->y += n->vy;
    n->vx += n->ax;
    n->vy += n->ay;
    n->angle = (uint16_t)(n->angle + n->nem_spin);
}

/* sub_080C73F8 (state 6): knocked away from Soma along angle 0xD000 (two
 * pixels a frame across, 1.5 up, falling by 0xF00 a frame) while the
 * linked sprite shows frame 3; once it is gone, state 1, or 0xFF dead. */
static void nemesis_recoil(AosEnemy *n, const AosSoma *soma) {
    switch (n->step) {
    case 0:
        n->timer = 0;
        n->phase = 0;
        if (soma->x < n->x) n->nem_flags |= NEM_SOMA_LEFT;
        else n->nem_flags &= (uint16_t)~NEM_SOMA_LEFT;
        n->step = 1;
        /* fallthrough */
    case 1: {
        int32_t speed = (n->nem_flags & NEM_SOMA_LEFT) ? 0x20000 : -0x20000;
        n->x += (speed >> 8) * (aos_sine(0xD000 + 0x4000) >> 8);
        n->y += (aos_sine(0xD000) >> 8) * (0x18000 >> 8) + (int16_t)n->phase * 0xF00;
        n->phase = (uint16_t)(n->phase + 1);
        if (n->nem_helper.state == 4) n->step++;
        break;
    }
    case 2:
        if (n->nem_helper.state != 0) break;
        if (n->hp <= 0) {
            if (n->state != 0xFF) {
                n->state = 0xFF;
                n->step = n->substep = 0;
            }
        } else {
            nemesis_enter(n, 1);
        }
        break;
    default: break;
    }
    if (n->hp <= 0) n->nem_flags |= NEM_NO_COLLIDE;
}

/* sub_080C750C (state 0xFF): sound 0x72, the trails go, it falls from 0.5
 * by 0x800 a frame, blinking, a flame of a 16 x 16 area every seventh
 * frame, until below screen row 0xAA or after 65 frames. */
static void nemesis_die(AosEnemy *n, int cam_y, uint32_t (*random)(void), AosHitReport *report) {
    if (n->step == 0) {
        aos_audio_play_song(0x72);
        n->vy = 0x8000;
        n->ay = 0x800;
        n->timer = 0;
        n->step = 1;
    }
    if (n->step == 1) {
        n->y += n->vy;
        n->vy += n->ay;
        if ((int16_t)((n->y >> 16) - cam_y) > 0xAA || n->timer++ > 0x40) n->removed = true;
    }
    if (n->timer % 7 == 0) spawn_flame_at(n->x, n->y, 0x100000, 0x100000, random, report);
    n->hidden = n->timer & 1;
}

/* sub_080C7604 (its hit callback, after the damage and the 8-frame flash):
 * three blows in four only count down + 0x24; the fourth, or a lethal one
 * (sound 0x1C3), sends it into its recoil (state 6) with the linked sprite
 * in state 3. Any blow shows it. */
void aos_enemy_nemesis_struck(AosEnemy *n) {
    if (n->nem_hits && n->hp > 0) {
        n->nem_hits--;
    } else {
        if (n->hp <= 0) aos_audio_play_song(0x1C3);
        nemesis_helper_set(n, 3);
        n->state = 6;
        n->step = 0;
        n->nem_hits = 3;
    }
    n->hidden = false;
}

/* EnemyNemesisUpdate: state 7 is inert, 0xFF dies; otherwise it enters the
 * homing list on screen (sub_08021924); alive and flashing from a blow
 * (sub_0806AD24) it only collides, outside state 0; else states 0-6 and
 * the collision pass until dead. Its body is never mirrored; its affine
 * angle (+ 0x1C) turns it. The first update emits its linked sprite and
 * the three trails (native: at creation). */
AosHitReport aos_enemy_nemesis_update(AosEnemy *n, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (n->state == 7) return report;
    if (!n->helper_spawned) {
        n->helper_spawned = true;
        static const uint8_t sequences[] = {9, 12, 6};
        AosEnemy helper = {0};
        helper.id = n->id;
        helper.role = AOS_ROLE_NEMESIS_HELPER;
        helper.static_frame = n->nem_helper.frame;
        helper.combat.attack_off = helper.combat.hurt_off = true;
        report.children[report.child_count++] = helper;
        for (unsigned i = 0; i < sizeof sequences; ++i) {
            AosEnemy effect = {0};
            effect.id = n->id;
            effect.role = AOS_ROLE_NEMESIS_EFFECT;
            effect.variant = effect.step = sequences[i];
            effect.static_frame = 0;
            effect.x = n->x;
            effect.y = n->y;
            effect.hidden = n->hidden;
            effect.translucent = true;
            effect.combat.attack_off = effect.combat.hurt_off = true;
            report.children[report.child_count++] = effect;
        }
        n->spawned_count = report.child_count;
    }
    if (n->state == 0xFF) {
        nemesis_die(n, cam_y, random, &report);
        nemesis_helper_step(n, soma);
        return report;
    }
    int sx = (n->x >> 16) - cam_x, sy = (n->y >> 16) - cam_y;
    if (!n->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report.targetable = true;
    bool pass = true;
    if (n->hp > 0 && n->hit_flash) {
        n->hit_flash--;
        pass = n->state != 0;
    } else {
        switch (n->state) {
        case 0: nemesis_appear(n, soma); break;
        case 1: nemesis_approach(n, soma, random); break;
        case 2: nemesis_choose(n, random); break;
        case 3: nemesis_dash(n); break;
        case 4: nemesis_swoop_update(n); break;
        case 5: nemesis_dive(n, soma); break;
        case 6: nemesis_recoil(n, soma); break;
        default: break;
        }
    }
    if (pass && !(n->nem_flags & NEM_NO_COLLIDE)) {
        AosHitReport hit = collide(n, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&n->combat);
    nemesis_helper_step(n, soma);
    return report;
}
