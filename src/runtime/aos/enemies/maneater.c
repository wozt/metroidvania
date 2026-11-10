/* Man Eater (Aria of Sorrow enemy id 0x40), ported from
 * EnemyManEaterCreate (0x080BAC3C) / EnemyManEaterUpdate (0x080BAD68)
 * and the cluster sub_080BA280..sub_080BB0F0 of code_080B0AD4.s.
 *
 * The native actor is a four-level hierarchy on the gEwramData + 0x4E4
 * slot table (stride 0x84, links + 0x10/+ 0x14/+ 0x15/+ 0x16): the
 * head spawns three level children (sub_080BAAC0, entity id 0x49),
 * each fan volley (state 1) spawns a volley entity (sub_080BA788)
 * which in turn spawns a mouth flash (sub_080BA280) and six bolts
 * (sub_080BA37C) in a fixed fan, and the death state rains
 * sub_080BABD4 particles through EntityCreateInRange.  Here every
 * descendant is a persistent child of an AOS_ROLE_MAN_* role linked
 * through `parent`; the + 0x4E4 slot indices fold into me_bolt/me_fan.
 *
 * Head fields: + 0xA state, + 0xB fan index, + 0xD timer, + 0xF half
 * death flag, + 0x10 parent slot, + 0x17 slot index, + 0x18 angle,
 * + 0x1C death flag (create sets 3; the hurt callback sub_080BB04C
 * clears + 0x34 hp which the shared lethal pass covers), + 0x1E spin
 * phase, + 0x20 volley life, + 0x30 param0, + 0x36 variant,
 * + 0x5C draw byte, + 0x65 frame. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <stdlib.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define toward_facing aos_enemy_toward_facing
#define player_dy aos_enemy_player_dy
#define player_in aos_enemy_player_in
#define player_in_range aos_enemy_player_in_range
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define random_centred(r, n) aos_enemy_random_centred((r), (n))

static bool me_is_maneater(uint8_t id) { return id == AOS_ENEMY_MAN_EATER; }

static int32_t me_sin(uint32_t angle) { return aos_sine(angle & 0xFFFF); }
static int32_t me_cos(uint32_t angle) { return aos_sine((angle + 0x4000) & 0xFFFF); }

/* The 0x085288A8 level-child table (four bytes per slot): byte 0 is the
 * child's + 0x65 frame, byte 1 the volley animation. */
static const uint8_t me_level_tab[3][4] = {
    {0x07, 0x02, 0, 0}, {0x04, 0x01, 0, 0}, {0x01, 0, 0, 0}};

/* ------------------------------------------------------------------ */
/* Children                                                             */
/* ------------------------------------------------------------------ */

/* sub_080BAAC0: a level child — waits on the parent, flashes its anim
 * on its fan turn, then deletes. */
static void me_level_update(AosEnemy *c, AosEnemy *p,
                            const AosAnimSet *anims) {
    if (!p || p->removed || !me_is_maneater(p->id)) {
        c->removed = true;
        return;
    }
    /* Tail of the native update: the child rides the parent's position. */
    c->x = p->x;
    c->y = p->y;
    switch (c->state) {
    case 0:
        c->hidden = false;
        c->static_frame = me_level_tab[c->me_bolt & 3][0];
        play_loop(c, anims, me_level_tab[c->me_bolt & 3][1], true);
        c->state = 1;
        return;
    case 1:
        if (p->state == 1 && p->timer == 0x30 &&
            c->me_bolt == p->me_fan) {
            play(c, anims, me_level_tab[c->me_bolt & 3][1]);
            c->state = 2;
            c->timer = 0x30;
        }
        return;
    case 2:
        step_anim(c, anims);
        if ((uint8_t)--c->timer == 0) c->state = 3;
        return;
    case 3:
        c->hidden = false;
        c->removed = true;
        return;
    default:
        return;
    }
}

/* sub_080BA37C: a fan bolt — flies its + 0x18 angle and dies with the
 * volley or off the margins. */
