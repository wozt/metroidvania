/* Manticore (EnemyManticoreCreate 0x08072854 / EnemyManticoreUpdate
 * 0x08072768, id 0x36): the winged boss that grows extra wings during
 * its attack and launches a sequential volley of six children.
 *
 * Native layout folded into AosEnemy:
 * - the + 0x14 container entity and its six child slots fold into the
 *   AOS_ROLE_MANTICORE_CHILD0..5 persistent children emitted over the
 *   first updates under the 8-child cap, chained in slot order like
 *   the native + 0x20 sibling links;
 * - the grown wings of sub_08073B54 (update sub_08073C3C) become
 *   AOS_ROLE_MANTICORE_WING children; the attack shot of
 *   sub_08073D98 (update sub_08074048) becomes AOS_ROLE_MANTICORE_SHOT;
 * - the + 0x6E/+ 0x6F attack counters, the + 0x24/+ 0x25 variant picks
 *   and the container + 0x1F launch count fold into mt_* fields; the
 *   sub_08073568 + 0x1C == 6 container entity folds its + 0x20 phase
 *   and + 0x1F/+ 0x1E substate machine into mt_cphase/mt_csub/mt_ccnt.
 *
 * Documented approximations: the state-0 six-case approach machine
 * keeps its probes, random attack triggers and palette flashes but
 * collapses the sub_08069A00 accel tail into move_toward; the
 * state-1 thirteen-case attack keeps its case order, the + 0x6E
 * == 0xE wing/shot/song marker and the + 0x25 variant pick, while
 * the container + 0x1F gates read the launched-child count; the
 * child launch keeps the native slot velocity table, facing offset
 * and sign while the sub_08073568 child brain is ported for the
 * volley children (the + 0x1C != 6 paths): the parent + 0x6D anim
 * dispatch, the anim-8 launch windows and the _08073A8A orbit
 * integrate, with the container entity itself folded into the
 * parent's mt_c* fields; the wing part's
 * sub_08073C3C flap machine collapses to an orbiting hitbox;
 * sub_0806AEAC maps to the shared generic flame death. */

#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <string.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define integrate aos_enemy_integrate
#define player_near aos_enemy_player_near

static bool mt_is_manticore(uint8_t id) { return id == 0x36; }

