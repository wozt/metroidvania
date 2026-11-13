#include "aos_menu_cursor.h"

#include <string.h>

uint32_t aos_menu_cursor_tick(AosMenuCursor *s, uint8_t *ctr,
                              const uint8_t *cols,
                              const uint8_t *pairs, uint32_t npairs,
                              AosMenuKindSpanFn span_fn, void *ctx,
                              uint32_t input, bool *song) {
    /* sel byte indexes the pair table; span = 67660(kind, idx) */
    uint8_t sel = cols[s->high];
    uint8_t pk = sel < npairs ? pairs[sel * 4] : 0;
    uint8_t pi = sel < npairs ? pairs[sel * 4 + 1] : 0;
    uint32_t span = span_fn(pk, pi, ctx);
    int32_t lo0 = s->low;
    uint32_t ret = 0;

    if (input & 0x10) {
        if (*ctr != 0) {
            if (s->mode == 2) {
                if (*ctr < span) { (*ctr)++; ret = 1; }
            } else if ((int32_t)*ctr + (int32_t)span <= 8) {
                (*ctr)++; ret = 1;
            }
            goto tail;      /* inner guards fail -> tail */
        }
        /* *ctr == 0 -> next test */
    }
    if (input & 0x20) {
        if (*ctr > 1) {
            (*ctr)--; ret = 1;
            goto tail;
        }
    }
    if (input & 0x40) {
        if (s->high == 0) goto tail;
        s->high = (uint8_t)(s->high - 1);
        if (s->low > (int32_t)s->high) s->low = (int8_t)s->high;
        ret = 2;
        goto tail;
    }
    if (input & 0x80) {
        if (s->high >= (int32_t)(int8_t)s->count - 1) goto tail;
        s->high = (uint8_t)(s->high + 1);
        if (s->low < (int32_t)s->high - 3)
            s->low = (int8_t)(s->high - 3);
        ret = 2;
        goto tail;
    }
    if (input & 0x100) {
        if (s->high >= (int32_t)(int8_t)s->count - 1) goto tail;
        s->low = (int8_t)(s->low + 4);
        s->high = (uint8_t)(s->high + 4);
        if (s->low > (int32_t)(int8_t)s->count - 4) {
            s->low = (int8_t)((uint8_t)s->count - 4);
            s->high = (uint8_t)(s->count - 1);
        }
        ret = 2;
        goto tail;
    }
    if (input & 0x200) {
        if (s->high == 0) goto tail;
        if (s->low <= 3) {
            s->low = 0; s->high = 0;
        } else {
            s->low = (int8_t)(s->low - 4);
            s->high = (uint8_t)(s->high - 4);
        }
        ret = 2;
    }
tail:
    s->diff = (uint8_t)(s->high - s->low);
    /* clamp: (int8)diff >= (int8)count && count != 0 */
    if ((int8_t)s->diff >= (int8_t)s->count && s->count != 0) {
        s->diff = (uint8_t)(s->count - 1);
        s->high = (uint8_t)(s->count - 1);
        ret = 3;
    }
    if (s->low != lo0) ret = 3;
    *song = (ret != 0);
    return ret;
}

uint32_t aos_menu_span_67660(uint8_t kind, uint8_t idx,
                             const uint8_t *t13294,
                             const uint8_t *t132b4,
                             const uint8_t *t132ef,
                             uint32_t desc_ret) {
    /* sub_08044E94(kind) side effect is not modelled; its return
     * value is what non-2/3/4 kinds yield. */
    switch (kind) {
    case 2: return t13294[idx];
    case 3: return t132b4[idx];
    case 4: return t132ef[idx];
    default: return desc_ret;
    }
}

