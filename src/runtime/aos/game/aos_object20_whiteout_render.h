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
 * 0x48 (the shared sub_08062B60 prologue — graphics 0x08260390,
 * frames 0x08267EE4 anim 0xE, draw sub_0803B9D0), update
 * sub_08065858, velocity signs flipped when dir!=0. The native
 * routine consumes ONE RandomNumberGenerator draw per successful
 * alloc for the +0x65 pick — variant 0 indexes the three u32
 * words at 0x081189A0 {0xA,0xB,0xC} by rng % 3 (low byte stored),
 * any other variant stores (rng % 7) + 2 — so wo_particle draws
 * it here and reports the result in field_65. The draw is
 * unconditional in the port: natively it is gated on the
 * sub_08062B60(0) alloc succeeding, which the renderer cannot
 * observe (documented approximation — only host pool exhaustion
 * would skip it). The fixed velocity/accel quad (+0x48 = 0x30000,
 * +0x4C = -0x8000, +0x50 = 0x800, +0x54 = -0x100, all negated
 * when dir != 0), +0xD = 0x40, +0x5C = 3 and +0x63 = 0x80 are
 * decoded constants the host reapplies in aos_wo_spark_spawn;
 * +0x40/+0x44 stay at the fresh-entity 0 like the burst
 * particles (documented approximation). */
