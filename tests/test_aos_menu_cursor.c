#include "../src/runtime/aos/game/aos_menu_cursor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int rebuild_calls;
static void rebuild_count(void *c) { (void)c; rebuild_calls++; }

static uint32_t test_span(uint8_t kind, uint8_t idx, void *ctx) {
    (void)ctx;
    /* span = kind*10 + idx for the test */
    return kind * 10 + idx;
}
static uint32_t test_w4(uint16_t id, void *ctx) {
    (void)ctx;
    return id == 0 ? 0 : 4;    /* every real id has word4 = 4 */
}

static uint32_t span3(uint8_t sel, void *ctx) {
    (void)sel; (void)ctx; return 3;
}
static uint32_t span5(uint8_t sel, void *ctx) {
    (void)sel; (void)ctx; return 5;
}

int main(void) {
    AosMenuCursor s; uint8_t ctr = 0, cols[8] = {0};
    bool song;

    /* counter+ : mode!=2 needs ctr+span <= 8. */
    memset(&s, 0, sizeof s); s.count = 10; s.mode = 0;
    ctr = 0;
    uint32_t r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL,
                                      0x10, &song);
    assert(r == 0 && ctr == 0);         /* ctr==0 -> fall through */
    ctr = 4;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x10, &song);
    assert(r == 1 && ctr == 5 && song); /* 4+3 <= 8 */
    ctr = 6;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x10, &song);
    assert(r == 0 && ctr == 6 && !song); /* 6+3 > 8 -> tail */
    /* mode==2 bound is ctr < span. */
    memset(&s, 0, sizeof s); s.count = 10; s.mode = 2;
    ctr = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x10, &song);
    assert(r == 1 && ctr == 3 && song);
    ctr = 3;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x10, &song);
    assert(r == 0 && ctr == 3 && !song);
    /* counter- needs ctr > 1. */
    memset(&s, 0, sizeof s); s.count = 10;
    ctr = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x20, &song);
    assert(r == 0 && ctr == 1 && !song);
    ctr = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x20, &song);
    assert(r == 1 && ctr == 1 && song);
    /* cursor up: high>0, low clamps to high. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 4; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x40, &song);
    assert(r == 2 && s.high == 3 && s.low == 2 && song);
    s.high = 1; s.low = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x40, &song);
    assert(s.high == 0 && s.low == 0);
    /* low stays when <= high. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 3; s.low = -5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x40, &song);
    assert(s.high == 2 && s.low == -5);
    /* cursor down: bound count-1, low window follows at -3. */
    memset(&s, 0, sizeof s); s.count = 6; s.high = 3; s.low = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x80, &song);
    assert(r == 3 && s.high == 4 && s.low == 1); /* low moved -> 3 */
    s.high = 5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x80, &song);
    assert(r == 0 && s.high == 5);       /* high >= count-1 */
    /* page down 0x100: +4 each, clamp to count-4/count-1. */
    memset(&s, 0, sizeof s); s.count = 20; s.high = 4; s.low = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x100, &song);
    assert(r == 3 && s.high == 8 && s.low == 5); /* low moved */
    s.high = 18; s.low = 10;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x100, &song);
    /* low 14 <= count-4 16 -> no clamp: high wraps to 22 like
     * the ROM; low moved -> ret 3. */
    assert(r == 3 && s.high == 22 && s.low == 14);
    /* page up 0x200: -4 each; low<=3 resets both. */
    memset(&s, 0, sizeof s); s.count = 20; s.high = 8; s.low = 5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x200, &song);
    assert(r == 3 && s.high == 4 && s.low == 1);
    memset(&s, 0, sizeof s); s.count = 20; s.high = 6; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x200, &song);
    assert(r == 3 && s.high == 0 && s.low == 0); /* low moved -> 3 */
    /* bit priority: 0x10 set with ctr==0 falls to 0x40. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 5; s.low = 3;
    ctr = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL,
                             0x10 | 0x40, &song);
    assert(r == 2 && s.high == 4);
    /* no input -> ret 0 but diff still written. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 5; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0, &song);
    assert(r == 0 && s.diff == 3 && !song);
    /* clamp: diff >= count -> ret 3 even without input. */
    memset(&s, 0, sizeof s); s.count = 4; s.high = 9; s.low = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0, &song);
    assert(r == 3 && s.high == 3 && s.diff == 3);
    /* low window follows on page down even near the bound. */
    memset(&s, 0, sizeof s); s.count = 6; s.high = 4; s.low = 4;
    r = aos_menu_cursor_tick(&s, &ctr, cols, span3, NULL, 0x80, &song);
    /* low 4 >= high-3=2 -> window stays; ret 2. */
    assert(r == 2 && s.high == 5 && s.low == 4);

    /* sub_08067660 span resolver. */
    static const uint8_t t94[4] = {9, 8, 7, 6};
    static const uint8_t tb4[4] = {5, 4, 3, 2};
    static const uint8_t tef[4] = {1, 1, 1, 1};
    assert(aos_menu_span_67660(2, 1, t94, tb4, tef, 0x9999) == 8);
    assert(aos_menu_span_67660(3, 2, t94, tb4, tef, 0x9999) == 3);
    assert(aos_menu_span_67660(4, 0, t94, tb4, tef, 0x9999) == 1);
    assert(aos_menu_span_67660(0, 0, t94, tb4, tef, 0x9999) == 0x9999);
    assert(aos_menu_span_67660(7, 0, t94, tb4, tef, 0x9999) == 0x9999);

    /* sub_08067000 quantity adjust. */
    {
        uint8_t st[0xA0]; uint8_t ctr = 0; bool snd = false;
        AosMenuDesc d = {0, 100};
        rebuild_calls = 0;
        /* default kind: plain subtract into +0x38. */
        memset(st, 0, sizeof st); st[0x38 + 5] = 20; ctr = 3;
        aos_menu_adjust_67000(&ctr, 0, st, 1, 5, &d, 7, &snd,
                              rebuild_count, NULL);
        assert(st[0x38 + 5] == 17 && ctr == 1 && snd == false &&
               rebuild_calls == 1);
        int32_t tot; memcpy(&tot, st + 0x34, 4);
        assert(tot == 50 * 3);           /* word4/2 * amount */
        /* kind 3: clamp + selection reset + sound. */
        memset(st, 0, sizeof st); st[0x58 + 2] = 2; st[0xC] = 2;
        ctr = 5; snd = false;
        aos_menu_adjust_67000(&ctr, 0, st, 3, 2, &d, 4, &snd,
                              rebuild_count, NULL);
        assert(st[0x58 + 2] == 0 && st[0xC] == 0xFF && snd);
        /* kind 3 nonzero stays, no sound. */
        memset(st, 0, sizeof st); st[0x58 + 2] = 9; st[0xC] = 2;
        ctr = 4; snd = false;
        aos_menu_adjust_67000(&ctr, 0, st, 3, 2, &d, 4, &snd,
                              rebuild_count, NULL);
        assert(st[0x58 + 2] == 5 && st[0xC] == 2 && !snd);
        /* kind 4 sign picks the +0x10/+0x11 selection row. */
        memset(st, 0, sizeof st); st[0x93 + 1] = 1;
        st[0x11 + 1] = 1;                /* flags8 negative -> +0x11 */
        AosMenuDesc dn = {-3, 0}; ctr = 5; snd = false;
        aos_menu_adjust_67000(&ctr, 0, st, 4, 1, &dn, 9, &snd,
                              rebuild_count, NULL);
        assert(st[0x93 + 1] == 0 && st[0x11 + 1] == 0xFF && snd);
        /* span 0 -> counter cleared. */
        memset(st, 0, sizeof st); ctr = 2; snd = false;
        aos_menu_adjust_67000(&ctr, 0, st, 1, 0, &d, 0, &snd,
                              NULL, NULL);
        assert(ctr == 0);
    }

    /* sub_08044F24/44F50/44E94/44EDC record resolvers. */
    assert(aos_menu_desc_kind(0) == 0);
    assert(aos_menu_desc_kind(1) == 2 && aos_menu_desc_kind(0x20) == 2);
    assert(aos_menu_desc_kind(0x21) == 3 && aos_menu_desc_kind(0x5B) == 3);
    assert(aos_menu_desc_kind(0x5C) == 4 && aos_menu_desc_kind(0x88) == 4);
    assert(aos_menu_desc_kind(0x89) == 0 && aos_menu_desc_kind(0xFFFF) == 0);
    assert(aos_menu_desc_kind(0x7B) == 4);   /* boundary: armor side */
    assert(aos_menu_desc_index(1) == 0 && aos_menu_desc_index(0x20) == 0x1F);
    assert(aos_menu_desc_index(0x21) == 0 && aos_menu_desc_index(0x5B) == 0x3A);
    assert(aos_menu_desc_index(0x5C) == 0 && aos_menu_desc_index(0x88) == 0x2C);
    assert(aos_menu_record_44e94(2, 3, NULL, NULL, NULL) == 48);
    assert(aos_menu_record_44e94(3, 2, NULL, NULL, NULL) == 56);
    assert(aos_menu_record_44e94(4, 4, NULL, NULL, NULL) == 80);
    assert(aos_menu_record_44e94(9, 0, NULL, NULL, NULL) == -1);
    assert(aos_menu_record_44edc(0, NULL, NULL, NULL) == -1);
    assert(aos_menu_record_44edc(1, NULL, NULL, NULL) == 0);
    assert(aos_menu_record_44edc(0x20, NULL, NULL, NULL) == 0x1F * 16);
    assert(aos_menu_record_44edc(0x21, NULL, NULL, NULL) == 0);
    assert(aos_menu_record_44edc(0x5B, NULL, NULL, NULL) == 0x3A * 28);
    assert(aos_menu_record_44edc(0x5C, NULL, NULL, NULL) == 0);
    assert(aos_menu_record_44edc(0x88, NULL, NULL, NULL) == 0x2C * 20);
    assert(aos_menu_record_44edc(0x89, NULL, NULL, NULL) == -1);

    /* sub_08066EA4 list builder. */
    {
        /* pairs: {kind2,idx5} at 0, {kind3,idx2} at 1, {kind2,idx9}
         * at 2 (npairs=3). ids: 0x06 -> kind2 idx5; 0x23 -> kind3
         * idx2; 0x0A -> kind2 idx9. */
        static const uint8_t pairs[] = {
            2, 5, 0, 0,   3, 2, 0, 0,   2, 9, 0, 0 };
        uint8_t list[0x90]; uint8_t ctr[0x80];
        memset(list, 0xAA, sizeof list); memset(ctr, 0xAA, sizeof ctr);
        aos_menu_build_66ea4(ctr, list, pairs, 3, test_span, NULL,
                             test_w4, NULL);
        /* ids 6, 0x23, 0x0A all have span (kind*10+idx != 0) and
         * word4 -> listed in id order: 6 -> pair0, 0x0A -> pair2,
         * 0x23 -> pair1. */
        assert(list[0] == 3);
        assert(list[1] == 0 && list[2] == 2 && list[3] == 1);
        /* ctr[i] = span of the listed pair != 0 -> all 1. */
        for (int i = 0; i < 3; i++) assert(ctr[i] == 1);
        assert(ctr[3] == 0);            /* memset tail */
    }
    /* id without a pair match is skipped; span-0 kinds reuse the
     * carried sb but still need word4 and a pair. */
    {
        static const uint8_t pairs[] = { 2, 5, 0, 0 };
        static uint32_t span_calls;
        /* kind2 idx5 -> span 25; make word4 zero for id 6 only. */
        uint8_t list[0x90]; uint8_t ctr[0x80];
        aos_menu_build_66ea4(ctr, list, pairs, 1, test_span, NULL,
                             test_w4, NULL);
        assert(list[0] == 1 && list[1] == 0);
        (void)span_calls;
    }

    /* sub_0806711c increment. */
    {
        uint8_t st[0xA0]; uint8_t ctr = 0; AosMenuDesc d = {0, 40};
        memset(st, 0, sizeof st); st[0x38 + 1] = 200; ctr = 100;
        int32_t before; memcpy(&before, st + 0x34, 4);
        aos_menu_adjust_6711c(&ctr, st, 2, 1, &d, 100, 0);
        assert(st[0x38 + 1] == 44);      /* 200+100 wraps u8 */
        assert(ctr == 1);
        int32_t after; memcpy(&after, st + 0x34, 4);
        assert(after == before - 100 * 40);
        /* kind 3 -> +0x58; mode_f 9 -> word4*8/10. */
        memset(st, 0, sizeof st); ctr = 0;
        aos_menu_adjust_6711c(&ctr, st, 3, 7, &d, 10, 9);
        assert(st[0x58 + 7] == 10);
        memcpy(&after, st + 0x34, 4);
        assert(after == -(10 * (40 * 8 / 10)));
    }
    /* sub_080671A4 afford check. */
    assert(aos_menu_can_afford_671a4(2, 2, 0, 1000, 0, 100) ==
           (1000 + (100 >> 1) * 2 <= 0xF423F));
    assert(!aos_menu_can_afford_671a4(0, 2, 0, 0, 0, 100)); /* amt 0 */
    assert(!aos_menu_can_afford_671a4(2, 2, 0, 0xF4230, 0, 100));
    /* non-buy: span+amount <= 9 and st34 >= cost. */
    assert(aos_menu_can_afford_671a4(3, 0, 0, 500, 5, 100));
    assert(!aos_menu_can_afford_671a4(5, 0, 0, 500, 5, 100)); /* 10 > 9 */
    assert(!aos_menu_can_afford_671a4(3, 0, 0, 200, 5, 100)); /* cost 300 */
    assert(aos_menu_can_afford_671a4(3, 0, 9, 240, 5, 100));  /* 240>=240 */
    assert(!aos_menu_can_afford_671a4(3, 0, 9, 239, 5, 100));

    /* sub_08067310 page init. */
    {
        AosMenuCursor cur; uint8_t mode = 0xFF, ctr[0x80];
        uint8_t list[0x90];
        static const uint8_t pairs[] = {2, 5, 0, 0};
        static const uint8_t pg1[] = {2, 0xAA, 0xBB};
        static const uint8_t pg2[] = {2, 0xCC, 0xDD};
        static const uint8_t *pages[] = {NULL, pg1, pg2};
        bool built = false, wops = false;
        /* flags 0x12 -> mode byte 2 (0x02 wins over 0x10). */
        memset(&cur, 0, sizeof cur); cur.mode = 0;
        cur.count = 9; cur.low = -3; cur.high = 7;
        memset(ctr, 0xAA, sizeof ctr);
        aos_menu_page_init_67310(&cur, &mode, ctr, list, pairs, 1,
                                 0x12, pages, 3, test_span, NULL,
                                 test_w4, NULL, &built, &wops);
        assert(mode == 2 && cur.count == 0 && cur.low == 0 &&
               cur.high == 0 && !built && wops);
        assert(ctr[0] == 1 && ctr[1] == 1 && ctr[2] == 0xAA); /* no clear in this path */
        /* flags 0x10 -> mode 1, empty page list -> ctr stays 0? */
        memset(&cur, 0, sizeof cur); memset(ctr, 0, sizeof ctr);
        aos_menu_page_init_67310(&cur, &mode, ctr, list, pairs, 1,
                                 0x10, pages, 3, test_span, NULL,
                                 test_w4, NULL, &built, &wops);
        assert(mode == 1 && !built);
        /* cursor mode 2 -> build path fills ctr via 66EA4. */
        memset(&cur, 0, sizeof cur); cur.mode = 2;
        memset(ctr, 0, sizeof ctr); memset(list, 0, sizeof list);
        aos_menu_page_init_67310(&cur, &mode, ctr, list, pairs, 1,
                                 0, pages, 3, test_span, NULL,
                                 test_w4, NULL, &built, &wops);
        assert(built && list[0] == 1 && ctr[0] == 1);
    }

    puts("aos_menu_cursor: ok");
    return 0;
}