void aos_menu_adjust_67000(uint8_t *ctr, uint8_t high,
                           uint8_t *st, uint8_t kind, uint8_t idx,
                           const AosMenuDesc *desc, uint32_t span,
                           bool *sound, void (*rebuild)(void *),
                           void *ctx) {
    /* sub_08067000 */
    uint8_t amount = *ctr;              /* r7 = counter byte */
    if (kind == 3) {
        int32_t v = (int32_t)st[0x58 + idx] - amount;
        if (v <= 0) {
            if (st[0xC] == idx) {
                st[0xC] = 0xFF;
                *sound = true;          /* sub_0804AD9C */
            }
            v = 0;
        }
        st[0x58 + idx] = (uint8_t)v;
    } else if (kind == 4) {
        uint32_t sign = (desc->flags8 < 0) ? 1u : 0u;
        int32_t v = (int32_t)st[0x93 + idx] - amount;
        if (v <= 0) {
            if (st[0x10 + sign + idx] == idx) {
                st[0x10 + sign + idx] = 0xFF;
                *sound = true;
            }
            v = 0;
        }
        st[0x93 + idx] = (uint8_t)v;
    } else {
        st[0x38 + idx] = (uint8_t)(st[0x38 + idx] - amount);
    }
    /* +0x34 += (desc.word4 / 2) * amount — C trunc division. */
    int32_t half = desc->word4 / 2;
    int32_t total;
    memcpy(&total, st + 0x34, sizeof total);
    total += half * (int32_t)amount;
    memcpy(st + 0x34, &total, sizeof total);
    /* counter byte <- (span != 0) */
    *ctr = (span != 0) ? 1 : 0;
    (void)high;
    if (rebuild) rebuild(ctx);          /* sub_08066EA4 tail */
}

int aos_menu_desc_kind(uint16_t id) {
    /* sub_08044F24 */
    if (id == 0) return 0;
    id--;
    if (id <= 0x1F) return AOS_MENU_KIND_ITEM;
    if (id <= 0x5A) return AOS_MENU_KIND_WEAPON;
    if (id <= 0x87) return AOS_MENU_KIND_ARMOR;
    return 0;
}

uint16_t aos_menu_desc_index(uint16_t id) {
    /* sub_08044F50 */
    if (id == 0) return 0;
    id--;
    if (id <= 0x1F) return id;
    if (id <= 0x5A) return (uint16_t)(id - 0x20);
    if (id <= 0x87) return (uint16_t)(id - 0x5B);
    return 0;
}

int aos_menu_record_44e94(uint8_t kind, uint8_t idx,
                          const uint8_t *items,
                          const uint8_t *weapons,
                          const uint8_t *armor) {
    /* sub_08044E94: record = table + idx * stride */
    switch (kind) {
    case AOS_MENU_KIND_ITEM:   return idx * 16;
    case AOS_MENU_KIND_WEAPON: return idx * 28;
    case AOS_MENU_KIND_ARMOR:  return idx * 20;
    default: return -1;
    }
    (void)items; (void)weapons; (void)armor;
}

int aos_menu_record_44edc(uint16_t id,
                          const uint8_t *items,
                          const uint8_t *weapons,
                          const uint8_t *armor) {
    /* sub_08044EDC: id-1 range picks table+stride; 0 -> -1 */
    if (id == 0) return -1;
    id--;
    if (id <= 0x1F) return id * 16;
    if (id <= 0x5A) return (id - 0x20) * 28;
    if (id <= 0x87) return (id - 0x5B) * 20;
    return -1;
    (void)items; (void)weapons; (void)armor;
}

void aos_menu_build_66ea4(uint8_t *ctr, uint8_t *list,
                          const uint8_t *pairs, uint32_t npairs,
                          AosMenuKindSpanFn span_fn, void *sctx,
                          uint32_t (*word4_at)(uint16_t id,
                                               void *ctx),
                          void *wctx) {
    /* sub_08066EA4 */
    list[0] = 0;
    uint8_t sb = 0;     /* ROM carries the caller's sb; 0 models it */
    for (uint16_t id = 0; id <= 0x87; id++) {
        uint8_t kind = (uint8_t)aos_menu_desc_kind(id);
        uint8_t idx = (uint8_t)aos_menu_desc_index(id);
        /* sub_08044E94(kind, idx) side effect not modelled */
        uint32_t w4 = word4_at ? word4_at(id, wctx) : 0;
        if (kind >= AOS_MENU_KIND_ITEM && kind <= AOS_MENU_KIND_ARMOR)
            sb = (uint8_t)span_fn(kind, idx, sctx);
        if (sb != 0 && w4 != 0) {
            int found = -1;
            for (uint32_t i = 0; i < npairs; i++) {
                if (pairs[i * 4 + 1] == idx &&
                    pairs[i * 4] == kind) {
                    found = (int)i;
                    break;
                }
            }
            if (found >= 0) {
                uint8_t c = list[0];
                list[0] = (uint8_t)(c + 1);
                list[1 + c] = (uint8_t)found;
            }
        }
    }
    memset(ctr, 0, 0x80);
    for (uint8_t i = 0; i < list[0]; i++) {
        uint8_t pi = list[1 + i];
        uint32_t span = span_fn(pairs[pi * 4], pairs[pi * 4 + 1],
                                sctx);
        ctr[i] = (span != 0) ? 1 : 0;
    }
}

