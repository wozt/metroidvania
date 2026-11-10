/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Time Attack checks (see mzm_time_attack.h); comments name
 * the pinned decompilation's routines. */
#include "mzm_time_attack.h"

#include <string.h>

/* hours << 24 | minutes << 16 | seconds << 8 | frames */
static uint32_t timer_value(MzmTimeAttackTimer t) {
    return (uint32_t)t.hours << 24 | (uint32_t)t.minutes << 16 | (uint32_t)t.seconds << 8 | t.frames;
}

bool mzm_time_attack_valid_file(const MzmTimeAttackTimer bosses[4], const uint32_t anti_cheat[4]) {
    uint32_t converted[4];
    for (int i = 0; i < 4; ++i) converted[i] = timer_value(bosses[i]);
    for (int i = 0; i < 4; ++i) {
        /* Every boss beaten in order (an equal time fails, as natively) */
        for (int j = i + 1; j < 4; ++j)
            if (converted[i] >= converted[j]) return false;
        if (converted[i] <= anti_cheat[i]) return false;
    }
    return true;
}

/* The sanity checks of CheckUnlockTimeAttack and TimeAttackCheckSetNewRecord:
 * more than 12 energy, 50 missile, 15 super missile or 9 power bomb tanks,
 * more than 14 or fewer than 8 abilities, or no missile tank. */
static bool abnormal(const MzmTimeAttackItems *items) {
    return items->energy > 12 || items->missiles > 50 || items->super_missiles > 15 ||
           items->power_bombs > 9 || items->abilities > 14 || items->abilities < 8 || items->missiles == 0;
}

uint32_t mzm_time_attack_unlock(uint32_t unlocked, const MzmTimeAttackTimer bosses[4],
                                const uint32_t anti_cheat[4], const MzmTimeAttackItems *items, uint8_t region) {
    if (unlocked & 1 || !mzm_time_attack_valid_file(bosses, anti_cheat) || abnormal(items)) return unlocked;
    uint32_t igt[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        igt[0] += bosses[i].hours;
        igt[1] += bosses[i].minutes;
        igt[2] += bosses[i].seconds;
        igt[3] += bosses[i].frames;
    }
    igt[0] %= 32;
    /* The bits' order of CheckUnlockTimeAttack: {array, bit} per flag */
    static const uint8_t order[29][2] = {
        {3, 7}, {1, 0}, {2, 7}, {3, 6}, {0, 0}, {1, 1}, {2, 6}, {3, 5}, {0, 1}, {2, 5},
        {3, 4}, {1, 2}, {2, 4}, {3, 3}, {0, 2}, {1, 3}, {1, 7}, {2, 3}, {0, 3}, {3, 2},
        {2, 2}, {3, 1}, {1, 4}, {2, 1}, {2, 0}, {3, 0}, {1, 6}, {0, 4}, {1, 5},
    };
    uint8_t flags[32];
    for (int i = 0; i < 29; ++i) flags[i] = (uint8_t)(igt[order[i][0]] >> order[i][1] & 1);
    flags[29] = (uint8_t)(region >> 1 & 1);
    flags[30] = (uint8_t)(region & 1);
    flags[31] = 1;
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        uint32_t value = 0;
        for (int j = 0; j < 8; ++j) value += (uint32_t)flags[i * 8 + j] << (7 - j);
        result += value << (24 - 8 * i);
    }
    return result;
}

/* TimeAttackGetGarbledSeedValue: bit `value` (MSB first) of the seed. */
static uint32_t seed_bit(const uint8_t *seed, int value) {
    int local = value / 8;
    return (uint8_t)(seed[local] << (value - local * 8)) >> 7;
}

/* TimeAttackGarbleSeedValue */
static void set_seed_bit(uint8_t *seed, int value, uint32_t bit) {
    int local = value / 8;
    int shift = 7 - (value - local * 8);
    seed[local] = (uint8_t)((seed[local] & ~(1u << shift)) | (bit & 1) << shift);
}

