/* Une (id 0x3D) — EnemyUneCreate 0x0807427C, EnemyUneUpdate
 * 0x08074130, state0 sub_080743B4, state1 sub_080748C0, hurt
 * sub_0807473C, debris sub_080747B4/sub_08074838.
 *
 * The ghost splits: a room-spawned Une (param1 nonzero) hides and
 * waits in state 3 while its create-time child (a full Une linked
 * through + 0x18) lives; when the child's population count dies the
 * spawner removes itself. Each state-0 Une that has touched the
 * floor probes the player in a 0xC8 x 0x140 box, plays the
 * one-shot animation chain 1 -> 2 -> 3 and then runs a split cycle
 * (step 4): every 0xF timer ticks it births another Une — same art,
 * random vx ± 0x20000 / vy -0x30000..-0x50000, gravity + 0x54 =
 * 0x2800 and step 0xA so it lands straight into the walk — and every
 * fourth tick it sheds a debris sprite (anim 4) that decays at
 * 7/8 a tick for 0x14 ticks. A population at the root's param0 cap
 * sends the surplus to step 5, which finishes on animation 5 and
 * enters the state-2 blink death; a dying child decrements the
 * root's live count once, and a count of zero removes the spawner.
 *
 * The + 0x54 motion gravity is carried in `ay` (zero after the first
 * floor touch), the hurt callback's + 0x2D = 8 flash is the shared
 * hit_flash, the lethal transition's + 0x5A palette merge and the
 * + 0x5C-1 debris draw byte are cosmetic and folded into the shared
 * draw, and sub_08021924's target registration is implicit in the
 * engine (documented). */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"

#define play_loop aos_enemy_play_loop
#define step_anim aos_enemy_step_anim
#define outside_margins aos_enemy_outside_margins
#define collide aos_enemy_collide
#define player_in aos_enemy_player_in
#define projectile_collide aos_enemy_projectile_collide
#define accel_move aos_enemy_accel_move

static int16_t une_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t une_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* sub_0806B0A8's room-window test, as in buer.c: the point leaves
 * the room's cells (30 x 26 for a single screen, 32 per screen
 * otherwise; negative pixels round toward zero). */
static bool room_outside(const AosEnemy *e, const AosCollision *layer, int dx, int dy) {
    if (!layer || !layer->width_screens || !layer->height_screens) return false;
    int width = layer->width_screens == 1 ? 30 : 32 * layer->width_screens;
    int height = layer->height_screens == 1 ? 26 : 32 * layer->height_screens;
    int x = (int16_t)((e->x >> 16) + dx), y = (int16_t)((e->y >> 16) + dy);
    int cx = (x < 0 ? x + 7 : x) >> 3, cy = (y < 0 ? y + 7 : y) >> 3;
    return cx < 0 || cx >= width || cy < 0 || cy >= height;
}

/* The root spawner every descendant links to (+ 0x18). A lone Une
 * (param1 == 0) has no link: it never reaches the split steps. */
static AosEnemy *une_root(AosEnemy *e) {
    return e->parent && e->parent->id == AOS_ENEMY_UNE ? e->parent : NULL;
}

static void une_child_init(AosEnemy *c, const AosEnemy *e,
                           const AosAnimSet *anims) {
    *c = *e;
    c->x = e->x;
    c->y = e->y;
    c->vx = c->vy = c->ax = 0;
    c->hidden = false;
    c->translucent = false;
    c->mirrored = false;
    c->removed = c->defeated = false;
    c->hit_flash = 0;
    c->state = 0;
    c->step = c->substep = 0;
    c->timer = 0;
    c->une_wait = 0;
    c->une_child = 0;
    c->boss_parts_spawned = false;
    c->une_draw = e->une_draw;
    c->anim.id = c->anim.frame = c->anim.tick = 0;
    c->anim.flags = 0;
    c->cycle_done = false;
    c->combat.attack_off = c->combat.hurt_off = true;
    c->contact_elements = e->contact_elements;
    play_loop(c, anims, 0, true);
}

/* The state-3 create child (native: inside EnemyUneCreate, one
 * pixel up, step 0, short-fuse + 0x1C = 1, linked to the spawner
 * itself). Emitted on the spawner's first state-3 update because
 * the create path has no report (documented). */
