#include "aos_menu_cursor.h"

#include <string.h>

uint32_t aos_menu_cursor_tick(AosMenuCursor *s, uint8_t *ctr,
                              const uint8_t *cols,
                              AosMenuSpanFn span_fn, void *ctx,
                              uint32_t input, bool *song) {
    uint32_t span = span_fn(cols[s->high], ctx);
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