/* TimeAttackGarbleRandomSeedValues: two bits swapped. */
static void swap_bits(uint8_t *seed, int a, int b) {
    uint32_t ra = seed_bit(seed, a), rb = seed_bit(seed, b);
    set_seed_bit(seed, a, rb);
    set_seed_bit(seed, b, ra);
}

/* TimeAttackEorSeed */
static void eor(uint8_t *seed, int length, uint8_t value) {
    for (int i = 0; i < length; ++i) seed[i] ^= value;
}

/* TimeAttackSeedRandomFunc1 to 5 */
static void random1(uint8_t *seed, int length) {
    int limit = length * 4;
    for (int i = 0; i < limit; i += 2) swap_bits(seed, i, i + limit);
}
static void random2(uint8_t *seed, int length) {
    int limit = length * 4, value = length * 8 - 1;
    for (int i = 0; i < limit; ++i, --value) swap_bits(seed, i, value);
}
static void random3(uint8_t *seed, int length) {
    int limit = length * 8 / 9, value = limit;
    for (int i = 0; i < limit; ++i, value += 8) swap_bits(seed, i, value);
}
static void random4(uint8_t *seed, int length) {
    int value = length;
    for (int i = 0; i < length; ++i, value += 7) swap_bits(seed, i, value);
}
static void random5(uint8_t *seed, int length) {
    int limit = length * 8 / 6, value = limit;
    for (int i = 0; i < limit; ++i, value += 5) swap_bits(seed, i, value);
}

/* TimeAttackSeedShuffleFunc1 to 8 (sTimeAttackSeedShuffleFunctionPointers
 * repeats them for rng 8 to 15) */
static void shuffle(uint8_t *seed, int length, uint8_t rng) {
    switch (rng & 7) {
    case 0:
        eor(seed, length, 70); random1(seed, length); random2(seed, length);
        eor(seed, length, 73); random3(seed, length); random4(seed, length); random5(seed, length);
        break;
    case 1:
        eor(seed, length, 144); random2(seed, length); random1(seed, length); random3(seed, length);
        eor(seed, length, 94); random5(seed, length); random4(seed, length);
        break;
    case 2:
        eor(seed, length, 145); random2(seed, length); random1(seed, length); random3(seed, length);
        eor(seed, length, 227); random5(seed, length); random4(seed, length);
        break;
    case 3:
        eor(seed, length, 89); random5(seed, length); random1(seed, length); random3(seed, length);
        eor(seed, length, 99); random4(seed, length); random1(seed, length); eor(seed, length, 57);
        break;
    case 4:
        eor(seed, length, 149); random1(seed, length); random2(seed, length);
        eor(seed, length, 252); random3(seed, length); random4(seed, length); random5(seed, length);
        break;
    case 5:
        eor(seed, length, 77); random2(seed, length); eor(seed, length, 74); random1(seed, length);
        eor(seed, length, 83); random3(seed, length); random5(seed, length); random4(seed, length);
        break;
    case 6:
        eor(seed, length, 65); random2(seed, length); random1(seed, length); random4(seed, length);
        eor(seed, length, 89); random5(seed, length); random3(seed, length); eor(seed, length, 79);
        random1(seed, length);
        break;
    default:
        eor(seed, length, 84); random3(seed, length); random4(seed, length); random5(seed, length);
        eor(seed, length, 79); random1(seed, length); random2(seed, length); eor(seed, length, 77);
        random4(seed, length); random5(seed, length);
        break;
    }
}

/* TimeAttackRandomizeSeed */
static void randomize(uint8_t *seed, uint32_t rng) {
    seed[12] = (uint8_t)((seed[0] & 0x80) | (seed[4] & 0x40) | (seed[8] & 0x20) | (seed[10] & 0x10));
    uint32_t added = rng * 16;
    seed[0] = (uint8_t)((seed[0] & 0x7F) | (added & 0x80));
    seed[4] = (uint8_t)((seed[4] & 0xBF) | (added & 0x40));
    seed[8] = (uint8_t)((seed[8] & 0xDF) | (added & 0x20));
    seed[10] = (uint8_t)((seed[10] & 0xEF) | (added & 0x10));
}

