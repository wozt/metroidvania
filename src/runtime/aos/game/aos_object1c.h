#ifndef AOS_OBJECT1C_H
#define AOS_OBJECT1C_H
#include <stdbool.h>
#include <stdint.h>

/* Object1CCreate (0x0805464C) / Object1CUpdate (0x0805474C), SPECIAL
 * id 28 (0x1C): the save point. Twenty identical kind-2 records
 * (persistent_index 1, one per save room) spawn it; the records'
 * x/y are unused because create hardcodes the room-pixel anchor
 * + 0x42 = 0x78 / + 0x46 = 0x50 and then floor-snaps + 0x46 through
 * sub_0806AF98 (collision probes against the gEwramData + 0xA094
 * origin — approximated here by the `ground_y` input).
 *
 * Create resolves the palette descriptor 0x082052E8 through
 * sub_0803AFB8 with a sub_0803B800 bank-0 fallback; a negative
 * result deletes the entity. It then copies OBJ palette rows
 * (sub_0803C7B4(0x0820C384, 0, 4, 0xA)), presets + 0x1A = 0xA,
 * allocates the affine slot + 0x17 through sub_0803E654(1) (a free
 * bit of the gUnk_030019D8 + 0x34 mask, -1 when exhausted) merged
 * into + 0x58 bits 0-4, points the text canvas at 0x06005000
 * (sub_080412DC), sets + 0x5C = 0x11 and spawns the type-0x1B
 * marker child (update sub_080544E8, draw EntityUpdateNothing,
 * frames 0x08257964) whose + 0x10 index lands in + 0x35. Finally
 * sub_08054E38 requests up to two more type-0x1B sparkle children
 * (update sub_08055070, frame records {0, y+2} and {4, y+1} off
 * + 0x1A) — the native attempts are fixed, per-spawn slot failures
 * are invisible to the parent, so the Out counts are requests.
 *
 * Object1CUpdate is a 100-entry jump table (_08054778) on the + 0xA
 * state byte; entries 4-9, 14-19 and 22-98 are dead (tail only)
 * and values above 0x63 skip the dispatch. Every path ends in the
 * shared tail (_08054DB0): SetPlayerInput(1, 0x40) while state != 0,
 * the sub_0803E058 affine registration (slot + 0x17, value + 0x1C)
 * every frame, the + 0xD frame counter, and — when + 0xD % 3 == 0
 * and + 0x31 != 1 — the + 0x34 palette pulse through
 * sub_0803C918(0x08119C10, src 0xA or 2, 2, 2).
 *
 * State 0 is the idle prompt: with Up (input_16 bit 0x40) held and
 * the player inside the fixed sub_08068AD4 rect (0x68, 0x80, 0x20,
 * 0x37 room pixels) + 0xB latches and + 0xD snapshots the old + 0xB;
 * otherwise + 0x42C bit 1 clears. While + 0xB is set each frame
 * waits on the sub_08021304 player-busy gate: idle opens the save
 * screen (song 0xF3, sub_0803D270/sub_0803CED4/sub_080235C8,
 * + 0x42C bit 0, sub_08054280 setup, sub_080213BC, + 0xA074 bits
 * 0-1 cleared, + 0x16 = 1) and advances to state 1; a busy player
 * cancels the prompt when + 0xD reaches 3.
 *
 * State 1 swaps the + 0x35 frame record's OBJ palette rows
 * (0x0820C384 rows 4 -> 0xE) and clears its + 0x58 bits 0/5, then
 * advances to state 2 — the YES/NO save prompt. State 2 reads the
 * state-block button words + 0x14 (r2) and + 0x16 (r3): + 0x16
 * bits 0-1 (A/B) held opens the confirm branch, + 0x14 bit 0 (A)
 * with + 0x16 != 0 commits — sub_08012688/sub_080135C0 refresh the
 * + 0x60 save block and SaveData_SaveSlotToSram writes the + 0x428
 * slot byte. Success (result 0) plays 0xF3, sets state 0x14, + 0x24
 * bit 1, swaps the frame palette to 0x082099FC and resets the text
 * window; failure plays 0xEF, flashes palette 0x0820C428 rows 0-1
 * to 0xD, sets text mode 2 and returns to state 0 through the
 * _08054B44 tail (+ 0xA074 bit 1, + 0xD = 0). Without A/B,
 * + 0x14 bits 0x10/0x20 (Right/Left) move the + 0x16 cursor with
 * song 0xF1; any other confirm press plays 0xF0 and advances to
 * state 3, which restores the 0x082099FC palette, resets the text
 * window and exits to state 0 through the same tail.
 *
 * States 10 and 20 share _08054C24: only while + 0x31 == 1 do they
 * spawn the eight type-0x1B beam children of sub_08054F74 (update
 * sub_0805514C), copy palette 0x08119C10 rows 0xC-0xD to row 2 and
 * advance (10 -> 11, 20 -> 21) with song 0xF6. States 11 and 21 ramp
 * the + 0x1C angle by the + 0x32/+ 0x33 sync bytes (+ 0x800 up to
 * 0x10000 while both are zero, -0xCC down to zero while + 0x33 is
 * set); state 21 additionally seeds its + 0xB substate machine and
 * drives the sub_0803FE64 display writes (regs 0x50/0x54) through
 * substates 1-4, finishing at + 0xA = 0x63 once + 0x32 == 1. State
 * 11 advances to 12 on + 0x32 != 0 (after the 0x3FBF/0 display
 * reset), state 12 sweeps reg 0x54 by + 0xD >> 2 and marks + 0x58
 * bit 0x20 after 0x40 frames, state 13 fades reg 0x54 from 0x10
 * and clears + 0x42C bit 1 into state 99. State 99 re-asserts the
 * 0x082099FC palette, the text reset, + 0x59 bit 3 and + 0xA074
 * bit 1 every frame — the dormant "used" state.
 *
 * The + 0x30..+ 0x33 bytes double as child->parent sync: the
 * sub_08055070 sparkle children write + 0x30/+ 0x31 on expiry and
 * the sub_0805514C beam children write + 0x32/+ 0x33, which is what
 * lets states 10/20/21 finish. Those child routines ARE ported
 * below as a bounded child array on AosObject1C: the type-0x1B
 * marker (draw sub_080544E8), the type-0x1B sparkles (update
 * sub_08055070, draw sub_0803B9D0), the type-0x1B beams (update
 * sub_0805514C spawns type-0x49 sparks through sub_080553A0/
 * sub_08055414, draw sub_0803B9D0; spark update sub_080552C4 swaps
 * its drawFunc to sub_080554BC at state 0) and the unreferenced
 * sub_08054574 fade routine. Native entity + 0x00 is the updateFunc
 * and + 0x04 the drawFunc (sub_08000B64 runs them in separate
 * passes); the port splits them into aos_object1c_children_update
 * and aos_object1c_children_draw. This module replaces the
 * runtime's draw_save_point placeholder (the generic pulsing box at
 * 120,104 in aria_runtime.c); the parent wires the + 0x42/+ 0x46
 * anchor, the + 0xA state and the frame-record palette flags to the
 * draw path.
 *
 * Approximations: the sub_08068AD4 probe is computed from the
 * player + 0x42/+ 0x46 inputs; sub_08021304, sub_0803E654's slot
 * allocation, EntityCreateInRange success, SaveData_SaveSlotToSram's
 * result, the + 0x428 slot byte and the sub_08054280 screen setup
 * are inputs/outputs; sub_0803E058's affine table write, the
 * sub_0803C7B4/sub_0803C918 palette DMAs, the sub_0803FE64 display
 * writes, sub_08040FE0/sub_08041304 text state, the PlaySong calls,
 * the sub_0803D270/sub_0803CED4/sub_080235C8/sub_080213BC screen
 * effects, the + 0x60 save-block refresh and the + 0x42C/+ 0xA074
 * global flag writes are reported, not executed; there is no
 * emulator comparison. Child approximations: sub_0803F2C8 frame
 * sets assume the frame record exists (the native fails silently),
 * the + 0x6D/+ 0x6E animation bytes are advanced by the unported
 * sub_0803F17C/sub_0803EE24 chain and are exposed as child fields
 * the host must drive, sub_080009E4/sub_0806CDB8/sub_0806CDFC and
 * RandomNumberGenerator are injected callbacks, sub_0803AC40
 * commits are reported without its visibility result, and
 * sub_08054574 has no caller in the disassembly dump. */

