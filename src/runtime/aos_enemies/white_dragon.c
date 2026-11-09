/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: White Dragon (enemy 0x0B), its segments and fireballs.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* A 16.16 product shifted down 16, rounded toward zero. */
static int32_t trunc16(int32_t v) { return v < 0 ? -((-v) >> 16) : v >> 16; }

/* The angle of a neck point: heading + sin(swing) * amplitude / 4096 +
 * sin(phase) / 16 (both rounded toward zero). */
static int32_t dragon_point_angle(const AosEnemy *d, uint16_t swing) {
    int32_t a = aos_sine(swing) * d->wd_swing;
    a = (a < 0 ? a + 0xFFF : a) >> 12;
    int32_t b = aos_sine(d->wd_phase);
    b = (b < 0 ? b + 0xF : b) >> 4;
    return (int16_t)d->wd_heading + a + b;
}

/* sub_08077080 (the head's draw callback): the head at its neck length
 * from the root at the current swing. */
static void dragon_place_head(AosEnemy *d) {
    int32_t angle = dragon_point_angle(d, d->wd_angle);
    d->x = AOS_FIXED(d->wd_home_x + trunc16(aos_sine((uint32_t)(angle + 0x4000)) * d->wd_radius));
    d->y = AOS_FIXED(d->wd_home_y + trunc16(aos_sine((uint32_t)angle) * d->wd_radius));
}

/* sub_080771C8 (a segment's draw callback): segment i at (9 - i) / 10 of
 * the neck, at the swing of (i + 1) * 8 frames ago (6 while dying). */
static void dragon_place_segment(AosEnemy *s, const AosEnemy *d) {
    int i = s->variant, k = d->state == 2 ? 6 : 8, h = (i + 1) * k;
    uint16_t swing = h < 100 ? d->wd_history[h] : 0;
    int32_t angle = dragon_point_angle(d, swing);
    int32_t dx = aos_sine((uint32_t)(angle + 0x4000)) * d->wd_radius * (9 - i) / 10;
    int32_t dy = aos_sine((uint32_t)angle) * d->wd_radius * (9 - i) / 10;
    s->x = AOS_FIXED(d->wd_home_x + trunc16(dx));
    s->y = AOS_FIXED(d->wd_home_y + trunc16(dy));
}

static void dragon_record(AosEnemy *d) {
    memmove(&d->wd_history[1], &d->wd_history[0], sizeof d->wd_history[0] * 99);
    d->wd_history[0] = d->wd_angle;
}

/* The heading eases by 0x40 toward 0 (facing right) or 0x8000. */
static void dragon_settle_heading(AosEnemy *d) {
    uint32_t h = d->wd_heading;
    if (d->mirrored) {
        if (h <= 0x7FFF) h += 0x10000;
        if (h <= 0xFFFF) {
            h += 0x40;
            if (h > 0x10000) h = 0x10000;
        } else {
            h -= 0x40;
            if (h <= 0x10000) h = 0x10000;
        }
    } else if (h <= 0x7FFF) {
        h += 0x40;
        if (h > 0x8000) h = 0x8000;
    } else {
        h -= 0x40;
        if (h <= 0x8000) h = 0x8000;
    }
    d->wd_heading = (uint16_t)h;
}

/* The wobble phase advances by 0x140 while away from the half turns. */
static bool dragon_wobble(AosEnemy *d) {
    uint16_t p = d->wd_phase;
    if (p > 0x8000 ? p <= 0xFEAF : p <= 0x7EAF) {
        d->wd_phase = (uint16_t)(p + 0x140);
        return true;
    }
    return false;
}

/* sub_080777F4(e, x, y): a flame (sprite frame 9) within 6 pixels of
 * (x, y) on the side the head faces, 3 up to 2 down, flickering its flips
 * for 10 frames (sub_08077BB0). */
static void dragon_flame(const AosEnemy *d, int32_t x, int32_t y, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy f = {0};
    f.id = d->id;
    f.role = AOS_ROLE_DRAGON_FLAME;
    f.static_frame = 9;
    f.mirrored = random() & 1;
    int32_t dx = (int32_t)(random() % 6);
    f.x = x + AOS_FIXED(d->mirrored ? dx : -dx);
    f.y = y + AOS_FIXED((int32_t)(random() % 6) - 3);
    f.combat.attack_off = f.combat.hurt_off = true;
    report->children[report->child_count++] = f;
}

/* sub_08077A88(e, x, y, frame): a falling piece, 0.25 either way (r &
 * 0x7FFF) - 0.25, gravity 0x1000 (7.0 cap), blinking out over 45 frames. */
