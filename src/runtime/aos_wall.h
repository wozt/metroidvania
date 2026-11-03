/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow breakable walls (special objects 0x08 and 0x09).
 *
 * Ports of cvaos Object08Create (0x0804FFA0) / Object08Update (0x0805008C)
 * and Object09Create (0x080501D0) / Object09Update (0x08050270), with the
 * shared helpers of code_08040A38: the solid blocks of sub_0804FCA0 and
 * their clearing sub_0804FE20, the debris of sub_0804FA88 (sub_0804F8A0),
 * the dust spawner sub_0804FB78 (sub_080503E0) and the pieces of
 * sub_0804F9B4. Parameter 0 selects an entry of 0x08526064 (its sprite,
 * box and block shape), parameter 1 the gEwramData + 0x344 event bit that
 * records the break (none when 0).
 *
 * Object 0x08 (+ 0x34 = 3) takes three blows of Soma's weapon (sound 0xD3
 * each; the collision bits 0x1A of sub_080421AC: the weapon slots, not the
 * red soul skills) and breaks at the fourth (0xAB): nine debris, its
 * blocks cleared, its bit set. Each blow raises dust. Object 0x09 breaks when its bit becomes set by something else. Both
 * sparkle (sub_08045CEC animation 12, row 6) every 16 frames while the
 * 0x13260 bit 0x1000000 (the Peeping Eye soul) is set; an object 0x08
 * without an event bit does nothing at all. The map update of
 * sub_08012154 / sub_08012048 is not ported. No SDL. */
#ifndef AOS_WALL_H
#define AOS_WALL_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"
#include "aos_combat.h"
#include "aos_effect.h"
#include "aos_soma.h"
#include "aos_weapon.h"

enum { AOS_WALL_ENTRIES = 18, AOS_WALL_DUSTS = 4, AOS_WALL_MAX_EFFECTS = 32,
       AOS_WALL_EVENT_WORDS = 8 };
#define AOS_WALL_PEEPING_EYE_FLAG 0x1000000u /* 0x13260: the Peeping Eye soul */

/* An entry of 0x08526064: the sprite frame (+ 0x65), the block offset
 * (+ 4, + 5), the box size (+ 6, + 7) and the block shape (+ 9: 0 a grid,
 * 1 a column, 2 a column 16 pixels left, 3 and 4 a row). */
typedef struct {
    uint8_t frame;
    int8_t dx, dy;
    uint8_t w, h;
    uint8_t shape;
} AosWallEntry;

extern const AosWallEntry aos_wall_entries[AOS_WALL_ENTRIES];

/* sub_0804FB78: 32 frames, dust every fourth. */
typedef struct {
    bool alive;
    uint8_t timer;
    uint8_t shape;
    int32_t x, y;
} AosWallDust;

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 in room pixels (16.16) */
    uint8_t index;          /* + 0x30 */
    int16_t event;          /* + 0x32 */
    bool object09;
    uint8_t state;          /* + 0x0A */
    int16_t hp;             /* + 0x34: three blows */
    uint8_t timer;          /* + 0x0D: the sparkle counter */
    bool broken;            /* deleted: no sprite, no blocks */
    AosCombat combat;       /* type 8 */
    AosWallDust dust[AOS_WALL_DUSTS];
} AosWall;

typedef struct {
    int effect_count;
    AosEffect effects[AOS_WALL_MAX_EFFECTS];
    uint16_t sound;         /* 0 for none */
    bool struck, broke;
} AosWallReport;

/* gEwramData + 0x344: the event bit n. */
bool aos_wall_event(const uint32_t events[AOS_WALL_EVENT_WORDS], int n);

/* Object08Create / Object09Create at room pixel (x, y): false (deleted)
 * when the entry is out of range or the event bit is already set; else
 * the blocks are made solid. */
bool aos_wall_create(AosWall *wall, AosCollision *layer, int x, int y, int param0, int param1,
                     bool object09, const uint32_t events[AOS_WALL_EVENT_WORDS]);

/* The weapon test of sub_080421AC (bits 2, 8, 0x10: sub_08041864) against
 * the wall's box (-w / 2, -h / 2, w, h), with the recent-hit slots. */
bool aos_wall_weapon_hit(AosWall *wall, const AosWeaponEntity *weapon, const AosSoma *soma,
                         const AosWeaponFrames *frames);

/* Object08Update / Object09Update and the wall's dust spawners for one
 * frame. `struck` is this frame's aos_wall_weapon_hit; `soul_flags` the
 * 0x13260 bits. The break sets the wall's event bit. */
void aos_wall_update(AosWall *wall, AosCollision *layer, uint32_t events[AOS_WALL_EVENT_WORDS],
                     uint32_t soul_flags, bool struck, uint32_t (*random)(void),
                     AosWallReport *report);

/* Whether the wall is still updated (a broken wall keeps its spawners). */
bool aos_wall_alive(const AosWall *wall);

#endif /* AOS_WALL_H */