enum { AOS_O1C_MAX_ROWS = 4 };

/* One palette-row copy: `desc` is the native descriptor address
 * (0x0820C384 / 0x082099FC / 0x08119C10 / 0x0820C428), `src`/`dst`
 * the row indices and `count` the clamped row count. */
typedef struct {
    uint32_t desc;
    uint8_t src, count, dst;
} AosObject1CRow;

/* ------------------------------------------------------------------
 * Child entities. Native entity + 0x00 is updateFunc and + 0x04 is
 * drawFunc (sub_08000B64 calls them in separate passes); each kind
 * below names both. Fields keep native entity offsets in comments.
 * ------------------------------------------------------------------ */
enum {
    AOS_O1C_CHILD_MARKER = 0, /* type-0x1B: draw sub_080544E8,
                               update EntityUpdateNothing */
    AOS_O1C_CHILD_SPARKLE,    /* type-0x1B: update sub_08055070,
                               draw sub_0803B9D0 */
    AOS_O1C_CHILD_BEAM,       /* type-0x1B: update sub_0805514C,
                               draw sub_0803B9D0; spawns type-0x49
                               sparks via sub_080553A0/sub_08055414 */
    AOS_O1C_CHILD_SPARK,      /* type-0x49: update sub_080552C4,
                               draw sub_080554BC (swapped in at
                               state 0; EntityCreateInRange arg is
                               sub_080552C4) */
    AOS_O1C_CHILD_FADE        /* sub_08054574 — no caller found in
                               the disassembly dump */
};

