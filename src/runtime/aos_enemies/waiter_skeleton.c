/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Waiter Skeleton (enemy 0x1E), its companion and tray.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_0806DFF8 + the AF194 spawn blocks: a tray child (+ 0x14 = the
 * animation index 0x11 or 9) sharing the body's + 0x10 palette byte,
 * + 0x3C contact and + 0x34 HP; no contact until it is thrown. */
static AosEnemy waiter_tray(const AosEnemy *parent, int16_t anim) {
    AosEnemy t = {0};
    t.id = parent->id;
    t.role = AOS_ROLE_WAITER_TRAY;
    t.x = parent->x;
    t.y = parent->y;
    t.mirrored = parent->mirrored;
    t.stats = parent->stats;
    t.hp = parent->hp;
    t.ws_anim = anim;
    t.static_frame = -1;
    t.attack_off = true;
    t.parent = (AosEnemy *)parent;
    t.combat.type = AOS_TYPE_ENEMY;
    return t;
}

/* The constructor's sub_080AEC0C companion: + 0x18 the parent, the same
 * + 0x10 / + 0x3C / + 0x34 copies, inert to hits. */
static AosEnemy waiter_companion(const AosEnemy *parent) {
    AosEnemy c = {0};
    c.id = parent->id;
    c.role = AOS_ROLE_WAITER_COMPANION;
    c.x = parent->x;
    c.y = parent->y;
    c.mirrored = parent->mirrored;
    c.stats = parent->stats;
    c.hp = parent->hp;
    c.static_frame = -1;
    c.parent = (AosEnemy *)parent;
    c.combat.type = AOS_TYPE_ENEMY;
    c.combat.attack_off = c.combat.hurt_off = true;
    return c;
}

/* Tray attach offsets 0x0811937C for parent animations 2-5
 * (sub_080AED28 case 1): {x, y} whole pixels added to the parent's
 * attachment point, x applied by the facing (subtracted when mirrored).
 * The attachment component index (1 for animations 2-3, 0 for 4-5) is
 * approximated by the frame edge like the companion's. */
static const int8_t waiter_tray_offset[6][2] = {
    {0, 0}, {0, 0}, {-11, -13}, {-11, -13}, {11, -8}, {11, -8},
};

/* sub_080AF194: the attack on state 1, stepped by + 0x0B. Step 0 picks
 * the normal tray (bit 1 set and RNG & 3 == 0) or the powered one;
 * steps 1 and 3 spawn the tray child (+ 0x14 = 0x11 / 9) and play
 * animations 6 / 7 once; steps 2 and 4 clear + 0x20 bits and, when the
 * animation ends, replay animation 3 looping and walk again (state 2). */
static void waiter_attack(AosEnemy *e, const AosEnemyKind *kind,
                          AosHitReport *spawned, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if ((e->ws_flags & 2) && !(random() & 3)) e->step = 1;
        else e->step = 3;
        break;
    case 1:
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN)
            spawned->children[spawned->child_count++] = waiter_tray(e, 0x11);
        play_loop(e, kind->anims, 6, false);
        e->step = 2;
        break;
    case 2:
        e->ws_flags &= ~3u;
        break;
    case 3:
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN)
            spawned->children[spawned->child_count++] = waiter_tray(e, 9);
        e->ws_flags |= 1;
        play_loop(e, kind->anims, 7, false);
        e->step = 4;
        break;
    default:
        break;
    }
    if ((e->step == 2 || e->step == 4) && (e->anim.flags & AOS_ANIM_ENDED)) {
        play_loop(e, kind->anims, 3, true);
        e->state = 2;
        e->substep = e->step = 0;
    }
}