void aos_menu_adjust_6711c(uint8_t *ctr, uint8_t *st,
                           uint8_t kind, uint8_t idx,
                           const AosMenuDesc *desc, uint8_t amount,
                           uint8_t mode_f) {
    /* sub_0806711C */
    uint8_t *cell;
    if (kind == AOS_MENU_KIND_WEAPON)      cell = st + 0x58 + idx;
    else if (kind == AOS_MENU_KIND_ARMOR) cell = st + 0x93 + idx;
    else                                  cell = st + 0x38 + idx;
    *cell = (uint8_t)(*cell + amount);
    int32_t w4 = desc->word4;
    if (mode_f == 9) w4 = (w4 * 8) / 10;   /* Div: (w4<<3)/10 */
    int32_t total;
    memcpy(&total, st + 0x34, sizeof total);
    total -= (int32_t)amount * w4;
    memcpy(st + 0x34, &total, sizeof total);
    *ctr = 1;
}

bool aos_menu_can_afford_671a4(uint8_t amount, uint8_t mode_a,
                               uint8_t mode_f, int32_t st34,
                               uint32_t span, int32_t word4) {
    /* sub_080671A4 */
    if (mode_a == 2) {
        int32_t half = word4 >> 1;      /* lsrs: logical */
        int32_t total = st34 + half * (int32_t)amount;
        return total <= 0xF423F && amount != 0;
    }
    if ((int32_t)span + amount > 9) return false;
    if (mode_f == 9) word4 = (word4 * 8) / 10;
    int32_t cost = word4 * (int32_t)amount;
    return st34 >= cost;
}

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
                              bool *window_ops) {
    /* sub_08067310 — state portion; DMA/window/text calls are
     * reported through window_ops. */
    *window_ops = true;
    cur->count = 0;
    cur->diff = 0;
    cur->low = 0;
    cur->high = 0;
    *mode_byte = 0;
    if (flags37e & 0x10) *mode_byte = 1;
    if (flags37e & 0x02) *mode_byte = 2;
    if (cur->mode == 2) {
        /* sub_08066EA4 + sub_08066B70 path */
        aos_menu_build_66ea4(ctr, list, pairs, npairs,
                             span_fn, sctx, word4_at, wctx);
        *built = true;
        return;
    }
    /* count-prefixed page list fills ctr with 1s */
    if (*mode_byte < npages && page_lists[*mode_byte]) {
        const uint8_t *pl = page_lists[*mode_byte];
        for (uint32_t i = 0; i < pl[0]; i++) ctr[i] = 1;
    }
    *built = false;
}