/* Bounded child storage: 1 marker + 2 sparkles + 8 beams + sparks.
 * Native beams spawn a spark every 4th timer tick per beam with a
 * ~21-frame lifetime, so 64 covers the steady state; overflow is
 * the EntityCreateInRange slot-failure analogue (spawn returns -1). */
enum { AOS_O1C_MAX_CHILDREN = 64 };

typedef struct {
    uint8_t kind;           /* AOS_O1C_CHILD_* */
    uint8_t active;         /* 0 = slot free / engine-deleted; the
                               routines only set the + 0x59 bit-3
                               delete mark, deletion is the host's */
    /* Common entity bytes. */
    uint8_t state;          /* +0xA state byte */
    uint8_t substate;       /* +0xB substate (sub_08054574) */
    uint8_t timer;          /* +0xD frame timer / sparkle countdown */
    int32_t field14;        /* +0x14 s32 — sub_08054574 16.16 ramp */
    int16_t field18;        /* +0x18 s16 — spark spawn arg (0) */
    int32_t field1c;        /* +0x1C s32 — spark spin-x velocity */
    int32_t field20;        /* +0x20 s32 — beam: index in low byte;
                               spark: spin-y velocity */
    int32_t field24;        /* +0x24 s32 — beam angle/lifetime */
    int16_t field26;        /* +0x26 s16 — beam state-1 trig scale */
    int32_t field28;        /* +0x28 s32 — beam ramp counter */
    int32_t field2c;        /* +0x2C s32 — beam secondary ramp */
    int32_t pos40;          /* +0x40 s32 16.16 x (spark/beam pos;
                               +0x42 s16 screen x is its high word) */
    int32_t pos44;          /* +0x44 s32 16.16 y (+0x46 high word) */
    int32_t vel48;          /* +0x48 s32 — spark velocity x */
    int32_t vel4c;          /* +0x4C s32 — spark velocity y */
    int32_t acc50;          /* +0x50 s32 — spark accel x (= beam
                               +0x50, copied at spawn) */
    int32_t acc54;          /* +0x54 s32 — spark accel y */
    uint8_t field5a;        /* +0x5A — spark bits 3-6 spawn nibble */
    uint8_t flags58;        /* +0x58 entity flags */
    uint8_t flags59;        /* +0x59: bit 0 engine-set sparkle gate,
                               bit 1 spawn mark (cleared by
                               sub_0803F2C8), bit 3 delete mark */
    uint8_t anim5c;         /* +0x5C animation index */
    uint8_t anim65;         /* +0x65 frame index at draw commits */
    int16_t pos42, pos46;   /* +0x42/+0x46 s16 — marker draw pos
                               (other kinds derive them from the
                               +0x40/+0x44 high words) */
    /* Animation state advanced by the unported sub_0803F17C ->
     * sub_0803EE24 chain; the routines read it but never write it
     * (sub_0803F2C8 resets it on a frame set). The host must drive
     * these — 0 stalls the sparkle state machine. */
    uint8_t anim6d;         /* +0x6D current frame */
    uint8_t anim6e;         /* +0x6E sub-frame */
    int owner;              /* native entity link: marker +0x11 =
                               parent +0x10 index, spark +0x34 =
                               beam entity — stored as a child-array
                               index (marker: env owner, spark:
                               beam index, others: -1) */
} AosObject1CChild;

