/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Peeping Eye (enemy 0x06) and its parts.
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Peeping Eye. A sequence is (frame, ticks) pairs ending at 0xFF: the
 * opening (0x08527400) and the four looks of 0x08527450 (0 blink, 1 hurt,
 * 2 and 3 looking up and down). */
typedef struct {
    uint8_t frame, ticks;
} PeepingStage;

static const PeepingStage peeping_opening[] = {
    {3, 2}, {2, 2}, {1, 2}, {0, 4}, {1, 2}, {2, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 20}, {0xFF, 0},
};
static const PeepingStage peeping_look_blink[] = {
    {0, 96}, {1, 4}, {2, 4}, {3, 6}, {2, 4}, {1, 4}, {0, 20}, {0xFF, 8},
};
static const PeepingStage peeping_look_hurt[] = {
    {8, 2}, {7, 2}, {6, 2}, {4, 2}, {5, 2}, {0, 2}, {8, 2}, {7, 2}, {6, 2},
    {4, 2}, {5, 2}, {0, 2}, {8, 6}, {7, 20}, {8, 7}, {0, 20}, {0xFF, 10},
};
static const PeepingStage peeping_look_up[] = {{10, 96}, {0, 20}, {0xFF, 9}};
static const PeepingStage peeping_look_down[] = {{9, 96}, {0, 20}, {0xFF, 0}};
static const PeepingStage *const peeping_looks[4] = {
    peeping_look_blink, peeping_look_hurt, peeping_look_up, peeping_look_down,
};

/* The angle from the eye's centre (1 and 23 pixels from it) to the target,
 * in whole pixels (truncated toward zero). */
static uint16_t peeping_aim(const AosEnemy *eye, const AosSoma *soma) {
    return aos_arctan2((int16_t)pixel_delta(target_x(soma), eye->x + 0x10000),
                       (int16_t)pixel_delta(target_y(soma), eye->y + 0x170000));
}

/* The opening's tick: true once it ended (the eye open, + 0x18 bit 1, a
 * blink starting at a random tick r & 0x3F). */
static bool peeping_open(AosEnemy *e, uint32_t (*random)(void)) {
    e->pe_flags |= 1;
    if (++e->pe_tick >= peeping_opening[e->pe_index].ticks) {
        e->pe_tick = 0;
        int8_t last = e->pe_index++;
        if (peeping_opening[e->pe_index].frame == 0xFF) {
            e->pe_flags |= 2;
            e->pe_frame = peeping_opening[last].frame;
            e->pe_index = 0;
            e->pe_seq = 0;
            e->pe_tick = (uint8_t)(random() & 0x3F);
            e->pe_hurt = false;
            return true;
        }
    }
    e->pe_frame = peeping_opening[e->pe_index].frame;
    return false;
}

/* A look's tick; a finished one starts again at a random tick. */
static bool peeping_look_tick(AosEnemy *e, uint32_t (*random)(void)) {
    e->pe_flags |= 1;
    const PeepingStage *look = peeping_looks[e->pe_seq];
    if (++e->pe_tick >= look[e->pe_index].ticks) {
        e->pe_tick = 0;
        if (look[++e->pe_index].frame == 0xFF) {
            e->pe_index = 0;
            e->pe_tick = (uint8_t)(random() & 0x3F);
            return true;
        }
    }
    return false;
}

/* sub_08088168 (state 0). Step 0 waits for the target in (x - 90, y - 100,
 * 180, 200) or a first wound (sub_0806D54C); step 1 opens, faces the
 * target and heads 64 pixels short of it, 64 above; step 2 picks a point
 * 16 to 63 pixels to either side of the target and 32 to 79 above it; step
 * 3 looks (a finished blink looks up or down when the target lies in those
 * quarters, any other look blinks again; a wound starts the hurt look,
 * facing the target) and steers: +- 0x800 per axis beyond 4 pixels, the
 * speeds keeping 31/32, moved by sub_0806D3D8(e, 2.0), back to step 2 once
 * within 4 pixels or after 120 frames. */
