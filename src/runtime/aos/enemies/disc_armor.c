/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Disc Armor (enemy 0x34) and its thrown disc,
 * the disc's shards and its orbit glow.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API.
 *
 * Native layout (cvaos code_080B0AD4): the armor (EnemyDiscArmorCreate
 * 0x080B2228, update 0x080B22D4) keeps its logical position in + 0x40/
 * + 0x44 and derives the drawn/hit position + 0x18/+ 0x1C every frame as
 * position + orbit offset (+ 0x20/+ 0x22, x negated while + 0x58 bit
 * 0x40). State 0 (sub_080B1F14) patrols with the sub_0806CAF8 walker on
 * probes 0x08528714 mode 0x14 and watches for Soma through
 * sub_0806D044/sub_0806BBC4; state 1 (sub_080B205C) faces the player,
 * throws the disc (sub_080B1678 through sub_0806DFF8) when the animation
 * position (+ 0x6E/+ 0x6F as a 16-bit word) reaches 0x10C, then eases the
 * orbit offsets -20/-36 toward -10/-16; state 2 (sub_080B2148) is the
 * death lurch (vy -40.0, gravity 0x12000, spin 0x5000, gore
 * sub_08045CEC, songs 0x6F then 0x6E); state 3 only steps the animation
 * under the global 0x8E & 0x40 flag.
 *
 * The disc (sub_080B1678) runs an eight-state machine: case 0 loads the
 * same resources, picks a random sign draw (sub_0803E654), installs the
 * orbit callback sub_080B14B4 as its + 0x18 child, plays song 0x74 and
 * the effects of sub_080458E4; cases 1-3 orbit around the armor with
 * sine offsets (sub_080009E4), wall probes (sub_0800207C/0x02058/0x01D94)
 * and gravity; case 3 also throws a type-5 shard (sub_080B1428 through
 * EntityCreateInRange 0x49..0xCF) aimed by + 0x42/+ 0x46 and plays
 * 0x104; case 4 bursts more shards on a timer, plays 0x73 every 16 ticks
 * and switches to case 5 when the armor's + 0x14 bit 1 is owed or the
 * disc comes within 0x2F of the aim point, computing the return heading
 * with ArcTan2 and the speed with Sqrt/3; case 5 homes back with a
 * shrinking radius and sine wobble; case 6 settles on the probe walker
 * 0x08528720 mode 2 and case 7 fades (sub_0803E6BC) then deletes.
 * The shard (sub_080B1428) picks animation + 0x14 (4 or 5), moves with
 * sub_0806D3D8 and deletes when its animation ends; the glow
 * (sub_080B14B4) keeps an affine orbit around the disc through
 * sub_080B23C4/sub_0803E058. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"
#include "../../aos/core/aos_collision.h"
#include "../../aos/math/aos_math.h"

#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define spawn_drift aos_enemy_spawn_drift
#define probe_walk aos_enemy_probe_walk
#define player_dx aos_enemy_player_dx
#define player_dy aos_enemy_player_dy
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define player_ahead_within aos_enemy_player_ahead_within

/* The armor's walker probes 0x08528714 and the disc's 0x08528720. */
static const AosProbes da_armor_probes = {2, -40, -1, 6, {-7, -24, 1, -16}};
static const AosProbes da_disc_probes = {1, -16, 16, 12, {0}};

/* sub_0806BC40: face the player (the + 0x58 bit 0x40 sign). */
static void da_face(AosEnemy *e, const AosSoma *soma) {
    if (!soma) return;
    int32_t dx = target_x(soma) - (e->x >> 16);
    e->mirrored = dx < 0;
}

