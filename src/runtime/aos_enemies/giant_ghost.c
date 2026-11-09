/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Giant Ghost (enemy 0x13) and its parts.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_0807E228(e, x, y, vx, vy, frame): a ghost wisp (sub_08069134 with
 * + 0x18 = 0, + 0x19 = 3: the common still frame 0x40 or 0x41 on OBJ row 6,
 * gravity 0x2000, both flips at random, 200 frames). */
static void ghost_wisp(int32_t x, int32_t y, int32_t vx, int32_t vy, bool second,
                       AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    report->effects[report->effect_count++] = (AosEffect){
        .kind = second ? AOS_EFFECT_ROCK_FRAGMENT_1 : AOS_EFFECT_ROCK_FRAGMENT_0,
        .x = x, .y = y, .vx = vx, .vy = vy, .ay = 0x2000, .wisp = true, .timer = 200};
}

/* The Giant Ghost's speed (+ 0x14) rises by 0x1000 to 0xC000 or falls by
 * 0x800 to 0x1000. */
static void giant_speed_up(AosEnemy *g) {
    if (g->speed <= 0xBFFF) g->speed += 0x1000;
}
static void giant_slow_down(AosEnemy *g) {
    if (g->speed > 0x1000) g->speed -= 0x800;
}

/* sub_080694B8 toward the point `above` pixels over its target, optionally
 * reversed (fleeing), then the move (sub_0806D3AC). */
static bool giant_head(AosEnemy *g, const AosSoma *soma, int32_t above, bool flee) {
    bool there = move_toward(g, target_x(soma), target_y(soma) - above, g->speed);
    if (flee) {
        g->vx = -g->vx;
        g->vy = -g->vy;
    }
    g->x += g->vx;
    g->y += g->vy;
    return there;
}

static void giant_flee_start(AosEnemy *g, const AosAnimSet *anims) {
    g->step = 10;
    g->gg_touched = false;
    play(g, anims, 2);
}

/* sub_0807D33C (state 0): hidden off the screen rows; it faces its target
 * and, one time in three with it in the 160 x 160 box around it, charges
 * (state 1); otherwise it drifts 48 pixels over it (speeding up 20 frames,
 * coasting 20, slowing 30); touching Soma (+ 0x21) it flees, up and away,
 * for as long. */
static void giant_drift(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims, int cam_y,
                        uint32_t (*random)(void)) {
    g->hidden = (uint16_t)((g->y >> 16) - cam_y + 0x1C) > 0xDC;
    switch (g->step) {
    case 0: {
        soldier_face(g, soma);
        uint32_t pick = random() % 3;
        if (pick == 0 &&
            player_near(soma, (g->x >> 16) - 0x50, (g->y >> 16) - 0x50, 0xA0, 0xA0)) {
            g->state = 1;
            g->step = 0;
            g->speed = 0;
            play_loop(g, anims, 0, false);
        } else {
            g->timer = 0;
            g->step = 1;
            g->gg_touched = false;
            play_loop(g, anims, 3, false);
        }
        break;
    }
    case 1:
        if (g->gg_touched) {
            giant_flee_start(g, anims);
            break;
        }
        giant_speed_up(g);
        giant_head(g, soma, 0x300000, false);
        if (g->timer++ > 0x13) {
            g->step = 2;
            g->timer = 0;
        }
        break;
    case 2:
    case 12:
        if (g->gg_touched) {
            if (g->step == 2) giant_flee_start(g, anims);
            else {
                g->step = 10;
                g->gg_touched = false;
            }
            break;
        }
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
            play(g, anims, 0);
        }
        break;
    case 3:
    case 13:
        if (g->gg_touched) {
            if (g->step == 3) giant_flee_start(g, anims);
            else {
                g->step = 10;
                g->gg_touched = false;
            }
            break;
        }
        giant_slow_down(g);
        giant_head(g, soma, 0x300000, g->step == 13);
        if (g->timer++ > 0x1D) g->step = g->timer = 0;
        break;
    case 10:
        g->speed = 0;
        g->step = 11;
        g->timer = 0;
        /* fallthrough */
    case 11:
        giant_speed_up(g);
        move_toward(g, target_x(soma), target_y(soma) - 0x300000, g->speed);
        g->vx = -g->vx;
        g->vy = -g->speed;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step = 12;
            g->timer = 0;
        }
        break;
    default: break;
    }
}

/* sub_0807D65C (state 1): it backs away from a point 16 pixels over its
 * target (speeding 20 frames, coasting 20, slowing 30), faces it for 20,
 * then (sound 0x87, animation 3) swoops at 2.0 to 32 pixels over where it
 * was, drifts 45 frames slowing by 1/8 (animation 4) and rests 30
 * (animation 0). */
