#ifndef AOS_OBJECT0F_H
#define AOS_OBJECT0F_H
#include <stdbool.h>
#include <stdint.h>

/* Object0F (sUnk_084F0DF8[15] / sUnk_084F0ED8[15]): the destructible
 * prop. Create reads param0's high byte as the OBJ palette row
 * (default 7 when zero), keeps the low byte, attaches the shared
 * 0x081C15F4/0x0820ED60 resources with animation 0xE, presets the
 * attack box 0x080CF8FA and installs the sub_08050F08 frame callback.
 *
 * Update runs the sub_08068AD4 proximity probe (-0x10, -0x20, 0x130,
 * 0xC0) which sets + 0x58 bit 0x20 while the player is out of range.
 * State 0 waits for sub_08045F9C — the attack-box matcher against the
 * player's + 0x70 slots — to report a hit: PlaySong 0xA2, + 0x58 bit
 * 0x20, + 0xD = 0x28 and state 1. State 1 counts + 0xD down; while it
 * reads above 0x1E on a multiple of four it emits a drifting
 * sub_08045CEC particle jittered by sub_0802D5EC(0x80000) around the
 * entity position with the + 0x5A palette bits; when the u8 counter
 * wraps to 0xFF it sets + 0x59 bit 3, the native done mark. */
typedef struct {
    uint8_t param0;         /* +0x30 low byte kept; high byte = palette row */
    uint8_t palette_row;    /* param0 >> 8, default 7 */
    uint8_t state;          /* +0xA: 0 idle, 1 breaking */
    uint8_t timer;          /* +0xD: 0x28 set on the hit, u8 wrap ends it */
    uint8_t flags58;        /* +0x58: bit 0x20 offscreen mark */
    uint8_t unk5a;          /* +0x5A draw byte: particle palette source */
    bool finished;          /* +0x59 bit 3: the native done mark */
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position */
    /* Outputs consumed by the runtime. */
    int song;               /* PlaySong request, -1 none */
    bool particle;          /* a sub_08045CEC drift was requested */
    int32_t particle_x, particle_y;
    uint8_t particle_palette;
} AosObject0f;

/* Object0FCreate. */
void aos_object0f_create(AosObject0f *o, int param0);

/* Object0FUpdate. `near` is the sub_08068AD4 probe result, `hit` the
 * sub_08045F9C attack-box match, `fx`/`fy` the entity fixed-point
 * position and `rng` the native RandomNumberGenerator for the
 * sub_0802D5EC jitter. */
void aos_object0f_update(AosObject0f *o, bool near, bool hit,
                         int32_t fx, int32_t fy,
                         uint32_t (*rng)(void));
#endif