static void peeping_watch(AosEnemy *e, const AosSoma *soma, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if (player_near(soma, (int16_t)((e->x >> 16) - 90), (int16_t)((e->y >> 16) - 100), 180,
                        200) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            e->timer = 0;
            e->pe_tick = 0;
            e->pe_index = 0;
        }
        return;
    case 1:
        if (!peeping_open(e, random)) return;
        soldier_face(e, soma);
        e->target_x = target_x(soma) + (e->mirrored ? -0x400000 : 0x400000);
        e->target_y = target_y(soma) - 0x400000;
        e->vx = e->vy = 0;
        e->step = 3;
        e->timer = 0;
        return;
    case 2: {
        int32_t side = (int32_t)(random() % 0x300000u) + 0x100000;
        int32_t tx = target_x(soma);
        e->target_x = (random() & 1) ? tx + side : tx - side;
        e->target_y = target_y(soma) - (int32_t)(random() % 0x300000u) - 0x200000;
        e->step = 3;
        e->timer = 0;
    }
        /* fall through */
    default:
        break;
    }
    if (peeping_look_tick(e, random)) {
        if (e->pe_seq == 0) {
            uint16_t a = peeping_aim(e, soma);
            if ((uint16_t)(a - 0x2000) <= 0x4000) e->pe_seq = 2;
            else if ((uint16_t)(a - 0xA000) <= 0x4000) e->pe_seq = 3;
        } else {
            e->pe_seq = 0;
        }
    }
    if (e->pe_hurt) {
        e->pe_hurt = false;
        if (e->pe_seq != 1) {
            soldier_face(e, soma);
            e->pe_seq = 1;
            e->pe_index = 0;
            e->pe_tick = 0;
        }
    }
    e->pe_frame = peeping_looks[e->pe_seq][e->pe_index].frame;
    int32_t dx = e->target_x - e->x, dy = e->target_y - e->y;
    e->ax = dx > 0x40000 ? 0x800 : dx < -0x40000 ? -0x800 : 0;
    e->ay = dy > 0x40000 ? 0x800 : dy < -0x40000 ? -0x800 : 0;
    e->vx = e->vx * 31 / 32;                    /* sub_0806D490(e, 32) */
    e->vy = e->vy * 31 / 32;                    /* sub_0806D4A8(e, 32) */
    accel_move(e, 0x20000);
    if ((dx < 0 ? -dx : dx) <= 0x3FFFF && (dy < 0 ? -dy : dy) <= 0x3FFFF) e->step = 2;
    if (e->timer++ > 0x77) e->step = 2;
}

/* sub_08088510 (state 4, under the global 0x8E & 0x40): 31 frames, the
 * opening, then looks in turn (a finished look blinks again). */
static void peeping_showcase(AosEnemy *e, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if (e->timer++ > 0x1D) e->step = 1;
        return;
    case 1:
        if (peeping_open(e, random)) e->step = 2;
        return;
    case 2:
    case 3:
        if (peeping_look_tick(e, random)) e->pe_seq = 0;
        e->pe_frame = peeping_looks[e->pe_seq][e->pe_index].frame;
        return;
    default:
        return;
    }
}

/* sub_08088638, the hit callback (the damage and flash are collide's): the
 * hurt flag, a 2.0 push away from the attacker (Soma, whose position
 * stands in for the weapon entity) and, at a lethal blow, sound 0x6E and
 * the death (state 2, 45 frames). */
static void peeping_struck(AosEnemy *e, const AosSoma *soma) {
    e->pe_hurt = true;
    uint16_t a = aos_arctan2((int16_t)pixel_delta(e->x, soma->x),
                             (int16_t)pixel_delta(e->y, soma->y));
    e->vx = fixed_mul(aos_sine(a + 0x4000u), 0x20000);
    e->vy = fixed_mul(aos_sine(a), 0x20000);
    if (e->hp <= 0) {
        aos_audio_play_song(0x6E);
        e->timer = 0x2D;
        e->state = 2;
        e->step = e->substep = 0;
    }
}

/* sub_0806D578(x, y, priority): a flame there, mirrored at random. */
static void peeping_flame(int32_t x, int32_t y, uint32_t (*random)(void), AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *flame = &report->effects[report->effect_count++];
    *flame = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = x, .y = y};
    flame->mirrored = (random() & 1u) != 0;
}

/* EnemyPeepingEyeCreate's parts, in its order: the eyeball (frame 0xF,
 * EnemyPeepingEyeCreateEyeball), the pupil (0x10, sub_08088D28) and six
 * segments (sub_08088978: frames 0xB, 0xB, 0xC, 0xC, 0xD, 0xE reaching 12,
 * 10, 8, 8, 7 and 3 pixels, angle (k + 1) * 5000 or, with the creation's
 * r & 1, (6 - k) * 5000, death delay 6 (k + 1), hidden). */
