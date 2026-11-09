/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Tsuchinoko (enemy 0x23), its spit and rock.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_080A776C: the fast spit — entity type 0x1B, role 0x48, update
 * sub_080A79D8, animation 4 looping. It spawns at the parent's pixel
 * position 0x1A pixels up, inherits the facing bit, flies at +/-3.0
 * with vy -1.0 under ay 0x1800 and copies the + 0x37 / + 0x2C / + 0x3C
 * bytes (the contact damage base). Its sub_080A785C installs the
 * 0x0808FCFC box preset with the sub_080A7A54 contact (the + 0x3C
 * damage byte, element 0x40, then + 0x59 |= 8) and the sub_080A7A90
 * hurt (any hit deletes it). */
static void tsuchinoko_spit(AosEnemy *e, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy s = {0};
    s.id = e->id;
    s.role = AOS_ROLE_TSUCHINOKO_SPIT;
    s.static_frame = -1;
    s.stats = e->stats;
    s.contact_elements = 0x40;
    s.mirrored = e->mirrored;
    s.x = (e->x >> 16) << 16;
    s.y = ((e->y >> 16) - 0x1A) << 16;
    s.vx = e->mirrored ? 0x30000 : (int32_t)0xFFFD0000;
    s.vy = (int32_t)0xFFFF0000;
    s.ay = 0x1800;
    report->children[report->child_count++] = s;
}

/* sub_080A78A4: the dormant burrower's rock — entity type 0x1B, role
 * 0x48, update sub_080A7AAC, still frame 0x12 (+ 0x65), + 0x72 |= 6.
 * Random mirrored and vflip bits, the parent's pixel position, vx
 * (RNG & 0x1FFFF) - 1.0 and vy -1.0 - (RNG & 0xFFFF) under ay 0x1800.
 * No contact or hurt callbacks: it is harmless and cannot be struck. */
static void tsuchinoko_rock(AosEnemy *e, AosHitReport *report,
                            uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy s = {0};
    s.id = e->id;
    s.role = AOS_ROLE_TSUCHINOKO_ROCK;
    s.static_frame = 0x12;
    s.stats = e->stats;
    s.mirrored = (random() & 1) != 0;
    s.vflip = (random() & 1) != 0;
    s.x = (e->x >> 16) << 16;
    s.y = (e->y >> 16) << 16;
    s.vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
    s.vy = (int32_t)0xFFFF0000 - (int32_t)(random() & 0xFFFF);
    s.ay = 0x1800;
    s.combat.attack_off = s.combat.hurt_off = true;
    report->children[report->child_count++] = s;
}

/* sub_080A7294: state 0, the hop state machine. */
static void tsuchinoko_ground(AosEnemy *e, AosSoma *soma,
                              const AosCollision *layer,
                              const AosEnemyKind *kind, AosHitReport *spawned,
                              uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    int px = (int16_t)(e->x >> 16), py = (int16_t)(e->y >> 16);
    switch (e->step) {
    case 0:
        /* The facing write of sub_0806CF2C into + 0x58 bit 6; the wake
         * is the notice box or the damage flag of sub_0806D54C. */
        face_player(e, soma);
        if (player_in(soma, px - 0x5A, py - 0x64, 0xB4, 0xC8) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            play_loop(e, anims, 1, false);
        }
        break;
    case 1:
        if (e->anim.frame == 2 && e->anim.tick == 0) {
            /* Near box: face away; far box hit: keep the facing;
             * outside both: face the player. */
            if (player_in(soma, px - 0x3C, py - 0x64, 0x78, 0xC8)) {
                e->mirrored = !(px < (int16_t)(target_x(soma) >> 16));
            } else if (!player_in(soma, px - 0x64, py - 0x64, 0xC8, 0xC8)) {
                face_player(e, soma);
            }
            e->step = 2;
            e->vy = (int32_t)0xFFFE8000;
            e->ay = 0x2800;
            e->vx = e->mirrored ? 0x10000 : (int32_t)0xFFFF0000;
        }
        break;
    case 2: {
        /* sub_08069A00: walk_collide with bounce 1.0, no head offset,
         * speed cap 8.0 and half width 8; only the ground bit matters. */
        int result = walk_collide(e, layer, 0x10000, 0, 0x80000, 8);
        if (!(result & 4)) break;
        if ((random() & 0x7F) <= 0xF) {
            e->state = 1;
            e->step = 0;
            break;
        }
        if ((random() & 0x7F) <= 0xF) {
            e->step = 0xA;
            break;
        }
        bool wounded = e->hp_home / 2 >= e->hp;
        if (wounded && (random() & 0x7F) <= 0x1F) {
            e->step = 0xA;
            break;
        }
        /* _080A7468: the quarter-HP gate, then the last RNG draw. */
        if (e->hp_home / 4 < e->hp || (random() & 0x7F) > 0x3F) {
            play_loop(e, anims, 1, false);
            e->step = 1;
        } else {
            e->step = 0xA;
        }
        break;
    }
    case 0xA:
        play_loop(e, anims, 2, false);
        face_player(e, soma);
        e->step = 0xB;
        break;
    case 0xB:
        if (e->anim.frame == 3 && e->anim.tick == 0x14) {
            play_loop(e, anims, 3, false);
            e->step = 0xC;
        }
        break;
    case 0xC:
        /* The dormant spitter: past frame 1 its timer counts and every
         * sixteenth tick launches a rock with song 0x152; the
         * animation end deletes it (+ 0x59 |= 8). */
        if (e->anim.frame > 1) {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if ((old & 0xF) == 0) {
                tsuchinoko_rock(e, spawned, random);
                aos_audio_play_song(0x152);
            }
        }
        if (e->anim.flags & AOS_ANIM_ENDED) e->removed = true;
        break;
    default:
        break;
    }
}

