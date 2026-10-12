/* SPDX-License-Identifier: GPL-3.0-only */
/* GBA PSG (the four Game Boy sound channels) driven by writes to the sound
 * registers 0x04000060-0x0400009F, and the GBA output mixer of SOUNDCNT_L,
 * SOUNDCNT_H and the two DirectSound FIFO samples. Hardware emulation, not
 * game code: the sound engine (mzm_audio) writes the registers as the game
 * does and reads the mixed output. */
#ifndef GBA_PSG_H
#define GBA_PSG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool on;
    int length;                     /* length counter */
    int volume, env_timer;          /* envelope */
    int period;                     /* cycles left before the next step */
    int position;                   /* duty step, wave sample */
    int sweep_timer, sweep_shadow;  /* channel 1 */
    bool sweep_on;
    uint16_t lfsr;                  /* channel 4 */
} GbaPsgChannel;

typedef struct {
    uint8_t regs[0x50];             /* 0x04000060-0x040000AF */
    uint8_t wave[2][16];            /* the two wave RAM banks */
    GbaPsgChannel ch[4];
    int sequencer_cycles, sequencer_step;
} GbaPsg;

/* SOUNDCNT_X off, everything cleared. */
void gba_psg_reset(GbaPsg *psg);
void gba_psg_write8(GbaPsg *psg, uint32_t address, uint8_t value);
void gba_psg_write16(GbaPsg *psg, uint32_t address, uint16_t value);
void gba_psg_write32(GbaPsg *psg, uint32_t address, uint32_t value);
uint8_t gba_psg_read8(const GbaPsg *psg, uint32_t address);
uint16_t gba_psg_read16(const GbaPsg *psg, uint32_t address);
/* Advance the channels by `cycles` of the 4.194304 MHz PSG clock. */
void gba_psg_step(GbaPsg *psg, int cycles);
/* The stereo output for the current PSG state and the DirectSound FIFO A
 * and B samples, as signed values (roughly +-2048). */
void gba_psg_mix(const GbaPsg *psg, int8_t fifo_a, int8_t fifo_b, int *left, int *right);

#endif /* GBA_PSG_H */
