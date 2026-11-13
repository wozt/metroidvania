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

static uint16_t rec_songs[16]; static int n_songs;
static void rec_song(uint16_t id, void *ctx) {
    (void)ctx; rec_songs[n_songs++] = id;
}
static uint32_t rec_renders[16]; static int n_renders;
static void rec_render(uint32_t arg, void *ctx) {
    (void)ctx; rec_renders[n_renders++] = arg;
}
static int n_popups;
static void rec_popup(void *ctx) { (void)ctx; n_popups++; }
static const AosMenuDesc test_desc = {0, 40};
static uint32_t span_small(uint8_t kind, uint8_t idx, void *ctx) {
    (void)kind; (void)idx; (void)ctx; return 2;
}
static const AosMenuDesc *test_desc_ki(uint8_t kind, uint8_t idx,
                                       void *ctx) {
    (void)kind; (void)idx; (void)ctx; return &test_desc;
}

static const uint8_t tpairs[] = {2, 5, 0, 0, 3, 2, 0, 0};
static uint32_t span3(uint8_t kind, uint8_t idx, void *ctx) {
    (void)kind; (void)idx; (void)ctx; return 3;
}
static uint32_t span5(uint8_t kind, uint8_t idx, void *ctx) {
    (void)kind; (void)idx; (void)ctx; return 5;
}

static int n_cells, n_clears, n_names, n_qtys, n_dashes,
           n_blanks, n_icons_i, n_icons_e, n_dels, n_cicons,
           n_descs, n_arrow_t, n_arrow_f, n_empties;
static void rd_cell(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                    void *c) { (void)x;(void)y;(void)w;(void)h;(void)c; n_cells++; }
static void rd_row_clear(uint8_t s, void *c) { (void)s;(void)c; n_clears++; }
static void rd_name(uint16_t t, void *c) { (void)t;(void)c; n_names++; }
static void rd_qty(uint16_t v, uint8_t s, void *c) { (void)v;(void)s;(void)c; n_qtys++; }
static void rd_qty_pos(uint8_t x, uint8_t s, void *c) { (void)x;(void)s;(void)c; n_dashes++; }
static void rd_dash(uint8_t x, uint8_t s, uint8_t n, void *c) { (void)x;(void)s;(void)n;(void)c; n_dashes++; }
static void rd_blanks_fn(uint8_t x, uint8_t y, uint8_t w, uint8_t h, void *c) { (void)x;(void)y;(void)w;(void)h;(void)c; n_blanks++; }
static void rd_icon_item(uint8_t k, uint8_t i, void *c) { (void)k;(void)i;(void)c; n_icons_i++; }
static void rd_icon_equip(uint8_t k, uint8_t i, void *c) { (void)k;(void)i;(void)c; n_icons_e++; }
static void rd_del_icon(void *c) { (void)c; n_dels++; }
static void rd_clear_icon(void *c) { (void)c; n_cicons++; }
static void rd_desc_text(uint16_t w, void *c) { (void)w;(void)c; n_descs++; }
static void rd_arrow(uint8_t w, bool t, void *c) { (void)w;(void)c; if (t) n_arrow_t++; else n_arrow_f++; }
static uint16_t rd_name_id(uint16_t w, void *c) { (void)w;(void)c; return w; }
static uint16_t rd_desc_id(uint16_t w, void *c) { (void)w;(void)c; return w; }
static uint8_t rd_recbuf[2][32];
static const uint8_t *rd_record_at(uint8_t k, uint8_t i, void *c) {
    (void)c;
    if (k == 2 && i == 5) { uint16_t w0=0x11; int32_t w4=40; memcpy((void*)rd_recbuf[0], &w0, 2); memcpy(rd_recbuf[0]+4, &w4, 4); return rd_recbuf[0]; }
    if (k == 3 && i == 2) { uint16_t w0=0x22; int32_t w4=80; memcpy(rd_recbuf[1], &w0, 2); memcpy(rd_recbuf[1]+4, &w4, 4); return rd_recbuf[1]; }
    return NULL;
}
static void rd_reset(void) {
    n_cells=n_clears=n_names=n_qtys=n_dashes=n_blanks=0;
    n_icons_i=n_icons_e=n_dels=n_cicons=n_descs=0;
    n_arrow_t=n_arrow_f=n_empties=0;
}

