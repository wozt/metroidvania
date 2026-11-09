/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Skull Archer (enemy 0x05) and its arrows.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_080AF8D0 / sub_080AFA9C: the next volley, in the order 0, 0, 1, 1, 2
 * (+ 0x14 counts 0..4); it plays animation 2 + volley once. */
static void archer_volley(AosEnemy *a, const AosAnimSet *anims) {
    int volley = a->volley_count / 2;
    if (++a->volley_count > 4) a->volley_count = 0;
    a->state = 1;
    a->step = a->substep = 0;
    a->volley = (uint8_t)volley;
    a->shot = 0;
    play_loop(a, anims, (unsigned)volley + 2, false);
}

/* sub_080AF934 step 0: an arrow (sub_080AF7EC) at the archer's position
 * plus the entry's y offset, with its facing and contact power, one hit
 * point, sprite frame 25 and a 2 x 2 box 18 pixels ahead and 24 up for both
 * roles (collision type 0xA). Sound 0x85. */
static AosEnemy make_arrow(const AosEnemy *archer, int8_t y_offset) {
    AosEnemy arrow = {0};
    arrow.id = archer->id;
    arrow.role = AOS_ROLE_ARROW;
    arrow.static_frame = 25;
    arrow.x = archer->x;
    arrow.y = archer->y + (int32_t)((uint32_t)(int32_t)y_offset << 16);
    arrow.mirrored = archer->mirrored;
    arrow.stats = archer->stats;
    arrow.hp = 1;
    arrow.combat.type = 0x0A;
    arrow.own_boxes = true;
    arrow.own_hurt = arrow.own_attack = (AosBox){-18, -24, 2, 2};
    return arrow;
}

/* sub_080AF8D0: standing, it faces the player and shoots when he enters
 * the 240 x 35 box centred on it (sub_0806E29C). */
static void archer_stand(AosEnemy *a, const AosSoma *soma, const AosAnimSet *anims) {
    face_player(a, soma);
    if (a->step == 0 && player_in(soma, (int16_t)((a->x >> 16) - 0x78),
                                  (int16_t)((a->y >> 16) - 0x11), 0xF0, 0x23))
        archer_volley(a, anims);
}

/* sub_080AF934: an arrow at the start of each frame its volley lists, then
 * a 32-frame pause after the animation, then standing or patrolling again. */
static void archer_shoot(AosEnemy *a, const AosEnemyKind *kind, AosHitReport *report) {
    if (a->step == 0) {
        unsigned v = a->volley < 4 ? a->volley : 0;
        if (a->shot < kind->volley_sizes[v] && a->shot < 8 &&
            kind->volleys[v][a->shot][0] == (int8_t)a->anim.frame && a->anim.tick == 0) {
            /* The arrow entity is created, then sound 0x85. */
            report->children[report->child_count++] = make_arrow(a, kind->volleys[v][a->shot][1]);
            aos_audio_play_song(0x85);
            a->shot++;
        }
        if (a->anim.flags & AOS_ANIM_ENDED) {
            a->step = 1;
            a->timer = 0x20;
        }
        return;
    }
    if (a->timer) {
        a->timer--;
        return;
    }
    a->step = a->substep = 0;
    if (!a->param0) {
        a->state = 0;
        play(a, kind->anims, 0);
    } else {
        a->state = 2;
        play(a, kind->anims, 1);
    }
}

/* sub_080AFA9C: patrolling at 0.25 (turning every 129 frames, no ledges,
 * slowed on slopes), backing away at 0.75 facing the player once he is
 * ahead within 79 pixels, and shooting when he is farther than 99. */
static void archer_patrol(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind) {
    if (a->step == 0) {
        if (a->timer++ > 0x80) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
        ground_walk(a, soma, layer, &kind->probes, 0x4000, 0xC);
        if (player_ahead_within(a, soma, 0x4F)) a->step = 1;
        return;
    }
    ground_walk(a, soma, layer, &kind->probes, (int32_t)0xFFFF4000, 0xD);
    int32_t dx = pixel_delta(target_x(soma), a->x), dy = pixel_delta(target_y(soma), a->y);
    if ((int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy)) > 0x63) archer_volley(a, kind->anims);
}

/* sub_080AF7EC: flying at 3.0 toward its facing with the collision pass;
 * stuck to Soma after a hit (state 1, 30 frames; the + 0x0F change at 10
 * frames left is not traced); struck (no hit point), it vanishes (state 2)
 * with a flame of sub_0806D5C0 within 4 x 4 pixels of (x + 8 * vx,
 * y - 22). Deleted beyond screen margin 4. The global pause 0x4BE is not
 * ported. */