/* sub_080AF2A0: the walk on state 2, stepped by + 0x0B, with the probe
 * walker sub_0806CAF8 (table 0x08528608, mode 0xC). Step 0 strolls at
 * 0x8000 and watches the 0xC8 x 0x50 box at (-0x64, -0x28): animation 4
 * looping, then step 1 — or step 3 with a 0x14 timer on RNG & 3 == 0.
 * Step 1 hurries at 0x10000 watching the 0xA0 x 0x50 box at (-0x50,
 * -0x28): animation 5 once, step 2. Step 2 sets + 0x20 bit 2 while the
 * animation frame is 2 (the tray's throw window); on its end it clears
 * bits 1-2, walks backwards at -0x8000 for 0x3C + RNG & 0xF frames
 * (animation 1 looping), then backwards at -0x8000 for 0x32 more
 * (animation 3 looping), then back to state 0 on animation 0. */
static void waiter_walk(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        ground_walk(e, soma, layer, &kind->probes, 0x8000, 0xC);
        if (player_in(soma, (int16_t)((e->x >> 16) - 0x64),
                      (int16_t)((e->y >> 16) - 0x28), 0xC8, 0x50)) {
            play_loop(e, kind->anims, 4, true);
            if (random() & 3) {
                e->step = 1;
            } else {
                e->step = 3;
                e->timer = 0x14;
                play_loop(e, kind->anims, 4, true);
            }
        }
        break;
    case 1:
        ground_walk(e, soma, layer, &kind->probes, 0x10000, 0xC);
        if (player_in(soma, (int16_t)((e->x >> 16) - 0x50),
                      (int16_t)((e->y >> 16) - 0x28), 0xA0, 0x50)) {
            play_loop(e, kind->anims, 5, false);
            e->step = 2;
        }
        break;
    case 2:
        if (e->anim.frame == 2) e->ws_flags |= 4;
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->ws_flags &= ~6u;
            e->step = 5;
            play_loop(e, kind->anims, 1, true);
            e->timer = 0x3C + (uint8_t)(random() & 0xF);
        }
        break;
    case 3:
        ground_walk(e, soma, layer, &kind->probes, 0x10000, 0xC);
        if (e->timer) e->timer--;
        else {
            e->step = 4;
            e->timer = 0x32;
            play_loop(e, kind->anims, 3, true);
        }
        break;
    case 4:
        ground_walk(e, soma, layer, &kind->probes, -0x8000, 0xC);
        if (e->timer) e->timer--;
        else e->step = 0;
        break;
    case 5:
        ground_walk(e, soma, layer, &kind->probes, -0x8000, 0xC);
        if (e->timer) e->timer--;
        else {
            e->state = 0;
            e->substep = e->step = 0;
            play_loop(e, kind->anims, 0, true);
        }
        break;
    default:
        break;
    }
}

/* sub_080AF450: step 0 plays sound 0x6B and throws 7 pieces
 * (animations 0xA-0x10) — 6 when + 0x20 bit 1 is clear — each a
 * sub_0806C5AC debris with vx (RNG & 0xF - 8) << 0xE, vy the same <<
 * 0xC - 0x28000, ay 0x2000 and life 0x28 + RNG & 0xF, then hides the
 * body (+ 0x58 bit 0x20) and arms step 1; step 1 deletes. */
static bool waiter_death(AosEnemy *e, const AosEnemyKind *kind,
                         AosHitReport *spawned, uint32_t (*random)(void)) {
    if (e->step != 0) return true;
    aos_audio_play_song(0x6B);
    unsigned first = (e->ws_flags & 2) ? 0xA : 0xB;
    for (unsigned anim = first; anim <= 0x10 &&
                                spawned->child_count < AOS_ENEMY_MAX_CHILDREN; ++anim) {
        AosEnemy d = make_debris(e, kind->anims, anim);
        d.vx = ((int32_t)(random() & 0xF) - 8) << 0xE;
        d.vy = (((int32_t)(random() & 0xF) - 8) << 0xC) + (int32_t)0xFFFD8000;
        d.ay = 0x2000;
        d.mirrored = e->mirrored;
        d.timer = (uint8_t)(0x28 + (random() & 0xF));
        spawned->children[spawned->child_count++] = d;
    }
    e->hidden = true;
    e->step = 1;
    return false;
}