static int eq_bump;
static int nstat_g, npos_g, nrecomp_g, nset6b_g;
static int16_t *eq_bump_stats;
static void eq_recompute(void *c){(void)c;nrecomp_g++;if(eq_bump&&eq_bump_stats)eq_bump_stats[0]+=5;}
static void eq_set6b(uint8_t v,uint8_t m,void*c){(void)v;(void)m;(void)c;nset6b_g++;}
static void eq_pos(uint8_t x,uint8_t y,void*c){(void)x;(void)y;(void)c;npos_g++;}
static uint8_t eq_last_mark[6]; static int eq_stat_n;
static void eq_stat(uint8_t mo,uint8_t y,int32_t v,uint8_t r3,uint8_t mk,uint32_t p,void*c){(void)mo;(void)y;(void)v;(void)r3;(void)p;(void)c;eq_last_mark[y/2-1]=mk;eq_stat_n++;}
static uint8_t eq_rec8_v;
static uint8_t eq_rec8(uint8_t i,void*c){(void)i;(void)c;return eq_rec8_v;}
static int cl_dma_n, cl_rel_n;
static void cl_dma(void*c){(void)c;cl_dma_n++;}
static void cl_rel(void*c){(void)c;cl_rel_n++;}
static int pp_hb_n, pp_dsp_n;
static void pp_hb(void*c){(void)c;pp_hb_n++;}
static void pp_dsp(uint8_t r,uint16_t v,void*c){(void)r;(void)v;(void)c;pp_dsp_n++;}

