/* SPDX-License-Identifier: GPL-3.0-only */
/* GBA PSG and output mixer (see gba_psg.h). Register offsets are relative to
 * 0x04000060 (SOUND1CNT_L). */
#include "gba_psg.h"

#include <string.h>

enum {
    BASE = 0x04000060,
    SOUND1CNT_L = 0x00, SOUND1CNT_H = 0x02, SOUND1CNT_X = 0x04,
    SOUND2CNT_L = 0x08, SOUND2CNT_H = 0x0C,
    SOUND3CNT_L = 0x10, SOUND3CNT_H = 0x12, SOUND3CNT_X = 0x14,
    SOUND4CNT_L = 0x18, SOUND4CNT_H = 0x1C,
    SOUNDCNT_L = 0x20, SOUNDCNT_H = 0x22, SOUNDCNT_X = 0x24,
    WAVE_RAM = 0x30,
    /* 512 Hz frame sequencer of the 4.194304 MHz clock. */
    SEQUENCER_PERIOD = 8192,
};

static const uint8_t duties[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 0},
};

static uint16_t reg16(const GbaPsg *psg, int offset) {
    return (uint16_t)(psg->regs[offset] | psg->regs[offset + 1] << 8);
}

static int frequency(const GbaPsg *psg, int control) {
    return reg16(psg, control) & 0x7FF;
}

/* The cycles of one step: a duty step (squares), a wave sample, an LFSR
 * shift (noise). */
static int period(const GbaPsg *psg, int channel) {
    switch (channel) {
    case 0: return (2048 - frequency(psg, SOUND1CNT_X)) * 4;
    case 1: return (2048 - frequency(psg, SOUND2CNT_H)) * 4;
    case 2: return (2048 - frequency(psg, SOUND3CNT_X)) * 2;
    default: {
        uint16_t control = reg16(psg, SOUND4CNT_H);
        int ratio = control & 7, shift = control >> 4 & 15;
        int divisor = ratio ? ratio * 16 : 8;
        return divisor << shift;
    }
    }
}

static int envelope_register(int channel) {
    return channel == 0 ? SOUND1CNT_H + 1 : channel == 1 ? SOUND2CNT_L + 1 : SOUND4CNT_L + 1;
}

static void trigger(GbaPsg *psg, int channel) {
    GbaPsgChannel *c = &psg->ch[channel];
    c->on = true;
    if (c->length == 0) c->length = channel == 2 ? 256 : 64;
    c->period = period(psg, channel);
    if (channel != 2) {
        uint8_t envelope = psg->regs[envelope_register(channel)];
        c->volume = envelope >> 4;
        c->env_timer = envelope & 7;
        /* A DAC turned off (initial volume 0 decreasing) silences it. */
        if ((envelope & 0xF8) == 0) c->on = false;
    } else {
        c->position = 0;
        if (!(psg->regs[SOUND3CNT_L] & 0x80)) c->on = false;
    }
    if (channel == 3) c->lfsr = 0x7FFF;
    if (channel == 0) {
        uint8_t sweep = psg->regs[SOUND1CNT_L];
        c->sweep_shadow = frequency(psg, SOUND1CNT_X);
        c->sweep_timer = sweep >> 4 & 7;
        c->sweep_on = (sweep & 0x70) || (sweep & 7);
    }
}

void gba_psg_reset(GbaPsg *psg) {
    memset(psg, 0, sizeof *psg);
}

/* The wave RAM bank the CPU accesses: the one not selected for playback. */
static int cpu_bank(const GbaPsg *psg) {
    return psg->regs[SOUND3CNT_L] & 0x40 ? 0 : 1;
}

