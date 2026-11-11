/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_blue_creaking_skull.h"

#include <stddef.h>

#include "../math/aos_math.h"

/* The 0x080E1AA8 record stream (10-byte records). Table A
 * (0x084F13DC) is {0, 1, NULL}; table B (0x084F13E8) is
 * {0, 2, 3, 4, 5, 6, 7, 0, NULL} — the last entry repeats the first
 * record before the terminator, as in the ROM. */
static const AosCreakingAttack CREAKING_ATTACKS[] = {
    {0x10, 0, 0x0000, {0xE000, 0x2000, 0x5800}, 0x1AA8}, /* 0x080E1AA8 */
    {0x10, 0, 0x0000, {0xE600, 0x1400, 0x5000}, 0x1AB2}, /* 0x080E1AB2 */
    {0x10, 0, 0x0000, {0xF000, 0x4000, 0x8000}, 0x1ABC}, /* 0x080E1ABC */
    {0x18, 0, 0x0000, {0x0800, 0x6000, 0xA000}, 0x1AC6}, /* 0x080E1AC6 */
    {0x10, 0, 0x0000, {0x0C00, 0x6800, 0xA800}, 0x1AD0}, /* 0x080E1AD0 */
    {0x06, 0, 0x0001, {0x0000, 0x5000, 0x8000}, 0x1ADA}, /* 0x080E1ADA */
    {0x0C, 0, 0x0001, {0xF800, 0x0400, 0x0800}, 0x1AE4}, /* 0x080E1AE4 */
    {0x20, 0, 0x0000, {0xF600, 0x0000, 0x0A00}, 0x1AEE}, /* 0x080E1AEE */
};

static const AosCreakingAttack *const CREAKING_TABLE_A[] = {
    &CREAKING_ATTACKS[0], &CREAKING_ATTACKS[1], NULL};
static const AosCreakingAttack *const CREAKING_TABLE_B[] = {
    &CREAKING_ATTACKS[0], &CREAKING_ATTACKS[2], &CREAKING_ATTACKS[3],
    &CREAKING_ATTACKS[4], &CREAKING_ATTACKS[5], &CREAKING_ATTACKS[6],
    &CREAKING_ATTACKS[7], &CREAKING_ATTACKS[0], NULL};

/* table_080E1AFC: per-child orbit radii in pixels (signed halfwords). */
static const int16_t CREAKING_RADII[AOS_CREAKING_SKULL_CHILDREN] = {0, 24, 30};

static int16_t creaking_dir8(int16_t cur, int16_t target) {
    /* sub_08068BC0: +1 when the short way runs up, -1 when it runs
     * down, 0 when equal — on the wrapped 16-bit difference. */
    uint16_t up = (uint16_t)(target - cur);
    uint16_t down = (uint16_t)(cur - target);
    if (cur == target) return 0;
    return up <= down ? -1 : 1;
}

/* The shared orbit base: the low halfword of the record pointer
 * (native_low) minus 0x8000, negated when the parent faces left —
 * the native reads + 0x2C as a halfword even though it holds the
 * record pointer. */
static int16_t creaking_base(const AosCreakingAttack *record, bool facing_left) {
    uint16_t low = record ? record->native_low : 0x1AA8;
    uint16_t base = facing_left ? (uint16_t)(0 - low) : (uint16_t)(low - 0x8000);
    return (int16_t)base;
}

static void creaking_child_a_update(AosCreakingChild *child,
                                    const AosCreakingSkullGuardian *skull,
                                    uint32_t (*random)(void)) {
    const AosCreakingAttack *record = skull->record;
    /* A nonzero parent + 0x30 (record picked this frame) forces the
     * aim state on any child already past its init. */
    if (child->state != 0 && skull->flag_30) child->state = 2;
    child->facing_left = skull->facing_left;
    switch (child->state) {
    case 0: {
        /* sub_0803E654(0): the palette-variant roll; a negative
         * (int16) result deletes the child through the fade path. */
        int16_t roll = (int16_t)(random() & 0xFFFF);
        if (roll < 0) {
            child->dead = true;
            return;
        }
        /* + 0x58 bits 0-4 take the roll; + 0x65 takes the
         * 0x080E1AF8[index] pick {2, 1} — read at draw time. */
        child->variant = (uint8_t)(roll & 0x1F);
        child->angle = (int16_t)record->angles[child->index];
        child->state = 1;
        child->attacking = true;
        break;
    }
    case 1:
        break;
    case 2: {
        int16_t target = (int16_t)record->angles[child->index];
        int dir = creaking_dir8(child->angle, target);
        int16_t frames = (int16_t)record->duration; /* record hw 0 */
        child->vel = (int16_t)((int32_t)(target - dir) / frames);
        child->frames = (uint16_t)frames;
        child->state = 3;
        break;
    }
    case 3:
        child->angle = (int16_t)(child->angle + child->vel);
        if (child->frames) {
            child->frames--;
        } else {
            child->angle = (int16_t)record->angles[child->index];
            child->state = 1;
        }
        break;
    case 4:
        child->dead = true;
        return;
    }
    /* The orbit tail: child 0 sits on the parent, child 1 circles at
     * radius 24 * scale / 0x10000 px on the record_low angle base. */
    if (child->index == 0) {
        child->x = skull->x;
        child->y = skull->y;
    } else {
        int32_t radius = (int32_t)((int64_t)CREAKING_RADII[child->index] *
                                   skull->scale >> 16);
        int16_t base = creaking_base(record, skull->facing_left);
        child->x = skull->x + aos_sine((uint16_t)(base + 0x4000)) * radius;
        child->y = skull->y + aos_sine((uint16_t)base) * radius;
    }
}

