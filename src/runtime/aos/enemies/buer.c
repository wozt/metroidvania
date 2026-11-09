/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Buer (enemy 0x0A), its fire and burst.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define buer_player_in_window aos_enemy_buer_player_in_window
#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define play_loop aos_enemy_play_loop
#define player_dx aos_enemy_player_dx
#define spawn_flame_at aos_enemy_spawn_flame_at
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_0806E29C(e, w, h): the target inside the w x h box centred on the
 * entity (unsigned 16-bit whole-pixel compares, inclusive). */
bool aos_enemy_buer_player_in_window(const AosEnemy *e, const AosSoma *soma,
                                  uint16_t width, uint16_t height) {
    uint16_t dx = (uint16_t)((target_x(soma) >> 16) - ((e->x >> 16) - (int)(width / 2)));
    uint16_t dy = (uint16_t)((target_y(soma) >> 16) - ((e->y >> 16) - (int)(height / 2)));
    return dx <= width && dy <= height;
}

/* sub_0806DF40(e, dx, dy) / sub_0806B0A8 (dx = dy = 0): the point outside
 * the room's cells (30 x 26 for a single screen, 32 per screen otherwise;
 * negative pixels round toward zero). */
static bool room_outside(const AosEnemy *e, const AosCollision *layer, int dx, int dy) {
    if (!layer || !layer->width_screens || !layer->height_screens) return false;
    int width = layer->width_screens == 1 ? 30 : 32 * layer->width_screens;
    int height = layer->height_screens == 1 ? 26 : 32 * layer->height_screens;
    int x = (int16_t)((e->x >> 16) + dx), y = (int16_t)((e->y >> 16) + dy);
    int cx = (x < 0 ? x + 7 : x) >> 3, cy = (y < 0 ? y + 7 : y) >> 3;
    return cx < 0 || cx >= width || cy < 0 || cy >= height;
}

/* The walker probes 0x08528B64 (exported as `probes buer`). */
static const AosProbes *buer_probes(const AosEnemyKind *kind) {
    return kind && kind->probes.count > 0 && kind->probes.count <= 8 ? &kind->probes : NULL;
}

/* sub_0806CAF8(e, + 0x18, 0x08528B64, mode) then sub_080CC5B4: the move is
 * undone when the point 24 pixels ahead (by the speed's sign) leaves the
 * room. */
static int buer_walk(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, int32_t speed, int mode) {
    const AosProbes *p = buer_probes(kind);
    int32_t x0 = b->x;
    int r = p ? ground_walk(b, soma, layer, p, speed, mode) : 0;
    if (room_outside(b, layer, b->spin < 0 ? -0x18 : 0x18, 0)) b->x = x0;
    return r;
}

/* The fire's facing (+ 0x58 bit 6 of the linked sprite). */
static void buer_fire_face(AosEnemy *b, bool right) { b->buer_fire_mirrored = right; }

/* sub_080CC20C(e, rolling): the walk (mode 2 rolling, else 6) at the spin
 * + 0x18, then on a downhill slope (0x40) the spin loses 0x333 of its
 * magnitude, uphill (0x20) gains it, otherwise it decays by 0x40 to at
 * least 0x4000; the affine angle + 0x14 turns by spin / 64; a wall, or no
 * floor while not rolling, sends it back at half the spin; the fire faces
 * the spin. */
static void buer_roll(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, bool rolling) {
    int r = buer_walk(b, soma, layer, kind, b->spin, rolling ? 2 : 6);
    if (r & 0x40) b->spin += b->spin > 0 ? -0x333 : 0x333;
    else if (r & 0x20) b->spin += b->spin > 0 ? 0x333 : -0x333;
    else if (b->spin >= 0) {
        b->spin -= 0x40;
        if (b->spin <= 0x3FFF) b->spin = 0x4000;
    } else {
        b->spin += 0x40;
        if (b->spin > -0x4000) b->spin = -0x4000;
    }
    int32_t s = b->spin;
    b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
    if ((r & 3) || (!rolling && !(r & 8))) b->spin = -s / 2;
    buer_fire_face(b, b->spin >= 0);
}

/* sub_080CC5D8(x, y, animation, flip): a graphics-only fire burst
 * (sub_080CC178: palette 6, the animation once, priority 0x11, still). */
