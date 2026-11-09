/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Biphron (enemy 0x4E) and its shot.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Biphron (EnemyBiphronCreate 0x08099C5C, EnemyBiphronUpdate
 * 0x0809A450 behind the sub_0806CC20 awake gate — approximated by
 * the usual activity/hit-stun handling): state 0 sub_08099CF0
 * patrols and hovers until the player box + RNG gate, state 1
 * sub_08099ED8 plays the attack and fires the sub_08099FD0
 * projectile at anim frame 4 (song 0x78), state 2 sub_08099DB8
 * is the death burst (song 0x6E), state 3 affine only. */
static AosHitReport biphron_update(AosEnemy *e, AosSoma *soma,
                                   const AosCollision *layer, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        if (e->step == 0) {
            /* sub_0806CF2C facing (+ 0x58 bit 6), vx = +-0x4000 by it,
             * vy = +0x10000, + 0x54 = 0x2800. */
            face_player(e, soma);
            e->vx = e->mirrored ? 0x4000 : -0x4000;
            e->vy = 0x10000;
            e->angle = 0x2800;
            e->step = 1;
        }
        /* sub_08069A00(e, 0x10000, -0x200000, 0x80000, 8): walk_collide
         * is its port; only the floor bit (& 4) feeds the hover
         * timer + 0xD. */
        if (walk_collide(e, layer, 0x10000, -0x200000, 0x80000, 8) & 4) {
            if (++e->timer > 0x3B) {
                e->timer = 0;
                e->step = 0;
                if (player_near(soma, (e->x >> 16) - 0x5A, (e->y >> 16) - 0x64,
                                0xB4, 0xC8) && (random() & 0x7F) <= 0x2F) {
                    e->state = 1;
                    e->step = 0;
                }
            }
        }
        break;
    case 1:
        if (e->step == 0) {
            play_loop(e, kind->anims, 1, false);
            e->step = 1;
        } else {
            /* Every update on anim frame 4 fires one projectile
             * (sub_08099FD0) and requests song 0x78; + 0x59 bit 0
             * (cycle_done) ends the attack back into state 0. */
            if (e->anim.frame == 4) {
                if (report.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    AosEnemy shot = {0};
                    shot.id = e->id;
                    shot.role = AOS_ROLE_BIPHRON_SHOT;
                    shot.static_frame = -1;
                    shot.x = e->x + (e->mirrored ? -0x1A0000 : 0x1A0000);
                    shot.y = e->y;
                    shot.mirrored = e->mirrored;
                    shot.vy = -0x10000;
                    /* The native sub_08042848 physics preset
                     * 0x0E14F2FB is not decoded; a modest forward
                     * drift stands in for it. */
                    shot.vx = e->mirrored ? 0x4000 : -0x4000;
                    shot.stats = e->stats;
                    shot.hp = 1;
                    shot.combat.type = AOS_TYPE_ENEMY;
                    shot.own_boxes = true;
                    shot.own_hurt = (AosBox){-8, -8, 16, 16};
                    shot.own_attack = (AosBox){-6, -6, 12, 12};
                    shot.contact_elements = 2;
                    shot.timer = 0xFF;
                    report.children[report.child_count++] = shot;
                }
                aos_audio_play_song(0x78);
            }
            if (e->cycle_done) {
                play_loop(e, kind->anims, 0, true);
                e->state = 0;
                e->step = 0;
            }
        }
        break;
    case 2:
        /* sub_08099DB8: at anim frame 1 two spark pairs
         * (sub_0806D644, approximated by drift sparks); from frame 6
         * a 0x50-tick burst — a sprite piece + song 0x6E every 16th
         * tick (sub_0809A340 is approximated by a spark), the blink
         * skipped — then the native + 0x59 |= 8 deletes it. */
        if (e->step == 0) {
            if (e->anim.frame == 1) {
                int32_t rx = (int32_t)(random() & 0x7FFFF) - 0x40000;
                spawn_drift(AOS_EFFECT_SPARK, e->x, e->y - 0x6000, &report);
                spawn_drift(AOS_EFFECT_SPARK, e->x + rx - 0x40000, e->y - 0x4000, &report);
                e->step = 1;
            }
        } else if (e->step == 1) {
            if (e->anim.frame == 6) {
                e->step = 2;
                e->timer = 0x50;
            }
        } else {
            if (e->timer > 0x2F && (e->timer & 0xF) == 0) {
                int32_t rx = (int32_t)(random() & 0xFFFFF) - 0x80000;
                spawn_drift(AOS_EFFECT_SPARK,
                            e->x + (e->mirrored ? -0x80000 : 0x80000) + rx - 0x80000,
                            e->y - 0x80000, &report);
                aos_audio_play_song(0x6E);
            }
            if (--e->timer == 0) {
                e->removed = true;
                return report;
            }
        }
        break;
    case 3:
        /* Affine identity only (sub_0803F17C epilogue). */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    default:
        break;
    }
    /* States 0-2: the sub_0806DF20 + sub_0806E314 epilogue — the
     * collision pass then the animation commit. */
    {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}

/* The sub_08099FD0 projectile (update sub_0809A29C): it rises,
 * drifts forward, dies on a wall/ceiling probe or off the timers,
 * damages Soma through its elements-2 contact record
 * (sub_0809A310) and leaves a spark trail with song 0x77 every
 * 4th tick (sub_0809A0DC trail approximated). */
static AosHitReport biphron_shot_update(AosEnemy *b, AosSoma *soma,
                                        const AosCollision *layer, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def) {
    AosHitReport report = {0};
    if (b->step == 0) {
        /* sub_0803F2C8(e, 4, 0, 0): the shot's own animation. */
        if (kind->anims && kind->anims->count > 4) play_loop(b, kind->anims, 4, false);
        b->step = 1;
    }
    if (b->timer && --b->timer == 0) {
        b->removed = true;
        return report;
    }
    b->x += b->vx;
    b->y += b->vy;
    int32_t x = b->x >> 16, y = (b->y >> 16) - 10;
    bool blocked = false;
    if (b->vx < 0 && aos_wall_push_right(layer, x - 8, y)) blocked = true;
    if (b->vx > 0 && aos_wall_push_left(layer, x + 8, y)) blocked = true;
    if (b->vy < 0) {
        int depth = aos_ceiling_depth(layer, b->x >> 16, b->y >> 16, 0, false);
        if (depth) {
            b->y += (int32_t)((uint32_t)depth << 16);
            blocked = true;
        }
    }
    if (blocked) {
        b->removed = true;
        return report;
    }
    if ((b->timer & 3) == 0) {
        spawn_drift(AOS_EFFECT_SPARK, b->x, b->y, &report);
        aos_audio_play_song(0x77);
    }
    report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(b, kind->anims);
    aos_combat_tick(&b->combat);
    return report;
}

/* Bomber Armor (EnemyBomberArmorUpdate 0x080B3040): it shares the Rock
 * Armor's walk (sub_080B2978, probes 0x0852872C) and death (sub_080B2CD8)
 * with it; only the throw (sub_080B2A80) and the bomb it throws
 * (sub_080B243C, with its fuse spark sub_080B2FC4) are its own. */
