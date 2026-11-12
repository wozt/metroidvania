#ifndef AOS_OBJECT2B_H
#define AOS_OBJECT2B_H
#include <stdbool.h>
#include <stdint.h>

/* Object2BCreate (0x08058508) / Object2BUpdate (0x08058544),
 * dispatch index 43 (0x2B): a plain animated sprite. Create
 * resolves tiles 0x081CBEDC through sub_0803B800, the palette
 * slot through sub_08068264(0x0820A1C4, 1) with the
 * sub_0803CC70 table pick at 0x08526618, frames 0x0821154C
 * through sub_0803B924 with drawFunc sub_0803B9D0 (exported as
 * special_43), deletes on load failure, presets
 * sub_0803F2C8(1, 0, 1), steps the animation once and sets
 * + 0x5C = 0x12 with + 0xA = 0.
 *
 * Update is a single sub_0803F17C animation step per frame. */
typedef struct {
    uint8_t anim;           /* sub_0803F17C step count */
} AosObject2B;

void aos_object2b_create(AosObject2B *o);
void aos_object2b_update(AosObject2B *o);
#endif