static void buer_burst(const AosEnemy *b, int k, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy shot = {0};
    shot.id = b->id;
    shot.role = AOS_ROLE_BUER_PROJECTILE;
    shot.static_frame = -1;
    shot.x = AOS_FIXED(b->x >> 16);
    shot.y = AOS_FIXED((int16_t)((b->y >> 16) + 0x18));
    shot.variant = (uint8_t)k;
    shot.mirrored = b->nem_passes != 0;     /* + 0x13: its target to the right */
    shot.combat.attack_off = shot.combat.hurt_off = true;
    report->children[report->child_count++] = shot;
}

/* sub_080CBC64 (state 2, step 1): it takes its target's side (+ 0x13),
 * then for 80 frames spins up by 0xCCC a frame toward it (at most 2.0,
 * which ends the stage at once), the fire facing the spin, turning by
 * 0x400 (0x800 below 51 frames) and, past 50, every third frame two fire
 * bursts (sound 0x84 every ninth); then 32 frames rolling on, spun by the
 * slopes, and again. */
static void buer_attack(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, AosHitReport *report) {
    switch (b->substep) {
    case 0:
        b->nem_passes = (uint8_t)(player_dx(b, soma) > 0 ? 0 : 1);
        b->timer = 0x50;
        b->substep = 1;
        /* fallthrough */
    case 1: {
        buer_walk(b, soma, layer, kind, b->spin, 6);
        bool right = b->nem_passes != 0;
        if (right) {
            b->spin += 0xCCC;
            if (b->spin > 0x20000) {
                b->spin = 0x20000;
                b->timer = 0;
            }
            if (b->spin > 0) buer_fire_face(b, true);
            b->angle += b->timer > 0x32 ? 0x400u : 0x800u;
        } else {
            b->spin -= 0xCCC;
            if (b->spin < -0x20000) {
                b->spin = -0x20000;
                b->timer = 0;
            }
            if (b->spin < 0) buer_fire_face(b, false);
            b->angle += b->timer > 0x32 ? (uint32_t)-0x400 : (uint32_t)-0x800;
        }
        uint8_t t = b->timer;
        if (t % 3 == 0 && t > 0x32) {
            if (t % 9 == 0) aos_audio_play_song(0x84);
            buer_burst(b, 0, report);
            buer_burst(b, 1, report);
        }
        if (--b->timer == 0xFF) {
            b->substep = 2;
            b->timer = 0x20;
        }
        break;
    }
    case 2: {
        int r = buer_walk(b, soma, layer, kind, b->spin, 6);
        if (r & 0x40) b->spin += b->spin > 0 ? -0x333 : 0x333;
        else if (r & 0x20) b->spin += b->spin > 0 ? 0x333 : -0x333;
        int32_t s = b->spin;
        b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
        if (--b->timer == 0xFF) b->substep = 0;
        break;
    }
    default: break;
    }
}

/* sub_080CBAEC (state 2): step 0 rolls (not rolling mode) and attacks
 * its target inside the 192 x 48 box on the fire's side; every 256 frames
 * an even draw makes it wait (step 2): for 32-63 frames the fire turns
 * round and its target inside the 96 x 48 box starts the attack, then an
 * even draw rolls on with the spin toward the fire's side, an odd one
 * waits again. */
static void buer_idle(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, uint32_t (*random)(void),
                      AosHitReport *report) {
    switch (b->step) {
    case 0: {
        buer_roll(b, soma, layer, kind, false);
        if (buer_player_in_window(b, soma, 0xC0, 0x30)) {
            int side = b->x < target_x(soma) ? 1 : b->x > target_x(soma) ? -1 : 0;
            if ((!b->buer_fire_mirrored && side == -1) || (b->buer_fire_mirrored && side == 1)) {
                b->step = 1;
                b->substep = 0;
            }
        }
        if (++b->timer == 0xFF) {
            b->timer = 0;
            if (!(random() & 1)) b->step = 2;
            b->substep = 0;
        }
        break;
    }
    case 1: buer_attack(b, soma, layer, kind, report); break;
    case 2:
        if (b->substep == 0) {
            b->timer = (uint8_t)((random() & 0x1F) + 0x20);
            b->substep = 1;
        }
        if (b->substep == 1) {
            b->buer_fire_mirrored = !b->buer_fire_mirrored;
            b->substep = 2;
        }
        if (buer_player_in_window(b, soma, 0x60, 0x30)) {
            b->step = 1;
            b->substep = 0;
        }
        if (--b->timer == 0xFF) {
            if (!(random() & 1)) {
                b->step = b->substep = 0;
                b->timer = 0;
                int32_t magnitude = b->spin < 0 ? -b->spin : b->spin;
                b->spin = b->buer_fire_mirrored ? magnitude : -magnitude;
            } else {
                b->substep = 0;
            }
        }
        break;
    default: break;
    }
}

