#ifndef AOS_OBJECT11_H
#define AOS_OBJECT11_H
#include <stdbool.h>
#include <stdint.h>

/* Object11Create (0x080512C4) / Object11Update (0x080512F8), the
 * dispatch pair sUnk_084F0DF8[17] / sUnk_084F0ED8[17]: a one-shot
 * marker. Create deletes the entity living in the gEwramData + 0x124C
 * slot (the save-point entity Object10 uses), then EntityCreateInRange
 * spawns an invisible type-0x1A helper in a 0x1A range whose callback
 * sub_080512FC sets its + 0x59 bit 4 and counts + 0x14 frames while
 * gEwramData + 0x42C bit 1 holds, capped at 0x34BBD. Update is a
 * native bx lr. */
typedef struct {
    bool helper;            /* the type-0x1A helper was spawned */
    bool removed_save;      /* the + 0x124C entity was deleted */
    uint32_t held_ticks;    /* helper + 0x14: frames the bit-1 hold lasts */
} AosObject11;

void aos_object11_create(AosObject11 *o);
/* `held` is gEwramData + 0x42C bit 1 (the held-input flag). */
void aos_object11_update(AosObject11 *o, bool held);
#endif