static void me_bolt_update(AosEnemy *b, AosEnemy *v,
                           const AosCollision *layer) {
    if (!v || v->removed || v->role != AOS_ROLE_MAN_VOLLEY ||
        b->me_life <= 0) {
        b->removed = true;
        return;
    }
    /* The fan angles spread around the horizontal: velocity from the
     * + 0x18 angle at the native ~8.0 px/frame (documented). */
    int32_t sp = 0x80000;
    b->vx = me_sin(b->me_ang) * sp / 0x10000;
    b->vy = -me_cos(b->me_ang + 0x4000) * sp / 0x10000;
    b->x += b->vx;
    b->y += b->vy;
    b->me_life--;
    int sx = (int)(b->x >> 16), sy = (int)(b->y >> 16);
    if (b->me_life <= 0 ||
        sx < -64 || sx > 64 * 30 + 64 || sy < -64 || sy > 26 * 32 + 64)
        b->removed = true;
    (void)layer;
}

/* sub_080BA788: the volley entity — emits the six-bolt fan on its
 * first update, then lives out its + 0x20 timer beside the parent. */
static void me_volley_update(AosEnemy *v, AosEnemy *p,
                             AosHitReport *report,
                             const AosAnimSet *anims) {
    if (!p || p->removed || !me_is_maneater(p->id)) {
        v->removed = true;
        return;
    }
    v->x = p->x;
    v->y = p->y;
    switch (v->state) {
    case 0:
        /* state 0: the same resources and frame as the head; the six
         * bolts go out with angles 0xAAAA * i - 0x2000 (i = 0..5). */
        v->hidden = false;
        play_loop(v, anims, 0, true);
        if (!v->me_seen) {
            v->me_seen = 1;
            for (int i = 0; i < 6; ++i) {
                if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) break;
                AosEnemy *b = &report->children[report->child_count++];
                *b = *v;
                b->role = AOS_ROLE_MAN_SHOT;
                b->parent = p;
                b->state = b->step = b->substep = 0;
                b->hp = 1;
                b->hit_flash = 0;
                b->defeated = false;
                b->hidden = false;
                b->removed = false;
                b->me_life = 0x60;
                b->me_bolt = (uint8_t)i;
                b->me_ang = (int16_t)((0xAAAAu * i - 0x2000) & 0xFFFF);
                b->me_fan = 0;
                b->vx = b->vy = b->ax = b->ay = 0;
                /* The bolt leaves from the parent's mouth offset. */
                b->x = p->x;
                b->y = p->y - AOS_FIXED(8);
                b->combat.attack_off = false;
                b->combat.hurt_off = true;
            }
        }
        v->state = 1;
        v->me_life = 0x40;
        return;
    case 1:
        step_anim(v, anims);
        if (--v->me_life <= 0) v->removed = true;
        return;
    default:
        v->removed = true;
        return;
    }
}

/* sub_080BABD4: a death particle — drifts, spits gore rows every
 * sixteenth tick while the head lives, then fades out. */
static void me_deathp_update(AosEnemy *d, AosEnemy *p,
                             AosHitReport *report,
                             const AosAnimSet *anims,
                             uint32_t (*random)(void)) {
    if (!p || p->removed || !me_is_maneater(p->id)) {
        d->removed = true;
        return;
    }
    d->x += d->vx;
    d->y += d->vy;
    if (d->timer == 0xA) d->me_seen = 1;
    if (!d->me_bolt && (d->timer & 0xF) == 0 &&
        report->child_count < AOS_ENEMY_MAX_CHILDREN) {
        /* sub_0806D5C0 gore row: a small hidden ember child. */
        AosEnemy *g = &report->children[report->child_count++];
        *g = *d;
        g->role = AOS_ROLE_MAN_DEATHP;
        g->parent = p;
        g->state = g->step = g->substep = 0;
        g->hp = 1;
        g->hidden = true;
        g->removed = false;
        g->defeated = false;
        g->me_bolt = 1;
        g->me_life = 0x10;
        g->vx = (int32_t)(random_centred(random, 0x4000));
        g->vy = (int32_t)(random_centred(random, 0x4000));
        g->timer = 0x10;
    }
    if ((uint8_t)--d->timer == 0) d->removed = true;
    (void)anims;
}