/* Eight simultaneous sub_0806D578 flames at the ROM's 0x081194DC offsets
 * (exported as `buer_death_offsets`), 4 pixels lower, each mirrored at
 * random. */
static void buer_terminal_flames(const AosEnemy *e, const AosEnemyKind *kind,
                                 uint32_t (*random)(void), AosHitReport *report) {
    if (!kind || !kind->buer_death_offsets_valid) return;
    for (unsigned i = 0; i < 8 && report->effect_count < AOS_ENEMY_MAX_EFFECTS; ++i) {
        AosEffect *flame = &report->effects[report->effect_count++];
        *flame = (AosEffect){
            .kind = AOS_EFFECT_FLAME,
            .x = (int32_t)((uint32_t)e->x + ((uint32_t)kind->buer_death_offsets[i][0] << 16)),
            .y = (int32_t)((uint32_t)e->y + ((uint32_t)kind->buer_death_offsets[i][1] << 16) +
                           0x40000u)};
        flame->mirrored = (random() & 1u) != 0;
    }
}

/* sub_080CBEA4 (state 3, dead): sound 0x6E, no attack; the fire stops
 * following and, for 97 frames, wobbles by a pixel every third frame and
 * moves with the body, flaming every seventh, while the body's spin from
 * 2.0 decays by 0x380 to 0x4000 turning it on the floor walker (speed 0);
 * then the fire goes and for 129 frames the body rolls on with a 32 x 32
 * flame every other frame (0x6E every 20th past 28); then eight flames
 * (sound 0x6E) and it is gone. */
static void buer_die(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, uint32_t (*random)(void), AosHitReport *report) {
    int32_t x0 = b->x, y0 = b->y;
    switch (b->step) {
    case 0:
        b->combat.attack_off = true;
        aos_audio_play_song(0x6E);
        b->buer_fire_following = false;
        b->timer = 0x60;
        b->step = 1;
        b->spin = b->spin >= 0 ? 0x20000 : -0x20000;
        /* fallthrough */
    case 1:
        if (b->timer % 7 == 0)
            spawn_flame_at(b->buer_fire_x, b->buer_fire_y, 0x100000, 0x100000, random, report);
        if (b->timer % 3 == 0) {
            b->buer_death_linked_wobble = 1;
            b->buer_fire_x += 0x10000;
            b->buer_fire_y += 0x10000;
        } else if (b->buer_death_linked_wobble) {
            b->buer_fire_x -= 0x10000;
            b->buer_fire_y -= 0x10000;
            b->buer_death_linked_wobble = 0;
        }
        if (b->spin >= 0) {
            b->spin -= 0x380;
            if (b->spin <= 0x3FFF) b->spin = 0x4000;
        } else {
            b->spin += 0x380;
            if (b->spin > -0x4000) b->spin = -0x4000;
        }
        {
            int32_t s = b->spin;
            b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
        }
        if (buer_probes(kind)) ground_walk(b, soma, layer, buer_probes(kind), 0, 2);
        b->buer_fire_x += b->x - x0;
        b->buer_fire_y += b->y - y0;
        if (--b->timer == 0xFF) {
            b->buer_fire_gone = true;
            b->timer = 0x80;
            b->step = 2;
        }
        break;
    case 2:
        if ((b->timer & 1) == 0) {
            spawn_flame_at(b->x, b->y, 0x200000, 0x200000, random, report);
            if (b->timer % 20 == 0 && b->timer > 0x1C) aos_audio_play_song(0x6E);
        }
        buer_roll(b, soma, layer, kind, true);
        if (--b->timer == 0xFF) b->step = 3;
        break;
    case 3:
        buer_terminal_flames(b, kind, random, report);
        aos_audio_play_song(0x6E);
        b->removed = true;
        break;
    default: break;
    }
}