static void giant_charge(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims) {
    switch (g->step) {
    case 0:
        giant_speed_up(g);
        giant_head(g, soma, 0x100000, true);
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 1:
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 2:
        giant_slow_down(g);
        giant_head(g, soma, 0x100000, true);
        if (g->timer++ > 0x1D) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 3:
        soldier_face(g, soma);
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
            aos_audio_play_song(0x87);
            g->target_x = target_x(soma);
            g->target_y = target_y(soma) - 0x100000;
            play_loop(g, anims, 3, false);
        }
        break;
    case 4:
        if (move_toward(g, g->target_x, g->target_y - 0x200000, 0x20000)) {
            g->step++;
            play(g, anims, 4);
            g->timer = 0;
        }
        g->x += g->vx;
        g->y += g->vy;
        break;
    case 5:
        g->vx = g->vx * 7 / 8;
        g->vy = g->vy * 7 / 8;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x2C) {
            g->step++;
            play_loop(g, anims, 0, false);
            g->timer = 0;
        }
        break;
    case 6:
        if (g->timer++ > 0x1D) {
            g->state = g->step = g->timer = 0;
            g->gg_touched = false;
        }
        break;
    default: break;
    }
}

/* sub_0807D88C (its hit callback, after the damage and the flash): dead,
 * sound 0x6B, animation 5 once, translucent, 90 frames of state 2 and four
 * wisps rising from it. */
static void giant_struck(AosEnemy *g, const AosEnemyKind *kind, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (g->hp > 0) return;
    aos_audio_play_song(0x6B);
    play_loop(g, kind->anims, 5, false);
    g->timer = 0x5A;
    g->state = 2;
    g->step = 0;
    g->translucent = true;
    for (int i = 0; i < 4; ++i) {
        int32_t x = g->x + (int32_t)(random() & 0xFFFFF) - 0x80000;
        int32_t y = g->y - (int32_t)(random() & 0xFFFFF);
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = -0x10000 - (int32_t)(random() & 0x1FFFF);
        ghost_wisp(x, y, vx, vy, random() & 1, report);
    }
}

/* EnemyGiantGhostUpdate: inside the activity window of sub_0806CC20 (only
 * colliding while flashing), state 0 drifts, 1 charges, 2 dies (blinking
 * out over 90 frames, a wisp every eighth while more than 40 are left), 3
 * (the global 0x8E & 0x40) only animates; then the animation step and the
 * collision pass. Its three parts (EnemyGiantGhostCreate's children) are
 * made by its first update. */
