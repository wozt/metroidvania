/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Killer Fish (enemy 0x07), its fry and bubbles.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define accel_move aos_enemy_accel_move
#define aim_at aos_enemy_aim_at
#define box_walk aos_enemy_box_walk
#define bubble_walk aos_enemy_bubble_walk
#define cell_kind aos_enemy_cell_kind
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
#define player_near aos_enemy_player_near
#define soldier_face aos_enemy_soldier_face
#define step_anim aos_enemy_step_anim
#define swim_walk aos_enemy_swim_walk
#define take_effects aos_enemy_take_effects
#define target_in_water aos_enemy_target_in_water
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define water_below aos_enemy_water_below
#define whole aos_enemy_whole

/* sub_080707BC(e, x, y, frame, vx, vy): a bubble (sub_080708B0) facing at
 * random, priority +/- 1 at random, deleted at once out of water. */
static void fish_bubble(const AosEnemy *e, const AosCollision *layer, int32_t x, int32_t y,
                        int frame, int32_t vx, int32_t vy, uint32_t (*random)(void),
                        AosHitReport *report) {
    bool mirrored = random() & 1;
    (void)random();     /* the priority */
    if (cell_kind(layer, x >> 16, y >> 16) != 3) return;
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_BUBBLE;
    b.static_frame = (int16_t)frame;
    b.x = x;
    b.y = y;
    b.vx = vx;
    b.vy = vy;
    b.mirrored = mirrored;
    b.combat.attack_off = b.combat.hurt_off = true;
    report->children[report->child_count++] = b;
}

/* sub_08070A30(e, x, y, frame): a bursting bubble (sub_0807091C) facing at
 * random, priority - 1. */
static void fish_pop(const AosEnemy *e, int32_t x, int32_t y, int frame,
                     uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_BUBBLE_POP;
    b.static_frame = (int16_t)frame;
    b.x = x;
    b.y = y;
    b.mirrored = random() & 1;
    b.combat.attack_off = b.combat.hurt_off = true;
    report->children[report->child_count++] = b;
}

/* sub_0806FA50 (state 0): it swims at 0.25 toward its facing, bobbing by
 * 0x400 between +/- 0.25, turning after 61 frames at the same x, pushed
 * back under the surface; every 120-239 frames a bubble 16 pixels ahead;
 * its target in water within 80 pixels starts the attack. */
static void fish_swim(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                      uint32_t (*random)(void), AosHitReport *report) {
    if (f->step == 0) {
        f->vx = f->mirrored ? 0x4000 : -0x4000;
        f->ax = f->vy = 0;
        f->ay = 0x400;
        f->fish_count = 0;
        f->fish_last_x = f->fish_bubble = 0;
        f->step = 1;
        return;
    }
    if (f->step != 1) return;
    int r = swim_walk(f, layer, 8, -0x70000, 1, 0x40000);
    if (f->vy > 0x4000) f->ay = -0x400;
    else if (f->vy < -0x4000) f->ay = 0x400;
    if (r & 4) f->ay = -0x400;
    if (r & 8) {
        f->vy = 0;
        f->ay = 0x400;
    }
    if (f->fish_last_x == (int16_t)(f->x >> 16)) {
        if (f->fish_count++ > 0x3B) {
            f->mirrored = !f->mirrored;
            f->vx = f->mirrored ? 0x4000 : -0x4000;
            f->fish_count = 0;
            f->fish_last_x = 0;
            f->vy = 0;
            f->ay = 0x400;
        }
    } else {
        f->fish_last_x = (int16_t)(f->x >> 16);
        f->fish_count = 0;
    }
    if (!(r & 0x80)) {
        f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
        f->ay = 0x400;
    }
    if (--f->fish_bubble <= 0) {
        fish_bubble(f, layer, f->x + (f->mirrored ? 0x100000 : -0x100000), f->y, 16, 0, -0x4000,
                    random, report);
        f->fish_bubble = (int16_t)(random() % 0x78 + 0x78);
    }
    if (target_in_water(soma, layer) && player_in_range(f, soma, 0x500000)) {
        f->state = 1;
        f->step = 0;
    }
}

/* Under the surface: pushed back into the water, vy 0, ay 0x400. */
static void fish_resurface(AosEnemy *f, const AosCollision *layer, int r) {
    if (r & 0x80) return;
    f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
    f->vy = 0;
    f->ay = 0x400;
}

/* sub_0806FCA4 (state 1): it faces its target, stops and picks a point
 * within 4 pixels of it, 16 above; after 40 frames it spits (animation 2):
 * four bubbles 10 pixels ahead at 3.0-4.0 forward, then darts at the point
 * at 2.0 for at most 60 frames and drifts 30 more frames slowing by 1/8. */