static void une_create_child(AosHitReport *report, AosEnemy *e,
                             const AosAnimSet *anims,
                             uint32_t (*random)(void)) {
    (void)random;
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    une_child_init(c, e, anims);
    c->y -= 0x10000;
    c->ay = 0x2800;             /* + 0x54 */
    c->une_child = 1;           /* + 0x1C */
    c->param0 = c->param1 = 0;
    c->hp = c->stats.hp;
    c->parent = e;
    c->spawned_count = 0;
    e->spawned_count++;
}

/* The step-4 split child: param0/param1 cleared, hp from the shared
 * record, step 0xA (lands straight into the walk), gravity
 * + 0x54 = 0x2800, linked to the root, root count incremented. */
static void une_split_child(AosHitReport *report, AosEnemy *e,
                            const AosAnimSet *anims,
                            uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) {
        /* EntityCreateInRange failure: the native decrements the
         * root's count (mirrored literally). */
        AosEnemy *root = une_root(e);
        if (root && root->spawned_count) root->spawned_count--;
        return;
    }
    uint32_t r = random();
    AosEnemy *c = &report->children[report->child_count++];
    une_child_init(c, e, anims);
    c->vx = (int32_t)(r & 0x3FFFF) - 0x20000;
    c->vy = -0x30000 - (int32_t)(random() & 0x1FFFF);
    c->ax = 0;
    c->ay = 0x2800;
    c->state = 0;
    c->step = 0xA;
    c->param0 = c->param1 = 0;
    c->hp = c->stats.hp;
    c->parent = e->parent;
    c->spawned_count = 0;
    AosEnemy *root = une_root(e);
    if (root) root->spawned_count++;
}

/* sub_080747B4/sub_08074838: the debris sprite — same art, anim 4
 * looping, vx (rng & 0x3FFFF) - 0x20000 / vy -0x20000 -
 * (rng & 0x1FFFF), lifetime + 0xD, removed past 0x13, velocity at
 * 7/8 a tick (sub_0806D490/sub_0806D4A8 through the signed division
 * of func_080DA7DC) then accel_move(0x80000). */
static void une_debris(AosHitReport *report, AosEnemy *e,
                       const AosAnimSet *anims,
                       uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    uint32_t r = random();
    AosEnemy *c = &report->children[report->child_count++];
    une_child_init(c, e, anims);
    c->role = AOS_ROLE_UNE_DEBRIS;
    c->state = 0;
    c->step = 0;
    c->vx = (int32_t)(r & 0x3FFFF) - 0x20000;
    c->vy = -0x20000 - (int32_t)(random() & 0x1FFFF);
    c->une_draw = (uint8_t)(e->une_draw - 1);
    play_loop(c, anims, 4, true);
}

/* sub_080743B4, state 0: the entry/aggro probe and the substep
 * machine. Steps 0/10 run the projectile motion until the floor,
 * then hold vx = 0 / vy = 0x10000 / gravity off and roll the split
 * wait; step 1/2 chain the one-shot animations; step 3 counts the
 * cycle timer and the population wait; step 4 births splits and
 * debris; step 5 exits through animation 5. */
