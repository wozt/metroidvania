/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression tests for the aos_mgba_lab input-schedule contract.
 *
 * The run loop applies frame-indexed key/poke events with an equality
 * match (`event.frame == frame`), so a schedule that arrives out of
 * order would silently drop every event whose frame already passed.
 * aos_mgba_normalize_keys()/aos_mgba_normalize_pokes() are the single
 * place that orders a schedule before execution; these tests pin down
 * that contract: chronological order, stable same-frame ordering (the
 * run loop's last-mask-wins rule depends on it), and explicit rejection
 * of negative frames instead of silent loss.
 *
 * These tests exercise the pure schedule/parsing layer only; they do
 * not boot an emulator core. End-to-end chronological application and
 * the A=jump/B=attack gameplay semantics are covered by the native
 * comparison scripts (tests/compare_aos_movement.py). */

#include "aos_mgba_session.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,     \
                    #cond);                                             \
        }                                                               \
    } while (0)

/* ---- key schedule normalization ------------------------------------ */

static void test_normalize_keys_sorts_and_preserves_order(void) {
    /* Deliberately out of order, with two events sharing frame 10 to
     * prove the sort is stable (input order kept for same frame). */
    AosKeyEvent in[] = {
        {40, 0},
        {10, 16},   /* RIGHT  */
        {30, 2},    /* B      */
        {10, 1},    /* A      */
        {20, 0},
    };
    int n = 0;
    AosKeyEvent *out = aos_mgba_normalize_keys(in, 5, &n);
    CHECK(out != NULL);
    CHECK(n == 5);
    if (out && n == 5) {
        CHECK(out[0].frame == 10 && out[0].mask == 16);
        CHECK(out[1].frame == 10 && out[1].mask == 1);  /* stable order */
        CHECK(out[2].frame == 20);
        CHECK(out[3].frame == 30);
        CHECK(out[4].frame == 40);
    }
    aos_mgba_free_keys(out);
}

static void test_normalize_keys_rejects_negative_frame(void) {
    AosKeyEvent in[] = {{5, 16}, {-1, 1}, {9, 0}};
    int n = 99;
    AosKeyEvent *out = aos_mgba_normalize_keys(in, 3, &n);
    CHECK(out == NULL);
    CHECK(n == -1);
    aos_mgba_free_keys(out);
}

static void test_normalize_keys_empty_and_null(void) {
    int n = 7;
    CHECK(aos_mgba_normalize_keys(NULL, 0, &n) == NULL && n == 0);
    /* count > 0 with NULL input is an invalid schedule, not empty. */
    n = 7;
    CHECK(aos_mgba_normalize_keys(NULL, 3, &n) == NULL && n == -1);
}

/* ---- poke schedule normalization ------------------------------------ */

static void test_normalize_pokes_sorts(void) {
    /* data may be NULL: normalization only orders by frame and must
     * not dereference or take ownership of the payload. */
    AosPokeEvent in[] = {
        {50, 0x02000000, NULL, 2},
        {5,  0x02000010, NULL, 1},
        {5,  0x02000020, NULL, 4},
        {30, 0x02000030, NULL, 1},
    };
    int n = 0;
    AosPokeEvent *out = aos_mgba_normalize_pokes(in, 4, &n);
    CHECK(out != NULL);
    CHECK(n == 4);
    if (out && n == 4) {
        CHECK(out[0].frame == 5 && out[0].address == 0x02000010);
        CHECK(out[1].frame == 5 && out[1].address == 0x02000020);
        CHECK(out[2].frame == 30);
        CHECK(out[3].frame == 50);
    }
    /* shallow copy: free only the array shell, never the shared data */
    free(out);
}

static void test_normalize_pokes_rejects_negative_frame(void) {
    AosPokeEvent in[] = {{0, 0x02000000, NULL, 1}, {-3, 0x02000010, NULL, 1}};
    int n = 0;
    AosPokeEvent *out = aos_mgba_normalize_pokes(in, 2, &n);
    CHECK(out == NULL);
    CHECK(n == -1);
    free(out);
}

/* ---- parsers feed normalizable schedules ---------------------------- */

static void test_parse_keys_then_normalize(void) {
    int count = 0;
    AosKeyEvent *keys =
        aos_mgba_parse_keys("40:0,10:16,30:2,10:1", &count);
    CHECK(keys != NULL);
    CHECK(count == 4);
    int n = 0;
    AosKeyEvent *sorted = aos_mgba_normalize_keys(keys, count, &n);
    CHECK(sorted != NULL && n == 4);
    if (sorted && n == 4) {
        CHECK(sorted[0].frame == 10 && sorted[0].mask == 16);
        CHECK(sorted[1].frame == 10 && sorted[1].mask == 1);
        CHECK(sorted[2].frame == 30 && sorted[2].mask == 2);
        CHECK(sorted[3].frame == 40 && sorted[3].mask == 0);
    }
    aos_mgba_free_keys(sorted);
    aos_mgba_free_keys(keys);
}

static void test_parse_keys_decimal_and_hex_masks(void) {
    /* Decimal is the default; a 0x prefix selects hex. The compare
     * scripts emit decimal masks (RIGHT=16, A|B=3). */
    int count = 0;
    AosKeyEvent *keys = aos_mgba_parse_keys("0:16,5:0x11,9:3", &count);
    CHECK(keys != NULL && count == 3);
    if (keys && count == 3) {
        CHECK(keys[0].mask == 16);   /* decimal RIGHT */
        CHECK(keys[1].mask == 0x11); /* hex RIGHT|A */
        CHECK(keys[2].mask == 3);    /* decimal A|B */
    }
    aos_mgba_free_keys(keys);
}

static void test_parse_peeks_frame_forms(void) {
    int count = 0;
    /* "ADDR:SIZE" -> post-run (frame -1); "F:ADDR:SIZE" -> frame F. */
    AosPeekRequest *peeks =
        aos_mgba_parse_peeks("0x02000010:4,60:0x02000524:8", &count);
    CHECK(peeks != NULL && count == 2);
    if (peeks && count == 2) {
        CHECK(peeks[0].frame == -1 && peeks[0].address == 0x02000010 &&
              peeks[0].size == 4);
        CHECK(peeks[1].frame == 60 && peeks[1].address == 0x02000524 &&
              peeks[1].size == 8);
    }
    aos_mgba_free_peeks(peeks);
}

static void test_parse_pokes_roundtrip(void) {
    int count = 0;
    AosPokeEvent *pokes =
        aos_mgba_parse_pokes("10:02000010:ff,20:02000020:0a0b", &count);
    CHECK(pokes != NULL && count == 2);
    if (pokes && count == 2) {
        CHECK(pokes[0].frame == 10 && pokes[0].address == 0x02000010 &&
              pokes[0].size == 1 && pokes[0].data[0] == 0xff);
        CHECK(pokes[1].frame == 20 && pokes[1].size == 2 &&
              pokes[1].data[0] == 0x0a && pokes[1].data[1] == 0x0b);
    }
    aos_mgba_free_pokes(pokes, count);
}

int main(void) {
    test_normalize_keys_sorts_and_preserves_order();
    test_normalize_keys_rejects_negative_frame();
    test_normalize_keys_empty_and_null();
    test_normalize_pokes_sorts();
    test_normalize_pokes_rejects_negative_frame();
    test_parse_keys_then_normalize();
    test_parse_keys_decimal_and_hex_masks();
    test_parse_peeks_frame_forms();
    test_parse_pokes_roundtrip();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    puts("aos_mgba_session schedule tests passed");
    return 0;
}