static void fish_attack(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, uint32_t (*random)(void),
                        AosHitReport *report) {
    switch (f->step) {
    case 0:
        soldier_face(f, soma);
        f->vx = f->ax = f->vy = f->ay = 0;
        f->timer = 0;
        f->fish_tx = (int16_t)((target_x(soma) >> 16) + (int32_t)(random() & 7) - 4);
        f->fish_ty = (int16_t)((target_y(soma) >> 16) - 0x10);
        f->step = 1;
        break;
    case 1:
        if (f->timer++ <= 0x27) break;
        f->timer = 0;
        f->step = 2;
        play_loop(f, kind->anims, 2, false);
        break;
    case 2:
        if (f->anim.frame == 0) break;
        f->timer = 0;
        f->step = 3;
        for (int i = 0; i < 4; ++i) {
            int32_t x, vx;
            if (f->mirrored) {
                x = f->x + 0xA0000;
                vx = (int32_t)(random() & 0xFFFF) + 0x30000;
            } else {
                x = f->x - 0xA0000;
                vx = (int32_t)0xFFFD0000 - (int32_t)(random() & 0xFFFF);
            }
            x = x - 0x20000 + (int32_t)(random() & 0x3FFFF);
            int32_t y = f->y - (int32_t)(random() & 0x3FFFF);
            int32_t vy = (i == 0 ? -0x6000 : -0x4000) - (int32_t)(random() & 0x1FFF);
            fish_bubble(f, layer, x, y, i == 0 ? 15 : 16, vx, vy, random, report);
        }
        break;
    case 3: {
        if (aim_at(f, AOS_FIXED(f->fish_tx), AOS_FIXED(f->fish_ty), 0x20000)) {
            f->step = 4;
            f->timer = 0;
            play(f, kind->anims, 1);
        }
        if (f->timer++ > 0x3B) {
            f->step = 4;
            f->timer = 0;
            play(f, kind->anims, 1);
        }
        fish_resurface(f, layer, swim_walk(f, layer, 8, -0x70000, 1, 0x40000));
        break;
    }
    case 4:
        f->vx = f->vx * 7 / 8;
        f->vy = f->vy * 7 / 8;
        fish_resurface(f, layer, swim_walk(f, layer, 8, -0x70000, 1, 0x40000));
        if (f->timer++ > 0x1D) {
            f->state = f->step = 0;
            play(f, kind->anims, 1);
        }
        break;
    default: break;
    }
}

/* sub_0806FF70 (state 2, dead): upside down it floats up (ay -0x320, at
 * most 0.25 a frame) to the surface (sound 0x6C), then bobs there with
 * bursting bubbles around it, blinking after 30 frames, gone after 70. */
static void fish_float(AosEnemy *f, const AosCollision *layer, const AosEnemyKind *kind,
                       uint32_t (*random)(void), AosHitReport *report) {
    switch (f->step) {
    case 0:
        f->vflip = true;
        f->vx = 1;
        f->ax = f->vy = 0;
        f->ay = -0x320;
        f->step = 1;
        break;
    case 1: {
        int r = swim_walk(f, layer, 0, -0x70000, 1, 0x40000);
        if (f->vy < -0x4000) f->vy = -0x4000;
        if ((r & 0x88) == 0x80) break;
        f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
        f->vx = f->ax = f->vy = 0;
        f->ay = 0x200;
        f->step = 2;
        f->timer = 0;
        aos_audio_play_song(0x6C);
        break;
    }
    case 2: {
        accel_move(f, 0x20000);
        if (f->vy > 0x2000) f->ay = -0x200;
        else if (f->vy < -0x2000) f->ay = 0x200;
        int32_t x = f->x + (int32_t)(random() & 0x1FFFFF) - 0x100000;
        int32_t y = f->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
        fish_pop(f, x, y, 15, random, report);
        if (f->timer++ > 0x1D) {
            int i = 0x46 - f->timer;
            f->hidden = i >= 0 && i <= 0x27 ? kind->blink[i] & 1 : false;
        }
        if (f->timer > 0x45) f->removed = true;
        break;
    }
    default: break;
    }
}

/* sub_08070168: a fry (sub_08070218) at the fish, animation 0 looping, the
 * collision type 8 with the boxes (-4, -4, 8, 8) of 0x0808FCFC, priority
 * +/- 1 at random and a quarter of the fish's contact power. */