static uint32_t pair_field(const uint8_t *pairs, uint8_t idx,
                           uint32_t npairs, int field) {
    if (idx >= npairs) return 0;
    return pairs[idx * 4 + field];
}

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
                                   uint16_t *sys_flags) {
    /* sub_08067488 */
    uint32_t ret = 0;
    /* active column index byte: list[1 + high] */
    uint8_t colidx = list[1 + cur->high];
    uint8_t kind = (uint8_t)pair_field(pairs, colidx, npairs, 0);
    uint8_t pidx = (uint8_t)pair_field(pairs, colidx, npairs, 1);
    const AosMenuDesc *desc = desc_ki ? desc_ki(kind, pidx, dctx)
                                    : NULL;
    int32_t st34;
    memcpy(&st34, st + 0x34, sizeof st34);

    if (cur->ctr_c == 0) {
        if (input & 0x2) {
            if (song) song(0xF0, ctx);
            return 1;
        }
        if (input & 0x1) {
            /* afford check on the highlighted column */
            uint32_t span = span_fn ? span_fn(kind, pidx, sctx) : 0;
            int32_t w4 = desc ? desc->word4 : 0;
            if (aos_menu_can_afford_671a4(ctr[cur->high], cur->mode,
                                          st[0xF], st34, span, w4)) {
                if (popup) popup(ctx);           /* sub_0806724C */
                if (song) song(0xF3, ctx);
                cur->sub_d = 2;
                cur->ctr_c++;
            } else if (song) song(0xEF, ctx);
            return ret;
        }
        /* cursor move + optional re-render */
        bool song_out = false;
        uint32_t moved = aos_menu_cursor_tick(
            cur, &ctr[cur->high], cols, pairs, npairs, span_fn,
            sctx, input, &song_out);
        if (moved && render) render(moved, ctx);   /* 66B70 arg */
        return ret;
    }
    /* +0xC != 0 */
    if ((input & 0x1) && cur->sub_d == 0) {
        uint8_t *cell_ctr = &ctr[cur->high];
        if (cur->mode == 2) {
            bool snd = false;
            aos_menu_adjust_67000(cell_ctr, cur->high, st, kind,
                                  pidx, desc, span_fn ?
                                  span_fn(kind, pidx, sctx) : 0,
                                  &snd, rebuild, ctx);
        } else {
            aos_menu_adjust_6711c(cell_ctr, st, kind, pidx, desc,
                                  *cell_ctr, st[0xF]);
        }
        if (song) song(0xF5, ctx);
        if (render) render(3, ctx);
        /* gold redraw + digit position — injected via render */
        *menu_dirty |= 2;                            /* +0x7864 */
        *sys_flags &= 0x9FFF;                        /* 0x2CB0 hword */
        cur->ctr_c = 0;
        return ret;
    }
    if ((input & 0x3) != 0 && cur->sub_d == 0) {
        if (song) song(0xF0, ctx);
        *menu_dirty |= 2;
        *sys_flags &= 0x9FFF;
        cur->ctr_c = 0;
        return ret;
    }
    if ((input & 0x10) && cur->sub_d == 0) {
        if (song) song(0xF1, ctx);
        cur->sub_d++;
        return ret;
    }
    if ((input & 0x20) && cur->sub_d != 0) {
        if (song) song(0xF1, ctx);
        cur->sub_d = 0;
    }
    return ret;
}

static const uint8_t *menu_record_ptr(AosMenuDrawOps *ops,
                                      uint8_t kind, uint8_t idx) {
    if (!ops->record_at) return NULL;
    return ops->record_at(kind, idx, ops->ctx);
}

static uint16_t rec_word0(const uint8_t *rec) {
    uint16_t v = 0;
    if (rec) memcpy(&v, rec, sizeof v);
    return v;
}

static int32_t rec_word4(const uint8_t *rec) {
    int32_t v = 0;
    if (rec) memcpy(&v, rec + 4, sizeof v);
    return v;
}