/* sub_080A7534: state 1, the rear-up attack. */
static void tsuchinoko_attack(AosEnemy *e, AosSoma *soma,
                              const AosEnemyKind *kind, AosHitReport *spawned,
                              uint32_t (*random)(void)) {
    (void)random;
    const AosAnimSet *anims = kind->anims;
    if (e->step == 0) {
        play_loop(e, anims, 2, false);
        face_player(e, soma);
        e->step = 1;
    } else {
        if (e->anim.frame == 3 && e->anim.tick == 0) {
            tsuchinoko_spit(e, spawned);
            aos_audio_play_song(0x85);
        }
        if (e->anim.flags & AOS_ANIM_ENDED) {
            /* Back to the hop launch: state 0, step 1 (the entry step),
             * animation 1 once. */
            e->state = 0;
            e->step = 1;
            play_loop(e, anims, 1, false);
        }
    }
}

/* sub_080A75B0: state 2, the death — vx = vy = 0, ay 0x1000, a gore
 * chunk of sub_0806DD64 at timer 0 and again at 0x34, the sub_0806D3D8
 * move capped at 0.5, the sub_0806D518 blink (not ported) and the
 * delete at timer 0. */
static void tsuchinoko_die(AosEnemy *e, AosHitReport *spawned,
                           uint32_t (*random)(void)) {
    if (e->step == 0) {
        e->vx = e->vy = 0;
        e->ay = 0x1000;
        int32_t ox = (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t oy = (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = (int32_t)(random() & 0x1FFFF) - 0x10000;
        spawn_particle(AOS_EFFECT_BLOOD, 0x11, e->x + ox, e->y + oy,
                       vx, vy, spawned);
        e->timer = 0x3C;
        e->step = 1;
    } else {
        if (e->timer == 0x34) {
            int32_t ox = (int32_t)(random() & 0x7FFFF) - 0x40000;
            int32_t oy = (int32_t)(random() & 0x7FFFF) - 0x40000;
            int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
            int32_t vy = (int32_t)(random() & 0x1FFFF) - 0x10000;
            spawn_particle(AOS_EFFECT_BLOOD, 0x11, e->x + ox, e->y + oy,
                           vx, vy, spawned);
        }
        accel_move(e, 0x8000);
        if (e->timer && --e->timer == 0) e->removed = true;
    }
}

static AosHitReport tsuchinoko_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    (void)cam_x; (void)cam_y;
    /* sub_0806AE54: + 0x2D counts down outside the death state. */
    if (e->state != 2 && e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0:
        tsuchinoko_ground(e, soma, layer, kind, &spawned, random);
        break;
    case 1:
        tsuchinoko_attack(e, soma, kind, &spawned, random);
        break;
    case 2:
        tsuchinoko_die(e, &spawned, random);
        break;
    case 3:
        /* Animation only (sub_0803F17C), no collision pass. */
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

/* sub_080A79D8 (the spit) and sub_080A7AAC (the rock): state 0 moves
 * with sub_0806D3D8 capped at 4.0 and deletes off screen through
 * sub_0806D128(1). Only the spit refreshes its animation and runs the
 * collision pass — its sub_080A7A54 contact deals the + 0x3C damage at
 * element 0x40 and deletes it; its sub_080A7A90 hurt deletes it on any
 * hit (handled in collide). The rock has no callbacks at all. */
static AosHitReport tsuchinoko_child_update(AosEnemy *s, AosSoma *soma,
                                            const AosCollision *layer,
                                            const AosEnemyKind *kind,
                                            const AosWeaponEntity *weapon,
                                            const AosWeaponFrames *weapon_frames,
                                            int soma_atk, int soma_def,
                                            int cam_x, int cam_y,
                                            uint32_t (*random)(void)) {
    (void)layer; (void)random;
    AosHitReport report = {0};
    if (s->state == 0) {
        accel_move(s, 0x40000);
        int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
        if (outside_margins(kind, 1, sx, sy)) {
            s->removed = true;
            return report;
        }
    }
    if (s->role == AOS_ROLE_TSUCHINOKO_SPIT) {
        step_anim(s, kind->anims);
        report = collide(s, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        if (report.soma_hit) s->removed = true;
    }
    aos_combat_tick(&s->combat);
    return report;
}
