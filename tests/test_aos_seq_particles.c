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

    puts("aos_seq_particles: ok");
    return 0;
}