/* sub_080009E4: the native sine table, 16.16 results. */
static int32_t mt_sin(uint32_t angle) {
    return aos_sine(angle & 0xFFFF);
}
static int32_t mt_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
/* sub_0803E86C: the 16.16 fixed multiply. */
static int32_t mt_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* sub_080733D4 launch velocity magnitude: slot * 0x80000 plus a base
 * of 0x60000 for slot 0 and 0xA0000 for slots 1-5 (the native
 * `slot << 0x13` + `0xC0 << 0xB` / `0xA0 << 0xC` table).  The sign
 * and the spawn offset follow the parent's + 0x58 bit 0x40 facing:
 * set (the enemy sits left of the player, the port's `mirrored`)
 * negates both, clear keeps them positive — the wrapper launch
 * fires the children backward; the brain's anim-8 windows re-launch
 * them forward. */
static int32_t mt_launch_mag(int slot) {
    return slot * 0x80000 + (slot == 0 ? 0x60000 : 0xA0000);
}

/* sub_0803E8BC: the 16.16 fixed divide (a8/b8 shifted back). */
static int32_t mt_div(int32_t a, int32_t b) {
    if (!b) return 0;
    if (b == 0x10000) return a;
    int32_t a8 = a < 0 ? -((-a) >> 8) : a >> 8;
    int32_t b8 = b < 0 ? -((-b) >> 8) : b >> 8;
    if (!b8) return 0;
    return ((a8 << 8) / b8) << 8;
}

/* Emit the six children of sub_08073308 over the first updates,
 * two per call at most under the room child cap.  Each child gets
 * its slot index in mt_slot, the parent's id byte semantics in
 * static_frame (native + 0x65 = 1 for slot 5 else 2) and links to
 * the parent; the native + 0x20 sibling chain becomes the
 * mt_launched launch gate. */
static void mt_emit_children(AosEnemy *e, AosHitReport *hit) {
    while (e->spawned_count < 6 && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = (uint8_t)(AOS_ROLE_MANTICORE_CHILD0 + i);
        c.parent = e;
        c.x = e->x;
        c.y = e->y;
        c.mt_slot = (uint8_t)i;
        c.mt_act = 0;
        c.static_frame = (int16_t)(i == 5 ? 1 : 2);
        c.hp = 1;
        hit->children[hit->child_count++] = c;
    }
}

/* sub_08073B54: grow a wing part (native entity 0x1B, update
 * sub_08073C3C) at the parent's position with the facing copy. */
static void mt_grow_wing(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_MANTICORE_WING;
    c.parent = e;
    c.x = e->x + (e->mirrored ? -0x1F0000 : 0x1F0000);
    c.y = e->y - 0xF0000;
    c.vx = c.vy = 0;
    c.hp = 1;
    c.static_frame = 0;
    hit->children[hit->child_count++] = c;
}

/* sub_08073D98: the attack shot (native entity 0x49, update
 * sub_08074048) spawned beside the parent, facing-mirrored. */
static void mt_fire_shot(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_MANTICORE_SHOT;
    c.parent = e;
    c.x = e->x + (e->mirrored ? -0x1F0000 : 0x1F0000);
    c.y = e->y - 0xF0000;
    c.vx = e->mirrored ? 0x40000 : -0x40000;
    c.vy = 0;
    c.hp = 1;
    c.static_frame = 5;
    hit->children[hit->child_count++] = c;
}

/* _08073A8A: the volley child's orbit integrate — the child's slot
 * and the folded container's + 0x20 phase (parent mt_cphase) derive
 * a heading, + 0x48 is rescaled by the phase factor, the position
 * advances by sine/cosine of the heading times that speed and both
 * + 0x48/+ 0x4C are cleared.  With no launch velocity armed the
 * child simply holds its position. */
static void mt_child_orbit(AosEnemy *e, AosEnemy *p) {
    int32_t cph = p->mt_cphase;
    int32_t q = ((cph - 0x4000) * 5) / 6;
    int32_t r5 = q * 24 + 0x70000;
    int32_t r7 = ((int32_t)e->mt_slot << 16) + r5 - 0x60000;
    int32_t head = mt_div(mt_mul(cph, r7), r5);
    int32_t sp = (cph - 0x7000) / 4;
    e->vx = mt_div(mt_mul(e->vx, sp + 0x4000), 0x4000);
    if (!e->mirrored)
        head = 0x10000 - head;
    e->x += mt_mul(mt_sin((uint32_t)(head + 0x4000)), e->vx);
    e->y += mt_mul(mt_sin((uint32_t)head), e->vx);
    e->vx = e->vy = 0;
}

/* sub_08073568 for the + 0x1C != 6 volley children: the parent + 0x6D
 * anim dispatch.  Only anim id 8 repositions and re-launches the
 * children (the two + 0xD timer windows of _080738D4/_08073940); every
 * other id — and the inactive parent of _080739D4 — falls into the
 * _08073A8A orbit integrate.  The brain holds no lifetime: the
 * children live until the wrapper deletes them with the parent. */
static void mt_child_update(AosEnemy *e, const AosAnimSet *anims,
                            AosSoma *soma, const AosEnemyKind *kind) {
    AosEnemy *p = e->parent;
    if (!p || !mt_is_manticore(p->id) || p->state == 2 || p->removed) {
        e->removed = true;
        return;
    }
    /* sub_080733D4 launch: the + 0x1D gate keeps the launch
     * one-shot per volley (the native slot-0 update clears the
     * + 0x20 sibling chain's + 0x1D flags when the next volley arms,
     * and the recursion launches all six children in one pass, slot
     * 5 first).  The child copies the container's + 0x40/+ 0x44
     * position plus the facing offset — x +/- 0xC0000 and y
     * -0xE0000 — and takes the signed slot velocity into + 0x48. */
    if (!e->mt_act) {
        e->x = p->x + (p->mirrored ? -0xC0000 : 0xC0000);
        e->y = p->y - 0xE0000;
        int32_t mag = mt_launch_mag(e->mt_slot);
        e->vx = p->mirrored ? -mag : mag;
        e->vy = 0;
        e->mirrored = p->mirrored;
        e->mt_act = 1;
        e->timer = 0;
    }
    /* Parent anim id 8: the launch windows — (timer - 0xA) & 0xFF in
     * [0, 4] uses the +/- 0x80000 offset, above 0xE the +/- 0xC0000
     * one, both with y - 0x150000, the slot velocity re-armed and
     * the facing bit inverted; the gap in between keeps the last
     * position. */
    if (p->anim.id == 8) {
        uint32_t t = (uint32_t)(p->timer - 0xA) & 0xFF;
        if (t <= 4 || t > 0xE) {
            int32_t off = t <= 4 ? 0x80000 : 0xC0000;
            e->x = p->x + (p->mirrored ? off : -off);
            e->y = p->y - 0x150000;
            int32_t mag = mt_launch_mag(e->mt_slot);
            e->vx = p->mirrored ? mag : -mag;
            e->vy = 0;
            e->mirrored = !p->mirrored;
        }
    }
    mt_child_orbit(e, p);
    (void)anims; (void)soma; (void)kind;
}

/* The + 0x1C == 6 container entity of sub_08073120 folded onto the
 * parent: its + 0x20 phase (+ mt_cphase, created 0x4000) and the
 * + 0x1F/+ 0x1E substate machine it runs while the parent's + 0x6D
 * anim id is 2 or 3.  sub_0806ADBC's + 0x2D hit-flash gate maps to
 * hit_flash (its gEwramData and + 0x38 branches are not ported); the
 * container's own position and + 0x48 speed only drive its drawing,
 * so they are not modeled. */
static void mt_container_tick(AosEnemy *p) {
    if (p->hit_flash)
        return;
    switch (p->anim.id) {
    case 0: {
        /* + 0x6E jump table: frames 0/5/6/7 ratchet up by 0x60 while
         * <= 0x3FFF, frames 1-4 decay by 0x60 while > 0x3800. */
        unsigned f = p->anim.frame;
        if (f > 7)
            break;
        if (f == 0 || f == 5 || f == 6 || f == 7) {
            if (p->mt_cphase <= 0x3FFF)
                p->mt_cphase += 0x60;
        } else if (p->mt_cphase > 0x3800) {
            p->mt_cphase -= 0x60;
        }
        break;
    }
    case 1: {
        /* Frames 2/3/8/9 decay, the rest grow by 0x40. */
        unsigned f = p->anim.frame;
        if (f == 2 || f == 3 || (f >= 8 && f <= 9)) {
            if (p->mt_cphase > 0x3800)
                p->mt_cphase -= 0x60;
        } else if (p->mt_cphase <= 0x3FFF) {
            p->mt_cphase += 0x40;
        }
        break;
    }
    case 2:
    case 3:
        switch (p->mt_csub) {
        case 0:
            p->mt_csub = 1;
            p->mt_ccnt = 0;
            break;
        case 1: {
            uint8_t c = ++p->mt_ccnt;
            if (c & 8) {
                /* Every second half of the ramp slews the phase
                 * toward 0x8000 by 0x40 (the + 0x4C heading writes of
                 * the native case are dead stores, overwritten by
                 * _08073A06). */
                if (p->mt_cphase - 0x8000 > 0x40)
                    p->mt_cphase -= 0x40;
                else if (p->mt_cphase - 0x8000 < -0x40)
                    p->mt_cphase += 0x40;
            }
            if (c > 0x1F) {
                p->mt_csub = 2;
                p->mt_ccnt = 0;
            }
            break;
        }
        case 2:
            if (++p->mt_ccnt > 0x13) {
                p->mt_ccnt = 0;
                p->mt_csub = 3;
                aos_audio_play_song(0x86);
            }
            break;
        case 3:
            p->mt_cphase += 0x800;
            if (p->mt_cphase > 0x7FFF) {
                p->mt_cphase = 0x8000;
                p->mt_csub = 4;
            }
            break;
        case 4:
            if (++p->mt_ccnt > 0xB) {
                p->mt_ccnt = 0;
                p->mt_csub = 5;
            }
            break;
        case 5:
            p->mt_cphase -= 0x400;
            if (p->mt_cphase <= 0x3FFF) {
                p->mt_cphase = 0x4000;
                p->mt_csub = 6;
                p->mt_ccnt = 0;
            }
            break;
        default:
            break;
        }
        break;
    default:
        /* Anim ids 4/7 hover and 8 launch: no + 0x20 phase writes. */
        break;
    }
}

/* sub_08073C3C: the grown wing — a flapping hitbox orbiting beside
 * the parent that dies with it. */
static void mt_wing_update(AosEnemy *e, const AosAnimSet *anims) {
    AosEnemy *p = e->parent;
    if (!p || !mt_is_manticore(p->id) || p->state == 2 || p->removed) {
        e->removed = true;
        return;
    }
    e->phase += 0x800;
    e->x = p->x + (p->mirrored ? -0x300000 : 0x300000) +
           mt_mul(0x80000, mt_sin(e->phase));
    e->y = p->y - 0x80000 + mt_mul(0x40000, mt_cos(e->phase));
    (void)anims;
}

/* sub_08074048: the attack shot — flies straight, dies on walls,
 * the parent or a lifetime. */
static void mt_shot_update(AosEnemy *e, const AosAnimSet *anims) {
    AosEnemy *p = e->parent;
    if (!p || !mt_is_manticore(p->id) || p->state == 2 || p->removed ||
        ++e->timer > 0x80) {
        e->removed = true;
        return;
    }
    integrate(e);
    (void)anims;
}

/* sub_080729E0 state 0: the six-case approach — edge probes both
 * sides, random attack triggers into state 1 (substep 0 or 0xA),
 * facing re-check, palette flash while waiting and the accel
 * approach collapsed into move_toward. */
static void mt_state0(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        /* Probe ahead at (-0x78,-0x28) size 0xF0x0x50 then aggro. */
        bool near = player_near(soma, e->x >> 16, e->y >> 16, 0xA0, 0x78);
        if (near) {
            e->step = 1;
            play(e, anims, 0);
            e->mt_phase = 9;
            e->timer = 0x64;
            e->vx = 0x200000;
            e->vy = 0;
        }
        break;
    }
    case 1: {
        /* Side probes: random attack entries on wall contact. */
        bool wall_l = aos_enemy_blocked_ahead(layer, (e->x - 0x600000) >> 16,
                                              (e->y - 0x3C0000) >> 16);
        bool wall_r = aos_enemy_blocked_ahead(layer, (e->x + 0x600000) >> 16,
                                              (e->y - 0x3C0000) >> 16);
        uint32_t r = random ? random() : 0;
        if ((wall_l && (r & 0xFF) <= 0x3F) ||
            (wall_r && (r & 0xFF) <= 0x7F)) {
            e->state = 1;
            e->step = wall_l ? 0 : 0xA;
            e->substep = 0;
            break;
        }
        if (aos_enemy_blocked_ahead(layer, e->x >> 16, e->y >> 16)) {
            e->step = 2;
            break;
        }
        /* Approach the player. */
        if (soma)
            aos_enemy_move_toward(e, target_x(soma), target_y(soma),
                                  0x10);
        break;
    }
    case 2:
        /* Facing re-check then back to the approach loop. */
        e->step = 1;
        break;
    case 3:
    case 5:
        /* Wait with palette flash then re-enter the approach. */
        if (++e->timer > 0x64) {
            e->step = 1;
            e->timer = 0;
        }
        break;
    default:
        e->step = 1;
        break;
    }
}

