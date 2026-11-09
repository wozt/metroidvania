/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Killer Mantle (enemy 0x55), its ghost and orbit.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_0806E350(c, parent, dx, dy): the child's whole-pixel position
 * becomes the parent's plus the offset — dx negated by the parent's
 * facing — keeping the child's own fraction. */
static void mantle_follow(AosEnemy *c, const AosEnemy *p, int dx, int dy) {
    int ex = (int)(p->x >> 16) + (p->mirrored ? -dx : dx);
    int ey = (int)(p->y >> 16) + dy;
    c->x = (c->x & 0xFFFF) | ((int32_t)ex << 16);
    c->y = (c->y & 0xFFFF) | ((int32_t)ey << 16);
}

/* sub_0806BBC4: the direction of the target from the entity (+1 to its
 * right, -1 to its left). The decoy-aware target_x approximates the
 * native target record; the target-facing variant sub_0806E3B0 reads
 * Soma's + 0x58 bit 0x40 (facing_left). */
static int mantle_player_dir(const AosEnemy *e, const AosSoma *soma) {
    return target_x(soma) > e->x ? 1 : -1;
}

/* sub_080BB2D4: the rocking patrol of state 1. Step 0 plays animation 1
 * once and clears the motion fields; step 1 waits for its end, plays
 * animation 2 and seeds + 0x14 = (random & 0xFF) + 0x100. Step 2 rocks:
 * + 0x18 counts up, + 0x54 = sine(+ 0x18 << 0xB) / 0x30 and the walk
 * speed is + 0x14 times sine(+ 0x18 * 0x300) >> 8, negated by + 0xC.
 * While Soma attacks (+ 0x10 & 0x360) within 0x4F pixels and both the
 * effective facing and the target facing point at him, + 0x18 = 0x30 and
 * + 0xD = 1 (the dash). The probe walker 0x085288B4 (mode 0) moves it;
 * past + 0x18 = 0x2F the 0xA0 x 0x46 box of sub_0806E29C starts the
 * attack (state 2) unless + 0xD is set, otherwise the walk turns around:
 * + 0xC flips, animation 2 or 7, a fresh speed with a bonus toward the
 * player and another one while + 0xD holds, + 0x18 = 0. */
static void mantle_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        play_loop(e, anims, 1, false);
        e->vy = e->ay = 0;
        e->substep = 0;
        e->spin = 0;
        e->step = 1;
        break;
    case 1:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        play_loop(e, anims, 2, false);
        e->step = 2;
        e->mantle_speed = (int32_t)(random() & 0xFF) + 0x100;
        break;
    case 2: {
        e->spin = (int16_t)(e->spin + 1) & 0xFFFF;
        int16_t phase = (int16_t)e->spin;
        e->ay = aos_sine((uint32_t)(phase << 0xB)) / 0x30;
        int32_t v = aos_sine((uint32_t)((int32_t)phase * 3 << 8));
        v = (v + (v < 0 ? 0xFF : 0)) >> 8;
        int32_t speed = e->mantle_speed * v;
        if (e->substep) speed = -speed;
        e->timer = 0;
        if (soma->flags & 0x360) {
            int32_t dx = (int16_t)(target_x(soma) >> 16) - (int16_t)(e->x >> 16);
            if (dx >= -0x4F && dx <= 0x4F) {
                int dir_eff = e->substep ^ (e->mirrored ? 1 : 0);
                int pdir = mantle_player_dir(e, soma);
                int tface = soma->facing_left ? 1 : 0;
                bool facing_ok = dir_eff == 0 ? pdir == -1 : pdir == 1;
                bool target_ok = tface == 0 ? pdir == -1 : pdir == 1;
                if (facing_ok && target_ok) {
                    e->spin = 0x30;
                    e->timer = 1;
                }
            }
        }
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, speed, 0);
        if ((int16_t)e->spin > 0x2F) {
            if (player_in_centred(e, soma, 0xA0, 0x46) && e->timer == 0) {
                e->state = 2;
                e->substep = e->step = 0;
            } else {
                e->substep ^= 1;
                play_loop(e, anims, e->substep ? 7 : 2, false);
                int dir_eff = e->substep ^ (e->mirrored ? 1 : 0);
                e->mantle_speed = (int32_t)(random() & 0xFF) + 0x100;
                int pdir = mantle_player_dir(e, soma);
                if ((dir_eff == 0) == (pdir == -1))
                    e->mantle_speed += 0x100 + (int32_t)(random() & 0x3F);
                if (e->timer) e->mantle_speed += 0x100;
                e->spin = 0;
            }
        }
        break;
    }
    default:
        break;
    }
}

