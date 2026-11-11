/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillStudentWitchUse. */
#include "aos/red_souls/aos_soul_red_student_witch.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
/* 0x084F12BC: one wall probe, ceiling -12, floor 0, half width 5. */
static const AosProbes probes = {1, -12, 0, 5, {-7}};

static void floor_row(int y) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
}

int main(void) {
    floor_row(20);
    /* A wall column ahead (it walks right at 2 px/frame). */
    for (int y = 0; y < H; ++y) cells[y * W + 20] = 0x03;
    /* soma_y chosen so the ghost's y sits on the floor row (result&8). */
    AosStudentWitch w = aos_student_witch_create(100 << 16, 184 << 16, false);
    assert(w.body.x == 108 << 16 && w.body.y == (184 - 0x1A) << 16);
    assert(!w.body.mirrored && w.state == 0);
    /* State 0 enters state 1 without walking yet. */
    bool song = true;
    assert(aos_student_witch_update(&w, &layer, &probes, false, &song));
    assert(w.state == 1 && !song);
    /* State 1 walks at -0x20000 with mode 8 until a wall result&3;
     * result&8 on the floor fires the one-shot song request. */
    int frames = 0;
    bool heard = false;
    while (w.state == 1 && frames < 0x400) {
        assert(aos_student_witch_update(&w, &layer, &probes, false, &song));
        heard |= song;
        ++frames;
    }
    assert(w.state == 2 && w.linger == 0x10);
    assert(frames > 0 && heard);
    /* Linger counts 0x10 down, then state 3 deletes next call. */
    while (w.state == 2)
        assert(aos_student_witch_update(&w, &layer, &probes, false, &song));
    /* state 3 was armed by the last linger tick; the next call deletes. */
    assert(!aos_student_witch_update(&w, &layer, &probes, false, &song));
    /* The margin probe forces state 3 mid-walk. */
    AosStudentWitch m = aos_student_witch_create(100 << 16, 184 << 16, false);
    aos_student_witch_update(&m, &layer, &probes, false, &song);
    assert(aos_student_witch_update(&m, &layer, &probes, true, &song));
    assert(m.state == 3);
    assert(!aos_student_witch_update(&m, &layer, &probes, false, &song));
    puts("aos_soul_red_student_witch: ok");
    return 0;
}