/* EnemyWaiterSkeletonUpdate (sub_080AF650): inside the sub_0806CC20
 * activity gate (approximated by the margin entry 4), state 0 faces the
 * player and wakes on the (-0xA0, -0x28, 0x140, 0x50) box while + 0x0B
 * is 0; state 1 attacks (sub_080AF194), state 2 walks
 * (sub_080AF2A0), state 3 dies (sub_080AF450, deleted when it returns
 * true) and state 4 only steps the animation. States 0-2 then run the
 * collision pass sub_0806E314 and the animation step sub_0803F17C;
 * state 4 skips both like the native early return. */
static AosHitReport waiter_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def, int cam_x,
                                  int cam_y, uint32_t (*random)(void)) {
    (void)cam_x; (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    if (!e->boss_parts_spawned) {
        /* The constructor's sub_080AEC0C child, emitted on the first
         * update (it is placed at the parent, no camera origin needed). */
        if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN)
            spawned.children[spawned.child_count++] = waiter_companion(e);
        e->boss_parts_spawned = true;
    }
    switch (e->state) {
    case 0:
        face_player(e, soma);
        if (e->step == 0 &&
            player_in(soma, (int16_t)((e->x >> 16) - 0xA0),
                      (int16_t)((e->y >> 16) - 0x28), 0x140, 0x50)) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 1:
        waiter_attack(e, kind, &spawned, random);
        break;
    case 2:
        waiter_walk(e, soma, layer, kind, random);
        break;
    case 3:
        if (waiter_death(e, kind, &spawned, random)) e->removed = true;
        break;
    case 4:
        step_anim(e, kind->anims);
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

/* sub_080AEC0C: the companion child. Hidden (+ 0x59 bit 8) and
 * skipped while the parent is in state 3 or + 0x20 bit 1 is clear.
 * State 0 loads the shared resources, copies + 0x5C and plays
 * animation 0x11 looping; state 1 pins it to the parent's attachment
 * (sub_0806C3F8 index 0, approximated by the frame edge), copies the
 * facing and replays animation 8 while + 0x20 bit 3 is set. States
 * above 1 only step the animation (the + 0x5A palette merge is not
 * ported). */
static void waiter_companion_update(AosEnemy *e, const AosEnemyKind *kind) {
    const AosEnemy *parent = e->parent;
    if (!parent || parent->state == 3 || !(parent->ws_flags & 2)) {
        e->hidden = true;
        if (parent && parent->removed) e->removed = true;
        return;
    }
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, 0x11, true);
        e->state = 1;
        break;
    case 1: {
        int dx, dy;
        skeleton_corner(parent, kind, &dx, &dy);
        skeleton_place(e, parent, dx, dy);
        e->mirrored = parent->mirrored;
        if ((parent->ws_flags & 8) && e->anim.id != 8)
            play_loop(e, kind->anims, 8, true);
        break;
    }
    default:
        break;
    }
    step_anim(e, kind->anims);
}

/* sub_080AED28: the tray child. Its preamble forces state 5 when + 0x34
 * is no longer positive, and converts a held tray (state <= 2) to the
 * flying state 3 on a palette or parent-state-5 change (the palette
 * byte is not modelled and the parent never reaches state 5, so only
 * the HP path fires). The + 0x4BE pause gate is not ported. State 0
 * keeps a sub_0803E654 sign draw (~half delete, approximated by the
 * RNG), copies the facing, plays + 0x14 looping and sits at the parent
 * plus the facing offset (-/+ 0xA), 0x10 up — hidden in state 2 when
 * + 0x20 bit 0 is set. State 1 follows the parent's attachment through
 * table 0x0811937C while its animation is 2-5 and throws (song 0x89,
 * contact armed, vx = -0x14000 - |dx| >> 6 negated by the facing, vy
 * -0x20000, ay 0x2000) when + 0x20 bit 2 is set. State 2 re-seats on
 * the parent's animation frames 1 and 3. State 3 flies at +/- 0x8000
 * with vy -0x28000 under ay 0x2000 (sound 0x89). State 4 spins
 * (+ 0x20 += vx >> 6), damps vx by 59/60, integrates and deletes off
 * the margin entry 4. State 5 bursts a sub_0806D5C0 dust puff within
 * 12 x 12 pixels and deletes. Contact (sub_080421AC) runs in states
 * 0-4, the animation step in all of them. */