static void peeping_parts(const AosEnemy *eye, AosHitReport *report) {
    static const uint8_t frames[6] = {0xB, 0xB, 0xC, 0xC, 0xD, 0xE};
    static const int32_t reach[6] = {0xC0000, 0xA0000, 0x80000, 0x80000, 0x70000, 0x30000};
    for (int i = 0; i < 8 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy c;
        make_static_child(&c, eye, AOS_ROLE_PEEPING_SEGMENT, i == 6 ? 0xF : i == 7 ? 0x10 : 0);
        c.mirrored = false;
        c.combat.attack_off = c.combat.hurt_off = true;
        if (i == 0 || i == 1) {
            c.variant = (uint8_t)(i + 6);
            c.static_frame = i == 0 ? 0xF : 0x10;
        } else {
            int k = i - 2;
            c.variant = (uint8_t)k;
            c.static_frame = frames[k];
            c.angle = (uint16_t)((eye->pe_reverse ? 6 - k : k + 1) * 5000);
            c.timer = (uint8_t)(6 * (k + 1));
            c.pe_radius = 0;
            c.pe_radius_target = reach[k];
            c.hidden = true;
        }
        report->children[report->child_count++] = c;
    }
}

/* EnemyPeepingEyeUpdate: + 0x18 bit 0 cleared; alive (not dying) beyond
 * sub_0806D128(e, 4) a placed eye (parameter 0 == 0) does nothing, and it
 * is hidden there; the flash counts down (sub_0806AE54) outside states 2
 * and 3. State 0 watches, 1 plays animation 0 once, 2 dies (frame 0,
 * hidden, one flame, 45 frames), 3 is removed on its fourth frame, 4 (the
 * global 0x8E & 0x40) only looks. + 0x18 bit 0 shows the frame + 0x19
 * instead of the animation; then the animation step and, outside states 2
 * and 3 for a placed eye, the collision pass. The hit stun of sub_0806D4C0
 * needs the global pause 0x4BE (taken as 0) and the tracking list of
 * sub_08021924 is not ported. */
static AosHitReport peeping_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!e->helper_spawned) {
        e->helper_spawned = true;
        peeping_parts(e, &spawned);
    }
    e->pe_flags &= (uint8_t)~1u;
    bool outside = outside_margins(kind, 4, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y);
    if (e->state != 2 && e->param0 == 0 && outside) return spawned;
    e->hidden = outside;
    if ((uint8_t)(e->state - 2) > 1 && e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0: peeping_watch(e, soma, random); break;
    case 1:
        if (e->step == 0) {
            play_loop(e, kind->anims, 0, false);
            e->step = 1;
        }
        break;
    case 2:
        e->pe_flags |= 1;
        e->pe_frame = 0;
        e->hidden = true;
        if (e->substep == 0) {
            e->substep = 1;
            peeping_flame(e->x, e->y, random, &spawned);
        }
        if ((int8_t)--e->timer <= 0) {
            e->state = 3;
            e->timer = 0;
        }
        break;
    case 3:
        e->hidden = true;
        if (e->timer++ > 2) e->removed = true;
        break;
    case 4: peeping_showcase(e, random); break;
    default: break;
    }
    e->static_frame = (e->pe_flags & 1) ? e->pe_frame : -1;
    if (e->static_frame < 0) step_anim(e, kind->anims);
    if (e->state != 4 && (uint8_t)(e->state - 2) > 1 && e->param0 == 0 && !e->removed) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* The parts. The eyeball (sub_080888D4) sits on the eye with its
 * visibility; the pupil (sub_08088724) also takes its facing and, once the
 * eye is open, moves its offset 0x4000 a frame toward 3 * (cos, sin) of the
 * angle to the target (exactly onto it within 0x3FFF). Both go when the
 * eye dies. A segment (sub_08088A64), unless the eye flashes
 * (sub_0806ADBC), sways once the eye is open (its angle 500 a frame between
 * 0x800 and 0x7800) while its radius grows by a 64th of its reach; it is
 * shown only while the eye is open. It hangs from the previous segment (the
 * eye for the first) at its angle, pulled toward 0x4000 by a quarter
 * (first), as is (second) or pushed away by a quarter (the others); the
 * tip turns by its angle + half its distance from 0x4000. When the eye
 * dies each segment flames after 6 (k + 1) frames and again 4 frames
 * later, then hides until the eye goes. Hidden beyond sub_0806D128(e, 1). */
