#ifndef AOS_OBJECT1B_H
#define AOS_OBJECT1B_H
#include <stdbool.h>
#include <stdint.h>

/* Object1BUpdate (0x08053FC8), paired with the already-covered
 * Object1BCreate: a rotation-synced object. sub_08021278 registers
 * it like Object0D. While state 0 and the entity holds the
 * gEwramData + 0x13190 focus slot, + 0xB toggles, the attack
 * preset 0x0C10F4F8 installs, the animation becomes + 0xB + 2,
 * PlaySong 0x133 fires and state becomes 1.
 *
 * State 1 ramps the shared gEwramData + 0x4C4 clock by 0x1000 a
 * frame while it stays at or below 0x13FFF. Every update then adds
 * the clock to the + 0x14 angle accumulator, derives
 * (angle >> 0x13) mod 3 and, on a change, pushes the matching
 * 0x081185C8 tile frame (0x18/0x1D/0x1E) through sub_0803C918 —
 * approximated here by the `frame` output. */
typedef struct {
    uint8_t state;          /* +0xA: 0 idle, 1 spinning */
    uint8_t toggle;         /* +0xB: direction flip on focus */
    int32_t angle;          /* +0x14: clock-driven angle */
    uint8_t frame;          /* +0x18: last pushed tile frame */
    int song;               /* PlaySong request, -1 none */
} AosObject1b;

void aos_object1b_create(AosObject1b *o);
/* `focused` is the + 0x13190 slot match; `clock` is the shared
 * gEwramData + 0x4C4 word, ramped in place like the native. */
void aos_object1b_update(AosObject1b *o, bool focused, int32_t *clock);
#endif