void gba_psg_write8(GbaPsg *psg, uint32_t address, uint8_t value) {
    if (address < BASE || address >= BASE + sizeof psg->regs) return;
    int offset = (int)(address - BASE);
    if (offset >= WAVE_RAM && offset < WAVE_RAM + 16) {
        psg->wave[cpu_bank(psg)][offset - WAVE_RAM] = value;
        return;
    }
    if (offset == SOUNDCNT_X) {
        psg->regs[offset] = (uint8_t)((psg->regs[offset] & 0x0F) | (value & 0x80));
        if (!(value & 0x80)) {
            for (int i = 0; i < 4; ++i) psg->ch[i].on = false;
        }
        return;
    }
    psg->regs[offset] = value;
    switch (offset) {
    case SOUND1CNT_H: psg->ch[0].length = 64 - (value & 63); break;
    case SOUND2CNT_L: psg->ch[1].length = 64 - (value & 63); break;
    case SOUND3CNT_H: psg->ch[2].length = 256 - value; break;
    case SOUND4CNT_L: psg->ch[3].length = 64 - (value & 63); break;
    case SOUND1CNT_X + 1: if (value & 0x80) trigger(psg, 0); break;
    case SOUND2CNT_H + 1: if (value & 0x80) trigger(psg, 1); break;
    case SOUND3CNT_X + 1: if (value & 0x80) trigger(psg, 2); break;
    case SOUND4CNT_H + 1: if (value & 0x80) trigger(psg, 3); break;
    case SOUND3CNT_L:
        if (!(value & 0x80)) psg->ch[2].on = false;
        break;
    }
    /* The trigger bits read back as 0. */
    if (offset == SOUND1CNT_X + 1 || offset == SOUND2CNT_H + 1 || offset == SOUND3CNT_X + 1 ||
        offset == SOUND4CNT_H + 1)
        psg->regs[offset] &= 0x7F;
}

void gba_psg_write16(GbaPsg *psg, uint32_t address, uint16_t value) {
    gba_psg_write8(psg, address, (uint8_t)value);
    gba_psg_write8(psg, address + 1, (uint8_t)(value >> 8));
}

void gba_psg_write32(GbaPsg *psg, uint32_t address, uint32_t value) {
    gba_psg_write16(psg, address, (uint16_t)value);
    gba_psg_write16(psg, address + 2, (uint16_t)(value >> 16));
}

uint8_t gba_psg_read8(const GbaPsg *psg, uint32_t address) {
    if (address < BASE || address >= BASE + sizeof psg->regs) return 0;
    int offset = (int)(address - BASE);
    if (offset >= WAVE_RAM && offset < WAVE_RAM + 16)
        return psg->wave[cpu_bank(psg)][offset - WAVE_RAM];
    if (offset == SOUNDCNT_X) {
        uint8_t status = 0;
        for (int i = 0; i < 4; ++i)
            if (psg->ch[i].on) status |= (uint8_t)(1 << i);
        return (uint8_t)((psg->regs[offset] & 0x80) | status);
    }
    return psg->regs[offset];
}

uint16_t gba_psg_read16(const GbaPsg *psg, uint32_t address) {
    return (uint16_t)(gba_psg_read8(psg, address) | gba_psg_read8(psg, address + 1) << 8);
}

static int control_register(int channel) {
    return channel == 0 ? SOUND1CNT_X : channel == 1 ? SOUND2CNT_H :
           channel == 2 ? SOUND3CNT_X : SOUND4CNT_H;
}

/* One 512 Hz frame sequencer step: length at 256 Hz, sweep at 128 Hz,
 * envelopes at 64 Hz. */
static void sequencer(GbaPsg *psg) {
    int step = psg->sequencer_step;
    psg->sequencer_step = (step + 1) & 7;
    if (!(step & 1)) {
        for (int i = 0; i < 4; ++i) {
            GbaPsgChannel *c = &psg->ch[i];
            if (!(psg->regs[control_register(i) + 1] & 0x40) || c->length <= 0) continue;
            if (--c->length == 0) c->on = false;
        }
    }
    if (step == 2 || step == 6) {
        GbaPsgChannel *c = &psg->ch[0];
        uint8_t sweep = psg->regs[SOUND1CNT_L];
        int time = sweep >> 4 & 7;
        if (c->sweep_on && time && --c->sweep_timer <= 0) {
            c->sweep_timer = time;
            int delta = c->sweep_shadow >> (sweep & 7);
            int next = sweep & 8 ? c->sweep_shadow - delta : c->sweep_shadow + delta;
            if (next > 2047) {
                c->on = false;
            } else if (sweep & 7) {
                c->sweep_shadow = next;
                psg->regs[SOUND1CNT_X] = (uint8_t)next;
                psg->regs[SOUND1CNT_X + 1] =
                    (uint8_t)((psg->regs[SOUND1CNT_X + 1] & 0xF8) | (next >> 8 & 7));
            }
        }
    }
    if (step == 7) {
        for (int i = 0; i < 4; ++i) {
            if (i == 2) continue;
            GbaPsgChannel *c = &psg->ch[i];
            uint8_t envelope = psg->regs[envelope_register(i)];
            int time = envelope & 7;
            if (!time || --c->env_timer > 0) continue;
            c->env_timer = time;
            if (envelope & 8) {
                if (c->volume < 15) c->volume++;
            } else if (c->volume > 0) {
                c->volume--;
            }
        }
    }
}

