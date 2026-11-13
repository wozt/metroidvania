#ifndef AOS_OBJECT1F_H
#define AOS_OBJECT1F_H
#include <stdbool.h>
#include <stdint.h>

/* Object1FCreate (0x080564F8) / Object1FUpdate (0x08056224),
 * dispatch index 31 (0x1F).
 *
 * Create chains three loaders: sub_08056514 attaches the
 * 0x082052F0 palette descriptor (opening-actor family) and
 * installs + 0 = sub_08055748 as the entity think callback;
 * sub_08055BE0 spawns eight type-0x1B children (frames
 * 0x08257BD8, animation table 0x08119E14 args 2,1,0xA,
 * + 0x65 = index, + 0x5C by index band, update
 * sub_08055DF8); sub_0805607C spawns two more (args 4,1,0xB,
 * + 0x42 = 0x54 / 0x9C, + 0x46 = 0x80, update sub_08056130).
 * sub_0805632C is the per-state display helper.
 *
 * Object1FUpdate is the dispatch-side three-state gate:
 *  - state 0 (+ 0xA == 0) fires when gEwramData + 0x14 bit
 *    0x40 is set, gEwramData + 0x64 == 1 and the
 *    sub_08068AD4 player probe (box 0x55/0x7F/0x49) passes:
 *    sub_0800C5A8(0), player + 0x5A = (+ 0x5A & ~7) | 2,
 *    gDisplayRegisters + 0x4A = 0x10, + 0x14 = 0x80, then
 *    -> 1.
 *  - state 1 sets gEwramData + 0x42C bit 0 and locks player
 *    input (SetPlayerInput(1, 0)); while + 0xD (s8) is
 *    non-negative it increments and writes + 0x4A =
 *    ((+ 0xD >> 3) << 8) | (0x10 - (+ 0xD >> 3)); on the
 *    negative path it sets player + 0x58 bit 0x20, clears
 *    player + 0x5A bits 0-2 and writes + 0x4A = 0x808. Each
 *    frame + 0x14 decrements; when the old value was 0 the
 *    sequence-end sub_08010978(gEwramData + 0x60) runs and
 *    -> 2.
 *  - state >= 2 is inert.
 *
 * Approximations: the + 0 = sub_08055748 think callback —
 * the nine-state cinematic machine — is NOT ported (it stays
 * an unassigned referrer), nor are the ten children's updates
 * (sub_08055DF8/sub_08056130/sub_0805632C), the
 * sub_08068AD4 probe geometry, the gDisplayRegisters writes
 * (outputs), the gEwramData + 0x42C/+ 0x14/+ 0x64/+ 0x60
 * globals (inputs) and there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xA: 0 gate, 1 sequence, 2 done */
    uint8_t substate;       /* +0xB: think-machine substate */
    uint8_t timer;          /* +0xD: u8 counter; dispatch reads s8 */
    uint16_t frames_left;   /* +0x14: 0x80 countdown */
    uint8_t flags58;        /* +0x58 of the parent entity */
    bool deleted;           /* sprite load failure */
    /* Per-frame outputs, reset by update. */
    int sound;              /* sub_0800C5A8(0) / PlaySong, -1 */
    bool input_lock;        /* SetPlayerInput(1, 0) */
    bool global_42c_bit0;   /* gEwramData + 0x42C |= 1 */
    int display_4a;         /* gDisplayRegisters + 0x4A, -1 none */
    int player_flags_5a;    /* +0x5A mask result, -1 none */
    bool player_flag20;     /* player +0x58 bit 0x20 */
    bool seq_end;           /* sub_08010978(gEwramData + 0x60) */
    unsigned child_count;   /* create-time children (10) */
    /* sub_08055748 think outputs, reset by think. */
    int think_song;         /* PlaySong id, -1 none */
    bool flash_2000;        /* gUnk_03002CB0 bit 0x2000 set/clear */
    bool flags_42c_40;      /* +0x42C bit 0x40 request */
    bool flags_42c_20000;   /* +0x42C bit 0x20000 request */
    bool flags_42c_clr41;   /* +0x42C &= ~0x41 request */
    bool flags_a074_2;      /* A074 bit 1 set request */
    bool flags_a074_clr3;   /* A074 &= ~3 request */
    bool player_5a_20;      /* player +0x5A bit 0x20 window */
    bool player_5a_clr79;   /* player +0x5A &= ~0x79 window */
    bool misc_ops;          /* sub_080115D4/12048/109B8/13C5C/1083C */
} AosObject1F;

/* Object1FCreate. `sprite_ok` is the sub_08056514 load result;
 * the ten children are counted, not spawned. */
void aos_object1f_create(AosObject1F *o, bool sprite_ok,
                         bool *deleted);

/* Object1FUpdate. `gate_open` bundles the three state-0
 * conditions (gEwramData + 0x14 bit 0x40, + 0x64 == 1 and the
 * sub_08068AD4 probe); the + 0xD sign picks the state-1
 * branch internally. */
void aos_object1f_update(AosObject1F *o, bool gate_open);

/* sub_08055748 — the entity + 0 think callback, a nine-state
 * cinematic machine sharing + 0xA/+ 0xB/+ 0xD/+ 0x14 with the
 * dispatch update. `flags_42c_40`/`flags_42c_20000`/
 * `flags_a074_2` are the gEwramData flag bits the machine
 * tests or writes, `s8_3f4` the + 0x3F4 signed byte (the -1
 * compare), `words_differ` the + 0x88 vs + 0x3F8 word
 * compare, `probe_ok` the sub_08021304() result and
 * `player_probe` the sub_08068AD4 gate. */
void aos_object1f_think(AosObject1F *o, bool flags_42c_40,
                        bool probe_ok, bool think_done,
                        bool s8_3f4_neg, bool words_differ);

#endif
