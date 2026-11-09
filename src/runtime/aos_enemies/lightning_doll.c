/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Lightning Doll (enemy 0x3B) and its burning pieces.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_080C9A24: five pieces of sub_080C9284 (+ 0x1D the index), set up on
 * their first update. */
static void doll_shatter(AosEnemy *e, AosHitReport *report) {
    for (int i = 0; i < 5 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy p = {0};
        p.id = e->id;
        p.role = AOS_ROLE_DOLL_PIECE;
        p.variant = (uint8_t)i;
        p.static_frame = -1;
        p.x = e->x;
        p.y = e->y;
        p.mirrored = e->mirrored;
        p.hidden = true;
        p.combat.attack_off = p.combat.hurt_off = true;
        report->children[report->child_count++] = p;
        e->spawned_count++;
    }
}

/* sub_080C9284: a piece's first update shows its sprite frame at the
 * doll's position with vx, vy = -vy and + 0x54 0x1800 (vx pointing left
 * with the player to the doll's right) and lights its glow (sub_080C9058:
 * the death flame's animation looping 16 pixels above it plus the y
 * offset, flipped at random). Then it flies: at timer 0x30 a flame of
 * sub_0806D5C0 and a dust particle of sub_0806DA6C / sub_0806D7F0
 * (animation 4, row 6, vy -0.25, + 0x18 = 4) at its position (both with
 * a 0 x 0 area: two RandomNumberGenerator draws each, % 0 being 0), x +=
 * vx, y += vy, vy += 0x1800; piece 1 alone bounces on floors (y - 15,
 * vy = -vy / 2, stopped at 0x5FFF or less; sound 0x153), meets ceilings
 * (y - 32) and walls (x +/- 8, y - 16, vx = 0). It blinks on odd timers
 * up to 0x3F and stops, hidden, when its timer runs out or it falls below
 * screen row 0xC8; it is deleted once its glow has played a cycle. */
static void doll_piece_update(AosEnemy *p, const AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, int cam_y, uint32_t (*random)(void),
                              AosHitReport *report) {
    const int8_t *r = p->id == AOS_ENEMY_KILLER_DOLL ? killer_doll_pieces[p->variant]
                                                      : lightning_doll_pieces[p->variant];
    switch (p->state) {
    case 0:
        p->static_frame = (uint8_t)r[2];
        p->hidden = false;
        p->vx = r[3] * 0x1000;
        p->vy = -(r[4] * 0x1000);
        p->ay = 0x1800;
        p->timer = (uint8_t)r[5];
        if (p->x < target_x(soma)) p->vx = -abs(p->vx);
        p->glow = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = p->x, .y = p->y - 0x100000};
        p->glow.mirrored = random() & 1;
        if (kind->flame_anims && aos_anim_start(&p->glow.anim, kind->flame_anims, 0, true))
            p->glow.started = true;
        p->state = 1;
        break;
    case 1: {
        if ((p->timer & 0xF) == 0 && p->timer <= 0x3F && p->timer > 0x20) {
            spawn_flame_at(p->x + r[0], p->y + r[1], 0, 0, random, report);
            (void)random();
            (void)random();
            if (report->effect_count < AOS_ENEMY_MAX_EFFECTS)
                report->effects[report->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_DUST, .x = p->x + r[0], .y = p->y + r[1],
                    .particle = true, .flags = 4, .vy = -0x4000};
        }
        p->x += p->vx;
        p->y += p->vy;
        p->vy += p->ay;
        if (p->variant == 1) {
            int32_t x = p->x >> 16, y = p->y >> 16;
            if (p->vy >= 0) {
                int depth = aos_floor_depth(layer, x, y - 15, 0, false);
                if (depth) {
                    aos_audio_play_song(0x153);
                    p->y += (int32_t)((uint32_t)depth << 16);
                    p->vy = -p->vy / 2;
                    if (abs(p->vy) <= 0x5FFF) p->vx = p->vy = p->ax = p->ay = 0;
                }
            } else {
                int depth = aos_ceiling_depth(layer, x, y - 32, 0, false);
                if (depth) p->y += (int32_t)((uint32_t)depth << 16);
            }
            y = p->y >> 16;
            int push = 0;
            if (p->vx > 0) push = aos_wall_push_left(layer, x + 8, y - 16);
            else if (p->vx < 0) push = aos_wall_push_right(layer, x - 8, y - 16);
            if (push) {
                p->x += (int32_t)((uint32_t)push << 16);
                p->vx = 0;
            }
        }
        if ((p->timer & 1) && p->timer <= 0x3F) p->hidden = !p->hidden;
        bool done = --p->timer == 0;
        if ((int16_t)((p->y >> 16) - cam_y) > 0xC8) done = true;
        if (done) {
            p->vx = p->vy = 0;
            p->state = 2;
            p->hidden = true;
        }
        break;
    }
    case 2:
        if (p->glow.cycle_done) {
            p->removed = true;
            return;
        }
        break;
    default:
        break;
    }
    /* The glow, updated after its piece. */
    if (p->glow.started) {
        p->glow.x = p->x;
        p->glow.y = (int32_t)((uint32_t)((p->y >> 16) - 16 + r[1]) << 16) | (p->y & 0xFFFF);
        bool ended = p->glow.anim.flags & AOS_ANIM_ENDED;
        int result = aos_anim_step(&p->glow.anim, kind->flame_anims);
        if (result == 4 || (result == 3 && !ended)) p->glow.cycle_done = true;
    }
}

