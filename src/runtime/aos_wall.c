/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow breakable walls: see aos_wall.h. */
#include "aos_wall.h"

#include <string.h>

/* 0x08526064: frame, block offset, box size and shape of each entry. */
const AosWallEntry aos_wall_entries[AOS_WALL_ENTRIES] = {
    {0, 0, 0, 32, 64, 1},    {1, 0, 0, 32, 64, 2},    {2, 0, 0, 32, 64, 1},
    {0, 0, 16, 80, 32, 3},   {1, 0, 0, 32, 64, 2},    {1, 0, 0, 32, 96, 2},
    {0, 32, 0, 64, 64, 1},   {1, 32, 0, 64, 64, 1},   {0, 0, -48, 32, 112, 0},
    {0, 0, 0, 32, 80, 1},    {1, 0, 0, 96, 32, 3},    {3, 0, -16, 80, 32, 4},
    {4, 0, 0, 32, 80, 2},    {5, 0, 0, 80, 32, 3},    {3, 0, 0, 32, 64, 2},
    {1, 0, -16, 32, 80, 1},  {0, 0, 0, 80, 32, 3},    {6, 0, 0, 32, 80, 1},
};

bool aos_wall_event(const uint32_t events[AOS_WALL_EVENT_WORDS], int n) {
    int word = (int16_t)n >> 5;
    if (word < 0 || word >= AOS_WALL_EVENT_WORDS) return false;
    return (events[word] >> (n & 31)) & 1;
}

static void set_event(uint32_t events[AOS_WALL_EVENT_WORDS], int n) {
    int word = (int16_t)n >> 5;
    if (word >= 0 && word < AOS_WALL_EVENT_WORDS) events[word] |= 1u << (n & 31);
}

/* sub_0804FCA0 (solid) / sub_0804FE20 (clear): the blocks of the shape,
 * 16 pixels apart, about the wall's position plus the entry's offset. */
static void set_blocks(const AosWall *wall, AosCollision *layer, bool solid) {
    const AosWallEntry *entry = &aos_wall_entries[wall->index];
    int x = (wall->x >> 16) + entry->dx, y = (wall->y >> 16) + entry->dy;
    int half_w = entry->w / 2, half_h = entry->h / 2;
    switch (entry->shape) {
    case 0:
        for (int dx = -half_w; dx < half_w; dx += 16)
            for (int dy = -half_h; dy < half_h; dy += 16)
                aos_collision_set_block(layer, x + dx, y + dy, solid);
        break;
    case 2:
        x -= 16;
        /* fall through */
    case 1:
        for (int dy = -half_h; dy < half_h; dy += 16)
            aos_collision_set_block(layer, x, y - dy, solid);
        break;
    case 3:
    case 4:
        for (int dx = -half_w; dx < half_w; dx += 16)
            aos_collision_set_block(layer, x - dx, y, solid);
        break;
    default:
        break;
    }
}

bool aos_wall_create(AosWall *wall, AosCollision *layer, int x, int y, int param0, int param1,
                     bool object09, const uint32_t events[AOS_WALL_EVENT_WORDS]) {
    memset(wall, 0, sizeof *wall);
    if (param0 < 0 || param0 >= AOS_WALL_ENTRIES) return false;
    /* Object09Create tests its bit whatever it is; Object08Create only a
     * non-zero one. */
    if ((object09 || param1 != 0) && aos_wall_event(events, param1)) return false;
    wall->x = (int32_t)((uint32_t)x << 16);
    wall->y = (int32_t)((uint32_t)y << 16);
    wall->index = (uint8_t)param0;
    wall->event = (int16_t)param1;
    wall->object09 = object09;
    wall->combat.type = AOS_TYPE_ENEMY;
    wall->combat.attack_off = true;       /* + 0x72 bit 1 */
    wall->hp = 3;
    if (layer) set_blocks(wall, layer, true);
    return true;
}

bool aos_wall_weapon_hit(AosWall *wall, const AosWeaponEntity *weapon, const AosSoma *soma,
                         const AosWeaponFrames *frames) {
    if (wall->broken || wall->object09 || wall->event == 0 || wall->state != 0 || !weapon ||
        !frames || !aos_combat_can_take(&wall->combat, AOS_TYPE_WEAPON))
        return false;
    int wx, wy, ww, wh;
    if (!aos_weapon_hitbox(weapon, soma, frames, &wx, &wy, &ww, &wh) || ww <= 1 || wh <= 1)
        return false;
    const AosWallEntry *entry = &aos_wall_entries[wall->index];
    AosBox box = {(int8_t)-(entry->w / 2), (int8_t)-(entry->h / 2), entry->w, entry->h};
    AosRect target = aos_entity_rect(box, wall->x >> 16, wall->y >> 16, false, false);
    if (wx + ww - 1 < target.x1 || target.x2 < wx || wy + wh - 1 < target.y1 || target.y2 < wy)
        return false;
    aos_combat_take(&wall->combat, AOS_TYPE_WEAPON,
                    aos_combat_cooldown(AOS_TYPE_WEAPON, soma->weapon.interval));
    return true;
}

static AosEffect *add_effect(AosWallReport *report, AosEffect effect) {
    if (report->effect_count >= AOS_WALL_MAX_EFFECTS) return NULL;
    report->effects[report->effect_count] = effect;
    return &report->effects[report->effect_count++];
}

