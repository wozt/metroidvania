/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_THROW_H
#define AOS_SKILL_THROW_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_enemy.h"

/* One record of 0x084F12C8 (8 x 24 bytes), the table of
 * Skill_Skeleton_ZombieSoldier_RockArmor_WaiterSkeleton_GiantSkeleton_BomberArmor_Use
 * indexed by the red soul's parameter (entity + 0x1E), exported as
 * metadata/skill_throws.tsv. */
typedef struct {
    int32_t vx, vy;     /* + 0x0, + 0x4: launch speed (vx mirrored facing left) */
    int32_t gravity;    /* + 0x8: + 0x54 */
    uint8_t resource;   /* + 0xC: graphics record of 0x080E13BC */
    uint8_t frame;      /* + 0xD: still frame (flag 1 clear) */
    uint8_t sequence;   /* + 0xE: animation (flag 1 set) */
    uint8_t palette;    /* + 0xF: palette row of sub_0803C7B4 */
    uint16_t song;      /* + 0x10: played at the throw */
    uint16_t flags;     /* + 0x12 */
    uint16_t spin;      /* + 0x14: + 0x2E rotation step (flag 0x80) */
    uint16_t field_16;  /* + 0x16: no traced reader */
} AosThrowRecord;

enum {
    AOS_THROW_RECORDS = 8,
    AOS_THROW_ANIMATED = 0x0001,
    AOS_THROW_ROLL = 0x0004,        /* walks the terrain and bounces */
    AOS_THROW_FUSE = 0x0040,        /* explodes after 0x50 frames (0x20: with a flash) */
    AOS_THROW_SPIN = 0x0080,
    /* The flags this port follows; the others (terrain probe 2, the flash
     * 0x20, shards 0x100, frame boxes 0x200, timers 0x400, the 0x13168
     * link 8, the landing 0x10) are refused. */
    AOS_THROW_PORTED_FLAGS = AOS_THROW_SPIN | AOS_THROW_ROLL | AOS_THROW_FUSE,
    AOS_THROW_FUSE_FRAMES = 0x50,
};

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t gravity;        /* + 0x54 */
    int16_t spin;           /* + 0x2E */
    uint16_t angle;         /* + 0x2C */
    bool facing_left;       /* + 0x58 bit 6 */
    uint8_t state;          /* + 0x0A: 0 created, 1 flying, 2 and 4 exploding,
                             * 3 deleted next */
    bool struck;            /* + 0x20: sub_0802DACC */
    int32_t roll_speed;     /* + 0x24: the record's vx, 0 once a roll stops */
    uint8_t timer;          /* + 0x0D */
    bool attacking;         /* + 0x72 bit 1 clear */
    bool hidden;            /* + 0x58 bit 5 after the explosion */
    bool exploded;          /* the update that ran sub_08045A98(x, y, 0) */
    AosEnemy body;          /* the probe walker's view of a rolling throw */
} AosThrow;

/* Whether this port follows every flag of a record. */
bool aos_throw_supported(const AosThrowRecord *record);
/* The skill entity as sub_08019478 leaves it, placed by sub_08031EA4 at
 * Soma's facing and (8, -0x1A) (whole pixels). */
AosThrow aos_throw_create(int32_t soma_x, int32_t soma_y, bool facing_left);
/* One update of the routine for a supported record: state 0 sets the
 * velocities (mirrored when facing left) and the spin, state 1 moves with
 * the gravity (sub_0806D430 / sub_0806D460, limit 8 pixels a frame), and
 * the tail sends it to state 3 when its moved position is outside the
 * screen margins (mx, my) of entry 4 around the camera (sub_0806D128) and
 * turns it by the spin. Returns false when the entity is deleted (state 3
 * on entry: its slot is released and the count drops). */
bool aos_throw_update(AosThrow *entity, const AosThrowRecord *record, int cam_x, int cam_y,
                      int mx, int my);
/* The same with the terrain a rolling throw (flag 4) walks: state 0 also
 * plays the record's song and arms a fused throw (flag 0x40: 0x50 frames,
 * its attack off); in state 1 the fuse counts down and at 0 explodes it
 * (`exploded`; sound 0xD7, its attack on, hidden, state 2 for 2 frames,
 * then state 4 for 16, then gone); a rolling throw steps the walker
 * sub_0806CAF8 (mode 0, the speed negated) with the gravity, turns at
 * walls and on a floor bounces at -vy / 2 with sound 0x11B when it fell
 * faster than 1.0, else stops rolling. */
bool aos_throw_update_world(AosThrow *entity, const AosThrowRecord *record,
                            const AosCollision *layer, const AosProbes *probes, int cam_x,
                            int cam_y, int mx, int my);
#endif