/* sub_0806D044: the whole-pixel distance to the player target. */
static int da_distance(const AosEnemy *e, const AosSoma *soma) {
    if (!soma) return 0x7FFF;
    int32_t dx = player_dx(e, soma), dy = player_dy(e, soma);
    return (int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
}

/* The armor's orbit draw offset + 0x18/+ 0x1C: position plus the
 * (mirrored-negated) orbit pixel offsets, in subpixels. */
static void da_orbit_pos(AosEnemy *e) {
    int32_t ox = e->da_orbit_x, oy = e->da_orbit_y;
    if (e->mirrored) ox = -ox;
    e->da_ox = e->x + (ox << 16);
    e->da_oy = e->y + (oy << 16);
}

/* sub_080B1F14 state 0 step 2: the aggro test of _080B1FE8 — Soma within
 * 0x4F pixels always, or within 0x8B while facing it (the sub_0806BBC4
 * direction checks collapse into the facing test). */
static bool da_aggro(const AosEnemy *e, const AosSoma *soma) {
    int dist = da_distance(e, soma);
    if (dist <= 0x4F) return true;
    bool facing = !soma || ((target_x(soma) < (e->x >> 16)) == e->mirrored);
    return facing && dist <= 0x8B;
}

/* sub_080B1F14: state 0, the patrol. Step 0 picks animation 0 (param0
 * clear) or 1 looping and clears the + 0xD timer; step 1 is the aggro
 * gate; step 2 walks with a speed pulsing on animation frame % 9
 * (0x1000 + mod * 0x2000 while mod <= 6, else 0), turns around on a
 * 0x1B-masked wall result of 8 or every + 0x30 param0 frames at frame
 * 0x11, and enters state 1 on the aggro test. */
static void da_patrol(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->param0 == 0) {
            e->step = 1;
            if (e->anim.id != 0) play_loop(e, kind->anims, 0, true);
        } else {
            e->step = 2;
            if (e->anim.id != 1) play_loop(e, kind->anims, 1, true);
        }
        e->timer = 0;
    }
    if (e->step == 1) {
        if (da_aggro(e, soma)) {
            e->state = 1;
            e->step = e->substep = 0;
        }
        return;
    }
    if (e->step != 2) return;
    unsigned mod = e->anim.frame % 9;
    int32_t speed = mod <= 6 ? (int32_t)(mod * 0x2000 + 0x1000) : 0;
    int result = probe_walk(e, layer, &da_armor_probes, speed, 0x14);
    if (e->anim.frame == 0x11) {
        e->timer++;
        if (e->timer > (uint8_t)(int8_t)e->param0) {
            e->timer = 0;
            e->mirrored = !e->mirrored;
        }
    }
    if ((result & 0x1B) == 8) e->mirrored = !e->mirrored;
    if (e->anim.tick == 0 && e->anim.frame == 0x11 && da_aggro(e, soma)) {
        e->state = 1;
        e->step = e->substep = 0;
    }
}

/* sub_080B205C: state 1, the attack. Step 0 faces the player, plays
 * animation 2 once, arms da_flags bit 0 and resets the orbit offsets to
 * -20/-36; step 1 throws the disc when the animation word reaches
 * 0x10C, then on da_flags bit 1 plays animation 3 and moves to step 2;
 * step 2 eases the orbit offsets toward -10/-16 and returns to state 0
 * when animation 3 ends. */
static void da_attack(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      AosHitReport *report) {
    switch (e->step) {
    case 0:
        da_face(e, soma);
        if (e->anim.id != 2) play_loop(e, kind->anims, 2, false);
        e->step = 1;
        e->da_flags = 1;
        e->da_orbit_x = -20;
        e->da_orbit_y = -36;
        break;
    case 1: {
        uint16_t word = (uint16_t)(e->anim.frame | (e->anim.tick << 8));
        if (word == 0x10C && report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_0806DFF8(e, sub_080B1678): the disc child inherits the
             * facing and a 1.25x contact stat (+ 0x3C += + 0x3C >> 2). */
            AosEnemy d = {0};
            d.id = e->id;
            d.role = AOS_ROLE_DISC_BLADE;
            d.x = e->da_ox;
            d.y = e->da_oy;
            d.stats = e->stats;
            d.stats.contact = (uint8_t)(e->stats.contact +
                                        (e->stats.contact >> 2));
            d.hp = 1;
            d.mirrored = e->mirrored;
            d.parent = e;
            d.static_frame = -1;
            d.combat.type = AOS_TYPE_ENEMY;
            report->children[report->child_count++] = d;
        }
        if (e->da_flags & 2) {
            if (e->anim.id != 3) play_loop(e, kind->anims, 3, false);
            e->step = 2;
        }
        break;
    }
    case 2:
        if (e->da_orbit_x < -10) e->da_orbit_x++;
        if (e->da_orbit_y < -16) e->da_orbit_y++;
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->da_flags = 0;
            e->state = 0;
            e->step = e->substep = 0;
            da_face(e, soma);
        }
        break;
    default:
        break;
    }
}