/* Parent snapshot the native spawn routines read (Object1CCreate's
 * EntityCreateInRange tail, sub_08054E38, sub_08054F74,
 * sub_08055414). */
typedef struct {
    int16_t palette;        /* parent +0x18 s16 -> sub_0803B924 */
    int16_t anim_1a;        /* parent +0x1A s16 (record word1 base) */
    int32_t pos40, pos44;   /* parent +0x40/+0x44 16.16 position */
    uint8_t affine_slot;    /* parent +0x17 raw -> beam +0x58 & 0x1F */
    int owner;              /* parent +0x10 entity index -> marker
                               child +0x11 (informational) */
} AosObject1CChildEnv;

/* Injected engine probes for the child update pass. */
typedef struct {
    /* sub_080009E4 angle lookup used by beam states 0/1; arg is the
     * native shifted angle (beam_index << 13, + field28 in state 1,
     * + 0x4000 for the quadrature call). NULL -> 0 (documented:
     * beam positions collapse to the constant offsets). */
    int32_t (*trig)(uint32_t angle, void *ctx);
    void *trig_ctx;
    /* sub_0806CDB8/sub_0806CDFC focus-entity +0x42/+0x46 s16 room
     * pixels, read by beam state 1. */
    int16_t focus_x, focus_y;
    /* RandomNumberGenerator for sub_080553A0/sub_08055414 spark
     * spawns. NULL -> 0. */
    uint32_t (*rng)(void *ctx);
    void *rng_ctx;
} AosObject1CChildrenIn;

enum { AOS_O1C_CHILD_MAX_COMMITS = 32, AOS_O1C_CHILD_MAX_FX = 16 };

/* One sub_0803AC40 sprite-position commit (drawFunc phase). */
typedef struct {
    uint8_t child;          /* index into o->children */
    int16_t x, y;           /* committed +0x42/+0x46 screen pos */
    uint8_t frame;          /* +0x65 at commit time */
} AosObject1CCommit;

/* One sub_0803F2C8 frame-set request. */
typedef struct {
    uint8_t child;
    uint8_t frame;          /* +0x6D arg */
} AosObject1CFrameSet;

/* Child engine effects. Update/draw entry points clear it; spawn
 * helpers only accumulate (pass NULL to skip recording). Counts
 * are capped at the array sizes, matching the palettes convention. */
typedef struct {
    unsigned commits;       /* sub_0803AC40 calls (draw pass) */
    AosObject1CCommit commit[AOS_O1C_CHILD_MAX_COMMITS];
    unsigned frame_sets;    /* sub_0803F2C8 requests (update pass) */
    AosObject1CFrameSet frame_set[AOS_O1C_CHILD_MAX_FX];
    unsigned anim_advances; /* sub_0803F17C calls */
    unsigned frame_inits;   /* sub_0803B924 frame-record inits */
    unsigned pal_resolves;  /* sub_0803AFB8 resolves (spark state 0;
                               result is the parent's own 0x082052E8
                               resolution, o->palette) */
    int pal_index;          /* last sub_0803AFB8 result, -1 none */
    unsigned spark_attempts;/* sub_08055414 EntityCreateInRange
                               attempts via beam sub_080553A0 */
    unsigned sparks_spawned;/* ... that found a slot */
    unsigned obj_palettes;  /* sub_0803C7B4 copies (sub_08054574) */
    AosObject1CRow obj_rows[AOS_O1C_MAX_ROWS];
    unsigned fade_finished; /* sub_08054574 routines returning 1 */
} AosObject1CChildrenOut;

