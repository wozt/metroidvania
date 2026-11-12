#ifndef AOS_OBJECT15_H
#define AOS_OBJECT15_H
#include <stdbool.h>
#include <stdint.h>

/* Object15Update (0x08052A34), paired with the already-covered
 * Object15Create: a story cutscene director. Create loads the
 * special_21 art, links a type-0x1B child through sub_080528E0,
 * presets + 0x18 = 0x6000 and + 0x1C = 0x200000, then picks state 4
 * (with the + 0x42C bit-0 lock and the + 0x348 bit-0x8000000 set)
 * when the gEwramData + 0x348 word already carries that bit, else
 * state 1.
 *
 * State 1 waits for the Soma probe (0x60 x 0x7F window), the player
 * + 0x1C bit 0x40 and a clear sub_08021304 gate, then locks input
 * (+ 0x42C bit 1), runs sub_0800C5A8(0) and enters state 2.
 * State 2 forces SetPlayerInput(1, 0) while + 0x1C exceeds
 * 0x90000 else (1, the + 0x1339A direction word), advances + 0x18
 * by 0x40, fires the sub_0803D18C/sub_0803CDF0 fade pair exactly at
 * + 0x1C == 0x100000 and decrements + 0x1C by 0x4000 until it is
 * no longer positive, entering state 3.
 * State 3 forces the + 0x1339A direction, shows the param0-selected
 * dialogue record (0x0852320C / 0x08517B70), runs sub_08011AD0 on
 * gEwramData + 0x60, releases + 0x42C bit 1 and fades — the state
 * advance is driven externally.
 * State 4 forces the direction by the x compare against the
 * + 0x13110 entity and returns to state 2 on the second probe. */
typedef struct {
    uint8_t param0;         /* +0x30 s16: dialogue record selector */
    uint8_t state;          /* +0xA */
    uint16_t angle;         /* +0x18: += 0x40 in state 2 */
    int32_t counter;        /* +0x1C: 0x200000 down by 0x4000 */
    int forced;             /* SetPlayerInput mode: -1 none, 0 lock,
                               1 direction word, 2 x-compare dir */
    int walk_dir;           /* state-4 direction: 0x20/0x10 */
    uint16_t dir_word;      /* the + 0x1339A direction input */
    bool fade;              /* the state-2 counter == 0x100000 pair */
    int dialogue;           /* 0/1 record shown in state 3, -1 none */
} AosObject15;

/* Object15Create; `event_set` is the + 0x348 bit-0x8000000 test. */
void aos_object15_create(AosObject15 *o, int param0, bool event_set);

/* Object15Update. `near`/`near2` are the two sub_08068AD4 probes,
 * `flag40` the player + 0x1C bit 0x40, `gate_clear` the
 * sub_08021304 == 0 result, `dir_word` the + 0x1339A input and
 * `soma_left` the state-4 x compare. */
void aos_object15_update(AosObject15 *o, bool near, bool near2,
                         bool flag40, bool gate_clear,
                         uint16_t dir_word, bool soma_left);
#endif
