#include "aos_object25.h"

#include "../core/aos_collision.h"

/* sub_0806D518's phase table at 0x08118CE0: 40 signed bytes,
 * out-of-range timers read as 0. Bit 0 drives + 0x58 bit 5. */
static const uint8_t PHASE[0x28] = {
    0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
    0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
};

/* The 0x085265F8 pointer table's four position lists (counts
 * 6/4/2/2 at 0x08526608), room pixels; entries are 16.16 in
 * the ROM and always integral here. */
static const int16_t SPAWN_X[4][6] = {
    {192, 320, 672, 736, 800, 864},
    {160, 288, 704, 832},
    {-16, 120},
    {-16, 120},
};
static const int16_t SPAWN_Y[4][6] = {
    {176, 176, 176, 176, 176, 176},
    {176, 176, 176, 176},
    {160, 168},
    {416, 424},
};
static const uint8_t SPAWN_COUNT[4] = {6, 4, 2, 2};

static void set_anim(AosObject25 *o, int table) {
    /* sub_0803F0E4(table, mode, flags): + 0x68 = the sequence
     * data, + 0x5B = mode, + 0x6C = flags | 8, + 0x59 bit 1
     * cleared. The runtime keeps the table id. */
    o->anim_table = (uint8_t)table;
    o->anim_seq = table;
    o->flags59 &= (uint8_t)~2;
}

static void physics_y(AosObject25 *o, const AosCollision *layer) {
    /* sub_0806D460(e, 0x80000): + 0x4C += + 0x54 (the accel is
     * never seeded — 0), clamp to +/- 0x80000, + 0x44 += + 0x4C,
     * then the sub_080698A8 floor probe. The tile walk is
     * approximated by aos_floor_depth at the entity pixel. */
    o->vel_y += 0;
    if (o->vel_y > 0x80000)
        o->vel_y = 0x80000;
    else if (o->vel_y < -0x80000)
        o->vel_y = -0x80000;
    o->pos_y += o->vel_y;
    if (layer && o->vel_y > 0) {
        int depth = aos_floor_depth(layer, o->pos_x >> 16,
                                    (o->pos_y >> 16) + 8, 0, false);
        if (depth < 0) {
            o->pos_y += (int32_t)depth << 16;
            o->vel_y = 0;
        }
    }
}

/* sub_0806AE54: while + 0x2D is armed it counts down and
 * rewrites the + 0x5A flash bits every substate-1 frame. */
static void blink_step(AosObject25 *o) {
    if (!o->blink)
        return;
    o->blink--;
    /* (+ 0x2C & 0xF) << 3 is approximated to 0: the nibble is an
     * unmodelled entity field. */
    o->flash = (o->flash & 0x86) | ((o->blink & 2) ? 0x20 : 0);
}

void aos_object25_create(AosObject25 *o, int16_t param0,
                         int16_t param1, int32_t pos_x, int32_t pos_y,
                         uint8_t flags4cc, uint16_t flags37e,
                         bool *deleted) {
    *o = (AosObject25){0};
    *deleted = false;
    o->pos_x = pos_x;
    o->pos_y = pos_y;
    o->param0 = param0;
    o->param1 = param1;
    o->anim_seq = -1;

    /* (gEwramData + 0x4CC & 3) == 0 with the + 0x37E bit 0x20
     * set deletes the entity. */
    if (!(flags4cc & 3) && (flags37e & 0x20)) {
        *deleted = true;
        o->deleted = true;
        return;
    }
    if (param1 == 1) {
        /* The invisible spawner node: + 0xA = 1, + 0x58 bit
         * 0x20, + 0xD = 0x78, sub_0806AF98 screen depenetration
         * (approximated away — it needs the tile probe). */
        o->state = 1;
        o->flags58 |= 0x20;
        o->timer = 0x78;
        return;
    }
    /* sub_08057DB8's special_37 triple always loads in the
     * runtime; the native failure path deletes. + 0x1A bits
     * 1-2, + 0x3E bit 0 and + 0x3C = 0x28 have no runtime
     * fields. */
    o->flags58 &= (uint8_t)~0x41;
    set_anim(o, 0);         /* sub_0803F0E4(0x08526550, 0, 0) */
    o->hp = 0xC8;           /* + 0x34 */
    o->kind = 0x14;         /* + 0x5C */
    o->state = 0;
    if (param1 == 2) {
        o->substate = 1;
        set_anim(o, 1);     /* sub_0803F0E4(0x08526564, 0, 1) */
        o->vel_x = 0x4000;
        o->vel_y = 0x80000;
        o->kind--;
    }
}