typedef struct {
    /* Entity bytes. */
    uint8_t state;          /* +0xA: 0 prompt, 1 open, 2 confirm,
                               3 decline, 10/20 beam gate, 11/21
                               spin, 12/13 fade, 99 dormant */
    uint8_t prompt;         /* +0xB: Up latch / state-21 substate */
    uint8_t timer;          /* +0xD: frame counter, prompt cancel,
                               substate timer */
    uint8_t save_sel;       /* +0x16: YES/NO cursor (1 = save) */
    uint8_t sync30;         /* +0x30: child sync (param0 low) */
    uint8_t sync31;         /* +0x31: beam-enable (param0 high) */
    uint8_t sync32;         /* +0x32: beam direction / completion */
    uint8_t sync33;         /* +0x33: beam spin direction */
    int32_t angle;          /* +0x1C: affine value, ramped by
                               states 11/21 */
    uint16_t flags24;       /* +0x24: bit 1 set on save commit */
    uint8_t affine_slot;    /* +0x17: sub_0803E654 slot, & 0x1F */
    int16_t palette;        /* +0x18: sub_0803AFB8 result */
    uint8_t anim_1a;        /* +0x1A = 0xA */
    uint8_t frame_rec;      /* +0x35: marker child's frame-record
                               index (+0x10), 0 when unspawned */
    uint8_t pulse;          /* +0x34: palette pulse toggle */
    int16_t pos_x;          /* +0x42 = 0x78 */
    int16_t pos_y;          /* +0x46: 0x50 floor-snapped */
    int32_t pos40;          /* +0x40 s32 16.16 x — approximated as
                               pos_x << 16 at create (the native
                               EntityCreateInRange seeds it from the
                               spawn record); beam state 1 subtracts
                               it from the focus-entity delta */
    int32_t pos44;          /* +0x44 s32 16.16 y, same approx */
    uint8_t flags58;        /* +0x58: bits 0-4 affine slot, bit 5
                               set by state 12 */
    uint8_t flags59;        /* +0x59: bit 3 set at state 99 */
    uint8_t frame_flags;    /* +0x58 of the +0x35 frame record:
                               bits 0/5 cleared (state 1), bit 5
                               set (states 2/3/99) */
    uint8_t anim;           /* +0x5C = 0x11 */
    bool deleted;           /* create palette failure */
    /* Child entities spawned by create (marker + sparkles) and by
     * states 10/20 (beams -> sparks). Native children occupy later
     * entity slots and update after the parent, so their sync-byte
     * writes land on the parent's NEXT update — call
     * aos_object1c_children_update after aos_object1c_update to
     * match. */
    AosObject1CChild children[AOS_O1C_MAX_CHILDREN];
    unsigned child_count;   /* high-water slot count; spawn helpers
                               reuse !active slots first */
} AosObject1C;

typedef struct {
    /* sub_0803E654(1) affine slot result (-1 = mask exhausted,
     * stored raw as 0xFF; only the + 0x58 merge masks to 5 bits). */
    int affine_slot;
    /* Final sub_0803AFB8(0x082052E8) palette index after the
     * sub_0803B800 bank-0 fallback; < 0 deletes the entity. */
    int palette_index;
    int16_t param0;         /* entity +0x30 s16 -> sync30/sync31 */
    int16_t param1;         /* entity +0x32 s16 -> sync32/sync33 */
    /* +0x46 after the sub_0806AF98 floor snap; pass 0x50 when no
     * collision probe is available. */
    int16_t ground_y;
    /* EntityCreateInRange(0x1B, 0x48) outcomes: the marker child
     * and the up-to-two sub_08054E38 sparkle children (native
     * attempts entries {0, y+2} and {4, y+1}; 0-2 successes). */
    bool marker_spawned;
    unsigned sparkle_spawned;
    /* The marker child's +0x10 frame-record index (only stored
     * when marker_spawned). */
    uint8_t frame_record;
    /* Parent +0x10 entity index, copied to the marker child's
     * +0x11 by the native create tail (informational only — the
     * ported children reach the parent implicitly). */
    uint8_t entity_index;
} AosObject1CCreateIn;

