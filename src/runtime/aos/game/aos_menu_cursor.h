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
    uint8_t ctr_c;      /* +0xC adjust counter */
    uint8_t sub_d;      /* +0xD sub-mode */
} AosMenuCursor;

typedef uint32_t (*AosMenuKindSpanFn)(uint8_t kind, uint8_t idx,
                                      void *ctx);

uint32_t aos_menu_cursor_tick(AosMenuCursor *s, uint8_t *ctr,
                              const uint8_t *cols,
                              const uint8_t *pairs, uint32_t npairs,
                              AosMenuKindSpanFn span_fn, void *ctx,
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

/* Item/weapon/armor record resolvers (code_08040A38.s).
 * sub_08044F24 maps a global item id to its kind: 0 -> 0,
 * 1-0x20 -> 2 (item), 0x21-0x7B -> 3 (weapon), 0x7C-0xA8 -> 4
 * (armor), else 0. sub_08044F50 maps it to the index inside the
 * kind (id-1, id-0x21, id-0x7C; 0 for id 0 or out of range).
 * sub_08044E94(kind, idx) picks the record: kind 2 -> 16-byte
 * item records, kind 3 -> 28-byte weapon records, kind 4 ->
 * 20-byte armor records; other kinds yield NULL. sub_08044EDC
 * resolves by global id through three tables: ids 1-0x20 ->
 * 16-byte records, 0x21-0x7B -> 28-byte, 0x7C-0xA8 -> 20-byte;
 * id 0 or out of range yields NULL. All four return a byte
 * offset into the injected table (or -1 for NULL). */
#define AOS_MENU_KIND_ITEM  2
#define AOS_MENU_KIND_WEAPON 3
#define AOS_MENU_KIND_ARMOR 4

int aos_menu_desc_kind(uint16_t id);          /* sub_08044F24 */
uint16_t aos_menu_desc_index(uint16_t id);    /* sub_08044F50 */
int aos_menu_record_44e94(uint8_t kind, uint8_t idx,
                          const uint8_t *items,
                          const uint8_t *weapons,
                          const uint8_t *armor);
int aos_menu_record_44edc(uint16_t id,
                          const uint8_t *items,
                          const uint8_t *weapons,
                          const uint8_t *armor);

/* sub_08066EA4: menu list builder — walks global ids 0..0x87,
 * resolves kind/index through sub_08044F24/44F50, reads the
 * kind span byte (loop-carried `sb`: only kinds 2/3/4 refresh
 * it, other kinds reuse the previous value — the ROM's first
 * iteration inherits the caller's register, modelled as 0) and
 * the descriptor word4 through sub_08044EDC; ids with both
 * nonzero are looked up in the 0x085269FC pair table (stride 4,
 * first match wins, -1 skips) and their table index appended to
 * the list at +0x135F4 (count byte + one byte per entry). The
 * tail clears the 0x80 counter bytes at base+0x3D1 and writes
 * (span != 0) for each listed column. `span_fn` is the
 * sub_08067660 lookup, `word4_at` the descriptor word4 (0 when
 * the record is absent). */
void aos_menu_build_66ea4(uint8_t *ctr, uint8_t *list,
                          const uint8_t *pairs, uint32_t npairs,
                          AosMenuKindSpanFn span_fn, void *sctx,
                          uint32_t (*word4_at)(uint16_t id,
                                               void *ctx),
                          void *wctx);

/* sub_0806711C: menu quantity increment — the mirror of
 * sub_08067000 without the clamps: the column counter byte is
 * ADDED into st[0x38/+0x58/+0x93 + idx] per kind (u8 wrap), the
 * st+0x34 total loses amount * desc.word4 (or *8/10 when
 * st[0xF] == 9), and the counter byte is set to 1. No sound,
 * no zero handling, no rebuild. */
void aos_menu_adjust_6711c(uint8_t *ctr, uint8_t *st,
                           uint8_t kind, uint8_t idx,
                           const AosMenuDesc *desc, uint8_t amount,
                           uint8_t mode_f);

/* sub_080671A4: menu afford check — span = sub_08067660(pair),
 * desc word4 resolved by the caller. Menu mode +0xA == 2 is the
 * buy page: allowed when amount != 0 and st+0x34 + amount *
 * (word4 >> 1) <= 0xF423F. Other modes: allowed when
 * span + amount <= 9 and st+0x34 >= amount * word4' where
 * word4' = word4 * 8 / 10 when st[0xF] == 9 else word4.
 * Returns the ROM boolean. */
bool aos_menu_can_afford_671a4(uint8_t amount, uint8_t mode_a,
                               uint8_t mode_f, int32_t st34,
                               uint32_t span, int32_t word4);

/* sub_08067310: menu page init — clears the cursor bytes
 * (+0x14..+0x17), writes the +0x3D0 mode byte (0, then 1 when
 * gEwramData+0x37E bit 0x10, then 2 when bit 0x02 — later wins),
 * and either rebuilds the list through sub_08066EA4 + renders
 * through sub_08066B70 when cursor mode +0xA == 2, or fills the
 * 0x80 counter bytes with 1 up to the count of the count-
 * prefixed list page_lists[mode_byte] (table 0x08526C6C)
 * before the same render. The BG/DMA/window/text setup calls
 * are injected as flags. */
void aos_menu_page_init_67310(AosMenuCursor *cur, uint8_t *mode_byte,
                              uint8_t *ctr, uint8_t *list,
                              const uint8_t *pairs, uint32_t npairs,
                              uint16_t flags37e,
                              const uint8_t *const *page_lists,
                              uint32_t npages,
                              AosMenuKindSpanFn span_fn, void *sctx,
                              uint32_t (*word4_at)(uint16_t id,
                                                   void *ctx),
                              void *wctx, bool *built,
                              bool *window_ops);

/* sub_08067488: menu page input handler — resolves the active
 * list (built +0x135F4 when cursor mode +0xA == 2, else the
 * count-prefixed page list for the +0x3D0 mode byte) and
 * dispatches the gEwramData+0x18 input bits in ROM order:
 * with +0xC == 0, bit 0x2 returns 1 after PlaySong(0xF0), bit
 * 0x1 runs the afford check on the highlighted column
 * (sub_080671A4) then sub_0806724C + PlaySong(0xF3) +
 * sub_d = 2 + ctr_c++ on success or PlaySong(0xEF) on failure,
 * anything else runs the cursor sub_0806688C and re-renders
 * through sub_08066B70 when it moved; with +0xC != 0, bit 0x1
 * and sub_d == 0 adjusts the quantity (sub_08067000 on mode 2,
 * sub_0806711C otherwise), re-renders, redraws the gold counter
 * and clears ctr_c; bit 0x3 confirms (PlaySong(0xF0), dirty
 * flags, ctr_c = 0); bit 0x10 enters a sub-mode and bit 0x20
 * leaves it (both PlaySong(0xF1)). Songs, the 66B70 render, the
 * 6724C popup, the gold redraw and the dirty/system flag writes
 * are injected; returns the ROM value (1 only on bit 0x2). */
typedef const AosMenuDesc *(*AosMenuDescKiFn)(uint8_t kind,
                                              uint8_t idx,
                                              void *ctx);
typedef void (*AosMenuSongFn)(uint16_t id, void *ctx);
typedef void (*AosMenuRenderFn)(uint32_t arg, void *ctx);

uint32_t aos_menu_page_input_67488(AosMenuCursor *cur,
                                   uint8_t *ctr, uint8_t *st,
                                   const uint8_t *cols,
                                   const uint8_t *list,
                                   const uint8_t *pairs,
                                   uint32_t npairs, uint16_t input,
                                   AosMenuKindSpanFn span_fn,
                                   void *sctx,
                                   AosMenuDescKiFn desc_ki,
                                   void *dctx,
                                   AosMenuSongFn song,
                                   AosMenuRenderFn render,
                                   void (*popup)(void *),
                                   void (*rebuild)(void *),
                                   void *ctx,
                                   uint8_t *menu_dirty,
                                   uint16_t *sys_flags);

/* sub_08066B70: menu list renderer — clamps the signed +0x16
 * window low against count-4 (floor 0), draws up to 3 visible
 * rows from list[1 + low + i]: cell rect + clear, name through
 * the 0x08506734 halfword table when the row is in range (text
 * 0x34F on the empty first row), dash marks past the count,
 * quantity right-aligned through 0x08506936/word4 with the
 * mode-2 halving and mode-9 *8/10 rule. The tail writes
 * +0x17 = low + diff, +0x14 = count, resolves the selected
 * column's span and, for stack_arg > 1, the description text,
 * the icon entity (sub_08048804 items / sub_08066A00 others)
 * and the 0x7FFF equipment-mark clears; stack_arg <= 1 skips
 * the detail. Finally the up/down arrow hints compare low
 * against 0 and count-4. Every draw call is injected through
 * AosMenuDrawOps; record bytes come from record_at (kind
 * dispatch: <3 items 16B, 3 weapons 28B, 4 armor 20B). */
typedef struct {
    void *ctx;
    void (*cell)(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                 void *ctx);                    /* sub_08040970 */
    void (*row_clear)(uint8_t slot, void *ctx); /* sub_08041204 */
    void (*name)(uint16_t text, void *ctx);     /* 41434+41338 */
    void (*empty_name)(void *ctx);              /* text 0x34F */
    void (*qty)(uint16_t value, uint8_t slot, void *ctx); /* 46EF4 */
    void (*qty_pos)(uint8_t x, uint8_t slot, void *ctx); /* 46E04 */
    void (*dash)(uint8_t x, uint8_t slot, uint8_t n, void *ctx);
    void (*blanks)(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                   void *ctx);                  /* sub_08047098 */
    void (*icon_item)(uint8_t kind, uint8_t idx, void *ctx); /* 48804 */
    void (*icon_equip)(uint8_t kind, uint8_t idx, void *ctx);/* 66A00 */
    void (*del_icon)(void *ctx);                /* EntityDelete */
    void (*clear_icon)(void *ctx);              /* sub_08040FE0 */
    void (*desc_text)(uint16_t word0, void *ctx);/* sub_08047390 */
    void (*arrow)(uint8_t which, bool textured, void *ctx);
    uint16_t (*name_id)(uint16_t word0, void *ctx);  /* 0x08506734 */
    uint16_t (*desc_id)(uint16_t word0, void *ctx);  /* 0x08506936 */
    const uint8_t *(*record_at)(uint8_t kind, uint8_t idx,
                                void *ctx);
} AosMenuDrawOps;

void aos_menu_render_66b70(AosMenuCursor *cur, uint8_t *ctr,
                           const uint8_t *st, const uint8_t *list,
                           const uint8_t *pairs, uint32_t npairs,
                           uint32_t stack_arg,
                           AosMenuKindSpanFn span_fn, void *sctx,
                           AosMenuDrawOps *ops);