/* EnemyBuerUpdate: state 4 (the global 0x8E & 0x40) rises 32 pixels once
 * and spins by 0x100 a frame; state 3 dies; otherwise it enters the homing
 * list, only collides while flashing from a blow, rolls (state 1, until 92
 * pixels below it leave the room: state 2) or idles and attacks (state 2),
 * deleted with its fire once it leaves the room (sub_080CC55C), then the
 * collision pass. The fire (sub_080CBA30) is made by the first update
 * (native: at creation). */
AosHitReport aos_enemy_buer_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!b->helper_spawned) {
        b->helper_spawned = true;
        AosEnemy fire = {0};
        fire.id = b->id;
        fire.role = AOS_ROLE_BUER_FIRE;
        fire.static_frame = 1;
        fire.x = b->x;
        fire.y = b->y;
        fire.mirrored = b->buer_fire_mirrored;
        fire.combat.attack_off = fire.combat.hurt_off = true;
        spawned.children[spawned.child_count++] = fire;
    }
    if (b->state == 4) {
        if (b->step == 0) {
            b->y = (int32_t)((uint32_t)b->y - (32u << 16));
            b->static_frame = 0;
            b->step = 1;
        }
        b->angle += 0x100u;
    } else if (b->state == 3) {
        buer_die(b, soma, layer, kind, random, &spawned);
    } else {
        int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
        if (!b->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report.targetable = true;
        bool pass = true;
        if (b->hp > 0 && b->hit_flash) {
            b->hit_flash--;
        } else if (room_outside(b, layer, 0, 0)) {
            /* sub_080CC55C: gone with its fire. */
            b->removed = true;
            pass = false;
        } else if (b->state == 1) {
            buer_roll(b, soma, layer, kind, true);
            if (room_outside(b, layer, 0, 0x5C)) {
                b->state = 2;
                b->step = b->substep = 0;
            }
        } else if (b->state == 2) {
            buer_idle(b, soma, layer, kind, random, &spawned);
        }
        if (pass && b->state != 3) {
            AosHitReport hit = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            bool targetable = report.targetable;
            report = hit;
            report.targetable |= targetable;
        }
        aos_combat_tick(&b->combat);
    }
    if (b->buer_fire_following) {
        b->buer_fire_x = b->x;
        b->buer_fire_y = b->y;
    }
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_080CC3C8: the fire shows the second frame where its body keeps it,
 * with the body's fire facing; gone with it. */
void aos_enemy_buer_fire_update(AosEnemy *fire) {
    const AosEnemy *b = fire->parent;
    if (!b || b->removed || b->buer_fire_gone) {
        fire->removed = true;
        return;
    }
    fire->x = b->buer_fire_x;
    fire->y = b->buer_fire_y;
    fire->mirrored = b->buer_fire_mirrored;
    fire->hidden = b->hidden;
}

/* sub_080CC178: a fire burst plays its animation once in place. */
void aos_enemy_buer_burst_update(AosEnemy *shot, const AosEnemyKind *kind) {
    if (shot->state == 0) {
        if (!kind->anims || shot->variant >= kind->anims->count) {
            shot->removed = true;
            return;
        }
        play_loop(shot, kind->anims, shot->variant, false);
        shot->state = 1;
    }
    if (aos_anim_step(&shot->anim, kind->anims) == 3) shot->removed = true;
}

/* White Dragon fields: the head's two attack countdowns (+ 0x10 / + 0x12,
 * then its target), its home (the neck's root, + 0x14 + + 0x18 / + 0x16 +
 * + 0x1A in room pixels), the swing amplitude + 0x1C, the wobble phase
 * + 0x1E, the swing angle + 0x48 and its rate + 0x4C, the heading + 0x4A,
 * the neck length + 0x4E, the breath angle + 0x52 and its sweep + 0x50,
 * and the 100 past swing angles (its 0x32-word allocation). */