static void peeping_part_update(AosEnemy *p, const AosEnemyKind *kind, const AosSoma *soma,
                                int cam_x, int cam_y, uint32_t (*random)(void),
                                AosHitReport *report) {
    AosEnemy *eye = (AosEnemy *)p->parent;
    if (!eye || eye->removed || eye->id != AOS_ENEMY_PEEPING_EYE) {
        p->removed = true;
        return;
    }
    if (p->variant >= 6) {
        if (p->state != 0) {
            p->removed = true;
            return;
        }
        p->hidden = eye->hidden;
        if (p->variant == 6) {
            p->x = eye->x;
            p->y = eye->y;
        } else {
            p->mirrored = eye->mirrored;
            if (!flag_8e_40 && (eye->pe_flags & 2)) {
                uint16_t a = peeping_aim(eye, soma);
                int32_t cx = aos_sine(a + 0x4000u) * 3, cy = aos_sine(a) * 3;
                int32_t d = p->pe_off_x - cx;
                p->pe_off_x = (d < 0 ? -d : d) <= 0x3FFF ? cx : p->pe_off_x + (d > 0 ? -0x4000 : 0x4000);
                d = p->pe_off_y - cy;
                p->pe_off_y = (d < 0 ? -d : d) <= 0x3FFF ? cy : p->pe_off_y + (d > 0 ? -0x4000 : 0x4000);
            }
            p->x = eye->x + p->pe_off_x;
            p->y = eye->y + p->pe_off_y;
        }
        if (eye->state == 2) {
            p->state = 1;
            p->removed = true;
        }
        return;
    }
    unsigned k = p->variant;
    eye->pe_seg[k] = p;
    const AosEnemy *prev = k ? eye->pe_seg[k - 1] : NULL;
    if (!prev || prev->removed || prev->parent != eye || prev->role != AOS_ROLE_PEEPING_SEGMENT)
        prev = eye;
    if (!(eye->hp > 0 && eye->hit_flash)) {
        if (p->step == 0) {
            if (eye->pe_flags & 2) p->step = 1;
        } else if (p->step == 1) {
            p->angle = (uint16_t)(p->angle + 500);
            if ((int16_t)p->angle > 0x7800) {
                p->angle = 0x7800;
                p->step = 2;
            }
        } else if (p->step == 2) {
            p->angle = (uint16_t)(p->angle - 500);
            if ((int16_t)p->angle <= 0x7FF) {
                p->angle = 0x800;
                p->step = 1;
            }
        }
        if (eye->pe_flags & 2) {
            p->hidden = false;
            if (p->pe_radius < p->pe_radius_target) {
                p->pe_radius += p->pe_radius_target / 64;
                if (p->pe_radius >= p->pe_radius_target) p->pe_radius = p->pe_radius_target;
            }
        } else {
            p->hidden = true;
        }
    }
    int32_t a = (int16_t)p->angle;
    int32_t pull = (a - 0x4000) / 4;
    int32_t bent = k == 0 ? a - pull : k == 1 ? a : a + pull;
    p->x = prev->x + fixed_mul(aos_sine((uint32_t)bent + 0x4000u), p->pe_radius);
    p->y = prev->y + fixed_mul(aos_sine((uint32_t)bent), p->pe_radius);
    if (k == 5) p->pe_rotation = (uint16_t)(a + (a - 0x4000) / 2);
    if (p->state == 0 && eye->state == 2) {
        p->state = 1;
        p->hidden = false;
    }
    if (p->state == 1) {
        p->hidden = false;
        if (p->timer-- == 0) {
            peeping_flame(p->x, p->y + 0x40000, random, report);
            p->state = 2;
            p->timer = 4;
        }
    }
    if (p->state == 2) {
        p->hidden = false;
        if (p->timer-- == 0) {
            peeping_flame(p->x, p->y + 0x40000, random, report);
            p->state = 3;
            p->hidden = true;
        }
    }
    if (p->state == 3) {
        p->hidden = true;
        if (eye->state == 3) p->removed = true;
    }
    if (!p->hidden && outside_margins(kind, 1, (p->x >> 16) - cam_x, (p->y >> 16) - cam_y))
        p->hidden = true;
}