/* sub_080B2148: state 2, the death lurch. Step 0 plays animation 8 once,
 * vy = -40.0, vx = 0, song 0x6F; step 1 integrates vy += 0x12000 and
 * vx += 0x5000 per tick, throws a gore drift (sub_08045CEC approximated
 * by AOS_EFFECT_BLOOD) on odd ticks and every eighth tick plays song
 * 0x6E with another gore; returns true when animation 8 ends. */
static bool da_death(AosEnemy *e, const AosEnemyKind *kind,
                     uint32_t (*random)(void), AosHitReport *report) {
    switch (e->step) {
    case 0:
        if (e->anim.id != 8) play_loop(e, kind->anims, 8, false);
        e->step = 1;
        e->vy = (int32_t)0xFFD80000;
        e->vx = 0;
        aos_audio_play_song(0x6F);
        break;
    case 1: {
        /* + 0x48 is the death spin (affine angle), not a walk speed —
         * mapped to e->angle for the renderer; + 0x4C accumulates the
         * 0x12000 gravity for the gore spawn height only (the native
         * state never rewrites + 0x40/+ 0x44). */
        e->timer++;
        e->vy += 0x12000;
        e->da_spin += 0x5000;
        e->angle = (uint32_t)e->da_spin;
        int32_t ny = e->y + e->vy;
        if (!(e->timer & 1)) {
            int32_t ox = (int32_t)((int16_t)aos_sine((uint32_t)(e->da_spin * 4)) << 4);
            int32_t rx = (int32_t)((int16_t)random() << 16);
            spawn_drift(AOS_EFFECT_BLOOD, e->x + ox + rx, ny, report);
        }
        if (!(e->timer & 7)) {
            aos_audio_play_song(0x6E);
            int32_t rx = (int32_t)((int16_t)random() << 16);
            int32_t ry = (int32_t)((int16_t)random() << 16);
            spawn_drift(AOS_EFFECT_BLOOD, e->x + rx, ny + ry, report);
        }
        if (e->anim.flags & AOS_ANIM_ENDED) return true;
        break;
    }
    default:
        break;
    }
    return false;
}

/* sub_080B2370: the hurt callback — state 0 wakes through sub_0806BC40
 * (face the player); a lethal hit (sub_0806E218's check, done by the
 * shared collide) enters state 2 with the steps cleared. */
static void da_hurt(AosEnemy *e, AosSoma *soma) {
    if (e->state == 0) da_face(e, soma);
    e->state = 2;
    e->step = e->substep = 0;
}

/* EnemyDiscArmorUpdate (0x080B22D4): the orbit offset recomputes
 * + 0x18/+ 0x1C each frame, the activity box of sub_0806CC20 gates
 * everything, states 0-2 dispatch then the shared tail (sub_080421AC
 * room query skipped, sub_0806DF20 = the animation step). */
