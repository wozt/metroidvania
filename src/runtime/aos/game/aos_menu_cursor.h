#ifndef FUSION_AOS_MENU_CURSOR_H
#define FUSION_AOS_MENU_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

/* sub_0806688C: the Aria menu cursor machine. `s` mirrors the
 * state bytes (+0x14 count, +0x15 scroll diff, +0x16 window low
 * (signed), +0x17 cursor high, +0xA mode). `cols` points at the
 * per-column selector bytes — the ROM reads entity+1+f17 at
 * entry. `ctr` is the per-call counter byte the ROM addresses as
 * base+0x3D1+f17 (computed by the caller with the entry f17).
 * `span_fn` resolves sub_08067660 over the 0x085269FC pair table
 * for the selected column; it runs once per call like the ROM.
 *
 * Input bits (priority order): 0x10 counter+, 0x20 counter-,
 * 0x40 cursor up, 0x80 cursor down, 0x100 page down,
 * 0x200 page up. Only the counter bits may fall through to the
 * next test when their guard fails; every other set bit commits
 * to the tail. Returns the ROM code: 0 nothing, 1 counter move,
 * 2 cursor move, 3 wrap/clamp (also sets *song for PlaySong
 * 0xF1). */
typedef struct {
    uint8_t count;      /* +0x14 */
    uint8_t diff;       /* +0x15 */
    int8_t  low;        /* +0x16 signed window low */
    uint8_t high;       /* +0x17 cursor high */
    uint8_t mode;       /* +0xA */
} AosMenuCursor;

typedef uint32_t (*AosMenuSpanFn)(uint8_t sel, void *ctx);

uint32_t aos_menu_cursor_tick(AosMenuCursor *s, uint8_t *ctr,
                              const uint8_t *cols,
                              AosMenuSpanFn span_fn, void *ctx,
                              uint32_t input, bool *song);

#endif

/* sub_08067660: menu span resolver — runs sub_08044E94(kind)
 * for its side effect, then returns the inventory byte at
 * gEwramData+0x13294+idx (kind 2), +0x132B4+idx (kind 3) or
 * +0x132EF+idx (kind 4); other kinds return the sub_08044E94
 * result itself. `desc_ret` injects that return. */
uint32_t aos_menu_span_67660(uint8_t kind, uint8_t idx,
                             const uint8_t *t13294,
                             const uint8_t *t132b4,
                             const uint8_t *t132ef,
                             uint32_t desc_ret);

/* sub_08067000: menu quantity adjust — decrements the per-column
 * counter byte by the column span, then per kind: default kinds
 * subtract into st[0x38+idx]; kind 3 into st[0x58+idx] clamping
 * to 0 with the st[0xC] selection reset to 0xFF + sound when the
 * count hits zero on the selected column; kind 4 into
 * st[0x93+idx] with the same clamp through st[0x10 + sign(desc8)
 * + idx]. The word total at st+0x34 gains desc.word4 / 2 * span,
 * the counter byte is overwritten with (span != 0), and the ROM
 * tail-calls the list rebuild sub_08066EA4 (injected). `st` is
 * the raw state buffer; `ctr` the sb+0x3D1+high byte computed by
 * the caller. */
typedef struct {
    int8_t  flags8;     /* descriptor byte 8 (sign for kind 4) */
    int32_t word4;      /* descriptor word 4 (halved into +0x34) */
} AosMenuDesc;

void aos_menu_adjust_67000(uint8_t *ctr, uint8_t high,
                           uint8_t *st, uint8_t kind, uint8_t idx,
                           const AosMenuDesc *desc, uint32_t span,
                           bool *sound, void (*rebuild)(void *),
                           void *ctx);