static AosHitReport giant_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!g->helper_spawned) {
        g->helper_spawned = true;
        static const uint8_t anims_of[3] = {6, 8, 7};
        for (int i = 0; i < 3; ++i) {
            AosEnemy p = {0};
            p.id = g->id;
            p.role = AOS_ROLE_GIANT_GHOST_PART;
            p.static_frame = -1;
            p.variant = (uint8_t)i;
            p.x = g->x;
            p.y = g->y;
            p.mirrored = g->mirrored;
            p.combat.attack_off = p.combat.hurt_off = true;
            play(&p, kind->anims, anims_of[i]);
            step_anim(&p, kind->anims);
            if (i == 0) p.translucent = true;
            if (g->state == 3 && i > 0) {
                p.target_x = p.x - 0x200000;
                p.target_y = p.y - 0x280000;
            }
            spawned.children[spawned.child_count++] = p;
        }
    }
    if (g->state == 3) {
        step_anim(g, kind->anims);
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    }
    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) {
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    }
    bool flashing = g->hp > 0 && g->hit_flash;
    if (flashing) {
        g->hit_flash--;
    } else {
        switch (g->state) {
        case 0: giant_drift(g, soma, kind->anims, cam_y, random); break;
        case 1: giant_charge(g, soma, kind->anims); break;
        case 2: {
            g->hidden = g->timer < 40 ? kind->blink[g->timer] & 1 : false;
            if ((int8_t)--g->timer <= 0) g->removed = true;
            if (g->timer > 0x28 && (g->timer & 7) == 1) {
                int32_t x = g->x + (int32_t)(random() & 0x1FFFFF) - 0x100000;
                int32_t y = g->y - (int32_t)(random() & 0x1FFFFF);
                int32_t vx = (int32_t)(random() & 0x7FFF) - 0x4000;
                ghost_wisp(x, y, vx, 0, random() & 1, &spawned);
            }
            break;
        }
        default: break;
        }
        step_anim(g, kind->anims);
    }
    if (!g->defeated && !g->removed) {
        AosHitReport hit = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        copy_collision_result(&report, &hit);
        if (hit.soma_hit) g->gg_touched = true;    /* sub_0807E328 */
        if (hit.killed) giant_struck(g, kind, random, &spawned);
        for (int e = 0; e < hit.effect_count && spawned.effect_count < AOS_ENEMY_MAX_EFFECTS; ++e)
            spawned.effects[spawned.effect_count++] = hit.effects[e];
    }
    aos_combat_tick(&g->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* The parts: 0 (sub_0807D9B8) the translucent aura 4 pixels behind and 18
 * up, pulsing its affine scale (+ 0x48 / + 0x4C = sin(+ 0x1C) / 16 +
 * 0x1B000 / 0x1D000, + 0x1C += 0x400 unless the body flashes), an
 * afterimage (sub_0807E074) every fourth frame; dying with the body, it
 * swells (each scale loses 1 / + 0x0D of itself) over 110 frames. 1
 * (sub_0807DBB4) and 2 (sub_0807DE14), the eyes, look at the target from
 * their centres (1, 1 and 4, 4 pixels from the body), 3 x 2 and 2 x 2
 * pixels out (1 pixel behind); under the global 0x8E & 0x40 they look at a
 * point sweeping 60 pixels diagonally. All share the body's visibility and
 * facing and go when it dies. */
static void giant_part_update(AosEnemy *p, const AosSoma *soma, const AosEnemyKind *kind,
                              uint32_t (*random)(void), AosHitReport *report) {
    const AosEnemy *g = p->parent;
    if (p->state == 0 && (!g || g->removed || g->id != AOS_ENEMY_GIANT_GHOST)) {
        p->removed = true;
        return;
    }
    if (p->variant == 0) {
        if (p->state == 0) {
            p->hidden = g->hidden;
            p->mirrored = g->mirrored;
            if (!(g->hp > 0 && g->hit_flash)) {
                p->phase = (uint16_t)(p->phase + 0x400);
                step_anim(p, kind->anims);
                if ((++p->timer & 3) == 0 && report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                    /* sub_0807E074: a 5-frame translucent afterimage. */
                    int32_t dx = (int32_t)(random() & 0x7FFFF) - 0x40000;
                    int32_t dy = (int32_t)(random() & 0x7FFFF) - 0x40000;
                    AosEnemy a = *p;
                    a.role = AOS_ROLE_GIANT_GHOST_TRAIL;
                    a.parent = NULL;
                    a.x = p->x + dx;
                    a.y = p->y + dy;
                    a.timer = 4;
                    a.state = 0;
                    report->children[report->child_count++] = a;
                }
            }
            p->x = AOS_FIXED((g->x >> 16) + (p->mirrored ? -4 : 4)) | (p->x & 0xFFFF);
            p->y = AOS_FIXED((g->y >> 16) - 0x12) | (p->y & 0xFFFF);
            int32_t s = aos_sine(p->phase);
            s = (s < 0 ? s + 0xF : s) >> 4;
            p->affine_sx = s + 0x1B000;
            p->affine_sy = s + 0x1D000;
            if (g->state == 2) {
                p->state = 1;
                p->step = 0;
                p->timer = 0x6E;
            }
            return;
        }
        if (p->step == 0) p->step = 1;
        p->affine_sx -= p->timer ? p->affine_sx / p->timer : 0;
        p->affine_sy -= p->timer ? p->affine_sy / p->timer : 0;
        step_anim(p, kind->anims);
        if ((int8_t)--p->timer <= 0) p->removed = true;
        return;
    }
    p->hidden = g->hidden;
    p->mirrored = g->mirrored;
    bool first = p->variant == 1;
    int32_t eye = first ? 0x10000 : 0x40000;
    int32_t reach_x = first ? 3 : 2;
    int32_t cx, cy, tx, ty;
    if (g->state == 3) {
        int32_t d = p->gg_scan_back ? -0x10000 : 0x10000;
        p->target_x += d;
        p->target_y += d;
        if ((int8_t)++p->gg_scan > 0x3B) {
            p->gg_scan = 0;
            p->gg_scan_back = !p->gg_scan_back;
        }
        cx = g->x - eye;
        cy = g->y - (first ? 0x1E0000 : 0x240000);
        tx = p->target_x;
        ty = p->target_y;
    } else if (!(g->hp > 0 && g->hit_flash)) {
        cx = g->x + (p->mirrored ? eye : -eye);
        cy = g->y - eye;
        tx = target_x(soma);
        ty = target_y(soma);
    } else {
        cx = cy = tx = ty = 0;
    }
    if (g->state == 3 || !(g->hp > 0 && g->hit_flash)) {
        uint16_t a = aos_arctan2((int16_t)trunc16(tx - cx), (int16_t)trunc16(ty - cy));
        p->vx = aos_sine((uint32_t)a + 0x4000) * reach_x + (p->mirrored ? -0x10000 : 0x10000);
        p->vy = aos_sine(a) * 2;
    }
    p->x = g->x + p->vx;
    p->y = g->y + p->vy;
    if (g->state == 2) p->removed = true;
}