/* sub_08072D68 state 1: the thirteen-case attack — windup, the
 * container + 0x1F launch gates, the + 0x6E == 0xE wing/shot/song
 * marker, the + 0x25 variant pick and the return to state 0. */
static void mt_state1(AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *hit,
                      uint32_t (*random)(void)) {
    (void)soma;
    switch (e->step) {
    case 0:
        play(e, anims, 2);
        e->mt_f6f = 0;
        e->step = 1;
        break;
    case 1:
        if (e->mt_phase)
            e->attack_off = true;
        e->step = 2;
        break;
    case 2:
        if (e->mt_launched > 2)
            e->attack_off = false;
        e->step = 3;
        break;
    case 3:
        if (e->mt_launched > 4) {
            play(e, anims, 3);
            e->attack_off = false;
            e->timer = 0;
            e->step = 4;
        }
        break;
    case 4:
        if (++e->timer > 0x1D) {
            e->state = 0;
            e->step = 2;
            e->substep = 0;
            play(e, anims, 0);
        }
        break;
    case 10:
        play(e, anims, 4);
        e->step = 11;
        e->mt_f6f = 0;
        e->timer = 0;
        e->mt_cnt24 = 0;
        {
            uint32_t r = random ? random() : 0x80;
            e->mt_var25 = (r & 0xFF) <= 0x7F ? 1 : (uint8_t)((r & 1) + 2);
        }
        break;
    case 11:
        /* + 0x6E == 0xE: grow a wing, fire the shot, songs 0x7A/0x77. */
        if (e->mt_phase == 0xE) {
            aos_audio_play_song(0x7A);
            mt_grow_wing(e, hit);
            mt_fire_shot(e, hit);
            aos_audio_play_song(0x77);
        }
        /* + 0x6E == 0x14: variant anim until + 0x24 >= + 0x25. */
        if (e->mt_phase == 0x14) {
            if (++e->mt_cnt24 >= e->mt_var25) {
                play(e, anims, 4);
                e->mt_f6f = 0;
                e->mt_phase = 2;
            }
        }
        if (e->cycle_done) {
            e->step = 12;
            e->timer = 0;
        }
        break;
    case 12:
        if (++e->timer > 3) {
            e->state = 0;
            e->step = 2;
            e->substep = 0;
            play(e, anims, 0);
        }
        break;
    default:
        e->step = 0;
        break;
    }
    /* The + 0x6E attack counter advances through the machine. */
    if (e->state == 1 && e->mt_phase < 0x20)
        e->mt_phase++;
}