/* sub_080BB518: the dash attack of state 2. Step 0 faces the player,
 * plays animation 3 once and sets + 0x1A bit 0 (the orbit blade's go).
 * Step 1 waits for its end, then plays animation 4, hops with
 * + 0x4C = sub_0802D5EC(0x2000) - 0x1A000 under + 0x54 = 0x1000,
 * seeds + 0x14 = 0x48000 + ((random & 0x1F) << 8), clears + 0x72 bit 1
 * (the contact is live for the dash) and plays song 0x89. Steps 2 and
 * 3 brake + 0x14 by 0x2000 then 0x1000 a frame, walk with the same
 * probes, and at animation 4's end leave the afterimage child
 * sub_080BB054, set + 0x72 bit 1 again, clear + 0x1A bit 1 and play
 * animation 5; at its end back to state 1 step 0. */
static void mantle_attack(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void),
                          AosHitReport *spawned) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        face_player(e, soma);
        play_loop(e, anims, 3, false);
        e->producer_timer |= 1;
        e->step = 1;
        break;
    case 1:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        e->step = 2;
        play_loop(e, anims, 4, false);
        e->vy = random_centred(random, 0x2000) + (int32_t)0xFFFE6000;
        e->ay = 0x1000;
        e->mantle_speed = 0x48000 + ((int32_t)(random() & 0x1F) << 8);
        e->combat.attack_off = false;
        aos_audio_play_song(0x89);
        break;
    case 2:
        if (e->mantle_speed > 0) e->mantle_speed -= 0x2000;
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, e->mantle_speed, 0);
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_0806DFF8(e, sub_080BB054): the afterimage follows the
             * parent from its own update. */
            AosEnemy g = {0};
            g.id = e->id;
            g.role = AOS_ROLE_MANTLE_GHOST;
            g.static_frame = -1;
            g.x = e->x;
            g.y = e->y;
            spawned->children[spawned->child_count++] = g;
        }
        e->combat.attack_off = true;
        e->producer_timer &= ~2;
        e->step = 3;
        play_loop(e, anims, 5, false);
        break;
    case 3:
        if (e->mantle_speed > 0) e->mantle_speed -= 0x1000;
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, e->mantle_speed, 0);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080BB664: the death of state 3. Step 0 plays animation 6 once and
 * song 0x6D; step 1 counts + 0xD and every fourth tick emits the gore
 * drift of sub_08045CEC(x, y, 0x16, 8): a unit direction at a random
 * angle, acceleration vx / 0x18 and vy / 0x18, vflip set. Returns true
 * when the animation ends (the entity is deleted). */
static bool mantle_death(AosEnemy *e, const AosEnemyKind *kind,
                         uint32_t (*random)(void), AosHitReport *spawned) {
    if (e->step == 0) {
        play_loop(e, kind->anims, 6, false);
        e->step = 1;
        aos_audio_play_song(0x6D);
    }
    e->timer++;
    if ((e->timer & 3) == 0) {
        int32_t vx, vy;
        random_direction(random, &vx, &vy);
        AosEffect *g = spawn_drift(AOS_EFFECT_HIT_ELEMENT_5, e->x, e->y, spawned);
        if (g) {
            g->vx = vx;
            g->vy = vy;
            g->ax = vx / 0x18;
            g->ay = vy / 0x18;
            g->vflip = true;
        }
    }
    return (e->anim.flags & AOS_ANIM_ENDED) != 0;
}

/* sub_080BB054: the afterimage of the dash end. State 0 plays animation
 * 8 once (+ 0x5C = 0x14 and the + 0x5A merge are not ported); state 1
 * follows the parent 12 px to its back (negated by the facing) and 4 px
 * down, deleting when the animation ends. */
static void mantle_ghost_update(AosEnemy *g, const AosEnemy *parent,
                                const AosAnimSet *anims) {
    if (g->state == 0) {
        play_loop(g, anims, 8, false);
        g->state = 1;
    }
    if (!parent) return;
    mantle_follow(g, parent, -0xC, 4);
    step_anim(g, anims);
    if (g->anim.flags & AOS_ANIM_ENDED) g->removed = true;
}