static void dragon_piece(const AosEnemy *from, int32_t x, int32_t y, int frame,
                         uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = from->id;
    p.role = AOS_ROLE_DRAGON_PIECE;
    p.static_frame = (int16_t)frame;
    p.x = x;
    p.y = y;
    p.mirrored = from->mirrored;
    p.vx = (int32_t)(random() & 0x7FFF) - 0x4000;
    p.ay = 0x1000;
    p.timer = 0x2C;
    p.combat.attack_off = p.combat.hurt_off = true;
    report->children[report->child_count++] = p;
}

/* The target's room position. */
static int dragon_tx(const AosSoma *soma) { return target_x(soma) >> 16; }
static int dragon_ty(const AosSoma *soma) { return target_y(soma) >> 16; }

/* sub_08076824 (state 0, also the inert state 4): the neck eases to 72
 * pixels and swings (rate easing to 0x400, amplitude to 0x80, wobble by
 * 0x140), the heading settles; then two countdowns: the first may start a
 * bite at a target in the 84 x 200 box 40 pixels ahead of its root, the
 * second a breath (fire or fireballs) at one in the 80 x 200 box ahead of
 * its head. */
static void dragon_idle(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void)) {
    if (d->step != 0) return;
    if (d->wd_radius <= 0x47) d->wd_radius++;
    else if (d->wd_radius > 0x48) d->wd_radius--;
    d->wd_phase = (uint16_t)(d->wd_phase + 0x140);
    if (d->wd_rate <= 0x3FF) d->wd_rate = (uint16_t)(d->wd_rate + 0x10);
    else if (d->wd_rate > 0x400) d->wd_rate = (uint16_t)(d->wd_rate - 0x10);
    d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
    dragon_record(d);
    if (d->wd_swing <= 0x7F) d->wd_swing++;
    else if (d->wd_swing > 0x80) d->wd_swing--;
    dragon_settle_heading(d);
    if (d->state == 4) return;
    int x = d->x >> 16, y = d->y >> 16;
    if (d->wd_cool_a) {
        d->wd_cool_a--;
    } else if ((random() & 0xFF) <= 0x40) {
        int bx = d->mirrored ? d->wd_home_x + 0x28 : d->wd_home_x - 0x7C;
        if (player_near(soma, bx, y - 0x64, 0x54, 0xC8)) {
            d->state = 1;
            d->step = 0;
            d->timer = 0;
            d->wd_cool_a = (int16_t)dragon_tx(soma);
            d->wd_cool_b = (int16_t)(dragon_ty(soma) - 0x14);
            return;
        }
    } else {
        d->wd_cool_a = (int16_t)(random() % 0x1E);
    }
    if (d->wd_cool_b) {
        d->wd_cool_b--;
    } else if ((random() & 0xFF) <= 0x40) {
        if (player_near(soma, d->mirrored ? x : x - 0x50, y - 0x64, 0x50, 0xC8)) {
            d->state = 1;
            d->step = (random() & 1) ? 0x14 : 0x0A;
            d->timer = 0;
            d->wd_cool_a = (int16_t)dragon_tx(soma);
            d->wd_cool_b = (int16_t)(dragon_ty(soma) - 0x14);
            d->wd_aim = aos_arctan2((int16_t)(dragon_tx(soma) - x),
                                    (int16_t)(dragon_ty(soma) - (y + 0x14)));
        }
    } else {
        d->wd_cool_b = (int16_t)(random() % 0x1E + 0x1E);
    }
}

/* Back to state 0 with new countdowns (60-119, 0-59 frames). */
static void dragon_rest(AosEnemy *d, uint32_t (*random)(void)) {
    d->state = d->step = 0;
    d->wd_cool_a = (int16_t)(random() % 0x3C + 0x3C);
    d->wd_cool_b = (int16_t)(random() % 0x3C);
}

/* sub_08077554: a fireball (sub_08077644, sprite frame 5) from the head at
 * 4.0 along the breath angle; contact element 2 with a knockback. */
static void dragon_fireball(const AosEnemy *d, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = d->id;
    b.role = AOS_ROLE_DRAGON_FIREBALL;
    b.static_frame = 5;
    b.x = d->x;
    b.y = d->y;
    b.mirrored = d->mirrored;
    b.vx = fixed_mul(aos_sine((uint32_t)((int16_t)d->wd_aim + 0x4000)), 0x40000);
    b.vy = fixed_mul(aos_sine((uint32_t)(int16_t)d->wd_aim), 0x40000);
    b.stats = d->stats;
    b.hp = 1;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-4, -4, 8, 8};
    b.contact_elements = 2;
    report->children[report->child_count++] = b;
}