void gba_psg_step(GbaPsg *psg, int cycles) {
    for (int i = 0; i < 4; ++i) {
        GbaPsgChannel *c = &psg->ch[i];
        if (!c->on) continue;
        int p = period(psg, i);
        if (p <= 0) p = 1;
        c->period -= cycles;
        while (c->period <= 0) {
            c->period += p;
            if (i < 2) {
                c->position = (c->position + 1) & 7;
            } else if (i == 2) {
                int length = psg->regs[SOUND3CNT_L] & 0x20 ? 64 : 32;
                c->position = (c->position + 1) % length;
            } else {
                int bit = (c->lfsr ^ (c->lfsr >> 1)) & 1;
                c->lfsr = (uint16_t)((c->lfsr >> 1) | bit << 14);
                if (reg16(psg, SOUND4CNT_H) & 8)
                    c->lfsr = (uint16_t)((c->lfsr & ~0x40) | bit << 6);
            }
        }
    }
    psg->sequencer_cycles += cycles;
    while (psg->sequencer_cycles >= SEQUENCER_PERIOD) {
        psg->sequencer_cycles -= SEQUENCER_PERIOD;
        sequencer(psg);
    }
}

/* A channel's output, centred (-15..15). */
static int channel_output(const GbaPsg *psg, int channel) {
    const GbaPsgChannel *c = &psg->ch[channel];
    if (!c->on) return 0;
    if (channel < 2) {
        int duty = psg->regs[channel == 0 ? SOUND1CNT_H : SOUND2CNT_L] >> 6;
        return duties[duty][c->position] ? c->volume : -c->volume;
    }
    if (channel == 2) {
        uint8_t control = psg->regs[SOUND3CNT_L];
        int bank = control & 0x40 ? 1 : 0;
        int position = c->position;
        if (control & 0x20) {
            bank ^= position >> 5;
            position &= 31;
        }
        uint8_t byte = psg->wave[bank][position >> 1];
        int sample = position & 1 ? byte & 15 : byte >> 4;
        uint8_t volume = psg->regs[SOUND3CNT_H + 1];
        int centred = sample * 2 - 15;
        if (volume & 0x80) return centred * 3 / 4;
        switch (volume >> 5 & 3) {
        case 0: return 0;
        case 1: return centred;
        case 2: return centred / 2;
        default: return centred / 4;
        }
    }
    return c->lfsr & 1 ? -c->volume : c->volume;
}

void gba_psg_mix(const GbaPsg *psg, int8_t fifo_a, int8_t fifo_b, int *left, int *right) {
    int out_left = 0, out_right = 0;
    if (psg->regs[SOUNDCNT_X] & 0x80) {
        uint16_t control = reg16(psg, SOUNDCNT_L);
        int psg_left = 0, psg_right = 0;
        for (int i = 0; i < 4; ++i) {
            int sample = channel_output(psg, i);
            if (control & 1 << (8 + i)) psg_right += sample;
            if (control & 1 << (12 + i)) psg_left += sample;
        }
        psg_right *= (control & 7) + 1;
        psg_left *= (control >> 4 & 7) + 1;
        uint16_t high = reg16(psg, SOUNDCNT_H);
        int shift = 2 - (high & 3);     /* 25%, 50%, 100% */
        if (shift < 0) shift = 0;
        out_left = psg_left >> shift;
        out_right = psg_right >> shift;
        int a = fifo_a * (high & 4 ? 4 : 2), b = fifo_b * (high & 8 ? 4 : 2);
        if (high & 0x100) out_right += a;
        if (high & 0x200) out_left += a;
        if (high & 0x1000) out_right += b;
        if (high & 0x2000) out_left += b;
    }
    *left = out_left;
    *right = out_right;
}