/* ------------------------------------------------------------------ */
/* Head                                                                 */
/* ------------------------------------------------------------------ */

static void me_emit_levels(AosEnemy *e, AosHitReport *report,
                           const AosAnimSet *anims) {
    if (e->boss_parts_spawned ||
        report->child_count + 3 > AOS_ENEMY_MAX_CHILDREN)
        return;
    for (int i = 0; i < 3; ++i) {
        AosEnemy *c = &report->children[report->child_count++];
        *c = *e;
        c->role = AOS_ROLE_MAN_LEVEL;
        c->parent = e;
        c->state = 0;
        c->step = c->substep = 0;
        c->timer = 0;
        c->hp = 1;
        c->hit_flash = 0;
        c->defeated = false;
        c->hidden = true;
        c->removed = false;
        c->me_bolt = (uint8_t)i;
        c->me_fan = 0;
        c->me_ang = 0;
        c->me_life = 0;
        c->me_seen = 0;
        c->vx = c->vy = c->ax = c->ay = 0;
        c->combat.attack_off = true;
        c->combat.hurt_off = true;
        play_loop(c, anims, me_level_tab[i][1], true);
        step_anim(c, anims);
    }
    e->boss_parts_spawned = true;
}

AosHitReport aos_enemy_man_eater_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    const AosAnimSet *anims = kind->anims;

    /* The descendant roles update against their parent slot. */
    if (e->role == AOS_ROLE_MAN_LEVEL) {
        me_level_update(e, e->parent, anims);
        return report;
    }
    if (e->role == AOS_ROLE_MAN_VOLLEY) {
        me_volley_update(e, e->parent, &report, anims);
        return report;
    }
    if (e->role == AOS_ROLE_MAN_SHOT) {
        me_bolt_update(e, e->parent, layer);
        /* The bolt's contact pass runs through the shared collide so
         * the weapon and Soma boxes apply (sub_080B9FC8's role). */
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    if (e->role == AOS_ROLE_MAN_DEATHP) {
        me_deathp_update(e, e->parent, &report, anims, random);
        return report;
    }
    if (e->role != AOS_ROLE_ENEMY || !me_is_maneater(e->id))
        return report;

    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (e->state != 0xFF && e->state != 3 && e->state != 4)
        e->hidden = outside_margins(kind, 4, sx, sy);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
        (uint16_t)sy <= 0xB0)
        report.targetable = true;

    /* sub_080BAD68 head: the + 0x1C death gate (the hurt callback
     * sub_080BB04C clears + 0x34; hp <= 0 lands here through the
     * shared lethal pass which already set state 3). */
    if (e->state != 3 && e->hp <= 0 && e->state != 4) {
        e->static_frame = 0x11;
        e->defeated = true;
        e->state = 3;
        e->timer = 0x80;
        e->ax = 0x40000;
        aos_audio_play_song(0x6B);
        if (e->param0) {
            /* sub_08034498(0) event + sub_080D6450/sub_080D6EF8 room
             * bookkeeping — approximated (documented). */
        }
    }

    /* Emit the three level children on the first live update. */
    if (e->state < 3)
        me_emit_levels(e, &report, anims);

    switch (e->state) {
    case 0:
        /* sub_0806E29C(e, 0xA0, 0x78): the aggro box. */
        if (player_in(soma, (int16_t)((e->x >> 16) - 0xA0 / 2),
                      (int16_t)((e->y >> 16) - 0x78 / 2), 0xA0, 0x78)) {
            e->state = 1;
            e->timer = 0x30;
            e->me_fan = 0;
        }
        break;
    case 1:
        if ((uint8_t)--e->timer == 0) {
            /* The fan volley: one volley entity per index, its angle
             * ((fan+1) << 16) / 3 - 0x4000 and slot fan. */
            if (report.child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy *v = &report.children[report.child_count++];
                *v = *e;
                v->role = AOS_ROLE_MAN_VOLLEY;
                v->parent = e;
                v->state = 0;
                v->step = v->substep = 0;
                v->timer = 0;
                v->hp = 1;
                v->hit_flash = 0;
                v->defeated = false;
                v->hidden = false;
                v->removed = false;
                v->me_seen = 0;
                v->me_bolt = e->me_fan;
                v->me_ang = (int16_t)((((e->me_fan + 1) << 16) / 3 - 0x4000)
                                      & 0xFFFF);
                v->me_life = 0;
                v->vx = v->vy = v->ax = v->ay = 0;
                v->combat.attack_off = true;
                v->combat.hurt_off = true;
                v->x = e->x;
                v->y = e->y - AOS_FIXED(8);
            }
            e->timer = 0x30;
            e->me_fan++;
            if (e->me_fan > 2) {
                e->state = 2;
                e->me_fan = 0;
            }
        }
        break;
    case 2: {
        /* sub_0806E43C chase: approach the point offset 2.0 px ahead
         * of the target, 3.0 above, at speed 8. */
        int32_t gx = target_x(soma) + ((e->x < target_x(soma)) ? -0x200000 : 0x200000);
        int32_t gy = target_y(soma) - 0x300000;
        int32_t dx = gx - e->x, dy = gy - e->y;
        int64_t adx = dx < 0 ? -(int64_t)dx : dx;
        int64_t ady = dy < 0 ? -(int64_t)dy : dy;
        int64_t dist = adx + ady;
        /* sub_0806E43C(e, gx, gy, 8): step 8/16 of the way per tick. */
        if (dist > 0) {
            e->x += (int32_t)(((int64_t)dx << 16) / dist) * 8 / 16;
            e->y += (int32_t)(((int64_t)dy << 16) / dist) * 8 / 16;
        }
        break;
    }
    case 3:
        /* The death rain: every other tick a sub_080BABD4 particle,
         * the shared drift, then the fade-out delete. */
        if ((e->timer & 1) == 0 &&
            report.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy *d = &report.children[report.child_count++];
            *d = *e;
            d->role = AOS_ROLE_MAN_DEATHP;
            d->parent = e;
            d->state = d->step = d->substep = 0;
            d->hp = 1;
            d->hit_flash = 0;
            d->defeated = false;
            d->hidden = false;
            d->removed = false;
            d->me_seen = 0;
            d->me_bolt = 0;
            d->me_life = 0x28 + (random_centred(random, 0x10));
            d->timer = d->me_life;
            d->vx = (int32_t)(random_centred(random, 0x4000));
            d->vy = (int32_t)(random_centred(random, 0x4000)) - 0x2000;
            d->ax = d->ay = 0;
            d->combat.attack_off = true;
            d->combat.hurt_off = true;
        }
        if ((uint8_t)--e->timer == 0x30) e->me_seen = 1;
        if (e->timer == 0) {
            e->hidden = false;
            e->removed = true;
        }
        break;
    case 4:
        /* Dormant under the 0x8E & 0x40 flag: nothing moves. */
        break;
    default:
        break;
    }

    /* The shared tail: the + 0x1E spin feeds the + 0x18 angle the
     * sub_0803E058 transform reads (the draw rotation is cosmetic). */
    e->me_ang = (int16_t)(e->me_ang + 0x80);
    int sv = (int)me_sin((uint16_t)e->me_ang);
    e->me_ang = (int16_t)((sv < 0 ? -((-sv) >> 8) : sv >> 8) << 4);

    /* The contact/hurt pass of the native update tail. */
    if (e->state != 3 && e->state != 4) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}
