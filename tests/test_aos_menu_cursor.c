#include "../src/runtime/aos/game/aos_menu_cursor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int rebuild_calls;
static void rebuild_count(void *c) { (void)c; rebuild_calls++; }

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

    puts("aos_menu_cursor: ok");
    return 0;
}