/* sub_080C8B3C step 2: a new drift (r % 6): 0 to 2 at 0.5 diagonally
 * toward the player, 3 turned over at 0.25 away from him, both for (r &
 * 0xFF) + 0x80 frames; 4 and 5 at 1.0 straight at his position 16 pixels
 * higher (sub_0806BAC8) until it reaches his x. */
static void doll_new_drift(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void)) {
    face_player(d, soma);
    d->vx = d->vy = d->ax = d->ay = 0;
    unsigned pattern = random() % 6;
    int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
    if (pattern <= 2) {
        d->vx = d->vy = 0x8000;
        if (dx > 0) d->vx = -d->vx;
        if (player_dy(d, soma) > 0) d->vy = -d->vy;
        d->leg = (int16_t)((random() & 0xFF) + 0x80);
    } else if (pattern == 3) {
        d->mirrored = !d->mirrored;
        d->vx = 0x4000;
        if (dx < 0) d->vx = -d->vx;
        d->leg = (int16_t)((random() & 0xFF) + 0x80);
    } else {
        int16_t ax = (int16_t)((target_x(soma) >> 16) - (d->x >> 16));
        int16_t ay = (int16_t)((target_y(soma) >> 16) - (d->y >> 16) - 0x10);
        uint16_t angle = aos_arctan2(ax, ay);
        d->vx = aos_fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
        d->vy = aos_fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
        int32_t frames = d->vx ? (target_x(soma) - d->x) / d->vx : 0;
        d->leg = (int16_t)(frames < 0 ? -frames : frames);
    }
}

/* sub_080C9788: beyond the room's columns (30 for one screen) it turns
 * back (vx negated and applied twice); otherwise the wall ahead (x +/- 8,
 * y - 16) pushes it back and turns it. Beyond its rows (26 for one screen)
 * vy turns back the same way; otherwise the floor (y + 1) or the ceiling
 * (y - 32) pushes it back and turns vy. */
static void doll_bounds(AosEnemy *d, const AosCollision *layer) {
    int32_t x = d->x >> 16, y = d->y >> 16;
    int32_t column = x < 0 ? (x + 7) >> 3 : x >> 3;
    if (column < 0 || column >= room_columns(layer)) {
        d->vx = -d->vx;
        d->x += d->vx * 2;
        d->mirrored = !d->mirrored;
    } else {
        int push = d->vx >= 0 ? aos_wall_push_left(layer, x + 8, y - 16)
                              : aos_wall_push_right(layer, x - 8, y - 16);
        if (push) {
            d->x += (int32_t)((uint32_t)push << 16);
            d->vx = -d->vx;
            d->mirrored = !d->mirrored;
        }
    }
    x = d->x >> 16;
    int32_t row = y < 0 ? (y + 7) >> 3 : y >> 3;
    int rows = layer->height_cells == 32 ? 0x1A : layer->height_cells;
    if (row < 0 || row >= rows) {
        d->vy = -d->vy;
        d->y += d->vy * 2;
        return;
    }
    int depth = d->vy >= 0 ? aos_floor_depth(layer, x, y + 1, 0, false)
                           : aos_ceiling_depth(layer, x, y - 32, 0, false);
    if (depth) {
        d->y += (int32_t)((uint32_t)depth << 16);
        d->vy = -d->vy;
    }
}