void mzm_time_attack_password(uint32_t unlocked, MzmTimeAttackTimer igt, const MzmTimeAttackItems *items,
                              bool valid_file, const MzmTimeAttackTimer bosses[4], uint8_t frame8,
                              const uint8_t characters[32], uint8_t password[20]) {
    /* passwordSeed: 88 bits (the native 62-byte array is written past its
     * end and read back) */
    uint8_t bits[88];
    int j = 0;
    const struct { uint32_t value; int count; } fields[] = {
        {unlocked, 32}, {igt.hours, 7}, {igt.minutes, 6}, {igt.seconds, 6}, {items->missiles, 6},
        {items->super_missiles, 5}, {items->power_bombs, 4}, {items->energy, 4},
        {items->abilities >= 14, 1}, {valid_file, 1},
        /* igtAtBosses1 and 2: the frames' low nibbles */
        {(uint32_t)((bosses[0].frames & 15) << 4 | (bosses[2].frames & 15)), 8},
        {(uint32_t)((bosses[3].frames & 15) << 4 | (bosses[1].frames & 15)), 8},
    };
    for (size_t f = 0; f < sizeof fields / sizeof fields[0]; ++f)
        for (int i = 0; i < fields[f].count; ++i) bits[j++] = (uint8_t)(fields[f].value >> i & 1);
    uint8_t seed[16];
    memset(seed, 0, sizeof seed);
    uint32_t sum = 0;
    for (j = 0; j < 11; ++j) {
        uint8_t value = 0;
        for (int i = 0; i < 8; ++i) value = (uint8_t)(value + (bits[j * 8 + i] << i));
        seed[j] = value;
        sum += value;
    }
    seed[11] = (uint8_t)sum;
    uint8_t rng = frame8 & 15;
    shuffle(seed, 12, rng);
    randomize(seed, rng);
    /* TimeAttackGeneratePassword: five bits a character */
    for (int i = 0; i < 20; ++i) {
        uint32_t index = seed_bit(seed, i * 5) << 4 | seed_bit(seed, i * 5 + 1) << 3 |
                         seed_bit(seed, i * 5 + 2) << 2 | seed_bit(seed, i * 5 + 3) << 1 |
                         seed_bit(seed, i * 5 + 4);
        password[i] = characters[index];
    }
}

bool mzm_time_attack_check_record(MzmTimeAttackRecord *record, uint32_t unlocked, MzmTimeAttackTimer igt,
                                  const MzmTimeAttackItems *items, const MzmTimeAttackTimer bosses[4],
                                  const uint32_t anti_cheat[4], uint8_t frame8,
                                  const uint8_t characters[32]) {
    /* Hours, minutes and seconds only */
    uint32_t current = timer_value(igt) & ~0xFFu;
    uint32_t any = timer_value(record->igt) & ~0xFFu, full = timer_value(record->igt100) & ~0xFFu;
    unsigned completion = (unsigned)items->energy + items->missiles + items->super_missiles +
                          items->power_bombs + items->abilities;
    uint8_t records = current < any ? 1 : 0;
    if (completion >= 100 && current < full) records |= 2;
    if (!records) return false;
    bool valid = unlocked & 1 ? mzm_time_attack_valid_file(bosses, anti_cheat) : false;
    if (abnormal(items)) valid = false;
    uint8_t password[20];
    mzm_time_attack_password(unlocked, igt, items, valid, bosses, frame8, characters, password);
    if (records & 1) {
        memcpy(record->password, password, sizeof password);
        record->igt = igt;
    }
    if (records & 2) {
        memcpy(record->password100, password, sizeof password);
        record->igt100 = igt;
    }
    return true;
}
