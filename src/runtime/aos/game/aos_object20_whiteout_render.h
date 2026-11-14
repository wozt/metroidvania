/* SPDX-License-Identifier: GPL-3.0-only */
/* sub_08064518 — the whiteout bar/tile renderer driven every
 * frame by the sub_080628AC param0-30 cinematic (ported as
 * aos_object20_whiteout_update in aos_object20_story.c).
 *
 * Native layout (entity = r7):
 *   +0xD   u8   frame/arm counter shared with the story state
 *   +0x14  u8   bar "intensity" byte (<<8 into the affine record)
 *   +0x15  u8   renderer sub-state: the case index 0..10 that
 *               sub_080628AC's case-3 gate waits to see == 0xA
 *   +0x16  u8   per-state one-shot init flag
 *   +0x18  u16  case4/5 bar-count ramp (left wipe edge)
 *   +0x1C  u16  BG affine X origin (bgx)
 *   +0x1E  u16  BG affine Y origin (bgy)
 *   +0x20  u16  bar strip screen-X origin
 *   +0x22  u16  bar strip screen-Y origin (top bar)
 *   +0x24  s16  per-state frame counter (counts down in states
 *               0/2, up in states 4/5, down in 6/7/8/9)
 *   +0x26  s8   jitter X (state 2 only; state 4/5 keeps its own
 *               copy in a stack slot)
 *   +0x27  s8   jitter Y
 *   +0x2C  u8   set to 0xE once in state 0
 *   +0x34  u8   sub_0806447C palette-cycle table index (0..5)
 *   +0x36  u16  sub_0806447C per-index dwell counter
 *   +0x38  u8   set to 5 once in the state4/5 init
 *   +0x3C  u8   flag byte; state4/5 exit ORs in bit 1
 *   +0x42  s16  entity screen X (written once in state 0)
 *   +0x46  s16  entity screen Y (written once in state 0)
 *
 * Every call runs HBlankEffectSetup(0, 0xA0, 0x10, 0x04000020)
 * (hblank writes 16-byte affine blocks to the BG2PA register
 * window) and then dispatches on +0x15 through the native jump
 * table at 0x08064584 (11 cases; values above 10 fall to the
 * shared exit sub_080650CE which only sets
 * gEwramData+0x7864 bit0 and returns). Each drawing state issues
 * 160 BgAffineSet BIOS calls (svc 0xE), one per scanline bar,
 * into a 0xA00-byte bank at gEwramData+0x786C selected by bit 3
 * of the gEwramData+0x7864 byte. The host renders the bars from
 * AosWoRenderOut::bars; the fixed src-param words 0x8000/0x8000
 * (identity affine scale/rotation pair) are implied and not
 * stored per bar.
 *
 * Engine effects that are not pure computation (entity spawns,
 * palette DMA, the sub_0806447C tile cycle, the entity-table
 * purge scans, PlaySong) are reported through Out; the decoded
 * 0x08118B34 spawn-function table and the 0x08526924 cycle
 * table are committed in the module so those paths carry real
 * arguments — only the 0x085268A8 tile-pick table (sub_08065800,
 * modelled separately in aos_seq_particles) stays uncommitted.
 * sub_08065800 is NOT a callee of
 * sub_08064518 — it is invoked by sub_080628AC alongside it and
 * is already modelled as aos_seq_table_pick in
 * aos_seq_particles; documented here as a separate, covered path.
 *
 * Integration status: partial — the state machine, bar geometry,
 * jitter, fade/spawn scheduling and all +0x15 transitions are
 * ported literally; BgAffineSet/DMA/palette writes are Out
 * events, not pixels. classified partial in
 * data/native_parity/annotations.tsv (no emulator comparison).
 */

#ifndef AOS_OBJECT20_WHITEOUT_RENDER_H
#define AOS_OBJECT20_WHITEOUT_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#define AOS_WO_BAR_COUNT 160        /* native loop r5 = 0..0x9F */
#define AOS_WO_MAX_EVENTS 8         /* per-frame event bound */

