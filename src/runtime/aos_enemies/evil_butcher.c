/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Evil Butcher (enemy 0x19), its cleaver, gore and particles.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Evil Butcher (EnemyEvilButcherCreate 0x080BFE1C / EnemyEvilButcherUpdate
 * 0x080BFF14 behind the sub_0806CC20 awake gate — approximated by the
 * usual activity/margin handling): state 0 sub_080BFB58 patrols with the
 * probe walker sub_0806CAF8 (table 0x085288FC, mode 0xC) at a speed that
 * follows the animation frame parity and tick, flips its facing after a
 * short obstacle timer and wakes into state 1 when the player enters the
 * 0xA0 x 0x78 box of sub_0806E29C at the start of an animation loop.
 * State 1 sub_080BFBF4 winds up (sub_0806BC40 facing, animation 1 once),
 * throws the cleaver child sub_080BF738 through sub_0806DFF8 at frame 2
 * tick 2 and returns to the patrol when the animation ends. State 2
 * sub_080BFCD8 is the death: song 0x1C1, a gore child sub_080BF990, a
 * still frame 6, then 0x40 frames of gore bursts — every fourth tick a
 * sub_08045CEC sparkle (animation 0xD, row 8) or a sub_080BF6B8 particle —
 * and EntityDelete at the end. State 3 only steps its animation. The
 * hurt callback sub_080BFECC enters state 2 at a lethal blow (the palette
 * bits of + 0x5A are not ported); the contact is the generic
 * sub_0806E1B8 (element 0, damage + 0x3C). The camera offsets of the
 * sub_080020A0 surface probes are normalized into room pixels, the
 * sub_080421AC box query, the + 0x58 fade bits, the + 0x59 flags, the
 * + 0x0F death flag and the sub_0806D4C0 pause are not ported. */
static const AosProbes *butcher_probes(const AosEnemyKind *kind) {
    return kind && kind->probes.count > 0 && kind->probes.count <= 8 ? &kind->probes : NULL;
}

