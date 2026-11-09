/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Skull Millione (enemy 0x5B), Cagnazzo (0x49) and Lubicant (0x63, the same code) with their fins.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"
#include "../aos_audio.h"

#include <stdlib.h>
#include <string.h>

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define move_toward aos_enemy_move_toward
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_centred aos_enemy_player_in_centred
#define random_centred aos_enemy_random_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* The records at 0x0852883C (20 bytes) of Skull Millione, Cagnazzo and
 * Lubicant: flags (+ 5: bit 0 a fin, bit 1 the hover attack), the lead on
 * the player's speed (+ 6), the idle wait (+ 7), the chase's top speed
 * (+ 0xC) and the dive's speed (+ 0x10). */
typedef struct {
    uint8_t flags;
    int8_t lead;
    uint8_t wait;
    int32_t top_speed, dive;
} SkullRecord;

static const SkullRecord skull_records[3] = {
    {0, 4, 0x20, 0x100000, 0x50000},
    {3, 2, 0x20, 0x140000, 0x40000},
    {1, 8, 0, 0x180000, 0x40000},
};

/* sub_080B7A34: it bobs (+ 0x18 from 0x8000, vy = sin(+ 0x18 * 0x5B0) /
 * 2) for the record's wait, then, with the player in the 0xDC x 0xA0 box
 * centred 40 pixels below it, faces him and chases (state 1); a fin
 * without the hover attack plays animation 3. */
static void skull_idle(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims) {
    const SkullRecord *r = &skull_records[e->variant];
    if (e->step == 0) {
        e->step = 1;
        e->phase = 0x8000;
        e->timer = r->wait;
    }
    e->phase++;
    int32_t s = aos_sine((uint32_t)(int16_t)e->phase * 0x5B0u & 0xFFFF);
    e->vy = s / 2;
    e->y += e->vy;
    if (e->timer) {
        e->timer--;
        return;
    }
    e->y += 0x280000;
    if (player_in_centred(e, soma, 0xDC, 0xA0)) {
        face_player(e, soma);
        e->state = 1;
        e->substep = e->step = 0;
        if ((r->flags & 1) && !(r->flags & 2)) play(e, anims, 3);
    }
    e->y -= 0x280000;
}

/* sub_080B7AEC: from rest its speed (+ 0x20) gains 0x2000 a frame up to
 * the record's top; it heads (sub_080694B8) for 80 pixels above the
 * player, the speed short of him, led by his vx times the record's lead,
 * facing him at each frame start, and dives (state 2) on arrival. */
static void skull_chase(AosEnemy *e, const AosSoma *soma) {
    const SkullRecord *r = &skull_records[e->variant];
    if (e->step == 0) {
        e->speed = 0;
        e->step = 1;
    }
    if (e->anim.tick == 0) face_player(e, soma);
    if (e->speed < r->top_speed) e->speed += 0x2000;
    int32_t tx = (int32_t)((uint32_t)(target_x(soma) >> 16) << 16);
    int32_t ty = (int32_t)((uint32_t)((target_y(soma) >> 16) - 0x50) << 16);
    tx += target_x(soma) > e->x ? -e->speed : e->speed;
    tx += soma->vx * r->lead;
    bool arrived = move_toward(e, tx, ty, e->speed);
    e->x += e->vx;
    e->y += e->vy;
    if (arrived) {
        e->state = 2;
        e->substep = e->step = 0;
    }
}

/* sub_080B7BC8: the dive. A hover-attack variant first circles the player
 * for 64 frames (animation 3: + 0x50 +/- 0x3000 toward him up to 2.5, +
 * 0x54 0x1800 down while above y - 104, else -0x3000 with vy at most
 * -0x1000, up to 0.5). Then animation 1 once rising (vy -2.0, + 0x54
 * 0x1000 up to 0, vx braked by -vx / 16 for 32 frames); at animation frame
 * 4 it plunges (the record's dive speed, + 0x54 -0x2000 down to -3.0;
 * sound 0x87, and 0x1BA for Cagnazzo) until the animation ends, then rises
 * until vy turns negative and bobs again (state 0, animation 0). */