typedef struct {
    uint8_t state_d;        /* +0xD */
    uint8_t field_14;       /* +0x14 intensity byte */
    uint8_t field_15;       /* +0x15 sub-state (0..10) */
    uint8_t field_16;       /* +0x16 one-shot init flag */
    uint16_t field_18;      /* +0x18 */
    uint16_t field_1c;      /* +0x1C bgx */
    uint16_t field_1e;      /* +0x1E bgy */
    uint16_t field_20;      /* +0x20 strip X origin */
    uint16_t field_22;      /* +0x22 strip Y origin */
    int16_t  field_24;      /* +0x24 frame counter (s16 compares) */
    uint8_t  field_26;      /* +0x26 jitter X (s8 when read) */
    uint8_t  field_27;      /* +0x27 jitter Y (s8 when read) */
    uint8_t  field_2c;      /* +0x2C */
    uint8_t  field_34;      /* +0x34 cycle table index */
    uint16_t field_36;      /* +0x36 cycle dwell */
    uint8_t  field_38;      /* +0x38 */
    uint8_t  field_3c;      /* +0x3C flags */
    int16_t  scr_42;        /* +0x42 screen X */
    int16_t  scr_46;        /* +0x46 screen Y */
} AosWoRender;

/* Injected native reads. */
typedef struct {
    uint32_t rng;           /* gEwramData+8 randomNumber — the
                             * RandomNumberGenerator LCG state;
                             * read AND written back via Out */
    uint8_t  ewram_7864;    /* gEwramData+0x7864 byte; bit 3 selects
                             * which 0xA00 affine bank r8 targets */
    uint8_t  field10;       /* entity +0x10, copied into the
                             * sub_080640A4/sub_08064370 fx spawns */
} AosWoRenderIn;

/* One BgAffineSet source record, reduced to the fields that vary
 * per bar. Native src param is 20 bytes: words +0/+4 are the
 * constant 0x8000/0x8000 pair; +8 x, +0xA y, +0xC bgx, +0xE bgy,
 * +0x10 (field_14 << 8). dest record is 16 bytes at r8, r8 += 0x10
 * per bar, bank base gEwramData+0x786C + ((bit3 ? 0 : 1) * 0xA00). */
typedef struct {
    uint16_t x;             /* +8  screen/param X (u16 wrap) */
    uint16_t y;             /* +0xA Y = field_22 - bar index (+jit) */
    uint16_t bgx;           /* +0xC = field_1c */
    uint16_t bgy;           /* +0xE = field_1e */
    uint16_t intensity;     /* +0x10 = field_14 << 8 */
} AosWoBar;

/* EntityCreateInRange / scripted-spawn requests. kind selects
 * which native spawn the host should perform; the decoded
 * arguments (including the committed 0x08118B34 update-function
 * words) travel here. */
enum {
    AOS_WO_SPAWN_TABLE_118B34 = 0, /* EntityCreateInRange(0x1B,
                                    * 0x48, table118B34[aux]) —
                                    * aux = table word index 0..4
                                    * (the native ldm copies the
                                    * same 5 words to
                                    * sp+0x1C..0x2F and each site
                                    * passes one as updateFunc);
                                    * fn_addr carries the decoded
                                    * Thumb update address */
    AOS_WO_SPAWN_FX_63DF8,         /* sub_080640A4 path (state-2
                                    * init, update sub_08063DF8) */
    AOS_WO_SPAWN_FX_64198,         /* sub_08064370 path; scr pos is
                                    * the decoded child +0x42/+0x46 */
    AOS_WO_SPAWN_SUB_6384C         /* EntityCreateInRange(0x1B,
                                    * 0x48, sub_0806384C) at the
                                    * state4/5 +0x24==0x60 point,
                                    * child +0x42=0x8C,+0x46=0x32,
                                    * child +0xD=4 */
};

typedef struct {
    uint8_t  kind;          /* AOS_WO_SPAWN_* */
    int16_t  scr_x;         /* child +0x42 where decoded, else 0 */
    int16_t  scr_y;         /* child +0x46 where decoded, else 0 */
    uint8_t  aux;           /* table word index for TABLE_118B34 */
    uint32_t fn_addr;       /* decoded update-function address for
                             * TABLE_118B34 (the table word itself),
                             * 0 for the other kinds */
} AosWoSpawn;

/* sub_08063024 sparkle/particle spawn: entity type 0x1B range
 * 0x48, update sub_08065858, velocity signs flipped when dir!=0. */
typedef struct {
    int16_t  x;             /* arg0 s16 */
    int16_t  y;             /* arg1 s16 */
    uint8_t  dir;           /* arg2 (0/1) — negates the velocity
                             * quad when nonzero */
    uint8_t  variant;       /* arg3 (always 0 on this path) */
} AosWoParticle;

