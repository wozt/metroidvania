#ifndef AOS_OBJECT13_H
#define AOS_OBJECT13_H
#include <stdbool.h>
#include <stdint.h>

/* Object13Update (0x08051B0C), paired with the already-covered
 * Object13Create: the swinging decoration (chain/lantern). The
 * + 0xA == 0xFF state runs a three-tick + 0xD countdown into
 * EntityDelete. + 0x50 is the swing amplitude clamped to 0x2300;
 * + 0x48 the u16 phase advancing 0x200 a frame while the amplitude
 * decays by 4.
 *
 * While param1 (+ 0x32) names an event bit that is still clear, an
 * amplitude above the param0 threshold (0x2100 for param0 == 0,
 * else 0x1200) and a phase inside [0xAC01, 0xB3FF] fires the event
 * once: phase -= 0x40, amplitude -= 0x100 and the + 0x344 bit is
 * set. The drawn sway offset is sub_080009E4(phase) * amplitude
 * >> 16. */
typedef struct {
    int16_t param0;         /* +0x30: threshold selector */
    int16_t param1;         /* +0x32: event bit index, 0 disables */
    int32_t amplitude;      /* +0x50: swing amplitude, cap 0x2300 */
    uint16_t phase;         /* +0x48: u16 phase, +0x200 per frame */
    uint8_t state;          /* +0xA: 0xFF runs the delete countdown */
    uint8_t death;          /* +0xD: 0..2 then the native delete */
    bool event_fired;       /* the + 0x344 bit write for this frame */
    bool deleted;           /* EntityDelete after the countdown */
    int32_t sway;           /* +0x20: sine(phase) * amplitude >> 16 */
} AosObject13;

/* Called when the entity is created (Object13Create setup is the
 * shared sprite attach covered by special_19). */
void aos_object13_create(AosObject13 *o, int param0, int param1,
                         int32_t amplitude, uint16_t phase,
                         uint8_t state);

/* Object13Update. `event_set` is the current + 0x344 bit state of
 * param1 (aos_wall_event on aria_events). */
void aos_object13_update(AosObject13 *o, bool event_set);
#endif
