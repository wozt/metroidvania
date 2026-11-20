#ifndef AOS_OBJECT0F_H
#define AOS_OBJECT0F_H
#include <stdbool.h>
#include <stdint.h>

/* Object0F (sUnk_084F0DF8[15] / sUnk_084F0ED8[15]): the destructible
 * prop — the breakable lamp/chandelier. Create reads param0's high
 * byte as the OBJ palette row (default 7 when zero), keeps the low
 * byte in + 0x30, attaches the shared 0x081C15F4/0x0820ED60 resources
 * with animation 0xE, presets the attack box 0x080CF8FA, sets + 0x72
 * bit 0x02 and installs the sub_08050F08 frame callback.
 *
 * Update runs the sub_08068AD4 proximity probe (-0x10, -0x20, 0x130,
 * 0xC0) which sets + 0x58 bit 0x20 while the player is out of range.
 * State 0 waits for sub_08045F9C — the attack-box matcher against the
 * player's + 0x70 slots — to report a hit: the sub_08045F9C drop
 * dispatch latches its pickup on the same frame, PlaySong 0xA2 (the
 * native break sound), + 0x58 bit 0x20, + 0xD = 0x28 and state 1.
 * State 1 counts + 0xD down; while it reads above 0x1E on a multiple
 * of four it emits a drifting sub_08045CEC particle jittered by
 * sub_0802D5EC(0x80000) around the entity position with the + 0x5A
 * palette bits; when the u8 counter wraps to 0xFF it sets + 0x59 bit
 * 3, the native done mark. */

/* sub_08045F9C's selector dispatch: + 0x30's low byte picks the drop
 * (param0's low byte for Object0F — every extracted record keeps it
 * 0, so only case 0 runs today), + 0x32 is the drop parameter
 * (param1). The gEwramData + 0x13266 byte reads 0 in the port (no
 * room flag forces selector 0), so case 0's money branch needs only
 * the full-MP-on-Soma gate the caller passes as `mp_full_soma`. */
typedef enum {
    AOS_OBJECT0F_DROP_NONE = 0,
    AOS_OBJECT0F_DROP_HEART,  /* sub_08043C10(x, y, param, 0) */
    AOS_OBJECT0F_DROP_MONEY,  /* sub_08043F70(x, y, variant) */
    AOS_OBJECT0F_DROP_ITEM,   /* sub_08044054(x, y, 2, param) */
} AosObject0fDrop;

typedef struct {
    uint8_t param0;         /* +0x30 low byte kept; high byte = palette row */
    int param1;             /* +0x32: sub_08045F9C drop parameter (s16) */
    uint8_t palette_row;    /* param0 >> 8, default 7 */
    uint8_t state;          /* +0xA: 0 idle, 1 breaking */
    uint8_t timer;          /* +0xD: 0x28 set on the hit, u8 wrap ends it */
    uint8_t flags58;        /* +0x58: bit 0x20 offscreen mark */
    uint8_t unk5a;          /* +0x5A draw byte: particle palette source */
    uint32_t anim_tick;     /* sub_08050F08's per-frame step count —
                             * same drawFunc stepping as the
                             * breakable, kept per entity. */
    bool finished;          /* +0x59 bit 3: the native done mark */
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position */
    /* Outputs consumed by the runtime. */
    int song;               /* PlaySong request, -1 none */
    bool particle;          /* a sub_08045CEC drift was requested */
    int32_t particle_x, particle_y;
    uint8_t particle_palette;
    int drop_kind;          /* AosObject0fDrop latched on the hit —
                             * sub_08045F9C runs on the accepted hit
                             * frame, so the pickup spawns immediately */
    bool drop_big;          /* sub_08043C10's big-heart flag (param != 0) */
    uint8_t drop_variant;   /* sub_08043F70 money variant */
    int drop_item;          /* sub_08044054 case-2 item argument */
    bool burst;             /* one-shot break flourish request on the
                             * hit frame (the shared burst_15 art) */
} AosObject0f;

/* Object0FCreate: `param0` is the record's + 0x30 (palette row in the
 * high byte, drop selector in the low byte), `param1` the + 0x32 drop
 * parameter. */
void aos_object0f_create(AosObject0f *o, int param0, int param1);

/* Object0FUpdate. `near` is the sub_08068AD4 probe result, `hit` the
 * sub_08045F9C attack-box match, `fx`/`fy` the entity fixed-point
 * position, `rng` the native RandomNumberGenerator for the
 * sub_0802D5EC jitter and the case-0 money roll, and `mp_full_soma`
 * sub_08045F9C case 0's money gate: currentMP == maxMP &&
 * currentCharacter == 0. */
void aos_object0f_update(AosObject0f *o, bool near, bool hit,
                         int32_t fx, int32_t fy,
                         uint32_t (*rng)(void), bool mp_full_soma);
#endif
