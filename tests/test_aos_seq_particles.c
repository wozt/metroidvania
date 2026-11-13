#include "../src/runtime/aos/game/aos_seq_particles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int spawn_calls;
static void *fake_spawn(const AosSeqParticleRecord *r, int16_t x,
                        int16_t y) {
    (void)r; (void)x; (void)y;
    return (void *)(uintptr_t)(++spawn_calls);
}
static int16_t fake_rng(void) { return 0x1234; }
static bool spawned_ok;
static bool spawn_child_7_9(int16_t x, int16_t y) {
    spawned_ok = x == 7 && y == 9;
    return true;
}
static int32_t sine_calls;
static int32_t fake_sine(uint32_t a) { (void)a; sine_calls++; return 0x40000; }

int main(void) {
    /* sub_08062D8C: nine records, NULL spawn skipped. */
    AosSeqParticleRecord recs[9];
    memset(recs, 0, sizeof recs);
    for (int i = 0; i < 9; ++i) {
        recs[i].word0 = 0xA0 + i;
        recs[i].xoff = (int16_t)(i * 4);
        recs[i].yoff = (int16_t)(i * -2);
        recs[i].timer = (uint8_t)(i + 1);
    }
    AosSeqParticle out[9];
    spawn_calls = 0;
    unsigned n = aos_seq_particles_spawn(recs, 100, 200, fake_spawn,
                                         fake_rng, out);
    assert(n == 9 && spawn_calls == 9);
    assert(out[0].x == 100 && out[0].y == 232 && out[0].timer == 1 &&
           out[0].field_18 == 0x1234 && out[0].field_16 == 0 &&
           out[0].field_17 == 0 && out[0].field_1f == 0xA0 &&
           (out[0].flags58 & 0x20) && out[0].callback == 0x08062C18);
    assert(out[8].x == 132 && out[8].y == 216 && out[8].field_16 == 0 &&
           out[8].field_17 == 8);

    /* NULL spawn entries are skipped. */
    n = aos_seq_particles_spawn(recs, 0, 0, NULL, NULL, out);
    assert(n == 0);

    /* sub_08062B60 descriptor. */
    AosSeqParticleSpawn s;
    memset(&s, 0, sizeof s);
    s = aos_seq_particle_spawn(NULL, true, 1);
    assert(!s.created && !s.deleted);
    s = aos_seq_particle_spawn((void *)1, false, 0);
    assert(s.created && s.anim5c == 0x11 && !s.scale_1c &&
           !s.affine_call && s.flags59 == 0);
    s = aos_seq_particle_spawn((void *)1, true, 3);
    assert(s.created && s.scale_1c == 0x12000 && s.flags58 == 3 &&
           s.flags59 == 0x10 && s.affine_call && s.anim5c == 0x11);
    s = aos_seq_particle_spawn((void *)1, true, -1);
    assert(s.deleted && !s.created);

    /* sub_08062C18: state 0 waits for the +0xD wrap then
     * spawns; state 1 flies and deletes past +0xD 0x3F. */
    AosSeqParticleEnt pe;
    memset(&pe, 0, sizeof pe);
    pe.state = 0; pe.timer = 1; pe.spawn_x = 7; pe.spawn_y = 9;
    spawned_ok = false;
    aos_seq_particle_update(&pe, NULL, NULL);
    assert(pe.state == 0);           /* timer 1 -> 0, no wrap */
    pe.timer = 0;
    aos_seq_particle_update(&pe, spawn_child_7_9, NULL);
    assert(pe.state == 1 && spawned_ok && pe.spin == 0x10000 &&
           pe.timer == 0 && pe.child_spawned);

    /* state 1: sine-driven flight, wobble on even ticks,
     * anim flip at +0xD > 0x20, delete past 0x3F. */
    memset(&pe, 0, sizeof pe);
    pe.state = 1; pe.timer = 0x0F; pe.field18 = 2;
    pe.flags16 = 1; pe.flags58 = 0x25;
    sine_calls = 0;
    aos_seq_particle_update(&pe, NULL, fake_sine);
    assert(sine_calls == 2);
    /* sine 0x40000 >> 8 << 10 = 0x100000 */
    assert(pe.vx == 0x100000 && pe.vy == 0x100000);
    assert(pe.x == 0x100000 && pe.y == 0x100000);
    assert(pe.timer == 0x10 && pe.anim5c == 3);
    assert(pe.field18 == 3 && (int32_t)pe.spin == 0x200);
    pe.timer = 0x3F; pe.state = 1;
    aos_seq_particle_update(&pe, NULL, NULL);
    assert(pe.timer == 0x40 && pe.anim5c == 0x11 &&
           pe.pal_restored && (pe.flags59 & 0x8));

    /* sub_08065834: bit-0 mark or animation step. */
    uint8_t ch = 0; bool step = false;
    aos_seq_particle_child_update(&ch, &step);
    assert(step && ch == 0);
    ch = 1; step = false;
    aos_seq_particle_child_update(&ch, &step);
    assert(!step && (ch & 0x9) == 0x9);

    /* sub_08065800: table pick by (room_word >> 1) % count. */
    static const uint8_t blob[24] = {
        1,2,3,4, 5,6,7,8,
        9,10,11,12, 13,14,15,16,
        17,18,19,20, 21,22,23,24 };
    AosSeqTableA8 tab;
    memset(&tab, 0, sizeof tab);
    tab.count = 3; tab.field2 = 0x1234; tab.records = blob;
    uint32_t w0; uint16_t h4;
    int idx = aos_seq_table_pick(&tab, 8, &w0, &h4);
    /* (8 >> 1) % 3 = 1 -> record 1: bytes 8..15 */
    assert(idx == 1);
    assert(w0 == 0x0C0B0A09u && h4 == 0x0E0D);
    idx = aos_seq_table_pick(&tab, 0, &w0, &h4);
    assert(idx == 0 && w0 == 0x04030201u && h4 == 0x0605);
    tab.count = 0;
    assert(aos_seq_table_pick(&tab, 4, &w0, &h4) == -1);

    puts("aos_seq_particles: ok");
    return 0;
}