AosHitReport aos_enemy_disc_armor_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    da_orbit_pos(e);
    {
        int sx = (int)(e->da_ox >> 16) - cam_x, sy = (int)(e->da_oy >> 16) - cam_y;
        if (outside_margins(kind, 4, sx, sy)) {
            aos_combat_tick(&e->combat);
            return report;
        }
    }
    bool dying = false;
    switch (e->state) {
    case 0:
        da_patrol(e, soma, layer, kind);
        break;
    case 1:
        da_attack(e, soma, kind, &spawned);
        break;
    case 2:
        dying = da_death(e, kind, random, &spawned);
        break;
    case 3:
        step_anim(e, kind->anims);
        break;
    default:
        break;
    }
    if (e->state != 3) step_anim(e, kind->anims);
    if (e->state != 2 && e->state <= 0xEF) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        report = hit;
        /* sub_080B2370: a lethal hit enters state 2. */
        if (hit.killed) {
            da_hurt(e, soma);
            dying = false;
        }
    }
    if (dying) {
        e->removed = true;
        return report;
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}

/* ---- the disc (sub_080B1678) ---- */

/* sub_080B15EC: refresh the glow child's orbit parameters — angle toward
 * the aim offsets, radius = distance to the armor's orbit position,
 * center = the armor's + 0x18/+ 0x1C. Returns the distance in
 * subpixels. */
static int32_t da_glow_params(AosEnemy *glow, const AosEnemy *disc,
                              const AosEnemy *armor) {
    if (!glow || !disc || !armor) return 0;
    int32_t tx = (int16_t)((armor->da_ox >> 16) + disc->da_tx) - (disc->da_ox >> 16);
    int32_t ty = (int16_t)((armor->da_oy >> 16) + disc->da_ty) - (disc->da_oy >> 16);
    glow->angle = aos_arctan2((int16_t)tx, (int16_t)ty);
    int32_t dx = (disc->x >> 16) - (armor->da_ox >> 16);
    int32_t dy = (disc->y >> 16) - (armor->da_oy >> 16);
    int32_t dist = (int32_t)aos_isqrt((uint32_t)(dx * dx + dy * dy)) << 16;
    glow->da_radius = dist;
    glow->da_ox = armor->da_ox;
    glow->da_oy = armor->da_oy;
    return dist;
}

/* The disc's position each frame: the armor's orbit draw position plus
 * the stored orbit vector (+ 0x48/+ 0x4C mapped to vx/vy). */
static void da_disc_place(AosEnemy *d, const AosEnemy *armor) {
    d->x = armor->da_ox + d->vx;
    d->y = armor->da_oy + d->vy;
}

/* sub_080B1428: the shard. State 0 picks animation + 0x14 (4 or 5) once;
 * state 1 moves with sub_0806D3D8 (speed 0x800000); the animation end
 * sets + 0x59 bit 8 and deletes. */
void aos_enemy_disc_shard_update(AosEnemy *s, const AosEnemyKind *kind,
                                 int cam_x, int cam_y) {
    (void)cam_x;
    (void)cam_y;
    if (s->state == 0) {
        unsigned anim = s->variant < 4 ? 4 : 5;
        if (s->anim.id != anim) play_loop(s, kind->anims, anim, false);
        s->state = 1;
    }
    /* sub_0806D3D8: integrate vx/vy with the 0x800000 limit and the
     * directional wall/floor probes — approximated by the shared
     * integrate plus blocked checks. */
    aos_enemy_integrate(s);
    step_anim(s, kind->anims);
    if (s->anim.flags & AOS_ANIM_ENDED) s->removed = true;
}

/* sub_080B14B4: the orbit glow child. Its + 0x14 angle advances with the
 * disc, + 0x20 radius and + 0x40/+ 0x44 center are refreshed by
 * da_glow_params; the affine orbit of sub_080B23C4/sub_0803E058 is
 * approximated by the renderer's angle spin. State 2 fades and deletes. */
