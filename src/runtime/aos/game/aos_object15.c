#include "aos_object15.h"

void aos_object15_create(AosObject15 *o, int param0, bool event_set) {
    o->param0 = (uint8_t)param0;
    o->state = event_set ? 4 : 1;
    o->angle = 0x6000;          /* + 0x18 preset */
    o->counter = 0x200000;      /* + 0x1C preset */
    o->forced = -1;
    o->dir_word = 0;
    o->fade = false;
    o->dialogue = -1;
}

void aos_object15_update(AosObject15 *o, bool near, bool near2,
                         bool flag40, bool gate_clear,
                         uint16_t dir_word, bool soma_left) {
    o->forced = -1;
    o->walk_dir = 0;
    o->fade = false;
    o->dialogue = -1;
    o->dir_word = dir_word;

    switch (o->state) {
    case 1:
        if (near && flag40 && gate_clear) {
            o->state = 2;
            o->forced = 0;              /* + 0x42C bit 1 lock */
        }
        break;
    case 2: {
        /* counter > 0x90000 locks input, else the direction word. */
        o->forced = o->counter > 0x90000 ? 0 : 1;
        o->angle = (uint16_t)(o->angle + 0x40);
        if (o->counter == 0x100000)
            o->fade = true;             /* sub_0803D18C/sub_0803CDF0 */
        o->counter -= 0x4000;
        if (o->counter <= 0)
            o->state = 3;
        break;
    }
    case 3:
        o->forced = 1;
        o->dialogue = o->param0 ? 1 : 0;   /* 0x0852320C / 0x08517B70 */
        /* + 0x42C bit 1 released; the state advance is external. */
        break;
    case 4:
        /* entity fx <= soma fx -> 0x20, else 0x10. */
        o->forced = 2;
        o->walk_dir = soma_left ? 0x20 : 0x10;
        if (near2)
            o->state = 2;
        break;
    }
}