static AosHitReport arrow_update(AosEnemy *arrow, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (arrow->state != 2 && arrow->hp <= 0) arrow->state = 2;
    if (outside_margins(kind, 4, (arrow->x >> 16) - cam_x, (arrow->y >> 16) - cam_y)) {
        arrow->removed = true;
        return report;
    }
    switch (arrow->state) {
    case 0:
        arrow->vx = arrow->mirrored ? 0x30000 : -0x30000;
        arrow->x += arrow->vx;
        report = collide(arrow, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        break;
    case 1:
        arrow->x = target_x(soma) + arrow->ax;
        arrow->y = target_y(soma) + arrow->ay;
        if (arrow->timer) arrow->timer--;
        else arrow->removed = true;
        break;
    default:
        spawn_flame_at(arrow->x + arrow->vx * 8, arrow->y - 0x160000, 0x40000, 0x40000, random,
                       &report);
        arrow->removed = true;
        return report;
    }
    aos_combat_tick(&arrow->combat);
    return report;
}

/* A debris piece (sub_0806C5AC) of its parent's kind, playing `anim`. */
static AosEnemy make_debris(const AosEnemy *parent, const AosAnimSet *anims, unsigned anim) {
    AosEnemy d = {0};
    d.id = parent->id;
    d.role = AOS_ROLE_DEBRIS;
    d.static_frame = -1;
    d.x = parent->x;
    d.y = parent->y;
    d.mirrored = parent->mirrored;
    d.combat.attack_off = d.combat.hurt_off = true;
    play_loop(&d, anims, anim, true);
    step_anim(&d, anims);
    return d;
}

/* sub_0806C5AC: a piece keeps 79/80 of vx, adds its accelerations (both
 * clamped to 8.0) and moves; it vanishes when the centre of its sprite's
 * last OAM component, 4 pixels lower (sub_0806C48C), is in a solid cell
 * (sub_080020A0), playing sound 0x162 and leaving a dust puff of sub_08045CEC
 * (animation 4, row 6) at that centre: vy = -0.25 +/- 0x800 with ay = vy / 32,
 * vx = +/- 0x1000 - the piece's vx with ax = -vx / 64 (the random draws are
 * made only when the puff is created; its + 0x0F = 1 is not modelled). It
 * also vanishes when it leaves the screen widened by 32 pixels. It never
 * collides. */
static void debris_update(AosEnemy *d, const AosCollision *layer, const AosEnemyKind *kind,
                          int cam_x, int cam_y, uint32_t (*random)(void), AosHitReport *report) {
    d->vx = d->vx * 79 / 80 + d->ax;
    if (d->ax >= 0 ? d->vx > 0x80000 : d->vx < -0x80000) d->vx = d->ax >= 0 ? 0x80000 : -0x80000;
    d->vy += d->ay;
    if (d->ay >= 0 ? d->vy > 0x80000 : d->vy < -0x80000) d->vy = d->ay >= 0 ? 0x80000 : -0x80000;
    d->x += d->vx;
    d->y += d->vy;
    int ax = 0, ay = 0;
    if (d->static_frame >= 0 && d->static_frame < 64) {
        ax = kind->static_anchors[d->static_frame][0];
        ay = kind->static_anchors[d->static_frame][1];
    } else if (d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES) {
        ax = kind->anchors[d->anim.id][d->anim.frame][0];
        ay = kind->anchors[d->anim.id][d->anim.frame][1];
    }
    if (d->mirrored) ax = -ax;
    if (d->vflip) ay = -ay;
    int32_t px = (d->x >> 16) + ax, py = (d->y >> 16) + ay + 4;
    if (aos_collision_cell(layer, px, py) & 1) {
        AosEffect *dust = spawn_drift(AOS_EFFECT_DUST, d->x + (int32_t)((uint32_t)ax << 16),
                                      d->y + (int32_t)((uint32_t)ay << 16), report);
        if (dust) {
            dust->vy = random_centred(random, 0x1000) - 0x4000;
            dust->ay = dust->vy / 32;
            dust->vx = random_centred(random, 0x2000) - d->vx;
            dust->ax = -dust->vx / 64;
        }
        aos_audio_play_song(0x162);
        d->removed = true;
        return;
    }
    int sx = (d->x >> 16) - cam_x, sy = (d->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x20) > 0x130 || (uint16_t)(sy + 0x20) > 0xE0) d->removed = true;
}

/* sub_080AFB9C: sound 0x6B, then seven pieces (animations 5 to 11) fly away from the
 * attacker at 0.5 to 2.4, rising at 0.5 to 3.5 with gravity 0x2000. */
static void archer_shatter(const AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                           AosHitReport *report) {
    aos_audio_play_song(0x6B);
    for (unsigned anim = 5; anim <= 11 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++anim) {
        AosEnemy d = make_debris(a, kind->anims, anim);
        d.vx = (int32_t)(((random() & 0xF) << 13) + 0x8000) * a->away;
        d.vy = (int32_t)((random() & 0x1F) << 12) + (int32_t)0xFFFC8000;
        d.ay = 0x2000;
        d.timer = (uint8_t)((random() & 0xF) + 0x28);
        report->children[report->child_count++] = d;
    }
}

/* EnemySkullArcherUpdate: inside the activity window, the state, then the
 * collision pass before the animation step; state 3 shatters it into its
 * pieces (sound 0x6B) and deletes it at once. */
static AosHitReport archer_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (a->state) {
    case 0: archer_stand(a, soma, kind->anims); break;
    case 1: archer_shoot(a, kind, &spawned); break;
    case 2: archer_patrol(a, soma, layer, kind); break;
    case 3:
        archer_shatter(a, kind, random, &report);
        a->removed = true;
        return report;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    sx = (a->x >> 16) - cam_x;
    sy = (a->y >> 16) - cam_y;
    if (!a->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    return report;
}
