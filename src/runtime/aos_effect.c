/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow shared effects; comments name the cvaos routine. */
#include "aos_effect.h"

/* sub_0803F17C with the + 0x59 bit 0 of a particle. */
static int effect_step(AosEffect *effect, const AosAnimSet *anims) {
    bool ended = effect->anim.flags & AOS_ANIM_ENDED;
    int result = aos_anim_step(&effect->anim, anims);
    if (result == 4 || (result == 3 && !ended)) effect->cycle_done = true;
    return result;
}

/* sub_08068F78 */
static bool particle_update(AosEffect *p, const AosAnimSet *anims, const uint8_t *blink) {
    if (!p->started) {
        /* sub_0806D894 / sub_0806D930 / sub_0806D644: start and one step. */
        if (!aos_anim_start(&p->anim, anims, 0, false)) return false;
        p->anim.id = 0;
        effect_step(p, anims);
        p->started = true;
    }
    if (p->flags & 1) p->vx = p->vx * 7 / 8;    /* sub_0806D490(e, 8) */
    if (p->flags & 2) p->vy = p->vy * 7 / 8;    /* sub_0806D4A8(e, 8) */
    /* sub_0806D3D8(e, 8.0) without accelerations */
    if (p->vx > 0x80000) p->vx = 0x80000;
    if (p->vy > 0x80000) p->vy = 0x80000;
    p->x += p->vx;
    p->y += p->vy;
    if (p->flags & 4) {
        if (p->timer <= 0x27) p->timer++;
        int index = 0x28 - p->timer;    /* sub_0806D518 */
        p->hidden = blink && index >= 0 && index <= 0x27 ? blink[index] & 1 : false;
    }
    if (p->cycle_done) return false;
    effect_step(p, anims);
    return true;
}

/* sub_08045C9C: the animation started at creation, then each frame the
 * motion and the step; deleted when the animation ends. */
static bool drift_update(AosEffect *e, const AosAnimSet *anims) {
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, false)) return false;
        e->anim.id = 0;
        e->started = true;
    }
    e->x += e->vx;
    e->vx += e->ax;
    e->y += e->vy;
    e->vy += e->ay;
    int result = aos_anim_step(&e->anim, anims);
    return result != 3 && result != 0;
}

static void clamped_move(int32_t *pos, int32_t *v, int32_t a);

/* sub_0802DA98, used by the four fragments of the Rock Armor soul's
 * landing: both accelerated axes move with the normal 8.0 cap, and the
 * entity ends outside screen-margin entry 4. Its art is a still frame. */
static bool ballistic_update(AosEffect *e, const AosAnimSet *anims,
                             const AosEffectWorld *world) {
    if (!world || !anims) return false;
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, true)) return false;
        e->anim.id = 0;
        e->started = true;
    }
    clamped_move(&e->x, &e->vx, e->ax);
    clamped_move(&e->y, &e->vy, e->ay);
    int sx = (e->x >> 16) - world->cam_x, sy = (e->y >> 16) - world->cam_y;
    /* Exported screen-margin entry 4 is (48, 48). */
    return sx >= -48 && sx <= 48 + 0xF0 && sy >= -48 && sy <= 48 + 0xA0;
}

/* sub_08069134 with + 0x18 = 0 and + 0x19 = 3: the accelerated move
 * (sub_0806D3D8, 8.0), both flips at random, deleted when + 0x0D runs out.
 * Its art is a still frame. */
static bool wisp_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world || !world->random || !anims) return false;
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, true)) return false;
        e->anim.id = 0;
        e->started = true;
    }
    clamped_move(&e->x, &e->vx, e->ax);
    clamped_move(&e->y, &e->vy, e->ay);
    e->mirrored = world->random() & 1;
    e->vflip = world->random() & 1;
    return --e->timer != 0;
}