void aos_object25_update(AosObject25 *o, bool active,
                         const AosCollision *layer,
                         int32_t room_ox, int32_t room_oy,
                         AosRandom random) {
    o->anim_seq = -1;
    o->particle_fx = 0;
    o->damage = -1;
    o->spawn_pending = false;
    if (o->deleted || o->finished)
        return;

    if (o->state == 1) {
        /* Object25Update state 1: the OLD u8 + 0xD above 0x77
         * resets and rolls the (rand & 1) == 0 gate. */
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old <= 0x77)
            return;
        o->timer = 0;
        if (random() & 1 || o->children > 9)
            return;
        int variant = (int)o->param0;
        if (variant < 0 || variant > 3)
            return;
        int pick = (int)(random() % SPAWN_COUNT[variant]);
        /* child + 0x37 / + 0x32 take the gate's r8 = 0, the
         * child's + 0x30 copies the parent's and + 0x36 the
         * parent's byte; + 0x59 bit 1 is set natively. */
        o->children++;
        o->spawn_pending = true;
        o->spawn_x = ((int32_t)SPAWN_X[variant][pick] << 16) - room_ox;
        o->spawn_y = ((int32_t)SPAWN_Y[variant][pick] << 16) - room_oy;
        o->spawn_param0 = (uint8_t)o->param0;
        return;
    }

    /* sub_08057BAC on + 0xB; `active` stands in for the + 0x59
     * bit-0 engine mark substate 0 waits on. */
    switch (o->substate) {
    case 0:
        if (!active)
            break;
        o->substate = 1;
        set_anim(o, 1);     /* sub_0803F0E4(0x08526564, 0, 1) */
        o->vel_x = 0x4000;
        o->vel_y = 0x80000;
        o->kind--;
        break;
    case 1: {
        o->vel_x = 0x4000;
        o->pos_x += o->vel_x;   /* sub_0806D3C0 */
        o->vel_x = 0;
        if (o->param0 == 2) {
            if (o->pos_x + room_ox <= 0x00D00000) {
                o->vel_y = 0x80000;
                physics_y(o, layer);
            }
        } else {
            o->vel_y = 0x80000;
            physics_y(o, layer);
        }
        blink_step(o);
        int32_t wx = o->pos_x + room_ox;
        int32_t bound;
        if (o->param0 == 0 || o->param0 == 1)
            bound = 0x040FFFFF;
        else if (o->param0 == 2 || o->param0 == 3)
            bound = 0x00FFFFFF;
        else
            bound = INT32_MAX;
        if (wx > bound) {
            o->substate = 10;
            o->timer = 1;
        }
        break;
    }
    case 10:
        if (!(o->timer & 3))
            o->particle_fx++;   /* sub_0806D5C0 burst */
        {
            unsigned idx = o->timer > 0x27 ? 0 : o->timer;
            o->flags58 = (uint8_t)((o->flags58 & ~0x20) |
                                   ((PHASE[idx] & 1) << 5));
        }
        o->timer--;
        if ((int8_t)o->timer <= 0) {
            o->flags59 |= 8;
            o->finished = true;
        }
        break;
    default:
        break;
    }
    /* Tail: sub_0803F17C animation step (tick-driven draw) and
     * the substate-1 sub_080421AC attack-slot read — both have
     * no runtime output. */
}

void aos_object25_hurt(AosObject25 *o, int damage) {
    if (o->deleted || o->finished || o->state != 0 || o->hp <= 0 ||
        o->blink > 0)
        return;
    /* sub_08057D38: + 0x2D = 8 invulnerability, the
     * sub_08042B8C damage number, then + 0x34 -= damage. */
    o->blink = 8;
    o->damage = damage;
    o->hp = (int16_t)(o->hp - damage);
    if (o->hp > 0)
        return;
    /* Death: + 0x5A keeps bits 1/2/7 ((+ 0x2C & 0xF) << 3
     * approximated 0), + 0xD = 0x28, + 0x2D = 0, + 0xB = 10,
     * table 0x08526580 mode 2. */
    o->flash &= 0x86;
    o->timer = 0x28;
    o->blink = 0;
    o->substate = 10;
    set_anim(o, 2);
}