static void skull_dive(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims) {
    const SkullRecord *r = &skull_records[e->variant];
    switch (e->step) {
    case 0:
        if (r->flags & 2) {
            play(e, anims, 3);
            e->timer = 0x40;
            e->step = 5;
            return;
        }
        /* fall through */
    case 1:
        play_loop(e, anims, 1, false);
        e->vy = -0x20000;
        e->ay = 0x1000;
        e->ax = -e->vx / 16;
        e->step = 2;
        e->timer = 0x20;
        /* fall through */
    case 2:
        e->x += e->vx;
        e->y += e->vy;
        if (e->timer) {
            e->timer--;
            e->vx += e->ax;
        }
        e->vy += e->ay;
        if (e->vy > 0) e->vy = 0;
        if (e->anim.frame == 4) {
            e->step = 3;
            e->vy = r->dive;
            e->ay = -0x2000;
            aos_audio_play_song(0x87);
            if (e->variant == 1) aos_audio_play_song(0x1BA);
        }
        return;
    case 3:
        e->vy += e->ay;
        if (e->vy < -0x30000) e->vy = -0x30000;
        e->y += e->vy;
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->ay = -0x2000;
            e->step = 4;
        }
        return;
    case 4:
        e->y += e->vy;
        e->vy += e->ay;
        if (e->vy >= 0) return;
        e->vy = 0;
        e->state = e->substep = e->step = 0;
        play(e, anims, 0);
        return;
    case 5:
        if (e->anim.frame == 0 && e->anim.tick == 0) face_player(e, soma);
        e->ax = target_x(soma) > e->x ? 0x3000 : -0x3000;
        if (target_y(soma) - 0x680000 > e->y) {
            e->ay = 0x1800;
        } else {
            if (e->vy > -0x1000) e->vy = -0x1000;
            e->ay = -0x3000;
        }
        /* sub_0806D430(e, 2.5), sub_0806D460(e, 0.5) */
        e->vx += e->ax;
        if (e->ax >= 0 ? e->vx > 0x28000 : e->vx < -0x28000)
            e->vx = e->ax >= 0 ? 0x28000 : -0x28000;
        e->x += e->vx;
        e->vy += e->ay;
        if (e->ay >= 0 ? e->vy > 0x8000 : e->vy < -0x8000) e->vy = e->ay >= 0 ? 0x8000 : -0x8000;
        e->y += e->vy;
        if (e->timer) e->timer--;
        else e->step = 1;
        return;
    default:
        return;
    }
}

/* sub_080B7D90: animation 2 once (sound 0x1BC) and 32 frames with a blood
 * splash of sub_08045CEC (animation 13, row 8) at x + (r & 7) - 4, y +
 * (r & 15) - 24 every 16, flipped at random; then it sinks (vy -0.5, +
 * 0x54 0x1000 up to 8.0) through the generic death of 50 frames. Beyond
 * the window (-128, -64, 0x1F0, 0x120) of the screen it is deleted. */
static void skull_die(AosEnemy *e, const AosEnemyKind *kind, int cam_x, int cam_y,
                      uint32_t (*random)(void), AosHitReport *report) {
    switch (e->step) {
    case 0:
        aos_audio_play_song(0x1BC);
        play_loop(e, kind->anims, 2, false);
        e->step = 1;
        e->timer = 0x20;
        /* fall through */
    case 1:
        if ((e->timer & 0xF) == 0) {
            int32_t x = e->x + (random_centred(random, 8) << 16);
            int32_t y = e->y + ((random_centred(random, 0x10) - 0x10) << 16);
            AosEffect *blood = spawn_drift(AOS_EFFECT_BLOOD, x, y, report);
            if (blood) blood->mirrored = random() & 1;
        }
        if (e->timer) {
            e->timer--;
            break;
        }
        e->vy = -0x8000;
        e->ay = 0x1000;
        e->step = 2;
        e->timer = 0x32;
        break;
    case 2:
        e->vy += e->ay;
        if (e->vy > 0x80000) e->vy = 0x80000;
        e->y += e->vy;
        generic_death(e, kind, random, report);
        break;
    default:
        break;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) e->removed = true;
}

/* sub_080B7988: the fin of Cagnazzo and Lubicant, an affine sprite (frame
 * 0xD) of their sheet. */