/* sub_08045000 state 1: the move, then the lifetime. */
static bool timed_update(AosEffect *e, const AosAnimSet *anims) {
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, true)) return false;
        e->anim.id = 0;
        e->started = true;
    }
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
    aos_anim_step(&e->anim, anims);
    return --e->timer != 0;
}

/* sub_080450C8 state 1: type-4 direct sequences move under the same terms
 * as the timed variant but end at their source sequence's final frame. */
static bool direct_update(AosEffect *e, const AosAnimSet *anims) {
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, false)) return false;
        e->anim.id = 0;
        e->started = true;
        /* State 0 calls sub_0803F0E4 to select the initial frame; motion
         * begins on the following state-1 update. */
        return true;
    }
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
    return aos_anim_step(&e->anim, anims) != 3;
}

/* sub_08045E80: hidden through its delay, then moved and animated. */
static bool burst_update(AosEffect *e, const AosAnimSet *anims) {
    if (!e->started) {
        e->hidden = true;
        if (e->timer) {
            e->timer--;
            return true;
        }
        e->hidden = false;
        e->started = true;
        if (!aos_anim_start(&e->anim, anims, 0, false)) return false;
        e->anim.id = 0;
    }
    clamped_move(&e->x, &e->vx, e->ax);
    clamped_move(&e->y, &e->vy, e->ay);
    return aos_anim_step(&e->anim, anims) != 3;
}

/* sub_0806D430 / sub_0806D460 with an 8.0 clamp in the direction of the
 * acceleration, then the move. */
static void clamped_move(int32_t *pos, int32_t *v, int32_t a) {
    *v += a;
    if (a >= 0 ? *v > 0x80000 : *v < -0x80000) *v = a >= 0 ? 0x80000 : -0x80000;
    *pos += *v;
}

/* sub_080228F8 */
static bool casing_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world) return false;
    int sx = (e->x >> 16) - world->cam_x, sy = (e->y >> 16) - world->cam_y;
    if (!e->started) {
        uint8_t cell = aos_collision_cell(world->layer, e->x >> 16, e->y >> 16);
        if ((cell & 8) && (cell & 1)) return false;
        if (!aos_anim_start(&e->anim, anims, 0, true)) return false;
        e->anim.id = 0;
        e->ay = 0x3000;
        e->state = 1;
        e->started = true;
    }
    clamped_move(&e->x, &e->vx, e->ax);
    clamped_move(&e->y, &e->vy, e->ay);
    if (e->state == 1 && (aos_collision_cell(world->layer, e->x >> 16, (e->y >> 16) + 2) & 1)) {
        e->vy = -(e->vy / 2);
        e->vx += (int32_t)(world->random() & 0x3FFF) - 0x2000;   /* sub_0802D5EC(0x4000) */
        e->state = 2;
    }
    sx = (e->x >> 16) - world->cam_x;
    sy = (e->y >> 16) - world->cam_y;
    return (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)(sy + 0x10) <= 0xC0;
}

/* sub_080C9198 */
static bool follow_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world) return false;
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, false)) return false;
        e->anim.id = 0;
        effect_step(e, anims);
        e->mirrored = world->random() & 1;
        e->timer = 0;
        e->started = true;
        return true;
    }
    if ((e->timer & 1) == 0) e->mirrored = !e->mirrored;
    e->x = world->player_x;
    e->y = (int32_t)((uint32_t)(world->player_top + 8) << 16) | (e->y & 0xFFFF);
    e->y += (int32_t)e->timer << 15;
    if (e->cycle_done) return false;
    e->timer++;
    effect_step(e, anims);
    return true;
}

/* sub_08068AD4(e, -16, -16, 0x110, 0xC0): on the screen widened by 16. */
static bool wall_effect_on_screen(const AosEffect *e, const AosEffectWorld *world) {
    int sx = (e->x >> 16) - world->cam_x, sy = (e->y >> 16) - world->cam_y;
    return (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)(sy + 0x10) <= 0xC0;
}