static AosHitReport waiter_tray_update(AosEnemy *e, AosSoma *soma,
                                       const AosCollision *layer,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames,
                                       int soma_atk, int soma_def, int cam_x,
                                       int cam_y, uint32_t (*random)(void)) {
    (void)layer; (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    AosEnemy *parent = e->parent;
    if (!parent) {
        e->removed = true;
        return report;
    }
    if (e->state != 5 && e->hp <= 0) e->state = 5;
    if (e->state <= 2 && parent->state == 5) {
        e->hidden = false;
        e->state = 3;
    }
    switch (e->state) {
    case 0:
        if (random() & 0x8000) {
            e->removed = true;
            break;
        }
        e->mirrored = parent->mirrored;
        play_loop(e, kind->anims, e->ws_anim, true);
        e->ws_offset = e->mirrored ? -0xA : 0xA;
        skeleton_place(e, parent, e->ws_offset, -0x10);
        if (parent->ws_flags & 1) {
            e->hidden = true;
            e->state = 2;
        } else {
            e->state = 1;
        }
        break;
    case 1:
        if ((uint8_t)(parent->anim.id - 2) <= 4) {
            int dx, dy;
            skeleton_corner(parent, kind, &dx, &dy);
            int xoff = waiter_tray_offset[parent->anim.id][0];
            int yoff = waiter_tray_offset[parent->anim.id][1];
            skeleton_place(e, parent,
                           dx + (parent->mirrored ? -xoff : xoff), dy + yoff);
            e->mirrored = parent->mirrored;
            if (parent->ws_flags & 4) {
                int32_t dist = target_x(soma) - e->x;
                if (dist < 0) dist = -dist;
                int32_t vx = (int32_t)0xFFFEC000 - (dist >> 6);
                e->vx = e->mirrored ? -vx : vx;
                e->vy = (int32_t)0xFFFE0000;
                e->ay = 0x2000;
                aos_audio_play_song(0x89);
                e->state = 4;
            }
        }
        break;
    case 2:
        if (parent->state == 3) {
            e->state = 3;
            break;
        }
        if (parent->anim.frame == 1) {
            e->hidden = false;
        } else if (parent->anim.frame == 3) {
            e->mirrored = parent->mirrored;
            e->ws_offset = e->mirrored ? 0xA : -0xA;
            skeleton_place(e, parent, e->ws_offset, -0x10);
            e->state = 1;
        }
        break;
    case 3:
        e->vx = e->mirrored ? -0x8000 : 0x8000;
        e->vy = (int32_t)0xFFFD8000;
        e->ay = 0x2000;
        aos_audio_play_song(0x89);
        e->state = 4;
        break;
    case 4: {
        int32_t v = e->vx;
        e->angle = (uint32_t)((int32_t)e->angle + (v < 0 ? (v + 0x3F) >> 6 : v >> 6));
        e->vx = e->vx * 59 / 60;
        e->vy += e->ay;
        if (e->ay >= 0 ? e->vy > 0x80000 : e->vy < -0x80000)
            e->vy = e->ay >= 0 ? 0x80000 : -0x80000;
        e->x += e->vx;
        e->y += e->vy;
        if (outside_margins(kind, 4, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y))
            e->removed = true;
        break;
    }
    case 5: {
        int32_t ox = (int32_t)(random() % 0xC0000) - 0x60000;
        int32_t oy = (int32_t)(random() % 0xC0000) - 0x60000;
        spawn_drift(AOS_EFFECT_DUST, e->x + ox, e->y + oy, &spawned);
        e->removed = true;
        break;
    }
    default:
        break;
    }
    /* The contact box arms only once the tray leaves the hand
     * (sub_0804277C's type 0xA with sub_080AF740 / sub_080AF784). */
    e->attack_off = e->state <= 2;
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

/* The Cockatrice's linked children share its resources: the shadow
 * (sub_080BB924) is the drifting soul copy and the rock (sub_080BBAC4)
 * is the attack's spinning projectile. */