static AosHitReport butcher_update(AosEnemy *e, AosSoma *soma,
                                   const AosCollision *layer, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    e->hidden = outside_margins(kind, 4, sx, sy);
    switch (e->state) {
    case 0:
        if (e->anim.frame == 0 && e->anim.tick == 0) {
            if (buer_player_in_window(e, soma, 0xA0, 0x78)) {
                e->state = 1;
                e->step = e->substep = 0;
            } else {
                uint8_t t = e->timer++;
                if (t > 2) {
                    e->timer = 0;
                    e->mirrored = !e->mirrored;
                }
            }
        }
        if (e->state == 0) {
            const AosProbes *p = butcher_probes(kind);
            int32_t speed = (e->anim.frame & 1) ? 0x3000
                                              : 0xA000 - ((int32_t)e->anim.tick << 0xB);
            int walk = p ? ground_walk(e, soma, layer, p, speed, 0xC) : 0;
            if ((walk & 0x1B) != 8 && e->timer <= 2) e->timer = 3;
        }
        break;
    case 1:
        switch (e->step) {
        case 0:
            face_player(e, soma);
            play_loop(e, kind->anims, 1, false);
            e->step = 1;
            break;
        case 1:
            if (e->anim.frame == 2 && e->anim.tick == 2 &&
                spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                /* sub_0806DFF8(e, sub_080BF738): the cleaver inherits the
                 * facing, the palette bank (+ 0x32) and the contact stats,
                 * placed on the pixel position 24 px up and 16 px to the
                 * facing side (the native + 0x42 / + 0x46 whole-pixel
                 * copies lose the fraction). */
                AosEnemy c = {0};
                c.id = e->id;
                c.role = AOS_ROLE_BUTCHER_CLEAVER;
                c.static_frame = -1;
                c.param1 = e->param1;
                c.stats = e->stats;
                c.x = (int32_t)(((int32_t)(e->x >> 16) + (e->mirrored ? 0x10 : -0x10)) << 16);
                c.y = (int32_t)(((e->y >> 16) - 0x18) << 16);
                c.mirrored = e->mirrored;
                spawned.children[spawned.child_count++] = c;
            }
            if (e->anim.flags & AOS_ANIM_ENDED) e->step = 3;
            break;
        case 3:
            e->state = e->step = e->substep = 0;
            play_loop(e, kind->anims, 0, true);
            break;
        default:
            break;
        }
        break;
    case 2:
        if (e->step == 0) {
            aos_audio_play_song(0x1C1);
            if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                /* EntityCreateInRange(0x49, 0xCF, sub_080BF990): the gore
                 * 18 px to the facing side (negated when mirrored) plus or
                 * minus 1 px, 32 px up, palette bank copied. */
                AosEnemy g = {0};
                g.id = e->id;
                g.role = AOS_ROLE_BUTCHER_GORE;
                g.static_frame = -1;
                g.param1 = e->param1;
                g.x = e->x + (e->mirrored ? -0x120000 : 0x120000) +
                     random_centred(random, 0x20000);
                g.y = e->y + random_centred(random, 0x20000) - 0x200000;
                spawned.children[spawned.child_count++] = g;
            }
            e->static_frame = 6;
            e->step = 1;
            e->timer = 0x40;
        } else {
            if ((e->timer & 3) == 0) {
                if (random() & 1) {
                    /* sub_08045CEC(x + r, y + r - 16 px, 0xD, 8): the
                     * centred draws are +/- 8 px in x and -32..0 px in y
                     * (2D5EC(0x100000) and 2D5EC(0x200000) - 0x100000). */
                    spawn_drift(AOS_EFFECT_DEATH_SPARKLE,
                                e->x + random_centred(random, 0x100000),
                                e->y + random_centred(random, 0x200000) - 0x100000,
                                &spawned);
                } else if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    /* EntityCreateInRange(0x49, 0xCF, sub_080BF6B8): a
                     * particle with the same scatter and a life of
                     * (r & 7) + 8 frames. */
                    AosEnemy p = {0};
                    p.id = e->id;
                    p.role = AOS_ROLE_BUTCHER_PARTICLE;
                    p.static_frame = -1;
                    p.param1 = e->param1;
                    p.x = e->x + random_centred(random, 0x100000);
                    p.y = e->y + random_centred(random, 0x200000) - 0x100000;
                    p.timer = (uint8_t)((random() & 7) + 8);
                    spawned.children[spawned.child_count++] = p;
                }
            }
            if (e->timer == 0) {
                e->removed = true;
            } else {
                if (e->timer == 0x10) e->helper_spawned = true;   /* + 0xF */
                e->timer--;
            }
        }
        break;
    case 3:
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    /* sub_0806E314 runs in states 0-2 (the defeated butcher keeps its
     * contact); sub_0806DF20 steps the animation everywhere but state 2. */
    report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (e->state != 2) step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* sub_080BF738: the thrown cleaver. State 0 (first update) shows still
 * frame 8 with the (-8, -4, 16, 8) boxes of sub_08042884 / sub_080428F0
 * (constant 0x0810FCF8), flies at +/- 0x28000 (2.5 px/frame) by the
 * facing, spins at -+0x1234, plays song 0x89, hp 1, contact element 1
 * (sub_0806E1E8) and the hurt callback sub_080BFF0C (any hit zeroes the
 * hit points — handled by the collide role list). State 1 accelerates at
 * 8.0, despawns beyond margin 3, dies into a 4 x 4 px flame of
 * sub_0806D5C0 when struck, and sticks on a solid cell 6 px ahead (the
 * sub_080020A0 probe): song 0x13A, hp 0, vx negated and halved plus a
 * random 0..0x7FFF, vy random - 0x18000, ay 0x2000, state 2. State 2
 * spins the affine angle (+ 0x18 += + 0x1A) while moving; state 3 and
 * the margin check delete it (the + 0x58 fade is cosmetic). */
static AosHitReport butcher_cleaver_update(AosEnemy *c, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def, int cam_x,
                                           int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&c->combat);
    switch (c->state) {
    case 0:
        c->static_frame = 8;
        c->vx = c->mirrored ? 0x28000 : (int32_t)0xFFFD8000;
        c->vy = 0;
        c->contact_elements = 1;
        c->own_boxes = true;
        c->own_hurt = c->own_attack = (AosBox){-8, -4, 16, 8};
        c->hp = 1;
        c->producer_timer = c->mirrored ? (uint16_t)-0x1234 : 0x1234;
        aos_audio_play_song(0x89);
        c->state = 1;
        break;
    case 1:
        accel_move(c, 0x80000);
        if (outside_margins(kind, 3, (c->x >> 16) - cam_x, (c->y >> 16) - cam_y))
            c->state = 3;
        if (c->hp == 0) {
            c->state = 3;
            spawn_flame_at(c->x, c->y, 0x40000, 0x40000, random, &report);
        } else if (aos_collision_cell(layer,
                                      (int16_t)((c->x >> 16) + (c->mirrored ? 6 : -6)),
                                      (int16_t)(c->y >> 16)) & 1) {
            aos_audio_play_song(0x13A);
            c->hp = 0;
            (void)random();              /* the + 0x58 fade draw */
            int32_t half = (c->vx + (c->vx < 0 ? 1 : 0)) >> 1;
            c->vx = -(half + (int32_t)(random() & 0x7FFF));
            c->vy = random_centred(random, 0x80000) - 0x18000;
            c->ay = 0x2000;
            c->state = 2;
        }
        break;
    case 2:
        accel_move(c, 0x80000);
        c->angle = (c->angle + c->producer_timer) & 0xFFFF;
        if (outside_margins(kind, 3, (c->x >> 16) - cam_x, (c->y >> 16) - cam_y))
            c->state = 3;
        break;
    case 3:
        c->removed = true;
        return report;
    default:
        break;
    }
    return report;
}