typedef struct {
    bool     hblank_setup;  /* HBlankEffectSetup(0,0xA0,0x10,
                             * 0x04000020) — every call */
    bool     tail_7864_or1; /* shared exit sub_080650CE:
                             * gEwramData+0x7864 |= 1 — runs on
                             * EVERY path, including +0x15 > 10 */
    uint32_t rng;           /* gEwramData+8 after this call's
                             * RandomNumberGenerator updates */
    uint8_t  bars_drawn;    /* 0 or AOS_WO_BAR_COUNT */
    AosWoBar bars[AOS_WO_BAR_COUNT];
    int      song;          /* PlaySong arg, -1 = none (state-0
                             * completion plays 0x166) */
    bool     palette_copy;  /* sub_0803C7B4(0x08119FDC,0,1,0xE)
                             * once in the state-0 init */
    bool     fade_pair;     /* sub_0803D18C + sub_0803CDF0 fired
                             * together; r=g=b=0x1F, a3 = arg3 */
    uint32_t fade_a3;
    uint32_t fade_s0;       /* 5th arg (stack [sp]) */
    uint32_t fade_s1;       /* 6th arg (stack [sp+4]) */
    bool     cycle_6447c;   /* sub_0806447C palette/tile cycler —
                             * states 4/5 every frame; mirrors
                             * cycle.ran (kept for callers that
                             * only want the call flag) */
    /* sub_0806447C decoded step — the 0x08526924 table is
     * committed in the module; `step` reports the dwell-expiry
     * branch with the consumed and next-entry fields below. The
     * four engine calls are DMA/sequence writes reported as
     * flags, matching the module's Out-event convention. */
    struct {
        bool     ran;        /* sub_0806447C invoked this call */
        bool     step;       /* +0x36 dwell expired: copies +
                              * sequence setup ran */
        uint8_t  index;      /* +0x34 index consumed (0..5) */
        uint8_t  next_index; /* +0x34 after the increment */
        uint32_t data;       /* entry data pointer (0x08119FDC) */
        uint16_t tile;       /* entry tile/record index */
        uint16_t dwell;      /* entry dwell threshold */
        uint32_t next_data;  /* post-increment entry (valid when
                              * next_index <= 5) */
        uint16_t next_tile;
        uint16_t next_dwell;
        uint8_t  seq_arg0;   /* entity +0x2C -> sub_0803CF34 arg0 */
        bool     copy_3c918; /* sub_0803C918(data, tile, 1, 0) */
        bool     copy_3c7b4; /* sub_0803C7B4(data, tile, 1, 0xE) */
        bool     seq_3d2d0;  /* sub_0803D2D0(0, next_data,
                              * next_tile, 0x10000, 0, next_dwell) */
        bool     seq_3cf34;  /* sub_0803CF34(+0x2C, next_data,
                              * next_tile, 0x10000, 0, next_dwell) */
    } cycle;
    bool     purge_547;     /* sub_08000EA8(0x80): delete all
                             * entities whose +0x547 == 0x80 */
    bool     purge_flagged; /* the 0xE0-entry scan at state4/5
                             * +0x24==0x9E: entities with +0x63==1
                             * and +0x58 bit0 get sub_0803E6BC +
                             * EntityDelete */
    bool     burst_63aec;   /* sub_08063AEC(entity, +0x20, +0x22)
                             * particle burst at the same point */
    int16_t  burst_x;
    int16_t  burst_y;
    uint8_t  spawn_count;
    AosWoSpawn spawns[AOS_WO_MAX_EVENTS];
    uint8_t  particle_count;
    AosWoParticle particles[AOS_WO_MAX_EVENTS];
} AosWoRenderOut;

/* Advance the renderer one frame. Mirrors sub_08064518 exactly:
 * HBlank setup, +0x15 jump-table dispatch, the 160-bar
 * BgAffineSet loops, every counter/RNG update and every +0x15
 * transition. The shared exit sub_080650CE
 * (gEwramData+0x7864 |= 1) runs on every path including
 * +0x15 > 10 and is reported as Out::tail_7864_or1. */
void aos_wo_render_update(AosWoRender *s,
                          const AosWoRenderIn *in,
                          AosWoRenderOut *out);

#endif /* AOS_OBJECT20_WHITEOUT_RENDER_H */
