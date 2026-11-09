/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Ripper (enemy 0x4A) and its blade.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_080C2ABC / sub_080C2B74: Ripper's short-range blade child. It is
 * placed twelve pixels toward the body's facing, accelerates horizontally,
 * and falls away after a weapon or player collision. */
static void ripper_blade(AosEnemy *owner, const AosEnemyKind *kind,
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy blade = {0};
    blade.id = owner->id;
    blade.role = AOS_ROLE_RIPPER_BLADE;
    blade.parent = owner;
    blade.x = owner->x + (owner->mirrored ? AOS_FIXED(12) : -AOS_FIXED(12));
    blade.y = owner->y - AOS_FIXED(12);
    blade.mirrored = owner->mirrored;
    blade.vx = owner->mirrored ? 0x30000 : -0x30000;
    blade.ax = owner->mirrored ? 0x50 : -0x50;
    blade.state = 4;
    blade.static_frame = -1;
    blade.stats = owner->stats;
    blade.hp = owner->hp;
    blade.combat.type = AOS_TYPE_ENEMY;
    blade.own_boxes = true;
    blade.own_hurt = blade.own_attack = (AosBox){-8, -8, 16, 16};
    play(&blade, kind->anims, 1);
    report->children[report->child_count++] = blade;
}

static AosHitReport ripper_blade_update(AosEnemy *b, AosSoma *soma,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    /* sub_080C2B74: native state 3 starts the blade's flight.
     * The host runtime creates the blade directly in its flying state 4. */
    if (b->state == 4 && !b->firing) {
        b->firing = true;
        aos_audio_play_song(0x89);
    }
    if (b->state == 4) {
        b->vx += b->ax;
        b->x += b->vx;
    } else {
        b->vy += b->ay;
        b->y += b->vy;
    }
    step_anim(b, kind->anims);
    int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) {
        b->removed = true;
        return report;
    }
    report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit || report.soma_hit) {
        /* sub_080C2DC4: only contact with Soma plays this sound. */
        if (report.soma_hit) aos_audio_play_song(0x13A);
        b->state = 6;
        b->vx = b->ax = 0;
        b->vy = 0x8000;
        b->ay = 0x1000;
        b->combat.attack_off = b->combat.hurt_off = true;
    }
    aos_combat_tick(&b->combat);
    return report;
}

/* sub_080C25F8 and sub_080C2A18. */
static AosHitReport ripper_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 3) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (e->step == 0) {
            /* sub_080C2A18: the Ripper's one-shot death sound. */
            aos_audio_play_song(0x72);
            play(e, kind->anims, 2);
            e->timer = 50;
            e->vx = e->ax = e->vy = 0;
            e->ay = 0x2000;
            e->step = 1;
        }
        e->vy += e->ay;
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;

    if (e->step == 0) {
        play(e, kind->anims, 0);
        e->phase = 0;
        e->vx = toward_facing(e, 0x8000);
        e->step = e->dropped ? 1 : 4;
    }
    if (e->step == 1) {
        e->vy = e->ay = 0x2000;
        e->step = 2;
    } else if (e->step == 2) {
        if (e->phase != 3) {
            play(e, kind->anims, 3);
            e->phase = 3;
        }
        if (walk_collide(e, layer, 0, -AOS_FIXED(10), 0, 8) & 4) {
            snap_to_floor(e, layer);
            e->timer = 0;
            e->step = 3;
        }
    } else if (e->step == 3) {
        if (++e->timer > 15) {
            e->step = 4;
            e->phase = 0;
            play(e, kind->anims, 0);
        }
    } else if (e->step == 4) {
        e->x += e->vx;
        face_player(e, soma);
        uint32_t jump = random() % 6;
        int32_t speed = jump == 3 ? 0x1A000 : 0xD000;
        e->vx = toward_facing(e, speed);
        e->vy = -0x30000;
        if (jump == 3) e->ay = 0x2800;
        else if (jump == 4) e->ay = 0x1400;
        else e->ay = 0xA000;
        if (jump == 3 || jump == 4) {
            play(e, kind->anims, 3);
            e->phase = 3;
            e->firing = false;
            e->step = 5;
        } else {
            play(e, kind->anims, 0);
            e->phase = 0;
            e->step = 6;
        }
        int dx = (e->x >> 16) - (target_x(soma) >> 16);
        if (dx >= -63 && dx <= 63 && (e->x >> 16) > 0 && (e->x >> 16) < 239)
            e->vx = -e->vx;
    } else if (e->step == 5 || e->step == 6) {
        if (e->step == 5 && e->phase == 3 && !e->firing && e->vy >= -e->ay && e->vy <= e->ay) {
            play_loop(e, kind->anims, 1, false);
            e->phase = 1;
            e->firing = true;
        }
        if (e->step == 5 && e->phase == 1 && (e->cycle_done || e->anim.frame > 0)) {
            ripper_blade(e, kind, &report);
            play(e, kind->anims, 3);
            e->phase = 3;
        }
        int collision = walk_collide(e, layer, 0, -AOS_FIXED(10), 0, 8);
        if (collision) {
            if (collision & 4) snap_to_floor(e, layer);
            e->step = 4;
            e->phase = 0;
            e->firing = false;
            play(e, kind->anims, 0);
        }
    }

    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}