/* sub_08076B34 (state 1). The bite (step 0): the neck draws in to 32
 * pixels, the swing speeds to 0x800 and its amplitude falls, the heading
 * turns by 0x80 toward the target; after 40 frames, once all are settled,
 * it lunges (sound 0x87): the neck grows by 4 to 100 for 40 frames. The
 * breaths (steps 10 / 20): 60 frames of flames at the mouth (sound 0x77,
 * frame 1; step 20 also draws in sparks), one frame opening (frame 2,
 * step 22 choosing its sweep), then 60 frames of a fireball every fourth
 * of the first 30 (0x77), step 22 sweeping by 0x300. */
static void dragon_attack(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void),
                          AosHitReport *report) {
    switch (d->step) {
    case 0: {
        int busy = 0;
        if (d->wd_radius > 0x20) {
            d->wd_radius--;
            busy = 1;
        }
        if (d->wd_rate <= 0x7FF) {
            d->wd_rate = 0x800;
            busy++;
        }
        d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
        dragon_record(d);
        if (d->wd_swing > 0) {
            d->wd_swing -= 2;
            busy++;
            if (d->wd_swing < 0) d->wd_swing = 0;
        }
        if (dragon_wobble(d)) busy++;
        uint32_t want = aos_arctan2((int16_t)(d->wd_cool_a - d->wd_home_x),
                                    (int16_t)(d->wd_cool_b - d->wd_home_y));
        uint32_t h = d->wd_heading;
        if (d->mirrored) {
            if (h <= 0x7FFF) h += 0x10000;
            if (want <= 0x7FFF) want += 0x10000;
        }
        if (h < want) {
            busy++;
            h += 0x80;
            if (h > want) h = want;
        } else if (h > want) {
            busy++;
            h -= 0x80;
            if (h < want) h = want;
        }
        d->wd_heading = (uint16_t)h;
        if (d->timer <= 0x27) {
            d->timer++;
            break;
        }
        if (busy) break;
        d->step = 1;
        d->timer = 0;
        aos_audio_play_song(0x87);
        break;
    }
    case 1:
        if (d->wd_radius <= 0x63) d->wd_radius += 4;
        if (d->timer++ > 0x27) dragon_rest(d, random);
        break;
    case 10:
    case 20:
        if (d->timer == 0) {
            d->static_frame = 1;
            aos_audio_play_song(0x77);
        }
        if (d->timer <= 0x1D ? (d->timer & 7) == 0 : (d->timer & 3) == 0)
            dragon_flame(d, d->x, d->y, random, report);
        if (d->step == 20 && (d->timer & 7) == 2 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_080778D4: a spark 16-23 pixels ahead, -16..15 up or
             * down, drawn into the mouth over 8 frames (sub_080779E4). */
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_DRAGON_SPARK;
            s.static_frame = 9;
            s.mirrored = random() & 1;
            int32_t dx = (int32_t)(random() & 7) + 0x10;
            s.x = d->x + AOS_FIXED(d->mirrored ? dx : -dx);
            s.y = d->y + AOS_FIXED((int32_t)(random() & 0x1F) - 0x10);
            s.target_x = d->x;
            s.target_y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            report->children[report->child_count++] = s;
        }
        if (d->timer++ > 0x3B) {
            d->step++;
            d->timer = 0;
        }
        break;
    case 11:
    case 21:
        if (d->timer == 0) {
            d->static_frame = 2;
            aos_audio_play_song(0x77);
        }
        if (++d->timer == 0) break;
        d->step++;
        d->timer = 0;
        if (d->step == 0x16) {
            uint32_t now = aos_arctan2((int16_t)(dragon_tx(soma) - (d->x >> 16)),
                                       (int16_t)(dragon_ty(soma) - ((d->y >> 16) + 0x14)));
            uint32_t was = d->wd_aim;
            if (now <= 0x3FFF) now += 0x10000;
            if (was <= 0x3FFF) was += 0x10000;
            d->wd_sweep_up = now > was;
        }
        break;
    case 12:
    case 22: {
        uint8_t t = ++d->timer;
        if ((uint8_t)(t - 1) > 0x3B) {
            dragon_rest(d, random);
            d->static_frame = 0;
            break;
        }
        if (t <= 0x1D && (t & 3) == 1) {
            dragon_fireball(d, report);
            aos_audio_play_song(0x77);
            if (d->step == 0x16)
                d->wd_aim = (uint16_t)(d->wd_aim + (d->wd_sweep_up ? 0x300 : -0x300));
        }
        break;
    }
    default: break;
    }
}