/* sub_080C8B3C: + 0x14 (attack wait) and + 0x18 (flight) count down.
 * Step 0 rests (animation 0) until the player is in its 0xF0 x 0x80 box,
 * then rises (animation 1 once, step 1) and flies (step 2, + 0x5C 3):
 * drifts of doll_new_drift, facing its vx, bounded by doll_bounds. Step 3
 * charges (animation 3 once, sound 0x75) then sparks (animation 4 looping)
 * for (r & 0xFF) + 0x80 frames, then flies again with + 0x14 = (r & 0x1F)
 * + 0x10 and + 0x18 += 0x20. Step 4 (+ 0x5C 9) sinks a pixel a frame to a
 * cell, plays animation 5 for (r & 0x3F) + 0x40 frames (drawn twice) and
 * rises again with + 0x18 = (r & 0x3FF) + 0x200. Each frame it moves by
 * its speed and adds its accelerations; flying, it lands once + 0x18 runs
 * out, and with + 0x14 out and the player ahead it attacks within 20
 * pixels (sub_0806BF78), else one frame in 1024. */
static void doll_act(AosEnemy *d, const AosSoma *soma, const AosCollision *layer,
                     const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (d->attack_wait) d->attack_wait--;
    if (d->flight) d->flight--;
    switch (d->step) {
    case 0:
        if (d->anim.id != 0) play(d, anims, 0);
        if (player_in_centred(d, soma, 0xF0, 0x80) && (target_x(soma) != d->x || target_y(soma) != d->y)) {
            if (d->anim.id != 1) play_loop(d, anims, 1, false);
            d->step = 1;
            d->substep = 0;
        }
        break;
    case 1:
        if (d->cycle_done) {
            d->timer = 0;
            d->step = 2;
            d->substep = 0;
        }
        break;
    case 2:
        if (d->substep == 0) {
            doll_new_drift(d, soma, random);
            if (d->anim.id != 2) play(d, anims, 2);
            d->substep = 1;
            break;
        }
        if (!d->mirrored && d->vx > 0) d->mirrored = true;
        if (d->mirrored && d->vx < 0) d->mirrored = false;
        if (d->leg-- <= 0) d->substep = 0;
        doll_bounds(d, layer);
        break;
    case 3:
        switch (d->substep) {
        case 0:
            if (d->anim.id != 3) play_loop(d, anims, 3, false);
            d->vx = d->vy = d->ax = d->ay = 0;
            d->timer = (uint8_t)((random() & 0xFF) + 0x80);
            d->substep = 1;
            aos_audio_hold(0x75);
            /* fall through */
        case 1:
            if (!d->cycle_done) break;
            if (d->anim.id != 4) play(d, anims, 4);
            d->substep = 2;
            break;
        case 2:
            /* The spark's loop is held again every 16 frames above 0x20. */
            if ((d->timer & 0xF) == 0 && d->timer > 0x20) aos_audio_hold(0x75);
            if (d->timer-- == 0) d->substep = 3;
            break;
        case 3:
            aos_audio_release(0x75);
            d->step = 2;
            d->substep = 0;
            d->attack_wait = (uint16_t)((random() & 0x1F) + 0x10);
            d->flight = (uint16_t)(d->flight + 0x20);
            break;
        default:
            break;
        }
        break;
    case 4:
        if (d->substep == 2) {
            if (--d->timer == 0xFF) {
                d->flight = (uint16_t)((random() & 0x3FF) + 0x200);
                if (d->anim.id != 1) play_loop(d, anims, 1, false);
                d->step = 1;
                d->substep = 0;
            }
            break;
        }
        if (d->substep == 0) d->vx = d->vy = d->ax = d->ay = 0;
        if (!aos_collision_cell(layer, d->x >> 16, d->y >> 16)) {
            d->y += 0x10000;
            break;
        }
        if (d->anim.id != 5) play_loop(d, anims, 5, false);
        d->timer = (uint8_t)((random() & 0x3F) + 0x40);
        d->substep++;
        break;
    default:
        break;
    }
    d->x += d->vx;
    d->y += d->vy;
    d->vx += d->ax;
    d->vy += d->ay;
    if (d->step != 2) return;
    if (d->flight == 0) {
        d->step = 4;
        d->substep = 0;
    }
    if (d->attack_wait) return;
    int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
    if (d->mirrored ? dx >= 0 : dx <= 0) return;
    if (player_within(d, soma, 0x140000) || (random() & 0x3FF) == 0) {
        d->step = 3;
        d->substep = 0;
    }
}