void aos_enemy_disc_glow_update(AosEnemy *g, const AosEnemyKind *kind) {
    if (g->state == 2) {
        g->removed = true;
        return;
    }
    /* The glow registers itself on its disc parent (the native + 0x18
     * link), like the Mandragora children do. */
    if (g->parent && g->parent->role == AOS_ROLE_DISC_BLADE &&
        g->parent->da_glow != g)
        g->parent->da_glow = g;
    AosEnemy *disc = g->parent;
    if (disc && disc->parent) {
        da_glow_params(g, disc, disc->parent);
        /* The affine scale of sub_080B14B4: radius/3 in 16.16, floored
         * at 0 and capped at 0x60000 before the /3 — mapped to `scale`
         * for the renderer's vertical stretch approximation. */
        int32_t r = g->da_radius > 0x60000 ? 0x60000 : g->da_radius;
        g->scale = (r < 0 ? 0 : r / 3) + 0x400;
        g->x = g->da_ox;
        g->y = g->da_oy;
    }
    step_anim(g, kind->anims);
}

/* A shard spawn through EntityCreateInRange(0x49, 0xCF) with callback
 * sub_080B1428: animation id 4 or 5 in + 0x14, aim offsets + 0x42/+ 0x46
 * and the facing flipped from the disc's + 0x58 bit 0x40. */
static void da_throw_shard(AosEnemy *d, AosHitReport *report, int anim,
                           int16_t tx, int16_t ty) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy s = {0};
    s.id = d->id;
    s.role = AOS_ROLE_DISC_SHARD;
    s.x = d->x;
    s.y = d->y;
    s.stats = d->stats;
    s.hp = 1;
    s.mirrored = !d->mirrored;
    s.variant = (uint8_t)anim;
    s.da_tx = tx;
    s.da_ty = ty;
    s.vx = d->vx;
    s.vy = d->vy;
    s.static_frame = -1;
    s.combat.type = AOS_TYPE_ENEMY;
    report->children[report->child_count++] = s;
}

/* The disc's sine orbit helpers: sub_080009E4 lookups on the stored
 * angles, results kept in 16.16 subpixels like the native code. */
static int32_t da_sin(int32_t angle) {
    return (int32_t)((int64_t)aos_sine((uint32_t)angle) );
}

/* sub_080B1678 cases 1-3: the outward orbit. The disc's + 0x48/+ 0x4C
 * orbit vector comes from sine(+ 0x1C) and sine(+ 0x1C + 0x4000) scaled
 * by the speed-derived factor r4; position = armor orbit pos + vector.
 * Wall probes push the position back; a horizontal hit enters state 2,
 * a floor hit state 3. */
static void da_disc_fly(AosEnemy *d, const AosEnemy *armor,
                        const AosCollision *layer) {
    /* + 0x28 is the orbit angle: it advances by 0x200 + wobble and caps
     * at 0x4000; + 0x1C spins by + 0x20 each frame. */
    int32_t wobble = da_sin(d->da_momentum) * 3 << 5;
    int32_t r4 = wobble >= 0 ? (wobble >> 16) : -((-wobble) >> 16);
    d->da_momentum += 0x200;
    if (d->da_momentum > 0x4000) {
        d->da_momentum = 0x4000;
        d->da_spin += 0x4000;
        if (d->da_spin > 0x800000) d->da_spin = 0x800000;
        d->vy += d->da_spin;
    } else {
        d->vy = r4 * da_sin(d->da_heading);
    }
    d->da_heading += d->da_radius; /* + 0x1C += + 0x20 */
    d->vx = r4 * da_sin(d->da_heading + 0x4000);
    da_disc_place(d, armor);
    /* Directional probes approximated by wall pushes at the disc's
     * pixel position. */
    int px = (int)(d->x >> 16), py = (int)(d->y >> 16);
    int push = d->vx < 0 ? aos_wall_push_right(layer, px - 16, py)
                         : aos_wall_push_left(layer, px + 16, py);
    if (push) {
        d->x += (int32_t)((uint32_t)push << 16);
        d->da_momentum = 0;
        d->state = 2;
        return;
    }
    push = py + 16 < 0 ? 0 : aos_floor_depth(layer, px, py + 16, 0, false);
    if (push) {
        d->x += (push << 16);
        if (d->da_momentum <= 0x3FFF) {
            d->da_spin = (int32_t)0xFFFD0000;
            d->da_momentum = 0;
        }
        d->state = 3;
    }
}