/* sub_08076ED8 (state 3, struck by the weapon while idle): 30 frames of
 * the neck shrinking by 2 to 48 and the swing amplitude growing by 2 to
 * 0x100 (frame 1), then state 0 (frame 0). */
static void dragon_recoil(AosEnemy *d) {
    if (d->wd_radius > 0x30) d->wd_radius -= 2;
    if (d->wd_swing <= 0xFF) d->wd_swing += 2;
    if (d->timer++ > 0x1D) {
        d->state = d->step = d->timer = 0;
        d->static_frame = 0;
    }
}

/* sub_08076F20 (state 2, dead, for 120 frames): the neck eases to 48, the
 * swing amplitude grows to 0x100 and its rate to 0x800, a flame at the head
 * every 4 frames and 0x6E every 16; then (step 1) the head goes (0x6B, two
 * pieces 8 pixels behind it, a flame) and its segments fall apart one by
 * one; gone after 50 frames. */
static void dragon_die(AosEnemy *d, uint32_t (*random)(void), AosHitReport *report) {
    if (d->step == 0) {
        if (d->wd_radius <= 0x2F) d->wd_radius++;
        else if (d->wd_radius > 0x30) d->wd_radius--;
        if (d->wd_swing > 0x100) d->wd_swing--;
        else if (d->wd_swing <= 0xFF) d->wd_swing += 4;
        if (d->wd_rate <= 0x7FF) d->wd_rate = (uint16_t)(d->wd_rate + 0x20);
        d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
        dragon_wobble(d);
        dragon_record(d);
        dragon_settle_heading(d);
        if ((d->timer & 3) == 0) dragon_flame(d, d->x, d->y, random, report);
        if ((d->timer & 0xF) == 0) aos_audio_play_song(0x6E);
        if ((int8_t)--d->timer > 0) return;
        d->hidden = true;
        d->step = 1;
        d->timer = 0;
        int32_t off = d->mirrored ? -0x80000 : 0x80000;
        aos_audio_play_song(0x6B);
        dragon_piece(d, d->x + off, d->y, 10, random, report);
        dragon_piece(d, d->x + off, d->y, 11, random, report);
        dragon_flame(d, d->x, d->y, random, report);
        return;
    }
    if (d->timer++ > 0x31) d->removed = true;
}

/* sub_080773AC (the head's hit callback, after the damage and the flash):
 * dead, sound 0x6E, 120 frames of state 2 (frame 1); struck by the weapon
 * (attacker type 3 or 4) while idle, the recoil (state 3, frame 1). */
static void dragon_struck(AosEnemy *d, uint8_t attacker) {
    if (d->hp <= 0) {
        aos_audio_play_song(0x6E);
        d->timer = 0x78;
        d->state = 2;
        d->step = 0;
        d->static_frame = 1;
    } else if (d->state == 0 && (uint8_t)(attacker - 3) <= 1) {
        d->state = 3;
        d->static_frame = 1;
        d->timer = 0;
    }
}

/* EnemyWhiteDragonUpdate: in the homing list on screen, its flash counted
 * down (not while dying); flashing (outside states 2 and 3) it only
 * collides; state 0 idles, 1 attacks, 2 dies, 3 recoils, 4 (inert) only
 * idles; then the collision pass (not in state 4) and the draw callback
 * placing the head. The ten segments are made by its first update
 * (native: at creation). */