void aos_menu_render_66b70(AosMenuCursor *cur, uint8_t *ctr,
                           const uint8_t *st, const uint8_t *list,
                           const uint8_t *pairs, uint32_t npairs,
                           uint32_t stack_arg,
                           AosMenuKindSpanFn span_fn, void *sctx,
                           AosMenuDrawOps *ops) {
    /* sub_08066B70 */
    uint8_t count = list[0];
    /* clamp: s8 low > count-4 -> low = count-4; s8 low < 0 -> 0 */
    if (cur->low > (int32_t)count - 4)
        cur->low = (int8_t)(count - 4);
    if (cur->low < 0) cur->low = 0;

    for (uint8_t i = 0; i < 3; i++) {
        int32_t row = (int32_t)cur->low + i;   /* r8 */
        if (stack_arg <= 2) {
            /* blank rows: dash marks only */
            if (ops->dash) ops->dash(0xE, 0xB + i, 3, ops->ctx);
            if (ops->qty_pos) ops->qty_pos(0x15, 0xB + i, ops->ctx);
            continue;
        }
        if (ops->cell) ops->cell(0, (uint8_t)(0xC + i), 0xF, 1,
                                 ops->ctx);
        if (ops->row_clear) ops->row_clear(i, ops->ctx);
        if (i >= count) {
            if (ops->dash) ops->dash(0xE, 0xB + i, 3, ops->ctx);
            if (ops->qty_pos) ops->qty_pos(0x15, 0xB + i, ops->ctx);
            continue;
        }
        /* name */
        uint8_t pidx = list[1 + row];
        uint8_t pk = pidx < npairs ? pairs[pidx * 4] : 0;
        uint8_t pi = pidx < npairs ? pairs[pidx * 4 + 1] : 0;
        const uint8_t *rec = menu_record_ptr(ops, pk, pi);
        uint16_t w0 = rec_word0(rec);
        if (i == 0 && count == 0) {
            if (ops->empty_name) ops->empty_name(ops->ctx);
        } else if (ops->name && ops->name_id) {
            ops->name(ops->name_id(w0, ops->ctx), ops->ctx);
        }
        /* quantity */
        uint8_t amount = ctr[row];
        int32_t w4 = rec_word4(rec);
        if (cur->mode == 2) {
            w4 = (int32_t)((uint32_t)w4 >> 1);   /* lsrs logical */
        } else if (st[0xF] == 9) {
            w4 = (w4 * 8) / 10;
        }
        uint16_t qty = (uint16_t)(w4 * (int32_t)amount);
        if (ops->qty) ops->qty(qty, 0xB + i, ops->ctx);
        if (ops->qty_pos) ops->qty_pos(0x15, 0xB + i, ops->ctx);
    }

    if (count == 0) {
        /* empty page: blanks + icon entity delete + mark clears */
        if (ops->blanks) ops->blanks(0xC, 6, 2, 0, ops->ctx);
        if (ops->del_icon) ops->del_icon(ops->ctx);
        if (ops->clear_icon) ops->clear_icon(ops->ctx);
        for (int k = 0; k < 6; k++)
            if (ops->arrow) ops->arrow(2, true, ops->ctx); /* 0x7FFF
                                                            * marks —
                                                            * folded */
        return;
    }

    /* tail: cursor writes + selected column span */
    cur->high = (uint8_t)(cur->low + cur->diff);
    cur->count = count;
    uint8_t sel = list[1 + cur->high];
    uint8_t sk = sel < npairs ? pairs[sel * 4] : 0;
    uint8_t si = sel < npairs ? pairs[sel * 4 + 1] : 0;
    uint32_t span = span_fn ? span_fn(sk, si, sctx) : 0;
    if (ops->qty) ops->qty((uint16_t)span, 6, ops->ctx);

    if (stack_arg > 1) {
        /* detail: description + icon + equip marks */
        const uint8_t *drec = menu_record_ptr(ops, sk, si);
        uint16_t w0 = rec_word0(drec);
        if (ops->clear_icon) ops->clear_icon(ops->ctx);
        if (ops->desc_text && ops->desc_id)
            ops->desc_text(ops->desc_id(w0, ops->ctx), ops->ctx);
        if (ops->icon_item) ops->icon_item(sk, si, ops->ctx);
        if (sk == AOS_MENU_KIND_ITEM) {
            for (int k = 0; k < 6; k++)
                if (ops->arrow) ops->arrow(2, true, ops->ctx);
        } else if (ops->icon_equip) {
            ops->icon_equip(sk, si, ops->ctx);
        }
    }

    /* arrow hints */
    if (cur->low > 0) {
        if (ops->arrow) ops->arrow(0, true, ops->ctx);
    } else if (ops->arrow) {
        ops->arrow(0, false, ops->ctx);
    }
    if ((int32_t)(int8_t)cur->count - 4 > (int32_t)cur->low) {
        if (ops->arrow) ops->arrow(1, true, ops->ctx);
    } else if (ops->arrow) {
        ops->arrow(1, false, ops->ctx);
    }
}

