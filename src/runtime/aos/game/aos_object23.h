#ifndef AOS_OBJECT23_H
#define AOS_OBJECT23_H
#include <stdbool.h>
#include <stdint.h>

/* Object23Create (0x0805780C) / Object23Update (0x080577FC),
 * dispatch index 35 (0x23): a plain animated sprite. Create
 * resolves tiles 0x081CBE94 through sub_0803B800, the palette
 * slot through sub_08068264(0x08209FD8, 3), frames 0x082107EC
 * through sub_0803B924 with drawFunc sub_0803B9D0 (exported as
 * special_35) via the sub_08057858 loader, deletes on load
 * failure, presets sub_0803F2C8(1, 0, 1), steps the animation
 * once and sets + 0x5C = 0x14 with + 0x5A = (+ 0x5A & ~0x06) |
 * 0x02 and + 0xA = 0.
 *
 * Update is a single sub_0803F17C animation step per frame. */
typedef struct {
    uint8_t anim;           /* sub_0803F17C step count */
} AosObject23;

void aos_object23_create(AosObject23 *o);
void aos_object23_update(AosObject23 *o);
#endif
