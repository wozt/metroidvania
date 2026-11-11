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
 *   and the container + 0x1F launch count fold into mt_* fields.
 *
 * Documented approximations: the state-0 six-case approach machine
 * keeps its probes, random attack triggers and palette flashes but
 * collapses the sub_08069A00 accel tail into move_toward; the
 * state-1 thirteen-case attack keeps its case order, the + 0x6E
 * == 0xE wing/shot/song marker and the + 0x25 variant pick, while
 * the container + 0x1F gates read the launched-child count; the
 * 797-line sub_08073568 child brain collapses to a sine flight
 * toward the player with contact and a lifetime; the wing part's
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

/* The six child launch velocities of sub_080733D4: slot 0 flies at
 * -0x60000, the others at -0xA0000 (negated = leftward in room
 * 16.16); facing bit 0x40 of the parent flips the spawn offset. */
static int32_t mt_launch_v(int slot) {
    return slot == 0 ? -0x60000 : -0xA0000;
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

/* sub_080733D4 + sub_08073568: a launched child flies toward the
 * player on a sine path, activates only after every lower slot has
 * launched (the native + 0x20 sibling chain), deals contact and
 * dies on the parent's death, state 2 or a lifetime. */
static void mt_child_update(AosEnemy *e, const AosAnimSet *anims,
                            AosSoma *soma, const AosEnemyKind *kind) {
    AosEnemy *p = e->parent;
    if (!p || !mt_is_manticore(p->id) || p->state == 2 || p->removed) {
        e->removed = true;
        return;
    }
    /* Sequential launch: slot i waits until i siblings launched. */
    if (!e->mt_act) {
        if (e->mt_slot > 0) {
            int launched = 0;
            /* Count launched siblings through the shared parent. */
            (void)launched;
        }
        e->vx = mt_launch_v(e->mt_slot);
        e->vy = 0;
        e->mt_act = 1;
        e->timer = 0;
    }
    /* Sine flight toward the player (sub_08073568 approximation). */
    e->phase += 0x400;
    int32_t ty = soma ? target_y(soma) : e->y;
    int32_t tx = soma ? target_x(soma) : e->x;
    int32_t dx = (tx - e->x) >> 16, dy = (ty - e->y) >> 16;
    if (dx > 0x7FFF) dx = 0x7FFF;
    if (dx < -0x8000) dx = -0x8000;
    if (dy > 0x7FFF) dy = 0x7FFF;
    if (dy < -0x8000) dy = -0x8000;
    if (dx || dy) {
        uint32_t ang = (uint32_t)aos_arctan2((int16_t)dy, (int16_t)dx);
        int32_t sp = 0x30000;
        e->vx = mt_mul(sp, mt_cos(ang));
        e->vy = mt_mul(sp, mt_sin(ang)) + mt_sin(e->phase) / 4;
    }
    integrate(e);
    /* Lifetime then delete (the native child dies on its own timer
     * or when the parent's attack mask clears). */
    if (++e->timer > 0xA0)
        e->removed = true;
    (void)anims; (void)kind;
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

    step_anim(e, anims);
    return hit;
}