/* sub_080C9630: under the global 0x8E & 0x40 it cycles its animations: 0
 * to its end (or 17 frames), 1 to its end, 2 for 65 frames, 3 to its end,
 * 4 for 129 frames, 5 for 49 frames. */
static void doll_cycle(AosEnemy *d, const AosAnimSet *anims) {
    static const uint8_t loops[6] = {0, 0, 1, 0, 1, 0};
    static const uint8_t limits[6] = {0x10, 0xFF, 0x40, 0xFF, 0x80, 0x30};
    uint8_t s = d->step;
    if (s > 5) return;
    if (d->anim.id != s) play_loop(d, anims, s, loops[s]);
    bool next;
    if (s == 1 || s == 3) next = d->cycle_done;
    else if (s == 0) next = d->cycle_done || d->timer++ > limits[0];
    else next = d->timer++ > limits[s];
    if (next) {
        d->timer = 0;
        d->step = s == 5 ? 0 : (uint8_t)(s + 1);
    }
    step_anim(d, anims);
}

/* EnemyLightningDollUpdate: state 2 cycles its animations; otherwise, alive
 * and flashing from a blow (sub_0806AD24, the sound 0x75 off) it only
 * collides; state 1 acts, 0xFF waits for its pieces. Then the animation
 * step: on a new sprite frame animation 4 sets the attack box (-14, -32,
 * 30, 28) of sub_08042884 (the hurtbox kept), any other the frame's boxes;
 * then the collision pass (sub_080421AC: no on-screen test). Its contact
 * callback sub_080C99E4, during animations 3 and 4, holds a spark (common
 * animation 16, row 6: sub_080C9108 / sub_080C9198) on the player. The
 * tracking list of sub_08021924 and the sound loop 0x75 are not ported. */
static AosHitReport doll_update(AosEnemy *d, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (d->state == 2) {
        doll_cycle(d, kind->anims);
        return report;
    }
    if (d->hp > 0 && d->hit_flash && d->state != 0xFF) {
        aos_audio_release(0x75);
        d->hit_flash--;
        report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&d->combat);
        return report;
    }
    if (d->state == 1) doll_act(d, soma, layer, kind->anims, random);
    if (d->state == 0xFF) {
        if (!d->spawned_count) d->removed = true;
        return report;
    }
    AosBox hurt, attack;
    bool hurt_on, attack_on;
    frame_boxes(d, kind, &hurt, &attack, &hurt_on, &attack_on);
    uint8_t before = d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES
                         ? kind->sprites[d->anim.id][d->anim.frame] : 0;
    step_anim(d, kind->anims);
    uint8_t after = d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES
                        ? kind->sprites[d->anim.id][d->anim.frame] : 0;
    if (before != after) {
        if (d->anim.id == 4) {
            d->own_boxes = true;
            d->own_hurt = hurt;
            d->own_attack = (AosBox){-14, -32, 30, 28};
        } else {
            d->own_boxes = false;
        }
    }
    report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.soma_hit && (d->anim.id == 3 || d->anim.id == 4) &&
        report.effect_count < AOS_ENEMY_MAX_EFFECTS) {
        AosEffect spark = {.kind = AOS_EFFECT_SPARK, .follow = true, .x = soma->x,
                           .y = soma->y - 0x100000};
        spark.mirrored = random() & 1;
        report.effects[report.effect_count++] = spark;
    }
    aos_combat_tick(&d->combat);
    return report;
}