typedef struct {
    bool deleted;
    bool marker_child;          /* type-0x1B child, sub_080544E8 */
    unsigned sparkle_children;  /* type-0x1B children requested by
                                   sub_08054E38 (<= 2) */
    bool canvas_set;            /* sub_080412DC(0x06005000) */
} AosObject1CCreateOut;

typedef struct {
    /* Player entity +0x42/+0x46 room pixels for the fixed
     * sub_08068AD4(0x68, 0x80, 0x20, 0x37) probe. */
    int16_t player_x, player_y;
    /* State-block button words +0x14 (r2) and +0x16 (r3). Which
     * is the edge word is not confirmed; observed bits: +0x16
     * bit 0x40 Up, bits 0-1 A/B; +0x14 bit 0 A, bits 0x10/0x20
     * Right/Left. */
    uint16_t buttons_14, buttons_16;
    /* sub_08021304() result: nonzero while the player entity is
     * busy (state, |vy| > 0x8000 or +0x10 & 0x30000120). */
    bool player_busy;
    /* gEwramData +0x428 SRAM slot byte and the
     * SaveData_SaveSlotToSram result for the frame the write
     * fires (0 = success). */
    uint8_t save_slot;
    int save_result;
} AosObject1CIn;

typedef struct {
    int song;               /* PlaySong id, -1 none */
    int forced_keys;        /* SetPlayerInput(1, arg): -1 none,
                               else the arg (0x40 when state != 0) */
    uint32_t flags42c_set;  /* bits OR'd into gEwramData +0x42C */
    uint32_t flags42c_clear;/* bits ANDNOT'd from +0x42C */
    uint8_t save_mode_set;  /* bits OR'd into +0xA074 byte */
    uint8_t save_mode_clear;/* bits ANDNOT'd from +0xA074 */
    /* Palette copies: `palettes` are sub_0803C918 rows into
     * gEwramData +0x1261C, `obj_palettes` sub_0803C7B4 rows into
     * +0x11DDC (also sets unk_123E1 = 0). */
    unsigned palettes;
    unsigned obj_palettes;
    AosObject1CRow pal_rows[AOS_O1C_MAX_ROWS];
    AosObject1CRow obj_rows[AOS_O1C_MAX_ROWS];
    /* sub_0803FE64 display-register writes (reg 0x50/0x54). */
    unsigned disp_writes;
    uint8_t disp_reg[AOS_O1C_MAX_ROWS];
    int16_t disp_val[AOS_O1C_MAX_ROWS];
    unsigned text_resets;   /* sub_08040FE0 text-window resets */
    int text_mode;          /* sub_08041304 arg, -1 none */
    bool save_setup;        /* sub_08054280 save-screen setup */
    bool fade_a;            /* sub_0803D270 */
    bool fade_b;            /* sub_0803CED4 */
    bool room_sync;         /* sub_080235C8 */
    bool aux_post;          /* sub_080213BC */
    bool save_block_refresh;/* sub_08012688 + sub_080135C0 on +0x60 */
    bool save_write;        /* SaveData_SaveSlotToSram(in->save_slot) */
    bool save_committed;    /* write returned 0 -> state 0x14 */
    bool save_rejected;     /* write returned nonzero -> state 0 */
    bool save_aux;          /* sub_0800EF58 on commit */
    unsigned beam_children; /* sub_08054F74 requests (<= 8,
                               update sub_0805514C); states 10/20
                               now also instantiate the beams —
                               their spawn effects land in
                               `children` below */
    /* Child spawn effects produced inside this update (the eight
     * beam spawns). Per-frame child logic/draw effects go through
     * aos_object1c_children_update/_draw with their own
     * AosObject1CChildrenOut. */
    AosObject1CChildrenOut children;
} AosObject1COut;

/* Native descriptor addresses reported through the Out rows. */
enum {
    AOS_O1C_DESC_MAIN = 0x0820C384,  /* create + state-1 palette */
    AOS_O1C_DESC_SAVE = 0x082099FC,  /* commit/decline/dormant swap */
    AOS_O1C_DESC_PULSE = 0x08119C10, /* idle pulse + beam rows */
    AOS_O1C_DESC_FLASH = 0x0820C428  /* save-fail palette flash */
};

/* Object1CCreate. `in->palette_index` below zero reproduces the
 * sub_0803AFB8/sub_0803B800 double failure -> EntityDelete. */