static AosEnemy make_fry(const AosEnemy *f, uint32_t (*random)(void)) {
    AosEnemy fry = {0};
    (void)random();     /* the priority */
    fry.id = f->id;
    fry.role = AOS_ROLE_FRY;
    fry.static_frame = -1;
    fry.x = f->x;
    fry.y = f->y;
    fry.stats = f->stats;
    fry.stats.contact = (uint8_t)(f->stats.contact >> 2);
    fry.hp = 1;
    fry.combat.type = AOS_TYPE_ENEMY;
    fry.own_boxes = true;
    fry.own_hurt = fry.own_attack = (AosBox){-4, -4, 8, 8};
    return fry;
}

/* EnemyKillerFishUpdate: outside screen margin 4 (unless dead) it does
 * nothing; hidden beyond it, in the homing list on screen; alive and
 * flashing from a blow it only collides; state 0 swims, 1 attacks, 2
 * floats up dead, 3 is the generic death (a fish out of water, step 0x63),
 * 5 only animates; then the animation step and the collision pass (not
 * while dying). The first update creates its fry (+ 0x32 of them; native:
 * at creation, in water only). */
AosHitReport aos_enemy_fish_update(AosEnemy *f, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (f->state == 5) {
        step_anim(f, kind->anims);
        return report;
    }
    if (!f->helper_spawned) {
        f->helper_spawned = true;
        if (f->step != 0x63)
            for (int i = 0; i < f->param1 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
                spawned.children[spawned.child_count++] = make_fry(f, random);
    }
    int sx = (f->x >> 16) - cam_x, sy = (f->y >> 16) - cam_y;
    bool outside = outside_margins(kind, 4, sx, sy);
    if (f->state != 2 && outside) return spawned;
    f->hidden = outside;
    bool dying = f->state == 2 || f->state == 3;
    if (f->hp > 0 && f->hit_flash) {
        f->hit_flash--;
        if (!dying) report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    } else {
        switch (f->state) {
        case 0: fish_swim(f, soma, layer, random, &spawned); break;
        case 1: fish_attack(f, soma, layer, kind, random, &spawned); break;
        case 2: fish_float(f, layer, kind, random, &spawned); break;
        case 3: generic_death(f, kind, random, &spawned); break;
        case 4: f->removed = true; break;
        default: break;
        }
        dying = f->state == 2 || f->state == 3;
        if (!dying) step_anim(f, kind->anims);
        if (!dying && !f->removed)
            report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    }
    aos_combat_tick(&f->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_080700CC (the fish's hit callback, after the damage and the flash):
 * a lethal blow plays animation 1 once and starts the death, 40 frames
 * (state 2 in water, 3 for a fish out of it). */
void aos_enemy_fish_struck(AosEnemy *f, const AosEnemyKind *kind) {
    if (f->hp > 0) return;
    play_loop(f, kind->anims, 1, false);
    step_anim(f, kind->anims);
    f->timer = 0x28;
    f->state = f->step == 0x63 ? 3 : 2;
    f->step = 0;
}

/* A new heading for a fry: v = 2.0 toward (tx, ty) (whole pixels over the
 * BIOS Sqrt distance). Returns the distance. */
static int fry_head(AosEnemy *y, int32_t tx, int32_t ty) {
    int32_t dx = whole(tx) - (y->x >> 16), dy = whole(ty) - (y->y >> 16);
    int distance = (uint16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
    y->vx = distance ? (dx << 17) / distance : 0;
    y->vy = distance ? (dy << 17) / distance : 0;
    return distance;
}

/* The fry's move: damped by 1/20, the box walker at 3.0, kept under the
 * surface. */
static void fry_move(AosEnemy *y, const AosCollision *layer) {
    y->vx = y->vx * 19 / 20;
    y->vy = y->vy * 19 / 20;
    if (!(box_walk(y, layer, 0x60000) & 0x80)) {
        y->y += (int32_t)((uint32_t)water_below(layer, y->x >> 16, (y->y >> 16) - 3) << 16);
        y->vy = 0;
    }
}

/* sub_08070218: a fry follows its fish (states 1 and 4: a new point within
 * 8 pixels of it every 20-39 frames, at 2.0 slowed by 1/20); its target in
 * water inside the 160 x 64 box around it, or its fish gone (state 4) or
 * dead, starts a frenzy (state 3): five dashes at points near the target
 * (near itself without one or with its fish gone), back to the fish
 * (state 4) once done or after touching Soma. Struck (sub_08070A08), it
 * bursts into a bubble and blinks out over 40 frames (state 2). Outside
 * screen margin 4 (unless dying) it does nothing. */
AosHitReport aos_enemy_fry_update(AosEnemy *y, AosSoma *soma, const AosCollision *layer,
                               const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                               int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    const AosEnemy *fish = y->parent;
    int sx = (y->x >> 16) - cam_x, sy = (y->y >> 16) - cam_y;
    bool outside = outside_margins(kind, 4, sx, sy);
    if (y->state != 2 && outside) return report;
    y->hidden = outside;
    if (fish) {
        if (fish->state == 4) y->fry_parent_gone = true;
        if (fish->hp <= 0 || fish->removed) y->fry_parent_dead = true;
    }
    bool pass = false;
    for (;;) {
        switch (y->state) {
        case 0:
            y->state = 1;
            y->vx = y->vy = y->ax = y->ay = 0;
            y->fish_count = 0;
            y->step = 0;
            /* fallthrough */
        case 1:
        case 4:
            if (y->fry_parent_gone || y->fry_parent_dead) {
                y->state = 3;
                y->step = 0;
                continue;
            }
            if (--y->fish_count <= 0) {
                y->fish_count = (int32_t)(random() % 0x14 + 0x14);
                int32_t fx = fish ? fish->x : y->x, fy = fish ? fish->y : y->y;
                int32_t tx = fx - 0x80000 + (int32_t)(random() & 0xFFFFF);
                int32_t ty = fy - 0x80000 + (int32_t)(random() & 0xFFFFF);
                int distance = fry_head(y, tx, ty);
                if (y->state == 4 && distance <= 9) y->state = 1;
            }
            y->mirrored = y->vx > 0;
            fry_move(y, layer);
            if (y->state != 4 &&
                player_near(soma, (y->x >> 16) - 0x50, (y->y >> 16) - 0x20, 0xA0, 0x40) &&
                target_in_water(soma, layer)) {
                y->state = 3;
                y->step = 0;
            }
            pass = true;
            break;
        case 2:
            y->hidden = y->timer <= 0x27 ? kind->blink[y->timer] & 1 : false;
            if (--y->timer == 0) y->removed = true;
            break;
        case 3:
            if (y->step == 0) {
                y->fry_touched = false;
                y->step = 1;
                y->fish_count = 0;
                y->vx = y->vy = y->ax = y->ay = 0;
                y->timer = 0;
            }
            if (--y->fish_count <= 0) {
                y->timer++;
                y->fish_count = (int32_t)(random() % 0x14 + 0x14);
                int32_t tx = 0, ty = 0;
                if (target_in_water(soma, layer)) {
                    tx = (int32_t)(random() & 0xFFFFF) + target_x(soma) - 0x80000;
                    ty = target_y(soma) - (int32_t)(random() & 0xFFFFF) - 0x100000;
                }
                if (tx == 0 || y->fry_parent_gone) {
                    tx = (int32_t)(random() & 0xFFFFF) + y->x - 0x80000;
                    ty = (int32_t)(random() & 0xFFFFF) + y->y - 0x80000;
                }
                fry_head(y, tx, ty);
            }
            if (y->vx > 0) y->mirrored = true;
            else if (y->vx < 0) y->mirrored = false;
            fry_move(y, layer);
            if (y->timer > 5 || y->fry_touched) y->state = 4;
            pass = true;
            break;
        default: break;
        }
        break;
    }
    if (pass) {
        report = collide(y, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (report.soma_hit) y->fry_touched = true;
        aos_combat_tick(&y->combat);
    }
    if (report.enemy_hit) {
        /* sub_08070A08: no damage; a bursting bubble where it was. */
        fish_pop(y, y->x, y->y, 15, random, &report);
        y->state = 2;
        y->timer = 0x28;
        y->step = 0;
        report.killed = false;
    }
    step_anim(y, kind->anims);
    return report;
}

/* sub_080708B0: a bubble keeps 7/8 of its vx, sways by 0.25 every 8 frames
 * and rises (sub_08069B70, head 2 up) until its head leaves the water or
 * it meets the ceiling. */
void aos_enemy_bubble_update(AosEnemy *b, const AosCollision *layer) {
    if (b->state == 0) {
        b->state = 1;
        b->timer = 0;
    }
    b->timer++;
    b->vx = b->vx * 7 / 8;
    if ((b->timer & 7) == 0) b->vx = (b->timer & 8) ? -0x4000 : 0x4000;
    if ((bubble_walk(b, layer, -0x20000, 1) & 0x88) != 0x80) b->removed = true;
}

/* sub_0807091C: a bursting bubble sways by 0.25 every 8 frames rising at
 * 0.25, shows the previous frame from its sixth update and is gone after
 * twelve. */
void aos_enemy_bubble_pop_update(AosEnemy *b) {
    if (b->state == 0) {
        b->state = 1;
        b->timer = 0;
    }
    b->timer++;
    if ((b->timer & 7) == 0) b->vx = (b->timer & 8) ? -0x4000 : 0x4000;
    b->vy = -0x4000;
    b->x += b->vx;
    b->y += b->vy;
    if (b->timer == 6) b->static_frame--;
    if (b->timer > 0xB) b->removed = true;
}