static AosEnemy skull_fin(const AosEnemy *owner) {
    AosEnemy f = {0};
    f.id = owner->id;
    f.role = AOS_ROLE_FIN;
    f.static_frame = 0xD;
    f.x = owner->x;
    f.y = owner->y;
    f.hidden = true;
    f.hp = 1;
    return f;
}

/* sub_080B7780: hidden until its owner plays animation 3, then held 12
 * pixels ahead of it and 6 above, turning by -0x1200 a frame (mirrored
 * +0x1200). Once the owner dies (state 3) it is flung off (vx (r & 63) -
 * 32 / 16, vy -3.5 + ((r & 15) - 8) / 16, + 0x54 0x2000 up to 8.0),
 * turning by 0xB00 (mirrored -0xB00), and deleted beyond margin entry 4. */
void aos_enemy_fin_update(AosEnemy *f, const AosEnemyKind *kind, int cam_x, int cam_y,
                       uint32_t (*random)(void)) {
    const AosEnemy *o = f->parent;
    if (f->state <= 1 && o && o->state == 3) f->state = 2;
    if (f->state <= 1 && o) {
        if (f->state == 0) {
            if (o->anim.id != 3) return;
            f->angle = 0;
            f->hidden = false;
            f->state = 1;
        }
        if (o->anim.id != 3) {
            f->hidden = true;
            f->state = 0;
            return;
        }
        f->mirrored = o->mirrored;
        f->x = (int32_t)((uint32_t)((o->x >> 16) + (f->mirrored ? 12 : -12)) << 16);
        f->y = (int32_t)((uint32_t)((o->y >> 16) - 6) << 16);
        f->angle = (uint16_t)(f->angle + (f->mirrored ? 0x1200 : -0x1200));
        return;
    }
    if (f->state == 2) {
        f->hidden = false;
        if (o) {
            f->mirrored = o->mirrored;
            f->x = (int32_t)((uint32_t)((o->x >> 16) + (f->mirrored ? 12 : -12)) << 16);
            f->y = (int32_t)((uint32_t)((o->y >> 16) - 6) << 16);
        }
        f->vy = (random_centred(random, 0x10) << 12) - 0x38000;
        f->ay = 0x2000;
        f->vx = random_centred(random, 0x40) << 12;
        f->ax = 0;
        f->angle = 0;
        f->state = 3;
    }
    f->angle = (uint16_t)(f->angle + (f->mirrored ? -0xB00 : 0xB00));
    /* sub_0806D3D8(e, 8.0) */
    f->vx += f->ax;
    f->vy += f->ay;
    if (f->vy > 0x80000) f->vy = 0x80000;
    f->x += f->vx;
    f->y += f->vy;
    if (outside_margins(kind, 4, (f->x >> 16) - cam_x, (f->y >> 16) - cam_y)) f->removed = true;
}

/* sub_080B7F40: hidden beyond margin entry 4 (no activity window); while
 * alive and flashing from a blow (sub_0806AD24) it only collides. Then
 * state 0 bobs, 1 chases, 2 dives, 3 dies, 4 only steps its animation;
 * outside the death the collision pass (sub_080421AC: no on-screen test)
 * and two animation steps (sub_0806DF20, then sub_0803F17C). The tracking
 * list of sub_08021924 and the routine sub_08021654 of its draw callback
 * sub_080B76E4 are not ported. */
AosHitReport aos_enemy_skull_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                 int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    const AosAnimSet *anims = kind->anims;
    e->hidden = outside_margins(kind, 4, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y);
    if (!e->dropped) {
        e->dropped = true;
        if ((skull_records[e->variant].flags & 1) && e->state != 4)
            spawned.children[spawned.child_count++] = skull_fin(e);
    }
    if (e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 3) report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&e->combat);
    } else {
        switch (e->state) {
        case 0: skull_idle(e, soma, anims); break;
        case 1: skull_chase(e, soma); break;
        case 2: skull_dive(e, soma, anims); break;
        case 3: skull_die(e, kind, cam_x, cam_y, random, &spawned); break;
        default: break;
        }
        if (!e->removed) {
            if (e->state != 3 && e->state != 4) {
                report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
                step_anim(e, anims);
            }
            step_anim(e, anims);
        }
        aos_combat_tick(&e->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}