bool aos_object1c_create(AosObject1C *o, const AosObject1CCreateIn *in,
                          AosObject1CCreateOut *out);

/* Object1CUpdate. `in` carries the player probe position, the two
 * state-block button words, the sub_08021304 busy result, the SRAM
 * slot byte and the synchronous SaveData_SaveSlotToSram result;
 * `out` is cleared and filled with this frame's engine effects. */
void aos_object1c_update(AosObject1C *o, const AosObject1CIn *in,
                          AosObject1COut *out);

/* ---- Child spawn helpers --------------------------------------
 * Each mirrors one native spawn site and returns the child-array
 * index, or -1 when AOS_O1C_MAX_CHILDREN is full / no !active slot
 * remains (the EntityCreateInRange failure analogue — native slot
 * failures are silent). `out` may be NULL; when given it accumulates
 * frame_inits/frame_sets without being cleared. */

/* Object1CCreate's EntityCreateInRange(0x1B, 0x48) tail: + 0x58 |=
 * 0x20, + 0x5C = 3, draw sub_080544E8, sub_0803B924(., 0x08257964,
 * env->palette, 0xE). */
int aos_object1c_marker_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                              AosObject1CChildrenOut *out);

/* One live sub_08054E38 record attempt: record word0 `frame` is the
 * sub_0803F2C8 frame arg AND the position-adjust selector (2 ->
 * y -= 0x9F, x -= 0x30; 7 -> y -= 0x9F, x += 0x30 on the +0x40/+0x44
 * high words), `rec_param` the s16 record word1 for sub_0803B924,
 * `anim5c` the record word3 (live entries: {0, +0x1A+2, 0x13} and
 * {4, +0x1A+1, 0x14}; records 1/2 are dead natively). */
int aos_object1c_sparkle_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                               uint16_t frame, int16_t rec_param,
                               uint8_t anim5c,
                               AosObject1CChildrenOut *out);

/* One sub_08054F74 iteration: record {frame 2, env->anim_1a + 3,
 * 0x11}, `index` the +0x20 beam index byte (native spawns 0-7). */
int aos_object1c_beam_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                            uint8_t index, AosObject1CChildrenOut *out);

/* sub_08055414: type-0x49 spark at 16.16 (x, y) with +0x50/+0x54
 * copied from `beam`, the +0x5A nibble from beam +0x5A bits 3-6,
 * +0x5C = beam +0x5C + 1, +0x58 low bits = beam +0x58 & 7, spin
 * velocities from two RandomNumberGenerator calls (in->rng). */
int aos_object1c_spark_spawn(AosObject1C *o, unsigned beam_idx,
                             int32_t x, int32_t y,
                             const AosObject1CChildrenIn *in,
                             AosObject1CChildrenOut *out);

/* sub_08054574 has no caller in the disassembly dump; this just
 * allocates a zeroed FADE child (state 0, substate 0). */
int aos_object1c_fade_spawn(AosObject1C *o, AosObject1CChildrenOut *out);

/* Free a child slot (the engine delete that follows the + 0x59
 * bit-3 mark). Fix up spark `owner` indices afterwards if needed. */
void aos_object1c_child_delete(AosObject1C *o, unsigned idx);

/* updateFunc pass: runs each active child's + 0x00 routine
 * (marker: nothing; sparkle sub_08055070; beam sub_0805514C which
 * may append sparks; spark sub_080552C4; fade sub_08054574) and
 * writes the + 0x30..+ 0x33 sync bytes into `o`. Call AFTER
 * aos_object1c_update to match the native slot order (children sit
 * in later slots). Newly spawned sparks are updated the same pass,
 * matching ascending slot order. */
void aos_object1c_children_update(AosObject1C *o,
                                  const AosObject1CChildrenIn *in,
                                  AosObject1CChildrenOut *out);

/* drawFunc pass: runs each active child's + 0x04 routine (marker
 * sub_080544E8 -> 3 commits; sparkle/beam sub_0803B9D0 -> 1 commit;
 * spark sub_080554BC once state >= 1 -> 2 commits) into `out`. */
void aos_object1c_children_draw(AosObject1C *o,
                                AosObject1CChildrenOut *out);

#endif