/* sub_080B1678 case 4: the shard burst. The + 0xD timer counts; while
 * <= 3 a type-5 shard is thrown each tick; every 16th tick plays 0x73;
 * the probe walker's 0x8 result on an aligned tick throws a type-4
 * shard; when the disc comes within 0x2F of the armor's aim point (or
 * the armor's + 0x14 bit 1 is owed through the wall result & 3) it
 * computes the return heading and speed and enters state 5 with song
 * 0x103. */
static void da_disc_burst(AosEnemy *d, AosEnemy *armor,
                          const AosCollision *layer, AosHitReport *report) {
    if (d->timer <= 3) {
        da_throw_shard(d, report, 5, d->da_tx, d->da_ty);
    }
    if (!(d->timer & 0xF)) aos_audio_play_song(0x73);
    d->timer++;
    if (d->da_momentum <= 0x3FFFF) d->da_momentum += 0x1000;
    int result = probe_walk(d, layer, &da_disc_probes,
                            -d->da_momentum, 8);
    if (result & 3) {
        armor->da_flags |= 2;
        d->state = 5;
    }
    if ((result & 8) && !(d->timer & 3)) {
        da_throw_shard(d, report, 4, d->da_tx, d->da_ty);
    }
    /* The disc's + 0x42/+ 0x46 are its pixel offset from the armor's
     * orbit position — refreshed every frame like the native reads. */
    d->da_tx = (int16_t)((d->x >> 16) - (armor->da_ox >> 16));
    d->da_ty = (int16_t)((d->y >> 16) - (armor->da_oy >> 16));
    /* Within 0x2F of the armor's aim point: switch to the return. The
     * armor's + 0x42/+ 0x46 hold the same offset for the armor, so the
     * native difference collapses to the disc's own offset here. */
    int diff = d->da_tx >= 0 ? d->da_tx : -d->da_tx;
    int diff2 = d->da_ty >= 0 ? d->da_ty : -d->da_ty;
    if ((diff <= 0x2F && diff2 <= 0x2F) || (armor->da_flags & 2)) {
        armor->da_flags |= 2;
        d->state = 5;
        int32_t ax = (armor->da_ox >> 16) + d->da_tx;
        int32_t ay = (armor->da_oy >> 16) + d->da_ty;
        int32_t ddx = (int16_t)(d->x - ((int32_t)ax << 16)) >> 16;
        int32_t ddy = (int16_t)(d->y - ((int32_t)ay << 16)) >> 16;
        d->da_heading = aos_arctan2((int16_t)ddx, (int16_t)ddy);
        int32_t dist = (int32_t)aos_isqrt((uint32_t)(ddx * ddx + ddy * ddy)) << 16;
        d->da_speed = dist / 3;
        d->da_momentum = 0;
        d->timer = 0;
        aos_audio_play_song(0x103);
    }
}

/* sub_080B1678 case 5: the return flight. The speed decays by 0x10000
 * per tick (min 1), ticks 5-7 add a sine wobble, the position follows
 * the heading with sine offsets and the radius shrinks by 0x140. */
static void da_disc_return(AosEnemy *d, const AosEnemy *armor) {
    d->da_speed -= 0x10000;
    d->timer++;
    if (d->timer > 4 && d->timer - 4 <= 3)
        d->da_speed += da_sin((uint32_t)(d->timer - 4) << 12) * 8;
    if (d->da_speed <= 0x1FFFF) d->da_speed++;
    int32_t spd = d->da_speed >= 0 ? (d->da_speed >> 16)
                                  : -((-d->da_speed) >> 16);
    d->vy = spd * da_sin(d->da_heading);
    d->vx = spd * da_sin(d->da_heading + 0x4000);
    da_disc_place(d, armor);
    d->da_radius -= 0x140;
    int32_t r = d->da_radius;
    if (d->mirrored) r = -r;
    d->da_heading += r;
}

