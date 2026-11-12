#include "aos_object28.h"

void aos_object28_create(AosObject28 *o, uint8_t variant) {
    /* Identical modeled state; the preset-4 vs preset-2 and the
     * + 0x1A vs + 0x18 gate offset are unmodelled divergences. */
    aos_object27_create(o, variant);
}

void aos_object28_update(AosObject28 *o, bool target_far) {
    aos_object27_update(o, target_far);
}
