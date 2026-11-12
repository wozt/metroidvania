#ifndef AOS_OBJECT2C_H
#define AOS_OBJECT2C_H
#include <stdbool.h>
#include <stdint.h>

/* Object2CCreate (0x080585B0) / Object2CUpdate (0x08058664),
 * dispatch index 44 (0x2C): the fountain. The parent update is
 * a bare frame (sub sp / add sp / bx lr) — all behaviour lives
 * in create, which spawns four type-0x49 children through
 * EntityCreateInRange with update sub_0805866C and drawFunc
 * sub_0803B9D0. Each child gets the sprite triple of
 * sub_080586E0 (tiles 0x081CBED4, palette
 * sub_08068264(0x0820A1C4, 0), frames 0x08211464 = special_44),
 * copies the parent's + 0x36 byte, takes a random + 0x40 word
 * (RandomNumberGenerator & 0xFFFFFF), and is configured per
 * index: + 0x37 = 0, + 0x46 y offset {0x1E, 0x3E, 0x56, 0x66},
 * + 0x48 {0x4000, 0x3000, 0x2000, 0x1000}, + 0x65 = the index.
 * sub_0805869C then marks + 0x72 bits 1-2, + 0x5C = 0x1D, clears
 * + 0x59 bit 0x20 and deletes the child if the sprite load
 * failed.
 *
 * The child update sub_0805866C integrates + 0x40 += + 0x48 and
 * runs the sub_0806D128 bounds probe (table 0x08118D08 record
 * 6 = {64, 64}) on + 0x42/+ 0x46; a miss writes + 0x42 =
 * 0xFFC4 (-60), the upward hop. */
typedef struct {
    /* Per-droplet parameters copied from the create loop; kept
     * for traceability — the runtime counts the children rather
     * than spawning them. */
    uint16_t y_off[4];       /* + 0x46: 0x1E, 0x3E, 0x56, 0x66 */
    uint16_t x_step[4];      /* + 0x48: 0x4000..0x1000 */
    uint8_t layer[4];        /* + 0x65: 0, 1, 2, 3 */
    int child_count;         /* always 4 after a successful create */
    bool deleted;            /* EntityDelete ran (sprite load fail) */
} AosObject2C;

void aos_object2c_create(AosObject2C *o);
void aos_object2c_update(AosObject2C *o);
#endif