AosHitReport aos_enemy_manticore_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;

    /* Children, wings and shots dispatch on their roles. */
    if (e->role >= AOS_ROLE_MANTICORE_CHILD0 &&
        e->role <= AOS_ROLE_MANTICORE_CHILD5) {
        mt_child_update(e, anims, soma, kind);
        return hit;
    }
    if (e->role == AOS_ROLE_MANTICORE_WING) {
        mt_wing_update(e, anims);
        return hit;
    }
    if (e->role == AOS_ROLE_MANTICORE_SHOT) {
        mt_shot_update(e, anims);
        return hit;
    }
    if (!mt_is_manticore(e->id))
        return hit;

    /* EnemyManticoreUpdate prologue: state 2 skips the activity
     * gates; the probe, below-screen blink, hit reports and the
     * + 0x65 anim-change tail fold into the shared collide. */
    if (e->state != 2) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    /* The + 0x46 + 0x20 >= 0x44000 below-screen blink of the update
     * tail maps to the hidden flag's complement. */
    if (e->y + 0x20 >= 0x44000)
        e->hidden = true;
    else
        e->hidden = false;

    /* The six children emit on the first updates. */
    if (!e->boss_parts_spawned) {
        mt_emit_children(e, &hit);
        if (e->spawned_count >= 6)
            e->boss_parts_spawned = true;
    }
    /* The container + 0x1F launch count the state-1 gates read. */
    e->mt_launched = e->spawned_count;

    switch (e->state) {
    case 0:
        mt_state0(e, soma, layer, anims, random);
        break;
    case 1:
        mt_state1(e, soma, anims, &hit, random);
        break;
    case 2:
        /* sub_0806AEAC: the shared generic flame death — handled by
         * the lethal branch below like the other ports. */
        break;
    default:
        /* State 3 dormant (sub_0803F17C tail only). */
        break;
    }

    /* Lethal: the shared hurt path already applied damage; the
     * native death is sub_0806AEAC (flame every 4 frames over
     * 16 x 12) reached through state 2. */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x6E);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x3C;
    }
    if (e->state == 2) {
        /* sub_0806AEAC flame death: a flame every 4 frames until
         * the timer ends, then the entity deletes. */
        if ((e->timer & 3) == 0)
            aos_enemy_spawn_flame_at(e->x, e->y - 0x80000, 0x100000,
                                     0xC0000, random, &hit);
        if (e->timer)
            e->timer--;
        else
            e->removed = true;
    }

    /* The folded + 0x1C == 6 container entity's + 0x20 phase and
     * attack substate machine (sub_08073568), advanced once per
     * parent update for the volley children's orbit integrate. */
    mt_container_tick(e);

    step_anim(e, anims);
    return hit;
}