void aos_menu_equip_stats_66a00(uint8_t kind, uint8_t idx,
                                AosMenuEquipOps *ops) {
    /* sub_08066A00 */
    int16_t old[6];
    memcpy(old, ops->stats, sizeof old);
    uint8_t sv68 = *ops->slot68, sv6c = *ops->slot6c,
            sv6d = *ops->slot6d;
    if (kind == AOS_MENU_KIND_WEAPON) {
        *ops->slot68 = idx;
    } else if (kind == AOS_MENU_KIND_ARMOR) {
        uint8_t flag8 = ops->armor_rec8 ? ops->armor_rec8(idx, ops->ctx)
                                      : 0;
        if (flag8 == 0) *ops->slot6c = idx;
        else            *ops->slot6d = idx;
    }
    if (ops->recompute) ops->recompute(ops->ctx);
    if (ops->set_6b) ops->set_6b(*ops->slot6b, 1, ops->ctx);
    for (int i = 0; i < 6; i++) {
        int32_t diff = (int16_t)(uint16_t)(ops->stats[i] - old[i]);
        uint8_t mark;
        if (diff == 0)      mark = 3;
        else if (diff > 0)  mark = 9;
        else                mark = 8;
        int32_t v = ops->stats[i];
        if (v > 0x3E8) v = 0x3E7;
        uint8_t y = (uint8_t)(i * 2 + 2);
        if (ops->pos) ops->pos(0x18, y, ops->ctx);
        if (ops->stat) ops->stat(0x1B, y, v, 3, mark, 0, ops->ctx);
    }
    *ops->slot68 = sv68;
    *ops->slot6c = sv6c;
    *ops->slot6d = sv6d;
    if (ops->recompute) ops->recompute(ops->ctx);
    if (ops->set_6b) ops->set_6b(*ops->slot6b, 1, ops->ctx);
}

uint8_t aos_menu_scroll_67804(int32_t delta, AosMenuScrollOps *ops) {
    /* sub_08067804 */
    if (ops->disp4c) *ops->disp4c = 8;
    if (ops->disp48)
        *ops->disp48 = (uint16_t)((*ops->disp48 & 0xFF3F) | 0xC0 |
                                 0x3F) & 0xFFFE;
    if (ops->input) *ops->input |= 0x2100;
    uint8_t v = (uint8_t)(*ops->scroll + delta);   /* strb wrap */
    uint8_t clamped = 0;
    if (v > 0x23) { v = 0x24; clamped = 1; }
    else if (v == 0) { v = 0; clamped = 1; }
    *ops->scroll = v;
    uint32_t q = ((uint32_t)v << 5) / 0x24;
    if (ops->disp3c)
        *ops->disp3c = (uint16_t)(((0x7C - q) << 8) | (q + 0x7C));
    return clamped;
}

void aos_menu_close_67898(AosMenuCloseOps *ops) {
    /* sub_08067898 */
    if (ops->input) *ops->input &= 0x1FFF;
    if (ops->disp4c) *ops->disp4c = 0;
    if (ops->flags42c && (*ops->flags42c & 0x10000)) {
        if (ops->dma_setup) ops->dma_setup(ops->ctx);
    } else {
        if (ops->release) ops->release(ops->ctx);
        if (ops->flags42c) *ops->flags42c &= ~0x20u;
    }
    if (ops->state417) *ops->state417 = 0xFF;
}

void aos_menu_popup_6724c(AosMenuPopupOps *ops) {
    /* sub_0806724C — bank = bit3 of +0x7864 ? 0 : 1, stride
     * 0xA00 bytes; tilemap at bank +0xC0 (hw index 0x60). */
    uint16_t *bank = ops->win_buf + ((*ops->dirty & 8) ? 0 : 0x500);
    if (ops->hblank) ops->hblank(ops->ctx);
    uint16_t *p = bank + 0x61; /* byte +0xC2 */
    for (int i = 0; i < 0x40; i++) p[i] = 0x40;
    bank[0xA0] = 0; /* byte +0x140 */
    *ops->dirty |= 1;
    if (ops->input) *ops->input |= 0x6000;
    if (ops->display) {
        ops->display(0x40, 0x3E, ops->ctx);
        ops->display(0x38, 0x28F0, ops->ctx);
        ops->display(0x3C, 0x7898, ops->ctx);
        ops->display(0x42, 0x3F, ops->ctx);
        ops->display(0x41, 0x2F, ops->ctx);
        ops->display(0x3A, 0x28, ops->ctx);
        ops->display(0x3E, 0x7898, ops->ctx);
    }
}