static bool still_frame(AosEffect *e, const AosAnimSet *anims) {
    if (e->started) return true;
    if (!aos_anim_start(&e->anim, anims, 0, true)) return false;
    e->anim.id = 0;
    e->started = true;
    return true;
}

/* sub_0804F8A0 */
static bool debris_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world || !world->random || !still_frame(e, anims)) return false;
    if (e->state == 0) {
        e->vx = (int32_t)((world->random() & 0x1FFF) << 4) - 0x8000;
        if (e->x < world->player_x) e->vx = -e->vx;
        e->vy = -(int32_t)((world->random() & 0x3FFF) << 4);
        e->state = 1;
    } else if (e->state == 1 &&
               (aos_collision_cell(world->layer, e->x >> 16, (e->y >> 16) + 4) & 1)) {
        e->kind = (uint8_t)(AOS_EFFECT_ROCK_FRAGMENT_0 + (world->random() & 1));
        e->vy = -(e->vy >> 1);
        e->state = 2;
    } else {
        e->x += e->vx;
        e->y += e->vy;
        e->vy += 0x3000;
    }
    return wall_effect_on_screen(e, world);
}

/* sub_080503E0 (its creator started and stepped the animation). */
static bool rise_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world || !world->random) return false;
    if (!e->started) {
        if (!aos_anim_start(&e->anim, anims, 0, false)) return false;
        e->anim.id = 0;
        aos_anim_step(&e->anim, anims);
        e->started = true;
    }
    if (e->state == 0) {
        e->vy = -0xC000 - (int32_t)((world->random() & 0xFF) << 4);
        e->state = 1;
    } else {
        e->y += e->vy;
    }
    if (!wall_effect_on_screen(e, world)) return false;
    return aos_anim_step(&e->anim, anims) != 3;
}

/* sub_0804F9B4 */
static bool piece_update(AosEffect *e, const AosAnimSet *anims, const AosEffectWorld *world) {
    if (!world || !world->random) return false;
    if (e->state == 0) {
        e->kind = (uint8_t)(AOS_EFFECT_WALL_PIECE_1 + (world->random() & 1));
        e->vx = (int32_t)((world->random() & 0x1FFF) << 4) + 0x20000;
        e->vy = -0x10000 - (int32_t)((world->random() & 0x3FFF) << 4);
        e->ay = 0x3000;
        e->state = 1;
    }
    /* Both kinds are one still frame: the art passed serves either. */
    if (!still_frame(e, anims)) return false;
    clamped_move(&e->x, &e->vx, e->ax);
    clamped_move(&e->y, &e->vy, e->ay);
    return wall_effect_on_screen(e, world);
}

bool aos_effect_update(AosEffect *effect, const AosAnimSet *anims, const uint8_t *blink,
                       const AosEffectWorld *world) {
    if (effect->debris) return debris_update(effect, anims, world);
    if (effect->rise) return rise_update(effect, anims, world);
    if (effect->piece) return piece_update(effect, anims, world);
    if (effect->casing) return casing_update(effect, anims, world);
    if (effect->follow) return follow_update(effect, anims, world);
    if (effect->particle) return particle_update(effect, anims, blink);
    if (effect->drift) return drift_update(effect, anims);
    if (effect->burst) return burst_update(effect, anims);
    if (effect->timed) return timed_update(effect, anims);
    if (effect->direct) return direct_update(effect, anims);
    if (effect->ballistic) return ballistic_update(effect, anims, world);
    if (effect->wisp) return wisp_update(effect, anims, world);
    if (!effect->started) {
        /* State 0: the graphics set up, the animation played once. */
        aos_anim_start(&effect->anim, anims, 0, false);
        effect->anim.id = 0;
        effect->started = true;
    } else if (effect->kind == AOS_EFFECT_FLAME) {
        effect->y -= 0x8000;
    }
    /* sub_0803F17C: deleted when the animation ends (3). */
    int result = aos_anim_step(&effect->anim, anims);
    return result != 3 && result != 0;
}