/* sub_080BF990: a gore chunk. State 0 (first update) keeps the butcher's
 * still frame 8, deletes itself on the sub_0803E654 sign draw (about
 * half), fades in + 0x58 bits (cosmetic) and falls with ay 0x2000 from
 * angle 0xC000. State 1 accelerates at 8.0 and sticks on a solid cell
 * 4 px below (the sub_080020A0 probe): song 0x13A, spin
 * (r & 0x7FF) + 0xC00, vx 0x8000 + (r & 0x3FFF) and vy random, both
 * negated together on a second draw, state 2. State 2 spins and moves;
 * state 3 deletes it. It never collides. */
static AosHitReport butcher_gore_update(AosEnemy *g, AosSoma *soma,
                                        const AosCollision *layer,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames,
                                        int soma_atk, int soma_def, int cam_x,
                                        int cam_y, uint32_t (*random)(void)) {
    (void)soma; (void)weapon; (void)weapon_frames; (void)soma_atk;
    (void)soma_def;
    AosHitReport report = {0};
    switch (g->state) {
    case 0:
        if ((int16_t)(random() & 0xFFFF) < 0) {
            g->removed = true;
            return report;
        }
        g->fade = (uint8_t)(random() & 0x1F);      /* + 0x58 low bits */
        g->static_frame = 8;
        g->vy = 0;
        g->ay = 0x2000;
        g->angle = 0xC000;
        g->state = 1;
        break;
    case 1:
        accel_move(g, 0x80000);
        if (outside_margins(kind, 3, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y))
            g->state = 3;
        if (aos_collision_cell(layer, (int16_t)(g->x >> 16),
                               (int16_t)((g->y >> 16) + 4)) & 1) {
            aos_audio_play_song(0x13A);
            g->producer_timer = (uint16_t)((random() & 0x7FF) + 0xC00);
            g->vx = 0x8000 + (int32_t)(random() & 0x3FFF);
            if (random() & 1) {
                g->producer_timer = (uint16_t)-g->producer_timer;
                g->vx = -g->vx;
            }
            g->vy = (int32_t)(random() & 0xFFFF) - 0xA8000;
            g->state = 2;
        }
        break;
    case 2:
        accel_move(g, 0x80000);
        g->angle = (g->angle + g->producer_timer) & 0xFFFF;
        if (outside_margins(kind, 3, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y))
            g->state = 3;
        break;
    case 3:
        g->removed = true;
        return report;
    default:
        break;
    }
    return report;
}

/* sub_080BF6B8: a death particle — still frame 9, then its + 0xD life
 * counts down (8..15 frames set by the spawner); gone when it wraps. */
static void butcher_particle_update(AosEnemy *p, const AosEnemyKind *kind) {
    (void)kind;
    if (p->state == 0) {
        p->static_frame = 9;
        p->state = 1;
    }
    if (p->timer == 0) p->removed = true;
    else p->timer--;
}

/* Dryad (EnemyDryadCreate 0x080AEAAC, EnemyDryadUpdate 0x080AEB8C): a
 * stationary spitter. State 0 sub_080AE4BC notices the player in the
 * (+ 0x42 - 0x6E, + 0x46 - 0x28) 0xDC x 0x50 box of sub_0806CFFC and on
 * RNG & 3 == 0 enters state 1 with animation 1 once. State 1
 * sub_080AE554 waits the animation end, then loops animation 2 and rolls
 * RNG & 7 == 0 while on screen (sub_08068AD4 margins; off screen it only
 * sleeps through + 0x59 bit 3 and a ledger clear — approximated by the
 * margin early-out) into animation 3 with timer 0x12; the last three
 * ticks play song 0x89 and call sub_080AE96C. State 2 waits the
 * animation end back to state 0 (no decoded writer enters it). State 3
 * sub_080AE650 is the death: timer 0x40, song 0x6E and a sub_0806D5C0
 * burst every eighth tick, the timer to zero deletes it; its affine
 * angle is + 0xD << 0xA while + 0x59 & 0x10 lasts (one turn). State 4
 * only animates. The hurt sub_080AE3FC deals the generic damage, +
 * 0x2D = 8, and on a lethal blow plays animation 5 (state 1, step != 0)
 * or 4, then state 3, timer 0x28, steps cleared. */