/* sub_080B1678 case 6: the settle. The glow child is released into
 * state 2, the margin check of sub_0806D128(4) fades out outside, the
 * probe walker 0x08528720 mode 2 bounces + 0x28, the angle + 0x14
 * advances by + 0x28 >> 6 and a stopped disc enters state 7. */
static void da_disc_settle(AosEnemy *d, const AosEnemy *armor,
                           const AosCollision *layer, const AosEnemyKind *kind,
                           int cam_x, int cam_y) {
    if (d->da_glow) {
        d->da_glow->state = 2;
        d->da_glow = NULL;
    }
    int sx = (int)(d->x >> 16) - cam_x, sy = (int)(d->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        d->state = 7;
        d->timer = 0;
        return;
    }
    int result = probe_walk(d, layer, &da_disc_probes, d->da_momentum, 2);
    if (result & 0x40) {
        if (d->da_momentum > 0) d->da_momentum -= 0x2000;
        else d->da_momentum += 0x2000;
    } else if (result & 0x20) {
        if (d->da_momentum <= 0) d->da_momentum -= 0x2000;
        else d->da_momentum += 0x2000;
    } else if (d->da_momentum >= 0) {
        d->da_momentum -= 0x100;
        if (d->da_momentum <= 0) {
            d->da_momentum = 0;
            d->state = 7;
            d->timer = 0;
        }
    } else {
        d->da_momentum += 0x100;
        if (d->da_momentum >= 0) {
            d->da_momentum = 0;
            d->state = 7;
            d->timer = 0;
        }
    }
    int32_t adv = d->da_momentum >= 0 ? d->da_momentum + 0x3F
                                     : d->da_momentum;
    d->angle = (uint32_t)((int32_t)d->angle + (adv >> 6));
    if (result & 3) d->da_momentum = -d->da_momentum / 2;
    da_disc_place(d, armor);
}

/* sub_080B1678 case 7: + 0x72 bit 1 and the + 0xF mark, then the + 0xD
 * countdown into the sub_0803E6BC fade and EntityDelete. */
static void da_disc_die(AosEnemy *d) {
    if (d->timer == 0) {
        d->removed = true;
        return;
    }
    d->timer--;
}

/* sub_080B1678: the disc update. States 0-5 first refresh the glow
 * child's orbit parameters through sub_080B15EC (the distance gates
 * the armor's + 0x14 bit 1 when beyond 96 px or when the armor is
 * dying); the jump-table cases then run and the shared tail stores
 * the frame delta into + 0x2C/+ 0x30 and applies the affine spin
 * (approximated by e->angle). */