int main(void) {
    AosMenuCursor s; uint8_t ctr = 0, cols[8] = {0};
    bool song;

    /* counter+ : mode!=2 needs ctr+span <= 8. */
    memset(&s, 0, sizeof s); s.count = 10; s.mode = 0;
    ctr = 0;
    uint32_t r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL,
                                      0x10, &song);
    assert(r == 0 && ctr == 0);         /* ctr==0 -> fall through */
    ctr = 4;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x10, &song);
    assert(r == 1 && ctr == 5 && song); /* 4+3 <= 8 */
    ctr = 6;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x10, &song);
    assert(r == 0 && ctr == 6 && !song); /* 6+3 > 8 -> tail */
    /* mode==2 bound is ctr < span. */
    memset(&s, 0, sizeof s); s.count = 10; s.mode = 2;
    ctr = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x10, &song);
    assert(r == 1 && ctr == 3 && song);
    ctr = 3;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x10, &song);
    assert(r == 0 && ctr == 3 && !song);
    /* counter- needs ctr > 1. */
    memset(&s, 0, sizeof s); s.count = 10;
    ctr = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x20, &song);
    assert(r == 0 && ctr == 1 && !song);
    ctr = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x20, &song);
    assert(r == 1 && ctr == 1 && song);
    /* cursor up: high>0, low clamps to high. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 4; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x40, &song);
    assert(r == 2 && s.high == 3 && s.low == 2 && song);
    s.high = 1; s.low = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x40, &song);
    assert(s.high == 0 && s.low == 0);
    /* low stays when <= high. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 3; s.low = -5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x40, &song);
    assert(s.high == 2 && s.low == -5);
    /* cursor down: bound count-1, low window follows at -3. */
    memset(&s, 0, sizeof s); s.count = 6; s.high = 3; s.low = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x80, &song);
    assert(r == 3 && s.high == 4 && s.low == 1); /* low moved -> 3 */
    s.high = 5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x80, &song);
    assert(r == 0 && s.high == 5);       /* high >= count-1 */
    /* page down 0x100: +4 each, clamp to count-4/count-1. */
    memset(&s, 0, sizeof s); s.count = 20; s.high = 4; s.low = 1;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x100, &song);
    assert(r == 3 && s.high == 8 && s.low == 5); /* low moved */
    s.high = 18; s.low = 10;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x100, &song);
    /* low 14 <= count-4 16 -> no clamp: high wraps to 22 like
     * the ROM; low moved -> ret 3. */
    assert(r == 3 && s.high == 22 && s.low == 14);
    /* page up 0x200: -4 each; low<=3 resets both. */
    memset(&s, 0, sizeof s); s.count = 20; s.high = 8; s.low = 5;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x200, &song);
    assert(r == 3 && s.high == 4 && s.low == 1);
    memset(&s, 0, sizeof s); s.count = 20; s.high = 6; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x200, &song);
    assert(r == 3 && s.high == 0 && s.low == 0); /* low moved -> 3 */
    /* bit priority: 0x10 set with ctr==0 falls to 0x40. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 5; s.low = 3;
    ctr = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL,
                             0x10 | 0x40, &song);
    assert(r == 2 && s.high == 4);
    /* no input -> ret 0 but diff still written. */
    memset(&s, 0, sizeof s); s.count = 10; s.high = 5; s.low = 2;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0, &song);
    assert(r == 0 && s.diff == 3 && !song);
    /* clamp: diff >= count -> ret 3 even without input. */
    memset(&s, 0, sizeof s); s.count = 4; s.high = 9; s.low = 0;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0, &song);
    assert(r == 3 && s.high == 3 && s.diff == 3);
    /* low window follows on page down even near the bound. */
    memset(&s, 0, sizeof s); s.count = 6; s.high = 4; s.low = 4;
    r = aos_menu_cursor_tick(&s, &ctr, cols, tpairs, 2, span3, NULL, 0x80, &song);
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

    /* sub_08067488 page input handler. */
    {
        static const uint8_t prs[] = {2, 5, 0, 0, 3, 2, 0, 0};
        static const uint8_t lst[] = {1, 0};
        uint8_t ctr[0x80]; uint8_t st[0xA0]; uint8_t cols[0x20];
        uint8_t dirty; uint16_t sysf;
        AosMenuCursor c;

        /* bit 0x2 with +0xC == 0 -> return 1 + PlaySong(0xF0). */
        memset(&c, 0, sizeof c); memset(ctr, 0, sizeof ctr);
        memset(st, 0, sizeof st); dirty = 0; sysf = 0xFFFF;
        n_songs = n_renders = n_popups = 0;
        uint32_t rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x2, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(rr == 1 && n_songs == 1 && rec_songs[0] == 0xF0);

        /* bit 0x1 afford ok -> popup + F3 + sub_d=2 + ctr_c=1. */
        memset(&c, 0, sizeof c); memset(ctr, 0, sizeof ctr);
        memset(st, 0, sizeof st); dirty = 0; sysf = 0xFFFF;
        n_songs = n_renders = n_popups = 0;
        ctr[0] = 1;                 /* amount 1, affordable */
        memcpy(st + 0x34, &(int32_t){1000}, 4);
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x1, span_small, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(rr == 0 && n_popups == 1 && c.sub_d == 2 &&
               c.ctr_c == 1 && rec_songs[0] == 0xF3);

        /* afford fail (amount too big vs st34) -> EF only. */
        memset(&c, 0, sizeof c); memset(ctr, 0, sizeof ctr);
        memset(st, 0, sizeof st); dirty = 0; sysf = 0xFFFF;
        n_songs = n_renders = n_popups = 0;
        memcpy(st + 0x34, &(int32_t){0}, 4);
        ctr[0] = 100;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x1, span_small, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        /* mode 0 path: span 25 + amount 100 > 9 -> fail */
        assert(rr == 0 && n_popups == 0 && rec_songs[0] == 0xEF &&
               c.ctr_c == 0);

        /* +0xC != 0, bit 0x1, sub_d == 0 -> adjust + F5 + dirty. */
        memset(&c, 0, sizeof c); c.ctr_c = 1; c.mode = 0;
        memset(ctr, 0, sizeof ctr); memset(st, 0, sizeof st);
        dirty = 0; sysf = 0xFFFF; n_songs = n_renders = 0;
        memcpy(st + 0x34, &(int32_t){100000}, 4);
        ctr[0] = 2; st[0x38 + 5] = 50;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x1, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(rr == 0 && rec_songs[0] == 0xF5 &&
               st[0x38 + 5] == 52 && c.ctr_c == 0 &&
               (dirty & 2) && (sysf & 0x9FFF) == 0x9FFF &&
               n_renders == 1 && rec_renders[0] == 3);

        /* mode 2 +0xC!=0 -> decrement path (67000). */
        memset(&c, 0, sizeof c); c.ctr_c = 1; c.mode = 2;
        memset(ctr, 0, sizeof ctr); memset(st, 0, sizeof st);
        dirty = 0; sysf = 0xFFFF; n_songs = 0;
        memcpy(st + 0x34, &(int32_t){500}, 4);
        ctr[0] = 3; st[0x58 + 5] = 20;      /* kind2->wait mode2 uses
                                             * 67000 kind dispatch:
                                             * kind=2 -> +0x38 */
        st[0x38 + 5] = 20;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x1, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(st[0x38 + 5] == 17 && c.ctr_c == 0);

        /* bit 0x2 confirm (bit 0x1 would adjust first) with
         * sub_d==0 -> F0 + dirty + ctr_c=0. */
        memset(&c, 0, sizeof c); c.ctr_c = 1;
        memset(ctr, 0, sizeof ctr); memset(st, 0, sizeof st);
        dirty = 0; sysf = 0xFFFF; n_songs = 0;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x2, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(rec_songs[0] == 0xF0 && (dirty & 2) &&
               c.ctr_c == 0);

        /* bit 0x10 enter / 0x20 exit sub-mode (+0xC must be
         * nonzero for this branch). */
        memset(&c, 0, sizeof c); c.ctr_c = 1;
        memset(ctr, 0, sizeof ctr); memset(st, 0, sizeof st);
        dirty = 0; sysf = 0xFFFF; n_songs = 0;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x10, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(c.sub_d == 1 && rec_songs[0] == 0xF1);
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x20, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(c.sub_d == 0 && rec_songs[1] == 0xF1);

        /* cursor path: bit 0x40 moves high, renders once. */
        memset(&c, 0, sizeof c); c.count = 10; c.high = 3; c.low = 1;
        memset(ctr, 0, sizeof ctr); memset(st, 0, sizeof st);
        dirty = 0; sysf = 0xFFFF; n_songs = n_renders = 0;
        rr = aos_menu_page_input_67488(
            &c, ctr, st, cols, lst, prs, 2, 0x40, test_span, NULL,
            test_desc_ki, NULL, rec_song, rec_render, rec_popup,
            NULL, NULL, &dirty, &sysf);
        assert(c.high == 2 && n_renders == 1);
    }

    /* sub_08066B70 renderer — ops recorder. */
    {
        static const uint8_t rprs[] = {2, 5, 0, 0, 3, 2, 0, 0};
        /* list: count 2, pair idx 0 (kind2/idx5) and 1 (kind3/idx2) */
        static const uint8_t rlist[] = {2, 0, 1};
        static uint8_t rctr[0x80]; static uint8_t rst[0xA0];
        AosMenuCursor rc; AosMenuDrawOps ops;
        memset(&ops, 0, sizeof ops);
        ops.cell = rd_cell; ops.row_clear = rd_row_clear;
        ops.name = rd_name; ops.qty = rd_qty;
        ops.qty_pos = rd_qty_pos; ops.dash = rd_dash;
        ops.blanks = rd_blanks_fn; ops.icon_item = rd_icon_item;
        ops.icon_equip = rd_icon_equip; ops.del_icon = rd_del_icon;
        ops.clear_icon = rd_clear_icon; ops.desc_text = rd_desc_text;
        ops.arrow = rd_arrow; ops.name_id = rd_name_id;
        ops.desc_id = rd_desc_id; ops.record_at = rd_record_at;
        rd_reset();
        /* records: kind2 idx5 -> word0 0x11 word4 40;
         * kind3 idx2 -> word0 0x22 word4 80 */
        memset(&rc, 0, sizeof rc); rc.mode = 0;
        rc.count = 5; rc.diff = 1; rc.low = 0; rc.high = 4;
        memset(rctr, 0, sizeof rctr); memset(rst, 0, sizeof rst);
        rctr[0] = 2; rctr[1] = 3;
        aos_menu_render_66b70(&rc, rctr, rst, rlist, rprs, 2, 3,
                              span_small, NULL, &ops);
        assert(n_cells == 3 && n_names == 2);
        /* 48804 icon entity fires for every kind; kind3 also
         * runs the 66A00 equip-icon path. */
        assert(n_dels == 0 && n_icons_i == 1 &&
               n_icons_e == 1);
        assert(rc.count == 2 && rc.high == (uint8_t)(0 + 1));
        /* empty list -> blanks + deletes + mark clears, no arrows
         * body beyond hints (low=0,count=0 -> down arrow textured?) */
        static const uint8_t elist[] = {0};
        memset(&rc, 0, sizeof rc); rc.mode = 0;
        rd_reset();
        aos_menu_render_66b70(&rc, rctr, rst, elist, rprs, 2, 3,
                              span_small, NULL, &ops);
        assert(n_blanks == 1 && n_dels == 1 &&
               n_cicons == 1);
        /* stack_arg <= 1 skips detail: no icon/desc. */
        memset(&rc, 0, sizeof rc); rc.mode = 0; rc.count = 5;
        rd_reset();
        aos_menu_render_66b70(&rc, rctr, rst, rlist, rprs, 2, 1,
                              span_small, NULL, &ops);
        assert(n_icons_i == 0 && n_descs == 0);
    }

    /* sub_08066A00 equip-stat comparison. */
    {
        static int16_t st[6] = {10, 20, 30, 40, 50, 60};
        static uint8_t sl68, sl6b, sl6c, sl6d;

        AosMenuEquipOps eo; memset(&eo, 0, sizeof eo);
        eo.slot68 = &sl68; eo.slot6b = &sl6b; eo.slot6c = &sl6c;
        eo.slot6d = &sl6d; eo.stats = st;
        eo.recompute = eq_recompute; eo.set_6b = eq_set6b;
        eo.pos = eq_pos; eo.stat = eq_stat; eo.armor_rec8 = eq_rec8;
        sl68 = 7; sl6c = 9; sl6d = 11; sl6b = 3;
        /* kind4 idx4, rec8 = 0 -> slot6c; stats get +5 on row0 */
        eq_rec8_v = 0;
        eq_stat_n = npos_g = nrecomp_g = nset6b_g = 0;
        memset(eq_last_mark, 0, sizeof eq_last_mark);
        eq_bump = 1; eq_rec8_v = 0;
        /* recompute bumps stats[0] on first call only */
        eq_bump_stats = st;
        aos_menu_equip_stats_66a00(4, 4, &eo);
        /* slots restored after the compare; the +5 bump on
         * stats[0] proves the candidate was staged. */
        assert(sl6c == 9 && sl6d == 11 && sl68 == 7);
        assert(eq_stat_n == 6 && nrecomp_g == 2 && nset6b_g == 2);
        assert(eq_last_mark[0] == 9);   /* stat up */
        /* kind3 -> slot68 staged then restored */
        sl68 = 7; eq_stat_n = 0; eq_bump = 0;
        aos_menu_equip_stats_66a00(3, 21, &eo);
        assert(sl68 == 7);
        /* kind2 -> no slot writes */
        sl6c = 9; sl6d = 11;
        aos_menu_equip_stats_66a00(2, 5, &eo);
        assert(sl6c == 9 && sl6d == 11);
    }
    /* sub_08067804 scroll + sub_08067898 close + sub_0806724C popup */
    {
        static uint16_t inw, d48, d3c, d4c; static uint8_t scr;
        static uint32_t fl; static uint8_t s417;
        int hb_n, dsp_n;
        AosMenuScrollOps so; AosMenuCloseOps co; AosMenuPopupOps po;
        memset(&so,0,sizeof so); memset(&co,0,sizeof co);
        memset(&po,0,sizeof po);
        so.input=&inw; so.disp48=&d48; so.disp3c=&d3c;
        so.disp4c=&d4c; so.scroll=&scr;
        co.input=&inw; co.disp4c=&d4c; co.flags42c=&fl;
        co.state417=&s417; co.dma_setup=cl_dma; co.release=cl_rel;
        co.ctx=NULL;
        inw=0; d48=0x1234; d3c=0; d4c=0; scr=10; fl=0; s417=0;
        hb_n=dsp_n=0; pp_hb_n=pp_dsp_n=0;
        assert(aos_menu_scroll_67804(5,&so)==0);
        assert(scr==15 && d4c==8);
        assert(aos_menu_scroll_67804(100,&so)==1 && scr==0x24);
        /* u8 wrap: -36 from 0x24 lands exactly on 0 -> low clamp */
        assert(aos_menu_scroll_67804(-36,&so)==1 && scr==0);
        fl=0x10000; s417=7; d4c=9;
        cl_dma_n=cl_rel_n=0;
        aos_menu_close_67898(&co);
        /* DMA path leaves +0x42C untouched (bit 0x20 clear only
         * happens on the release path). */
        assert(cl_dma_n==1 && cl_rel_n==0 && fl==0x10000);
        fl=0; aos_menu_close_67898(&co);
        assert(cl_dma_n==1 && cl_rel_n==1 && fl==0);
        assert(s417==0xFF);
        /* popup: dirty bit3 clear -> bank1 (win_buf+0x500) */
        static uint16_t wbuf[0xA00]; static uint8_t dirty;
        po.dirty=&dirty; po.input=&inw; po.win_buf=wbuf;
        po.hblank=pp_hb; po.display=pp_dsp; po.ctx=NULL;
        dirty=0; hb_n=dsp_n=0; pp_hb_n=pp_dsp_n=0; inw=0;
        aos_menu_popup_6724c(&po);
        assert(dirty&1 && (inw&0x6000)==0x6000);
        assert(wbuf[0x500+0x61]==0x40 && wbuf[0x500+0xA0]==0);
        assert(pp_hb_n==1 && pp_dsp_n==7);
        /* bit3 set -> bank0 */
        dirty=8; memset(wbuf,0,sizeof wbuf);
        aos_menu_popup_6724c(&po);
        assert(wbuf[0x61]==0x40 && wbuf[0xA0]==0);
    }

    puts("aos_menu_cursor: ok");
    return 0;
}
