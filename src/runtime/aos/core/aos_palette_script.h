/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow palette-script instances (cvaos code_08039340):
 * the per-entity palette animation state that sub_0803CC70
 * registers into one of the sixteen slots of gEwramData's
 * unk_12E60 table and that sub_0803C150 advances once per frame
 * from the sub_0803CDDC frame tail. No SDL.
 *
 * Native layout (include/structs/ewram.h EwramData_unk12E60,
 * size 0xC):
 *   +0x0  u8 *unk_0   palette bank table (sub_08068264's
 *                     descriptor; entry n is 0x20 bytes = 16
 *                     BGR555 colours, bank count at +2)
 *   +0x4  u8 *unk_4   script: u16 entry count, then count
 *                     4-byte entries {palette bank, duration,
 *                     u16 unused}
 *   +0x8  s16 unk_8   frames counted inside the current phase
 *   +0xA  u8  unk_A   current script entry (phase)
 *   +0xB  u8  unk_B   unused by sub_0803C150
 *
 * sub_0803CC70(palette_table, script, slot) writes the two
 * pointers and zeroes +8/+0xA; sub_0803CCBC(slot) clears all
 * four fields; sub_0803C150 skips a slot whose +0 or +4 is NULL.
 * Each tick, when +8 == 0, the routine DMAs the current entry's
 * palette bank into the OBJ palette staging (unk_11DDC rows
 * slot..0xF, flags unk_123DC/unk_123E1) — the palette *application*
 * has no host consumer here because the object exporter bakes each
 * script bank into its own frame, so this module ports the
 * instance state machine only. The remaining native pieces stay
 * unported and are documented on the functions below: the global
 * unk_12FE0 bit-0 skip gate and the sixteen-slot table itself
 * (the port keeps the state inside each owning entity instead —
 * see the door's palette_age note in aos_door.h).
 *
 * Known native registrants (asm cross-references of
 * sub_0803CC70): the wooden/green door's sub_0804D8F0
 * (scripts 0x08525564 / 0x08525574, banks 3-5 and 9-11 of
 * 0x08209AE0), Object16Create (0x08526414 over palette
 * 0x0825E890), Object2B's loader sub_08058554 (0x08526618 over
 * 0x0820A1C4), Giant Ghost's sub_0807E2CC (0x08526D60 over
 * 0x0820A698) and Legion's sub_0809DB00 case machine, which
 * re-arms 0x085282D0 and 0x085282F4 over 0x0820AC4C mid-fight.
 * Only the door currently has per-phase palette art exported, so
 * it is the module's first consumer; the others register the
 * same state once their palette banks are exported. */
#ifndef AOS_PALETTE_SCRIPT_H
#define AOS_PALETTE_SCRIPT_H

#include <stdbool.h>
#include <stdint.h>

/* One registered palette-script instance: the port of a single
 * unk_12E60 slot. `banks`/`ticks` are the script's per-entry
 * byte 0 (palette bank inside the entity's palette descriptor)
 * and byte 1 (phase duration in frames); either may be NULL when
 * the consumer only needs the phase (banks) or nothing (both —
 * an unregistered slot, matching the native NULL-pointer skip).
 * The native pointers reference ROM script bytes; the port takes
 * the already-extracted per-phase arrays instead. */
typedef struct {
    const uint8_t *banks;   /* script entry byte 0 per phase, or NULL */
    const uint8_t *ticks;   /* script entry byte 1 per phase, or NULL */
    uint16_t count;         /* script header u16 entry count */
    int16_t frame;          /* +8: frames counted in the phase */
    uint8_t phase;          /* +0xA: current script entry */
} AosPaletteScript;

/* sub_0803CC70: (re)register the instance — pointers stored,
 * frame and phase counters zeroed. A NULL `ticks` or `count` 0
 * leaves the slot unregistered, like the native NULL checks. */
void aos_palette_script_init(AosPaletteScript *script,
                             const uint8_t *banks, const uint8_t *ticks,
                             uint16_t count);

/* sub_0803CCBC: clear the slot — pointers dropped, counters zeroed. */
void aos_palette_script_clear(AosPaletteScript *script);

/* sub_0803C150's per-instance advance, one call per frame: the
 * +8 frame counter increments and, once it reaches the current
 * entry's duration, resets to 0 while +0xA advances and wraps at
 * the entry count — so a phase of d frames is shown for d ticks
 * and the loop restarts at entry 0. Unregistered instances are
 * skipped like the native NULL-pointer test. The palette DMA the
 * native performs when +8 == 0 has no host consumer (banks are
 * baked into the exported frames); `frame == 0` after a tick is
 * the point a consumer would reload its palette. */
void aos_palette_script_tick(AosPaletteScript *script);

/* The current script entry (0 for an unregistered instance). */
int aos_palette_script_phase(const AosPaletteScript *script);

/* The current entry's palette bank, or -1 when the script carries
 * no bank table. */
int aos_palette_script_bank(const AosPaletteScript *script);

/* sub_0803C150's phase resolution for an instance tracked only by
 * its elapsed frames (`age` = frames since registration, the door's
 * palette_age): walks the looping (duration) entries the same way
 * the native +8/+0xA counters do and returns the entry index.
 * `ticks` are the per-phase durations, `count` the script's entry
 * count; a NULL/empty script resolves to phase 0. */
int aos_palette_phase(uint32_t age, const uint8_t *ticks, int count);

#endif /* AOS_PALETTE_SCRIPT_H */