/* sub_080BB0F0: the orbiting blade linked to its parent (+ 0x14). The
 * parent's state 3 ends it in state 4 (the + 0x4BE pause and the + 0x2D
 * hit flash freeze states 1-3 — only the flash is modelled). State 0
 * draws sub_0803E654 and deletes on a negative result (~half); else it
 * keeps still frame 0x12, hidden. States 0 and 1 fall through into the
 * next like the native jump table: state 1 waits for the parent's
 * + 0x1A bit 0 (set by the first attack), resets + 0x1C and + 0x1A =
 * 0xD000 and unhides; state 2 grows + 0x1C by 0x1000 a frame while the
 * bit holds, spins + 0x1A by 0x300 and orbits 8 px to the facing side —
 * on a cleared bit it counts + 0xD down from 8 into state 3, which
 * shrinks by 0x2000, spins by 0x200 and returns to state 1 hidden.
 * States 1-3 end with the affine sub_0803E058 (angle = + 0x1A negated
 * by the child's facing, scale = + 0x1C): mapped to `angle` and
 * `walk_timer` for the renderer; the scale itself is not drawn. */
static void mantle_orbit_update(AosEnemy *o, const AosEnemy *parent,
                                uint32_t (*random)(void)) {
    if (!parent) {
        o->removed = true;
        return;
    }
    if (parent->state == 3) o->state = 4;
    if (o->state != 0 && parent->hit_flash) return;
    if (o->state == 0) {
        if (random() & 0x8000) {
            o->removed = true;
            return;
        }
        /* The low 5 bits of + 0x58 (the fade) and the + 0x59 / + 0x5C
         * bookkeeping are not ported. */
        o->static_frame = 0x12;
        o->hidden = true;
        o->state = 1;
    }
    if (o->state == 1) {
        if (!(parent->producer_timer & 1)) return;
        o->walk_timer = 0;
        o->producer_timer = 0xD000;
        o->hidden = false;
        o->state = 2;
    }
    if (o->state == 2) {
        if (parent->producer_timer & 1) {
            if (o->walk_timer <= 0xFFFF) o->walk_timer += 0x1000;
            o->producer_timer = (uint16_t)(o->producer_timer + 0x300);
            mantle_follow(o, parent, 8, 0);
        } else {
            o->timer = 8;
            o->state = 3;
        }
    } else if (o->state == 3) {
        if (o->walk_timer > 0) o->walk_timer -= 0x2000;
        o->producer_timer = (uint16_t)(o->producer_timer + 0x200);
        mantle_follow(o, parent, 8, 0);
        if (o->timer) {
            o->timer--;
        } else {
            o->state = 1;
            o->hidden = true;
        }
    } else if (o->state == 4) {
        /* sub_0803E6BC fade then EntityDelete, same call. */
        o->removed = true;
        return;
    }
    /* The affine tail of states 1-3 (and the > 4 default): the angle is
     * + 0x1A sign-extended, negated by the child's + 0x58 bit 0x40. */
    uint16_t a = (uint16_t)(int16_t)o->producer_timer;
    o->angle = o->mirrored ? (uint32_t)(uint16_t)(-a) : a;
}

/* EnemyKillerMantleUpdate (sub_080BB824): inside the activity window
 * of sub_0806CC20, state 0 waits for the player in the 0xDC x 0xA0 box
 * of sub_0806E29C, state 1 patrols, state 2 attacks, state 3 dies
 * (deleted when sub_080BB664 ends) and state 4 only steps the
 * animation. States 0-3 then run the collision pass sub_0806E314 and
 * the animation step sub_0803F17C; state 4 skips both. */
static AosHitReport killer_mantle_update(AosEnemy *e, AosSoma *soma,
                                         const AosCollision *layer,
                                         const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames,
                                         int soma_atk, int soma_def, int cam_x,
                                         int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    e->hidden = outside_margins(kind, 4, sx, sy);
    /* + 0x2D counts down in the shared update path (sub_0806E314),
     * including the defeated mantle — the orbit child reads it. */
    if (e->hit_flash) e->hit_flash--;
    /* sub_0806DFF8(e, sub_080BB0F0) in the create: the orbit child is
     * emitted once, here, since a create cannot return children. */
    if (!e->boss_parts_spawned && spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy o = {0};
        o.id = e->id;
        o.role = AOS_ROLE_MANTLE_ORBIT;
        o.static_frame = -1;
        o.x = e->x;
        o.y = e->y;
        spawned.children[spawned.child_count++] = o;
        e->boss_parts_spawned = true;
    }
    switch (e->state) {
    case 0:
        if (player_in_centred(e, soma, 0xDC, 0xA0)) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 1:
        mantle_patrol(e, soma, layer, kind, random);
        break;
    case 2:
        mantle_attack(e, soma, layer, kind, random, &spawned);
        break;
    case 3:
        if (mantle_death(e, kind, random, &spawned)) e->removed = true;
        break;
    case 4:
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
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}