static AosHitReport dragon_update(AosEnemy *d, AosSoma *soma, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!d->helper_spawned) {
        d->helper_spawned = true;
        for (int i = 9; i >= 0 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN; --i) {
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_WHITE_DRAGON_PART;
            s.static_frame = -1;
            s.variant = (uint8_t)i;
            s.x = d->x;
            s.y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            play(&s, kind->anims, 1);
            step_anim(&s, kind->anims);
            spawned.children[spawned.child_count++] = s;
        }
        d->spawned_count = spawned.child_count;
    } else if (d->spawned_count < 10 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
        /* The remaining segments, one report later (eight per report). */
        for (int i = 9 - d->spawned_count; i >= 0 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN;
             --i) {
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_WHITE_DRAGON_PART;
            s.static_frame = -1;
            s.variant = (uint8_t)i;
            s.x = d->x;
            s.y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            play(&s, kind->anims, 1);
            step_anim(&s, kind->anims);
            spawned.children[spawned.child_count++] = s;
            d->spawned_count++;
        }
    }
    int sx = (d->x >> 16) - cam_x, sy = (d->y >> 16) - cam_y;
    if (d->state != 2) {
        if (!d->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report.targetable = true;
        if (d->hit_flash) d->hit_flash--;
    }
    bool hold = d->hit_flash && d->state != 3 && d->state != 2;
    if (!hold) {
        switch (d->state) {
        case 0:
        case 4: dragon_idle(d, soma, random); break;
        case 1: dragon_attack(d, soma, random, &spawned); break;
        case 2: dragon_die(d, random, &spawned); break;
        case 3: dragon_recoil(d); break;
        default: break;
        }
    }
    if (d->state != 4 && !d->removed && !d->defeated) {
        bool targetable = report.targetable;
        AosHitReport hit = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        copy_collision_result(&report, &hit);
        report.targetable = targetable;
    }
    aos_combat_tick(&d->combat);
    if (!(d->state == 2 && d->step == 1)) dragon_place_head(d);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_08077458: a segment shares its head's visibility; with the head dead
 * (state 2) segments 0-5 flame every 8 frames for the first (10 - i) * 8,
 * and once the head is gone (step 1) they show again and, every four
 * frames, the next one falls apart into a piece (frame 3); it animates
 * unless the head flashes; sub_080771C8 places it. */
static void dragon_segment_update(AosEnemy *s, const AosEnemyKind *kind,
                                  uint32_t (*random)(void), AosHitReport *report) {
    const AosEnemy *d = s->parent;
    if (!d || d->removed || d->id != AOS_ENEMY_WHITE_DRAGON) {
        s->removed = true;
        return;
    }
    s->hidden = d->hidden;
    int i = s->variant;
    if (d->state == 2 && d->step == 1) {
        s->hidden = false;
        if (i <= 5 && (d->timer & 7) == 0) dragon_flame(d, s->x, s->y, random, report);
        if ((d->timer >> 2) == i + 1) {
            dragon_piece(s, s->x, s->y, 3, random, report);
            s->removed = true;
            return;
        }
    }
    if (d->state == 2 && d->step == 0 && i <= 5 && d->timer < (10 - i) * 8 &&
        (d->timer & 7) == 0)
        dragon_flame(d, s->x, s->y, random, report);
    if (!(d->hp > 0 && d->hit_flash)) step_anim(s, kind->anims);
    dragon_place_segment(s, d);
}

/* sub_08077644: a fireball moves for 6 frames (frame 5), grows for 6
 * (frame 6, box (-5, -5, 10, 10)), slows by 1/6 for 14 (frame 7, box (-8,
 * -8, 16, 16)), fades for 4 (frame 8, the small box) and 4 more (frame 9,
 * no collision); its flips flicker at random. */
static AosHitReport dragon_fireball_update(AosEnemy *b, AosSoma *soma, const AosEnemyKind *kind,
                                           int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    b->mirrored = random() & 1;
    b->vflip = random() & 1;
    switch (b->state) {
    case 0:
    case 1:
        b->x += b->vx;
        b->y += b->vy;
        if (b->timer++ > 5) {
            b->timer = 0;
            b->static_frame++;
            b->own_hurt = b->own_attack =
                b->state == 0 ? (AosBox){-5, -5, 10, 10} : (AosBox){-8, -8, 16, 16};
            b->state++;
        }
        break;
    case 2:
        b->vx = b->vx * 5 / 6;
        b->vy = b->vy * 5 / 6;
        b->x += b->vx;
        b->y += b->vy;
        if (b->timer++ > 0xD) {
            b->timer = 0;
            b->static_frame++;
            b->own_hurt = b->own_attack = (AosBox){-5, -5, 10, 10};
            b->state = 3;
        }
        break;
    case 3:
        if (b->timer++ > 3) {
            b->timer = 0;
            b->static_frame++;
            b->state = 4;
        }
        break;
    default:
        if (b->timer++ > 3) b->removed = true;
        return report;
    }
    report = collide(b, soma, kind, NULL, NULL, 0, soma_def);
    aos_combat_tick(&b->combat);
    return report;
}


/* Bone Pillar (EnemyBonePillarCreate / EnemyBonePillarUpdate, sharing
 * sub_080B3BAC / sub_080B3DCC with the Fish Head): an unseen controller
 * and + 0x30 (1-8) skulls stacked 22 pixels apart, facing alternately,
 * each its own enemy (stats, hit points, rewards). */
