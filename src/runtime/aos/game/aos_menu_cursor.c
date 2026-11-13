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