typedef struct {
    int16_t  x;             /* arg0 s16 -> +0x42 */
    int16_t  y;             /* arg1 s16 -> +0x46 */
    uint8_t  dir;           /* arg2 (0/1) — negates the velocity
                             * quad when nonzero */
    uint8_t  variant;       /* arg3 — 0 on every sub_08064518
                             * call site; the +0x65 pick formula
                             * is still ported for completeness */
    uint8_t  field_65;      /* +0x65 — the RNG-picked frame byte */
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
    uint8_t  burst_parent;  /* caller entity +0x10 — copied into
                             * each particle's +0x11 (the parent
                             * slot byte sub_08063944 reads
                             * entityData[+0x11].unk_3C through) */
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

/* ------------------------------------------------------------------
 * 0x08118B34 spawn-child update functions — the updateFunc words the
 * renderer's spawn events carry. Native routines, decoded from
 * third_party/cvaos/asm/code/code_08060B98.s:5109-5655:
 *   aux 0  sub_08063420  table 0x081189CC, 15 entries
 *   aux 1  sub_08063558  table 0x08118A44, 10 entries
 *   aux 2  sub_08063600  table 0x08118A94,  5 entries
 *   aux 3  sub_08063698  table 0x08118ABC, 10 entries
 *   aux 4  sub_08063740  table 0x08118B0C,  5 entries
 *
 * All five run the same state machine on the child entity:
 *   +0xA == 0 : +0xD = table[+0xC].delay (+4), +0xA = 1
 *   +0xA == 1 : +0xD--; when the stored u8 wraps to 0xFF, +0xA = 2
 *   +0xA >= 2 : fire once at s16(+0x42+dx)/s16(+0x46+dy), then
 *               +0xA = 0 and +0xC++
 *   tail      : +0xC == entry count -> entity +0x59 |= 8
 *
 * Fire variants:
 *   aux 0/4: parity of +0xA — odd calls sub_08063168(x,y); even
 *     calls EntityCreateInRange(0x1B,0x48,sub_08065835) gated by
 *     sub_0806E0D0, finished with sub_0803F2C8(arg1 = RNG & 3) and
 *     sub_0803F17C. +0xA is always 2 at fire time in the native
 *     machine, so the odd branch is unreachable through the state
 *     machine itself but is ported for faithfulness.
 *   aux 2:   sub_08063168(x,y); the routine draws no RNG itself.
 *   aux 1/3: two RNG draws jitter the fire position by
 *     2-(rng&3) per axis, then sub_080632B0(x,y).
 *
 * sub_08063168 (carrier + 3x sub_080630E8 particles) and
 * sub_080632B0 (carrier + 2x particles, variant table
 * 0x081189B8 = {0,2,3,2,3}) are now ported below as the burst
 * API: the child machine still only REPORTS the burst/carrier
 * events (it does not consume the callees' internal RNG draws),
 * and the host drives aos_wo_burst_63168/aos_wo_burst_632b0/
 * aos_wo_carrier_spawn when it consumes each event — calling
 * them in the same frame preserves the native RNG order. Once
 * +0xC reaches the entry count the native
 * routine keeps reading past the copied stack table until the
 * host deletes the entity on +0x59 bit3; the port reports done
 * and stops touching table state (documented approximation).
 *
 * The host owns the child pool: instantiate one AosWoSpawnChild
 * per AOS_WO_SPAWN_TABLE_118B34 spawn event (bounded by
 * AOS_WO_MAX_EVENTS per frame) and call the update once per
 * frame per live child.
 * ------------------------------------------------------------------ */

/* Child state mirroring the native entity bytes the five routines
 * touch. `aux` is a port-side dispatch selector: native code picks
 * the routine through the updateFunc pointer; aux == the 0x08118B34
 * word index, which maps 1:1 onto the five routines. */
typedef struct {
    uint8_t  aux;        /* table word index 0..4 */
    uint8_t  field_a;    /* entity +0xA phase: 0 init / 1 delay /
                          * >=2 fire (always 2 in the native
                          * machine) */
    uint8_t  field_c;    /* entity +0xC entry index */
    uint8_t  field_d;    /* entity +0xD delay counter */
    int16_t  scr_x;      /* entity +0x42 (spawn event scr_x) */
    int16_t  scr_y;      /* entity +0x46 (spawn event scr_y) */
    uint8_t  field_59;   /* entity +0x59 flags; bit3 = walk done */
} AosWoSpawnChild;

typedef struct {
    uint32_t rng;        /* gEwramData+8 LCG state (read+write) */
} AosWoSpawnChildIn;

typedef struct {
    uint32_t rng;        /* gEwramData+8 after this call's draws —
                          * only the draws inside the ported routine
                          * are consumed; callee-internal draws are
                          * not */
    bool     done;       /* entity +0x59 bit3 set after this call —
                          * the entry walk reached the table end and
                          * the host should delete the child */
    /* aux 0/4 even fire: EntityCreateInRange(0x1B,0x48,
     * sub_08065835) + sub_0806E0D0 gate + sub_0803F2C8/sub_0803F17C
     * setup. A request, not a guarantee: native deletes the new
     * entity when allocation or sub_0806E0D0 fails. */
    bool     carrier_spawn;
    int16_t  carrier_x;  /* grandchild +0x42 = fire x */
    int16_t  carrier_y;  /* grandchild +0x46 = fire y */
    uint8_t  carrier_variant; /* RNG & 3 -> sub_0803F2C8 arg1 */
    /* aux 0/4 odd fire and aux 2 fire: sub_08063168(x,y) — one
     * sub_08065835 carrier plus 3 sub_080630E8 particles whose
     * positions/velocities come from the callee's own RNG draws. */
    bool     burst_63168;
    int16_t  burst_x;
    int16_t  burst_y;
    /* aux 1/3 fire: sub_080632B0(x,y) — one carrier plus 2
     * sub_080630E8 particles; the carrier variant is picked inside
     * the callee by RNG % 5 over the 0x081189B8 word table
     * {0,2,3,2,3}. */
    bool     burst_632b0;
    int16_t  burst_632b0_x;
    int16_t  burst_632b0_y;
} AosWoSpawnChildOut;

/* Instantiate child state from a renderer spawn event. Returns
 * false (state untouched) unless sp is AOS_WO_SPAWN_TABLE_118B34
 * with aux < 5 — the only kind these five update functions serve. */
bool aos_wo_spawn_child_init(AosWoSpawnChild *c,
                             const AosWoSpawn *sp);

/* Advance one child one frame. Mirrors the shared native machine
 * described above; engine effects (entity creation, sprite setup,
 * the sub_08063168/sub_080632B0 bursts) are reported through Out. */
void aos_wo_spawn_child_update(AosWoSpawnChild *c,
                               const AosWoSpawnChildIn *in,
                               AosWoSpawnChildOut *out);

/* ------------------------------------------------------------------
 * Whiteout burst callees — the entity effects the spawn-child
 * machine reports but does not itself advance. Decoded from
 * third_party/cvaos/asm/code/code_08060B98.s:4720-5107 and
 * :9678-9698:
 *
 *   sub_080630E8  particle update (the updateFunc the burst
 *                 loops store at entity +0). +0xA == 0 counts
 *                 +0xD down; on the u8 wrap to 0xFF it clears
 *                 +0x58 bit5 and sets +0xA = 1. +0xA == 1 runs
 *                 one integration step — +0x40 += +0x48,
 *                 +0x44 += +0x4C, then +0x48 += +0x50,
 *                 +0x4C += +0x54 (position uses the OLD
 *                 velocity) — followed by the sub_08068AD4 box
 *                 probe (-0x80, -0x40, 0x1F0, 0x120); a zero
 *                 result sets +0x59 bit3. +0xA >= 2 idles.
 *                 No RNG.
 *
 *   sub_08063168(x, y)  burst: EntityCreateInRange(0x1B, 0x48,
 *                 sub_08065834) carrier gated by sub_0806E0D0,
 *                 then THREE sub_08062B60(0) particles, six RNG
 *                 draws each (scr pos +/- jitter, +0x4C/+0x54
 *                 velocity/accel, +0x65 table pick, +0xD delay).
 *
 *   sub_080632B0(x, y)  same shape with TWO particles; first
 *                 draws RNG % 5 over the committed 0x081189B8
 *                 word table {0,2,3,2,3} for the carrier's
 *                 sub_0803F2C8 variant BEFORE creating it.
 *
 *   sub_08065834  carrier update (Thumb pointer 0x08065835 —
 *                 the "sub_08065835" of the spawn-child events):
 *                 +0x59 bit0 set -> +0x59 |= 8, else
 *                 sub_0803F17C animation step.
 *
 * Entity fields the port tracks mirror the native bytes; the
 * sub_08062B60 engine writes (+0x2C = 0xE, frames 0x08267EE4
 * animation 0xE, draw sub_0803B9D0, graphics 0x08260390) and
 * the sub_0803F2C8/sub_0803F17C/sub_0806E0D0/sub_08068AD4
 * engine calls are injected or reported, matching the module's
 * Out-event convention. Fresh-entity bytes the native relies on
 * the entity pool to clear (+0x44/+0x48..+0x54 accumulators,
 * +0x59) are zeroed by the port — documented approximation;
 * +0x40/+0x44 are NOT zeroed: the strh +0x42/+0x46 spawn writes
 * set their high words (scr << 16), matching the native bytes.
 * ------------------------------------------------------------------ */

/* Carrier entity spawned by sub_08063168/sub_080632B0 (and
 * directly by the child machine's even +0xA fire). */
typedef struct {
    int16_t  scr_42;    /* +0x42 screen X (strh) */
    int16_t  scr_46;    /* +0x46 screen Y (strh) */
    uint8_t  field_5c;  /* +0x5C = 0 written at spawn */
    int16_t  variant;   /* sub_0803F2C8 arg1 (s16): 0 for
                         * sub_08063168 carriers, the 0x081189B8
                         * pick for sub_080632B0, RNG & 3 for the
                         * child machine's direct carrier_spawn */
    uint8_t  flags59;   /* +0x59: bit0 arms deletion, bit3 = the
                         * host deletes the carrier */
} AosWoBurstCarrier;

typedef struct {
    bool anim_step;     /* sub_0803F17C ran this call */
    bool done;          /* +0x59 bit3 set after this call —
                         * the host should delete the carrier */
} AosWoBurstCarrierOut;

/* sub_080630E8 particle entity state — every byte/word the
 * update routine and the two burst spawn loops touch. */
typedef struct {
    uint8_t  field_a;    /* +0xA phase: 0 delay / 1 integrate /
                          * >=2 idle */
    uint8_t  field_d;    /* +0xD delay counter */
    int16_t  scr_42;     /* +0x42 screen X (strh at spawn) */
    int16_t  scr_46;     /* +0x46 screen Y */
    int32_t  field_40;   /* +0x40 position X accumulator */
    int32_t  field_44;   /* +0x44 position Y accumulator */
    int32_t  field_48;   /* +0x48 velocity X (stays 0 — the spawn
                          * loops never write it) */
    int32_t  field_4c;   /* +0x4C velocity Y */
    int32_t  field_50;   /* +0x50 accel X (stays 0) */
    int32_t  field_54;   /* +0x54 accel Y */
    uint8_t  field_58;   /* +0x58 flags: bit5 set at spawn, cleared
                          * when the +0xD delay wraps */
    uint8_t  field_59;   /* +0x59 flags: bit3 = delete */
    uint8_t  field_5c;   /* +0x5C = 3 at spawn (overwrites the
                          * sub_08062B60 0x11) */
    uint8_t  field_63;   /* +0x63 = 0x80 at spawn */
    uint8_t  field_65;   /* +0x65 = low byte of the RNG % 3 pick
                          * from the 0x081189AC word table
                          * {0xC,0xB,0xC} */
} AosWoBurstParticle;

#define AOS_WO_BURST_MAX_PARTICLES 3  /* sub_08063168 spawns 3,
                                       * sub_080632B0 spawns 2 */

/* One burst call's products: the carrier (when it survived
 * allocation and the sub_0806E0D0 gate) plus its particles. */
typedef struct {
    AosWoBurstCarrier carrier;
    bool carrier_valid;                     /* carrier kept */
    AosWoBurstParticle particles[AOS_WO_BURST_MAX_PARTICLES];
    uint8_t particle_count;                 /* 0..3 */
} AosWoBurst;

/* Injected native reads for the burst spawners. */
typedef struct {
    uint32_t rng;               /* gEwramData+8 LCG state
                                 * (read AND written via Out) */
    bool carrier_alloc_ok;      /* EntityCreateInRange(0x1B,0x48,
                                 * sub_08065834) returned non-NULL */
    bool gate_6e0d0_ok;         /* sub_0806E0D0(entity,
                                 * 0x08260390, 0x08266544,
                                 * 0x08267EE4, 0, same) low byte
                                 * nonzero — a zero result makes
                                 * the native EntityDelete the
                                 * carrier (particles still spawn) */
    uint8_t particle_alloc_fail;/* bit i: particle i's
                                 * sub_08062B60(0) returned NULL
                                 * (entity pool or the 0x08260390
                                 * graphics load failed) — that
                                 * particle is skipped and consumes
                                 * NO RNG draws */
} AosWoBurstIn;

typedef struct {
    uint32_t rng;           /* gEwramData+8 after this call's draws */
    bool carrier_spawned;   /* carrier survived alloc + gate */
    bool carrier_deleted;   /* alloc ok but the sub_0806E0D0 gate
                             * failed -> EntityDelete ran */
    bool carrier_f2c8;      /* sub_0803F2C8(entity, variant,0,0) */
    bool carrier_f17c;      /* sub_0803F17C(entity) */
} AosWoBurstOut;

/* Injected reads for the sub_080630E8 particle update. */
typedef struct {
    bool probe_inside;  /* sub_08068AD4(entity, -0x80, -0x40,
                     * 0x1F0, 0x120) result — nonzero keeps the
                     * particle, zero sets +0x59 bit3 */
} AosWoBurstParticleIn;

typedef struct {
    bool probe_ran;     /* the +0xA == 1 integrate/probe phase ran */
    bool probe_missed;  /* probe returned 0 -> +0x59 |= 8 */
    bool done;          /* +0x59 bit3 after this call */
} AosWoBurstParticleOut;

/* sub_08063168's carrier creation, exposed for the child
 * machine's direct carrier_spawn events (identical native
 * sequence: EntityCreateInRange + sub_0806E0D0 gate +
 * +0x42/+0x46/+0x5C writes + sub_0803F2C8/sub_0803F17C).
 * `variant` is the sub_0803F2C8 arg1 — RNG & 3 for the child
 * path, 0 inside sub_08063168, the 0x081189B8 pick inside
 * sub_080632B0. Returns true when the carrier was kept;
 * out->carrier_deleted reports the gate-failure EntityDelete.
 * Draws no RNG (out->rng == in->rng). */
bool aos_wo_carrier_spawn(AosWoBurstCarrier *c, int16_t x,
                          int16_t y, int16_t variant,
                          const AosWoBurstIn *in,
                          AosWoBurstOut *out);

/* sub_08063168(x, y): carrier + 3 particles, 18 RNG draws when
 * every sub_08062B60 succeeds. Resets *b. */
void aos_wo_burst_63168(AosWoBurst *b, int16_t x, int16_t y,
                        const AosWoBurstIn *in,
                        AosWoBurstOut *out);

/* sub_080632B0(x, y): RNG % 5 carrier-variant draw, carrier +
 * 2 particles — 13 RNG draws when every alloc succeeds. */
void aos_wo_burst_632b0(AosWoBurst *b, int16_t x, int16_t y,
                        const AosWoBurstIn *in,
                        AosWoBurstOut *out);

/* sub_08065834: the carrier's per-frame update. */
void aos_wo_carrier_update(AosWoBurstCarrier *c,
                           AosWoBurstCarrierOut *out);

/* sub_080630E8: the particle's per-frame update. */
void aos_wo_particle_update(AosWoBurstParticle *p,
                            const AosWoBurstParticleIn *in,
                            AosWoBurstParticleOut *out);

/* ------------------------------------------------------------------
 * sub_08063024 / sub_08065858 — the renderer's sparkle particles.
 *
 * sub_08063024(x, y, dir, variant) is the spawn behind every
 * AosWoParticle event (code_08060B98.s:4624-4719): after the
 * sub_08062B60(0) alloc it writes +0 = sub_08065858, +0x42/+0x46 =
 * the s16 args, the fixed velocity/accel quad (+0x48 = 0x30000,
 * +0x4C = -0x8000, +0x50 = 0x800, +0x54 = -0x100, negated when
 * dir != 0), +0xD = 0x40, +0x5C = 3, +0x63 = 0x80 and the +0x65
 * RNG pick reported in AosWoParticle::field_65. The strh
 * +0x42/+0x46 writes are the HIGH words of the +0x40/+0x44
 * 16.16 position pair (+0x42/+0x46 alias them), so field_40/44
 * start at x/y << 16 with the fresh-entity low words 0.
 *
 * sub_08065858 (code_08060B98.s:9701-9746) is its update: the
 * ballistic step (position += velocity, velocity += accel on
 * +0x40/+0x44/+0x48/+0x4C/+0x50/+0x54) then the sub_08068AD4
 * probe — the same (-0x80, -0x40, 0x1F0, 0x120) rect the
 * sub_080630E8 particle uses — and a miss sets +0x59 bit 3.
 * aos_flyer_step_65858 (aos_flyer.c) ports the same native
 * routine for the thrown-object record; this variant runs on the
 * spark's own field layout like the sub_080630E8 particle does.
 * Unlike sub_080630E8 there is no +0xA delay phase: every call
 * integrates and probes. The host computes probe_inside from the
 * 16.16 accumulators exactly like the burst-particle path.
 * Draw assets are the shared 0x1B/0x48 entity (graphics
 * 0x08260390, frames 0x08267EE4 anim 0xE) — the same export the
 * wo_bursts draw path already consumes; wiring the spark pool
 * into the draw pass is host-side work outside this module.
 * ------------------------------------------------------------------ */

/* One sub_08063024 sparkle entity — every byte the spawn and the
 * sub_08065858 update touch. */
typedef struct {
    int16_t  scr_42;    /* +0x42 screen X (strh at spawn) */
    int16_t  scr_46;    /* +0x46 screen Y */
    int32_t  field_40;  /* +0x40 position X accumulator — fresh
                         * entity zero (documented) */
    int32_t  field_44;  /* +0x44 position Y accumulator */
    int32_t  field_48;  /* +0x48 velocity X = ±0x30000 */
    int32_t  field_4c;  /* +0x4C velocity Y = ∓0x8000 */
    int32_t  field_50;  /* +0x50 accel X = ±0x800 */
    int32_t  field_54;  /* +0x54 accel Y = ∓0x100 */
    uint8_t  field_59;  /* +0x59: bit3 = delete */
    uint8_t  field_65;  /* +0x65 frame pick (from the Out event) */
    uint8_t  field_d;   /* +0xD = 0x40 (spawn write; the update
                         * never reads it) */
    uint8_t  field_5c;  /* +0x5C = 3 */
    uint8_t  field_63;  /* +0x63 = 0x80 */
} AosWoSpark;

/* Fill *p from a reported AosWoParticle spawn event: the decoded
 * spawn writes of sub_08063024. The +0x65 pick already consumed
 * its RNG draw inside the renderer. */
void aos_wo_spark_spawn(AosWoSpark *p, const AosWoParticle *ev);

/* Injected reads for the sub_08065858 update — the same probe
 * contract as AosWoBurstParticleIn. */
typedef struct {
    bool probe_inside;  /* sub_08068AD4(entity, -0x80, -0x40,
                     * 0x1F0, 0x120) result */
} AosWoSparkIn;

typedef struct {
    bool probe_missed;  /* probe returned 0 -> +0x59 |= 8 */
    bool done;          /* +0x59 bit3 after this call */
} AosWoSparkOut;

/* sub_08065858: ballistic step + probe; a miss sets +0x59 bit3. */
void aos_wo_spark_update(AosWoSpark *p, const AosWoSparkIn *in,
                         AosWoSparkOut *out);

/* ------------------------------------------------------------------
 * sub_08063AEC / sub_08063944 — the state-4/5 +0x24 == 0x9E burst.
 *
 * sub_08063AEC(caller, x, y) (code_08060B98.s:5990-6152) memcpy's
 * the committed 20 x 8-byte record table at 0x08118B4C onto the
 * stack and loops sl = 0..0x13: per record it runs the
 * sub_08062B60(1) alloc prologue (entity type 0x1B range 0x48,
 * graphics 0x08260390, +0x2C = 0xE, frames 0x08267EE4 anim 0xE,
 * draw sub_0803B9D0, +0x1C = 0x12000, sub_0803E654(entity,1),
 * +0x59 |= 0x10, +0x5C = 0x11), stores update sub_08063944,
 * +0x11 = caller +0x10, +0x17 = sl, +0x20/+0x22 = the s16 args,
 * +0x28 = rec u8@+2, +0x29 = rec u8@+3 >> 1, +0x24 = rec u16@+0
 * minus ((RNG & 0xFFF) + 0x280) (u16 wrap), +0x38 = 0, +0x26 = 1,
 * then writes +0x42/+0x46 = +0x20/+0x22 (the high words of the
 * +0x40/+0x44 16.16 position pair — the +0x42/+0x46 "screen
 * words" are aliases of those high halves, which is why every
 * entity read/write of +0x42/+0x46 here lands on field_40/44's
 * top words), offsets them by (sub_080009E4(+0x24+0x4000) *
 * +0x28) >> 13 / (sub_080009E4(+0x24) * +0x29) >> 13, stores
 * +0x48/+0x4C = sub_080009E4(+0x24+0x4000/+0x24) >> 8, +0x2A = 0,
 * +0x1C = 0x40000, +0x26 = rec u16@+4 << 1, +0xD = sl - 0x80 -
 * sl/5 (trunc), +0x5C = 0x11 and +0x65 = RNG % 3 + 0xA. Two RNG
 * draws per successful alloc, in that order; a NULL
 * sub_08062B60 skips the record before any draw.
 *
 * sub_08063944 (code_08060B98.s:5781-5987) is the particle
 * update. +0xA == 0: recomputes the +0x42/+0x46 anchor from
 * +0x20/+0x22 plus the per-record trig offsets, integrates
 * +0x40/+0x44 by +0x48/+0x4C * (s16)+0x2A (the odd/even +0xD
 * counter), bumps +0x24 by 0x20 and — only while
 * entityData[+0x11].unk_3C bit 1 is set — damps +0x48/+0x4C to
 * >> 2, sets +0xA = 1 and +0xD = +0x17 * (6 - (+0x17 >> 2)).
 * +0xA == 1 counts +0xD down; on the u8 wrap to 0xFF it clears
 * +0xD and sets +0xA = 2, otherwise it integrates +0x40/+0x44
 * by +0x48/+0x4C once. +0xA >= 2 runs sub_0806E43C(entity,
 * 0x780000, 0x500000, 3) — the Sqrt/ArcTan2/sub_08068CDC
 * homing step toward screen (0x78, 0x50) ported below — and a
 * zero result (particle within one pixel of the target) runs
 * sub_0803E6BC + sets +0x59 bit 3; a nonzero result decrements
 * +0x1C by 0x1000 while nonzero (the affine fade). Every call
 * ends in sub_0803E058 (affine write — reported).
 *
 * sub_080009E4 is the ROM's Q16 sine lookup (cvaos
 * code_080009A0.c); its 1024-entry u16 table is committed in
 * the module. Sqrt (BIOS svc 8) and ArcTan2 (BIOS svc 0xA) are
 * ported as exact-floor isqrt and a documented atan2
 * quantization — a 1-LSB angle difference is possible against
 * the BIOS table. The entityData[+0x11].unk_3C bit-1 read is
 * injected through AosWo63AECIn::parent_flag3c_bit2.
 * ------------------------------------------------------------------ */

/* The committed 20 x 8-byte records of 0x08118B4C. Byte layout
 * per record: u16 f0 @+0 (the +0x24 base angle), u8 f2 @+2
 * (+0x28), u8 f3 @+3 (+0x29 = f3 >> 1), u16 f4 @+4 (+0x26 =
 * f4 << 1); bytes +5..+7 are copied but never read. */
typedef struct {
    uint16_t f0;
    uint8_t  f2;
    uint8_t  f3;
    uint16_t f4;
} AosWo63AECRec;

#define AOS_WO63AEC_MAX 20        /* sl loop bound 0x13 inclusive */
#define AOS_WO63AEC_POOL 64       /* host pool bound — the native
                                 * bound is the 0xE0-slot entity
                                 * table; 64 covers repeated
                                 * bursts without modelling it */

/* One sub_08063AEC particle (the sub_08063944 entity). +0x42/
 * +0x46 are the high words of field_40/field_44 — kept as the
 * derived aliases they are (field_40 >> 16), matching the
 * sub_08068AD4/sub_0803AC40/sub_0806E43C reads. */
typedef struct {
    uint8_t  field_a;    /* +0xA phase: 0 anchor/integrate,
                          * 1 delay, >=2 homing */
    uint8_t  field_d;    /* +0xD counter */
    uint8_t  field_11;   /* +0x11 parent slot byte (caller +0x10) */
    uint8_t  field_17;   /* +0x17 record index 0..19 */
    int16_t  field_20;   /* +0x20 spawn X arg (u16 on the wire) */
    int16_t  field_22;   /* +0x22 spawn Y arg */
    uint16_t field_24;   /* +0x24 angle; += 0x20 per phase-0 call */
    uint16_t field_26;   /* +0x26 = rec f4 << 1 (affine scale) */
    uint8_t  field_28;   /* +0x28 = rec f2 (X offset gain) */
    uint8_t  field_29;   /* +0x29 = rec f3 >> 1 (Y offset gain) */
    int16_t  field_2a;   /* +0x2A odd-+0xD counter (s16) */
    uint32_t field_38;   /* +0x38 = 0 (sub_0803E058 arg0) */
    int32_t  field_40;   /* +0x40 16.16 pos X (+0x42 = hi word) */
    int32_t  field_44;   /* +0x44 16.16 pos Y (+0x46 = hi word) */
    int32_t  field_48;   /* +0x48 velocity X */
    int32_t  field_4c;   /* +0x4C velocity Y */
    int32_t  field_1c;   /* +0x1C affine/alpha — 0x40000 at spawn,
                          * -0x1000 per homing call while nonzero */
    uint8_t  field_58;   /* +0x58 = (sub_0803E654 ret & 0x1F) */
    uint8_t  field_59;   /* +0x59: bit3 = delete (0x10 set at
                          * the sub_08062B60(1) prologue) */
    uint8_t  field_5c;   /* +0x5C = 0x11 */
    uint8_t  field_65;   /* +0x65 = RNG % 3 + 0xA still frame */
} AosWo63AEC;

typedef struct {
    uint32_t rng;           /* gEwramData+8 LCG state */
    uint32_t alloc_fail;    /* bit i: record i's sub_08062B60(1)
                             * returned NULL — the record is
                             * skipped before any RNG draw */
    uint8_t  flags58_init;  /* sub_0803E654(entity,1) result &
                             * 0x1F written to +0x58 (bit5
                             * cleared) — same value applied to
                             * every record of the burst */
} AosWo63AECIn;

typedef struct {
    uint32_t rng;           /* gEwramData+8 after this call */
    uint8_t  spawned;       /* records that survived alloc */
} AosWo63AECBurstOut;

/* sub_08063AEC(caller, x, y): appends up to AOS_WO63AEC_MAX
 * particles to pool[count..], bounded by cap. `parent` is the
 * caller entity's +0x10 byte (stored at particle +0x11). */
void aos_wo_burst_63aec(AosWo63AEC *pool, uint8_t *count,
                        uint8_t cap, int16_t x, int16_t y,
                        uint8_t parent, const AosWo63AECIn *in,
                        AosWo63AECBurstOut *out);

/* Injected reads for the sub_08063944 update. */
typedef struct {
    bool parent_flag3c_bit2;/* entityData[+0x11].unk_3C bit 1 —
                             * the phase-0 -> phase-1 switch.
                             * The host cannot index the native
                             * entity table; the Object20 apply
                             * path feeds its own +0x3C bit 1
                             * (documented interpretation: the
                             * +0x10 byte indexes the caller). */
} AosWo63AECUpdateIn;

typedef struct {
    bool affine_3e058;      /* sub_0803E058 ran (every call) */
    bool e6bc_call;         /* sub_0803E6BC(entity, +0x58 & 1)
                             * on the homing-arrival delete */
    bool done;              /* +0x59 bit3 — host deletes */
} AosWo63AECUpdateOut;

/* sub_08063944: one particle update. */
void aos_wo_63aec_update(AosWo63AEC *p,
                         const AosWo63AECUpdateIn *in,
                         AosWo63AECUpdateOut *out);

#endif /* AOS_OBJECT20_WHITEOUT_RENDER_H */