static void creaking_child_b_update(AosCreakingChild *child,
                                    const AosCreakingSkullGuardian *skull) {
    /* sub_08030650: the third skull registers its {-16,-16,32,32}
     * contact box once, then orbits only while the parent's + 0x31
     * is nonzero — radius alternating 0x30/0x10 every frame. */
    child->facing_left = skull->facing_left;
    if (child->state == 0) {
        child->state = 1;
        child->attacking = true;
        child->x = skull->x;
        child->y = skull->y;
    }
    if (!skull->field31) {
        child->armed = true;
        return;
    }
    child->armed = false;
    child->tick++;
    int radius = (child->tick & 1) ? 0x30 : 0x10;
    int16_t base = creaking_base(skull->record, skull->facing_left);
    child->x = skull->x + aos_sine((uint16_t)(base + 0x4000)) * radius;
    child->y = skull->y + aos_sine((uint16_t)base) * radius;
}

bool aos_creaking_skull_guardian_update(AosCreakingSkullGuardian *skull,
                                        AosGuardian *guardian, AosStats *stats,
                                        const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                        uint16_t held, uint16_t pressed,
                                        uint32_t *soma_flags, int32_t soma_x,
                                        int32_t soma_y, bool soma_facing_left,
                                        bool empowered, uint32_t (*random)(void)) {
    skull->song_9b = false;
    /* sub_0802DE5C upkeep first; a nonzero result is EntityDelete —
     * the children die with their slot table entries in the native. */
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        for (int k = 0; k < AOS_CREAKING_SKULL_CHILDREN; ++k)
            skull->children[k].dead = true;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    skull->flag_30 = 0;
    /* With + 0xC set the native re-reads gEwramData + 0x1311C
     * [source->+ 0x28]; `empowered` stands in for that slot. */
    if (skull->flag_c && empowered) {
        skull->table = CREAKING_TABLE_B;
        skull->state = 3;
        skull->flag_c = 0;
    }
    /* + 0x42/+ 0x46 copy Soma's pixel position with y - 0x14; + 0x58
     * keeps bit 6 as the inverse of Soma's and clears bit 0. */
    skull->x = soma_x;
    skull->y = soma_y - (0x14 << 16);
    skull->facing_left = !soma_facing_left;
    switch (skull->state) {
    case 0:
        /* The art load (sub_0803B800/0x3C7B4/0x3B924), the + 0x58
         * bit-5 set, the sub_08042A54(2, 0x1E) parameter byte, the
         * + 0x131C2 power 0x28 / + 0x131C4 elements 0 globals and the
         * three sub_0802DD00 child spawns are the caller's/runtime's
         * bookkeeping; the state machine keeps the record pointers. */
        skull->record = &CREAKING_ATTACKS[0];
        skull->table = CREAKING_TABLE_A;
        for (int k = 0; k < AOS_CREAKING_SKULL_CHILDREN; ++k) {
            AosCreakingChild *child = &skull->children[k];
            *child = (AosCreakingChild){.index = (uint8_t)k,
                                        .x = skull->x, .y = skull->y};
        }
        skull->state = 1;
        break;
    case 1:
        skull->scale += 0x1000;
        if (skull->scale > 0xFFFF) skull->state = 2;
        break;
    case 2:
        skull->table = CREAKING_TABLE_A;
        skull->state = 3;
        skull->flag_c = 1;
        break;
    case 3: {
        const AosCreakingAttack *entry = skull->table[skull->index];
        if (!entry) {
            skull->state = 2;
            skull->index = 0;
            break;
        }
        skull->record = entry;
        skull->flag_30 = 1;
        skull->timer = (uint8_t)(entry->duration - 1);
        skull->field31 = (uint8_t)entry->field31;
        skull->state = 4;
        break;
    }
    case 4:
        if (skull->timer) {
            skull->timer--;
            break;
        }
        if (skull->table == CREAKING_TABLE_B && skull->index == 3)
            skull->song_9b = true;
        skull->state = 3;
        skull->index++;
        break;
    }
    for (int k = 0; k < AOS_CREAKING_SKULL_CHILDREN; ++k) {
        AosCreakingChild *child = &skull->children[k];
        if (child->dead) continue;
        if (k < 2)
            creaking_child_a_update(child, skull, random);
        else
            creaking_child_b_update(child, skull);
    }
    return true;
}
