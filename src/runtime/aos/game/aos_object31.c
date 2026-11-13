#include "aos_object31.h"

/* ROM tables dumped from the (USA) image:
 *   0x0811865C u16[4] screen-x init {72,72,156,156}
 *   0x08118664 u16[4] screen-y init {26,44,26,44}
 *   0x0811866C u16[4] anim select  {0,1,0,1}
 *   0x08118674 u32[4] wait frames  {60,120,240,360}
 *   0x08118D08 s16 pairs[7]; index 3 = (48,48), the
 *   sub_0806D128 probe rect (x -48..288, y -48..208). */
static const uint16_t k_pos_x[4] = {72, 72, 156, 156};
static const uint16_t k_pos_y[4] = {26, 44, 26, 44};
static const uint16_t k_anim[4] = {0, 1, 0, 1};
static const uint32_t k_wait[4] = {60, 120, 240, 360};

static bool palette_path(int param) { return param == 0 || param == 2; }

/* sub_0806D128(e, 3): returns 1 (leave state) once the
 * entity's + 0x42/+ 0x46 screen pixels leave the (48,48)
 * rect dumped at 0x08118D08 entry 3. */
static bool probe_offscreen(int16_t sx, int16_t sy) {
    return sx < -48 || sx > 288 || sy < -48 || sy > 208;
}

/* sub_0806D460 twin-axis gravity step shared with Object21:
 * velocity += accel clamped to +/-0x70000, position += vel. */
static void physics_y(AosObject31 *o) {
    o->vel_y += o->accel_y;
    if (o->vel_y > 0x70000) o->vel_y = 0x70000;
    if (o->vel_y < -0x70000) o->vel_y = -0x70000;
    o->pos_y += o->vel_y;
}

static void physics_x(AosObject31 *o) {
    o->vel_x += o->accel_x;
    if (o->vel_x > 0x70000) o->vel_x = 0x70000;
    if (o->vel_x < -0x70000) o->vel_x = -0x70000;
    o->pos_x += o->vel_x;
}

/* sub_0806D3C0: integrate + 0x48 into + 0x40. */
static void integrate_x(AosObject31 *o) { o->pos_x += o->vel_x; }

/* sub_0803E058 approximation: the + 0x58 byte feeds an angle;
 * the spin state accumulates + 0x1C into + 0x18 instead. */
static void rotate(AosObject31 *o, int step) {
    o->angle += o->spin_total / 0x100 + step / 0x1000;
    (void)o->variant_byte;
}

void aos_object31_create(AosObject31 *o, int param,
                         uint32_t event_37e, int rand6,
                         bool palette_ok, bool *deleted) {
    *o = (AosObject31){0};
    o->param = (uint8_t)(param & 3);
    o->palette_slot = -1;
    if (event_37e & 4) {           /* + 0x37E bit 2 -> delete */
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* Sprite triple is the special_47 bank; the runtime only
     * needs the load to have succeeded (palette_ok models
     * sub_0803B800 >= 0 && sub_08068264 success). */
    if (!palette_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* sub_0803F2C8(k_anim[param], 0, 1) + sub_0803F17C. */
    (void)k_anim[o->param];
    o->spawn_idx = (uint8_t)(rand6 % 6);   /* + 0x6F */
    /* + 0x5C = 0x1A is an auxiliary timer the port does not
     * consume; + 0x59 &= ~0x1D ; + 0x58 &= ~0x41 */
    o->flags59 = 0;
    o->variant_byte = 0;
    if (palette_path(o->param)) {
        /* sub_0803E654(1): palette slot pick. */
        if (!palette_ok) {               /* -1 -> EntityDelete */
            o->deleted = true;
            *deleted = true;
            return;
        }
        o->variant_byte = (uint8_t)(o->param & 0x1F);
        o->flags59 |= 0x10;
        o->palette_slot = o->param & 0x1F;
        rotate(o, 0x10000);
    }
    o->pos_x_px = (int16_t)k_pos_x[o->param];
    o->pos_y_px = (int16_t)k_pos_y[o->param];
    o->wait = (int32_t)k_wait[o->param];
    o->state = 0;
}

bool aos_object31_update(AosObject31 *o, int16_t sx, int16_t sy,
                         uint32_t event_42c) {
    o->child_spawn = false;
    bool alive = true;

    switch (o->state) {
    case 0:
        if (event_42c & 0x1000) o->state = 1;
        break;
    case 1:
        if (--o->wait <= 0) {
            if (palette_path(o->param)) {
                o->state = 10;
                o->spin_total = 0;
                o->spin_acc = 0;
                o->spin_rate = 4;
                o->accel_x = 0x40000;
                o->accel_y = 0x200;
                o->timer = 0x3C;
                o->child_spawn = true;   /* sub_0805940C fx */
            } else {
                o->state = 2;
                o->accel_y = 0x400;
            }
        }
        break;
    case 2:
        physics_y(o);
        if (probe_offscreen(sx, sy)) o->state = 3;
        break;
    case 3:
        if (palette_path(o->param)) {
            o->palette_slot = -1;        /* sub_0803E6BC */
            o->deleted = true;
            alive = false;
        } else {
            o->state = 4;
        }
        break;
    case 10:
        physics_x(o);
        physics_y(o);
        if (--o->timer <= 0) {
            o->state = 11;
            o->accel_x = 0;
        }
        break;
    case 11:
        o->spin_acc += o->spin_rate;
        if (o->spin_acc > 0x60) o->spin_acc = 0x60;
        o->spin_total += o->spin_acc;
        rotate(o, 0x20000);
        integrate_x(o);
        physics_y(o);
        if (probe_offscreen(sx, sy)) o->state = 3;
        break;
    default:
        break;                          /* cases 4-9: render only */
    }

    if (palette_path(o->param)) rotate(o, 0x20000);
    return alive;
}