/* sub_0802D5EC(n): (random & (n - 1)) - n / 2. */
static int random_centred(uint32_t (*random)(void), uint32_t n) {
    return (int)(random() & (n - 1)) - (int)(n >> 1);
}

static int32_t whole(int pixels) {
    return (int32_t)((uint32_t)pixels << 16);
}

/* sub_0804FA88(wall, dy): three rows 16 pixels apart of three translucent
 * debris (sub_0804F8A0: frames 0x40 to 0x42 at random, both flips at
 * random, priority 0x15) on the wall's x. */
static void debris(const AosWall *wall, int dy, uint32_t (*random)(void),
                   AosWallReport *report) {
    for (int row = -16; row <= 16; row += 16)
        for (int k = 0; k < 3; ++k) {
            AosEffect piece = {.debris = true, .translucent = true};
            piece.kind = (uint8_t)(AOS_EFFECT_ROCK_FRAGMENT_0 + (random() >> 4) % 3);
            piece.x = whole(wall->x >> 16);
            piece.y = whole((wall->y >> 16) + dy + row);
            piece.mirrored = (random() >> 7) & 1;
            piece.vflip = (random() >> 7) & 1;
            if (!add_effect(report, piece)) return;
        }
}

/* sub_0804FB78: every fourth frame of 32 (and the last), a rising dust
 * (sub_080503E0, animation 5, row 6) spread over the shape, both flips at
 * random, translucent. */
static void dust_update(AosWallDust *d, uint32_t (*random)(void), AosWallReport *report) {
    if ((d->timer & 3) == 0) {
        AosEffect dust = {.kind = AOS_EFFECT_SMOKE, .rise = true, .translucent = true};
        dust.x = d->x;
        dust.y = d->y;
        if (d->shape == 1 || d->shape == 2) {
            dust.x += whole(random_centred(random, 0x20));
            dust.y += whole(random_centred(random, 0x40));
        } else if (d->shape == 3 || d->shape == 4) {
            dust.x += whole(random_centred(random, 0x40));
            dust.y += whole(random_centred(random, 0x20));
        }
        dust.mirrored = (random() >> 7) & 1;
        dust.vflip = (random() >> 7) & 1;
        add_effect(report, dust);
    }
    if (d->timer == 0) d->alive = false;
    else d->timer--;
}

/* The break: debris (or, for an Object 0x09 grid, 21 pieces of its own
 * art, sub_0804F9B4, then debris 32 pixels up), sound 0xAB, the blocks
 * cleared, deleted. */
static void wall_break(AosWall *wall, AosCollision *layer, uint32_t (*random)(void),
                       AosWallReport *report) {
    if (!wall->object09 || aos_wall_entries[wall->index].shape != 0) {
        debris(wall, 0, random, report);
    } else {
        for (int col = -1; col <= 1; ++col)
            for (int row = -3; row <= 3; ++row) {
                AosEffect piece = {.kind = AOS_EFFECT_WALL_PIECE_1, .piece = true,
                                   .translucent = true};
                piece.x = whole((wall->x >> 16) - col * 8 + random_centred(random, 4));
                piece.y = whole((wall->y >> 16) - (row * 8 + 0x20) + random_centred(random, 4));
                if (!add_effect(report, piece)) break;
            }
        debris(wall, -32, random, report);
    }
    report->sound = 0xAB;
    report->broke = true;
    if (layer) set_blocks(wall, layer, false);
    wall->broken = true;
}

void aos_wall_update(AosWall *wall, AosCollision *layer, uint32_t events[AOS_WALL_EVENT_WORDS],
                     uint32_t soul_flags, bool struck, uint32_t (*random)(void),
                     AosWallReport *report) {
    if (!wall->broken && (wall->object09 || wall->event != 0)) {
        if ((soul_flags & AOS_WALL_PEEPING_EYE_FLAG) && (++wall->timer & 0xF) == 0)
            add_effect(report, (AosEffect){.kind = AOS_EFFECT_BURST_12, .drift = true,
                                           .x = wall->x, .y = wall->y});
        if (wall->state == 0) {
            if (wall->object09) {
                if (aos_wall_event(events, wall->event)) wall_break(wall, layer, random, report);
            } else if (struck) {
                report->struck = true;
                for (int k = 0; k < AOS_WALL_DUSTS; ++k)
                    if (!wall->dust[k].alive) {
                        wall->dust[k] = (AosWallDust){true, 0x20,
                                                      aos_wall_entries[wall->index].shape,
                                                      wall->x, wall->y};
                        break;
                    }
                if (wall->hp != 0) {
                    report->sound = 0xD3;
                    wall->hp--;
                } else {
                    wall_break(wall, layer, random, report);
                    set_event(events, wall->event);
                }
            }
        }
    }
    for (int k = 0; k < AOS_WALL_DUSTS; ++k)
        if (wall->dust[k].alive) dust_update(&wall->dust[k], random, report);
    aos_combat_tick(&wall->combat);
}

bool aos_wall_alive(const AosWall *wall) {
    if (!wall->broken) return true;
    for (int k = 0; k < AOS_WALL_DUSTS; ++k)
        if (wall->dust[k].alive) return true;
    return false;
}