AosHitReport aos_enemy_disc_blade_update(AosEnemy *d, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    (void)soma;
    (void)weapon;
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)random;
    AosHitReport report = {0}, spawned = {0};
    AosEnemy *armor = d->parent;
    if (!armor || armor->removed) {
        d->removed = true;
        return report;
    }
    /* The entry block: states 0-5 refresh the glow orbit through
     * sub_080B15EC; a disc beyond 96 px of the armor's orbit position
     * (or a dead armor) raises the armor's + 0x14 bit 1 — here the
     * da_flags bit 1 the attack state watches. */
    if (d->state <= 5 && d->da_glow) {
        int32_t dist = da_glow_params(d->da_glow, d, armor);
        if (dist > 0x600000) armor->da_flags |= 2;
    }
    /* The launch init of _080B16D2 runs as the disc enters its flight:
     * the launch speed is picked by facing (mirrored -1.0, else +4.0;
     * state 5 clears it), + 0x28 copies it, + 0x4C = + 0x30,
     * + 0x54 = 0x4000, + 0x50 = 0. */
    if (d->state == 0) {
        d->da_launch = d->mirrored ? -0x10000 : 0x40000;
        d->da_momentum = d->da_launch;
        d->vy = d->da_lvy;
        d->da_spin = 0x4000;
    }
    switch (d->state) {
    case 0:
        /* Resource load + glow child + song 0x74 + the two
         * sub_080458E4 effects — emitted as an update-side child since
         * creates cannot return children. */
        if (!d->boss_parts_spawned &&
            spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy g = {0};
            g.id = d->id;
            g.role = AOS_ROLE_DISC_GLOW;
            g.x = armor->da_ox;
            g.y = armor->da_oy;
            g.stats = d->stats;
            g.hp = 1;
            g.mirrored = d->mirrored;
            g.parent = d;
            g.static_frame = -1;
            g.da_radius = d->mirrored ? -0x80 : 0x80;
            g.combat.type = AOS_TYPE_ENEMY;
            g.combat.attack_off = true;
            g.combat.hurt_off = true;
            if (g.anim.id != 7) play_loop(&g, kind->anims, 7, false);
            step_anim(&g, kind->anims);
            spawned.children[spawned.child_count++] = g;
            d->boss_parts_spawned = true;
        }
        /* sub_080458E4(e, 0x60, 12, 2) and (0x60, 6, 2): two spark
         * bursts approximated by drift sparks. */
        spawn_drift(AOS_EFFECT_SPARK, d->x, d->y, &spawned);
        spawn_drift(AOS_EFFECT_SPARK, d->x, d->y, &spawned);
        aos_audio_play_song(0x74);
        /* Position = the armor's + 0x18/+ 0x1C, + 0x1C = 0x8000 and
         * + 0x20 = +/-0x80 by the facing (negated when + 0x58 & 0x40). */
        d->x = armor->da_ox;
        d->y = armor->da_oy;
        d->da_heading = 0x8000;
        d->da_radius = d->mirrored ? -0x80 : 0x80;
        d->state = 1;
        break;
    case 1:
        da_disc_fly(d, armor, layer);
        break;
    case 2:
    case 3:
        /* The falling/bounce states share the orbit placement with a
         * growing spin (sub_080B19D8/sub_080B1A4A): + 0x54 += 0x4000
         * capped 0x800000, + 0x4C += + 0x54, position = armor orbit +
         * vector, floor probe. */
        d->da_spin += 0x4000;
        if (d->da_spin > 0x800000) d->da_spin = 0x800000;
        d->vy += d->da_spin;
        da_disc_place(d, armor);
        {
            int px = (int)(d->x >> 16), py = (int)(d->y >> 16);
            int depth = aos_floor_depth(layer, px, py + 0x1000, 0, false);
            if (depth) {
                d->x += (depth << 16);
                if (d->state == 3) {
                    /* Case 3 (sub_080B1A4A): a type-5 shard every frame
                     * with song 0x104, then state 4 once the spin
                     * drops under 0x8000. */
                    da_throw_shard(d, &spawned, 5, d->da_tx, d->da_ty);
                    aos_audio_play_song(0x104);
                    if (d->da_spin <= 0x7FFF) {
                        d->state = 4;
                        d->vy = 0;
                    }
                } else {
                    d->da_momentum = 0;
                    d->state = 3;
                }
            }
        }
        break;
    case 4:
        da_disc_burst(d, armor, layer, &spawned);
        break;
    case 5:
        da_disc_return(d, armor);
        break;
    case 6:
        da_disc_settle(d, armor, layer, kind, cam_x, cam_y);
        break;
    case 7:
        da_disc_die(d);
        break;
    default:
        break;
    }
    /* States 0-5 advance the glow orbit angle (post block). */
    if (d->state <= 5) {
        int32_t adv = d->mirrored ? -0x2F00 : 0x2F00;
        d->angle = (uint32_t)((int32_t)d->angle + adv);
    }
    /* Frame delta into + 0x2C/+ 0x30 and the affine spin on e->angle. */
    /* The disc's own collision pass: contact only (sub_080B240C). */
    AosHitReport hit = {0};
    if (d->state <= 0xEF && d->state != 7) {
        hit = collide(d, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
        hit.soma_hit = false; /* the disc hurts through its own box */
    }
    report = hit;
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    (void)target_x;
    (void)target_y;
    (void)player_ahead_within;
    return report;
}