static void une_state0(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind, AosHitReport *spawned,
                       uint32_t (*random)(void)) {
    /* Steps 0/10 share the entry block: the projectile motion runs
     * until the floor bit, then vx = 0 / vy = 0x10000 / gravity off,
     * the step-0xA shortcut or the aggro box advances the chain and
     * the split wait is rolled. Steps 1-9 dispatch below. */
    if (e->step == 0 || e->step == 0xA) {
        int flags = 0xF;
        if (e->ay != 0) {
            flags = projectile_collide(e, layer, 0x10000, -0x80000);
            if (flags & 3) e->vx = -e->vx;
            if (flags & 8) e->vy += 0x10000;
        }
        if (!(flags & 4)) return;
        e->vx = 0;
        e->vy = 0x10000;
        e->ay = 0;
        if (e->step == 0xA) {
            e->step = 1;
            play_loop(e, kind->anims, 1, false);
        } else if (player_in(soma, une_px(e) - 0x64, une_py(e) - 0xA0,
                             0xC8, 0x140)) {
            play_loop(e, kind->anims, 1, false);
            e->step++;
        }
        e->une_wait = (int32_t)(random() % 0xC8) + 0x118;
        return;
    }
    switch (e->step) {
    case 1:
        if (e->cycle_done) {
            play_loop(e, kind->anims, 2, false);
            e->step++;
        }
        break;
    case 2:
        if (e->cycle_done) {
            play_loop(e, kind->anims, 3, true);
            e->step++;
            /* + 0x1C: the create child gets the short 0xA fuse. */
            e->timer = e->une_child ? 0xA : (uint8_t)(random() % 0x28 + 0x64);
        }
        break;
    case 3: {
        uint8_t t = e->timer;
        e->timer = (uint8_t)(t - 1);
        if (t == 0) {
            AosEnemy *root = une_root(e);
            if (root && root->spawned_count < root->param0) {
                e->timer = 0;
                e->step++;
            } else {
                e->timer = (uint8_t)(random() % 0x28 + 0x64);
            }
        }
        if (--e->une_wait <= 0) {
            AosEnemy *root = une_root(e);
            if (root) {
                if (root->spawned_count >= root->param0) {
                    e->step = 5;
                    play_loop(e, kind->anims, 5, false);
                } else {
                    e->une_wait = 0x8C;
                }
            }
        }
        break;
    }
    case 4: {
        uint8_t t = e->timer;
        e->timer = (uint8_t)(t + 1);
        if (t > 0x1D) {
            e->timer = (uint8_t)(random() % 0x28 + 0x64);
            e->step = 3;
            break;
        }
        if (e->timer == 0xF) une_split_child(spawned, e, kind->anims, random);
        if ((e->timer & 3) == 0) une_debris(spawned, e, kind->anims, random);
        break;
    }
    case 5:
        if (e->cycle_done) {
            e->state = 2;
            e->timer = 1;
        }
        break;
    default:
        break;
    }
}

/* sub_080748C0, state 1: a one-shot return to animation 0. */
static void une_state1(AosEnemy *e, const AosEnemyKind *kind) {
    if (e->step == 0) {
        play_loop(e, kind->anims, 0, false);
        e->step = 1;
    }
}

/* State 2: the blink death on the shared 0x08118CE0 table
 * (kind->blink), + 0xD counting down from 0x28, removed at zero
 * and the root's count decremented once. */
static void une_state2(AosEnemy *e, const AosEnemyKind *kind) {
    e->hidden = e->timer < 40 && (kind->blink[e->timer] & 1);
    uint8_t t = e->timer;
    e->timer = (uint8_t)(t - 1);
    if (t <= 1) {
        e->removed = true;
        AosEnemy *root = e->parent;
        if (root && root->id == AOS_ENEMY_UNE && root->spawned_count)
            root->spawned_count--;
    }
}

AosHitReport aos_enemy_une_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def,
                                  int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (e->role == AOS_ROLE_UNE_DEBRIS) {
        /* sub_08074838: friction, accel_move(0x80000), animation
         * step, removed past + 0xD = 0x13. */
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = (int32_t)(((int64_t)e->vy * 7) / 8);
        accel_move(e, 0x80000);
        step_anim(e, kind->anims);
        e->timer++;
        if (e->timer > 0x13) e->removed = true;
        return report;
    }
    int sx = une_px(e) - cam_x, sy = une_py(e) - cam_y;
    if (e->state != 3 && e->state != 2) {
        /* sub_08068AD4(-0x10, 0, 0x110, 0xB0) + sub_08021924: the
         * activity box marks the enemy targetable. */
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (room_outside(e, layer, 0, 0)) {
            e->state = 2;
            e->timer = 1;
        }
    }
    /* sub_0806AD24: alive and flashing — collide only and return,
     * skipping the state machine, the animation step and the tail
     * collision (the dormant + 0x38 paths are approximated by the
     * shared hit_flash, documented). */
    if (e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 2)
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
        return report;
    }
    switch (e->state) {
    case 0:
        une_state0(e, soma, layer, kind, &spawned, random);
        break;
    case 1:
        une_state1(e, kind);
        break;
    case 2:
        une_state2(e, kind);
        break;
    case 3:
        /* The deferred create child: the native spawns it inside
         * EnemyUneCreate; our create has no report, so the first
         * state-3 update emits it (documented). */
        if (!e->boss_parts_spawned) {
            e->boss_parts_spawned = true;
            une_create_child(&spawned, e, kind->anims, random);
        }
        if (e->spawned_count == 0) e->removed = true;
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
        (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon,
                                   weapon_frames, soma_atk, soma_def);
        aos_enemy_copy_collision_result(&report, &hit);
    }
    for (unsigned i = 0; i < spawned.child_count &&
         report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}
