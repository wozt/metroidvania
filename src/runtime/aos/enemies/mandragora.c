/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Mandragora (enemy 0x4F) and its two linked
 * body parts plus its death-seed volley.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API.
 *
 * Native layout (cvaos code_080C0A1C): the head entity (EnemyMandragoraCreate
 * 0x080CEF34, update 0x080CF99C) spawns a body child (sub_080CEFE8, update
 * sub_080CF0E4) which spawns a tail child (sub_080CF2B8, update
 * sub_080CF3BC); all three share tiles 0x081F429C, palette 0x0820C004 and
 * frames 0x08250E94 and split the animation ids: the head uses 0-2, the
 * body 3, the tail 4-5. The native + 0x11/+ 0x12 links are entity slot
 * indices; here `parent` links upward and each child registers itself into
 * the owner's mg_body/mg_tail pointers. The head's death state 0xF0 calls
 * sub_080CF804 for five seeds (EntityCreateInRange 0x49..0xCF, update
 * sub_080CF910) whose six-byte records live at 0x08528CC4; the seeds are
 * visual only (no contact/hurt callbacks natively).
 *
 * The native + 0x20 sensor byte is shared by the three entities: the body
 * and tail OR family flags into the head's byte and the head's own
 * collision pass rewrites it every active frame. mg_sensors reproduces
 * that: it is rewritten after the head's collide (0x18 when the attack
 * connected) and frozen while the head is in states 3/0xF0, which never
 * reach the collision pass. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define player_in_centred aos_enemy_player_in_centred
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

/* The five seed records of 0x08528CC4 (6 bytes each): x/y pixel offsets,
 * sprite frame, x speed << 12, negated y speed << 12, lifetime. All x/y
 * offsets are zero — the seeds leave from the head's position. */
static const int8_t mg_seed_table[5][6] = {
    {0, 0, 0x1F, 0x10, 0x20, 0x28},
    {0, 0, 0x20, 0x0A, 0x18, 0x1E},
    {0, 0, 0x21, 0x08, 0x10, 0x1E},
    {0, 0, 0x22, 0x04, 0x18, 0x30},
    {0, 0, 0x23, 0x00, 0x14, 0x30},
};

/* sub_080CFA68: mark the tail, the body and the head + 0x59 |= 8 through
 * the + 0x12 chain; here each entity's `removed` flag is that mark. */
static void mg_mark_removed(AosEnemy *e) {
    AosEnemy *body = e->mg_body;
    AosEnemy *tail = body ? body->mg_tail : NULL;
    if (tail) {
        tail->mg_flags |= 8;
        tail->removed = true;
    }
    if (body) {
        body->mg_flags |= 8;
        body->removed = true;
    }
    e->mg_flags |= 8;
    e->removed = true;
}

/* sub_080CF804: the death volley. Each record spawns one seed entity at
 * the head's position (the x offset mirrors with + 0x58 bit 0x40), bumps
 * the head's + 0x13 live-seed count, inherits + 0x5C/+ 0x2C/+ 0x58 and
 * draws through the generic callback. */
static void mg_seed_volley(AosEnemy *e, AosHitReport *report) {
    for (int i = 0; i < 5 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        const int8_t *r = mg_seed_table[i];
        int32_t bx = r[0], vx = (int32_t)r[3] << 12;
        if (e->mirrored) {
            bx = -bx;
            vx = -vx;
        }
        AosEnemy s = {0};
        s.id = e->id;
        s.role = AOS_ROLE_MANDRAGORA_SEED;
        s.x = e->x + (bx << 16);
        s.y = e->y + ((int32_t)r[1] << 16);
        s.vx = vx;
        s.vy = -((int32_t)r[4] << 12);
        s.timer = (uint8_t)r[5];
        s.static_frame = r[2];
        s.mirrored = e->mirrored;
        s.stats = e->stats;
        s.hp = 1;
        s.mg_flags = 2;
        s.combat.type = AOS_TYPE_ENEMY;
        /* No contact or hurt callbacks natively: the seeds only drift. */
        s.combat.attack_off = s.combat.hurt_off = true;
        report->children[report->child_count++] = s;
        e->mg_seeds++;
    }
}

/* Append a spawned-parts report into the collision report without
 * dropping either side's children or effects. */
static void mg_merge_spawned(AosHitReport *report, const AosHitReport *spawned) {
    for (int i = 0; i < spawned->child_count &&
                    report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report->children[report->child_count++] = spawned->children[i];
    for (int i = 0; i < spawned->effect_count &&
                    report->effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report->effects[report->effect_count++] = spawned->effects[i];
}

/* sub_080CF748: head state 0xF0. Step 0 hides the head, folds the + 0x2C
 * palette group into + 0x5A bits 3-6 (+ 0x2C is not modelled, so the
 * field keeps its low bits), fires the seed volley once, plays song
 * 0x6B unless the body's + 0x20 bit 1 is still set, and enters step 1.
 * Step 1 deletes the whole chain through sub_080CFA68 once the seeds
 * are gone and either the body bit is clear or the body hides without
 * the tail's + 0x10 mark. */
static void mg_death(AosEnemy *e, AosHitReport *report) {
    AosEnemy *body = e->mg_body;
    if (e->step == 0) {
        e->hidden = true;
        e->mg_draw = (uint8_t)(e->mg_draw & 0x87);
        mg_seed_volley(e, report);
        e->step = 1;
        if (!(e->mg_sensors & 2)) aos_audio_play_song(0x6B);
        return;
    }
    if (e->step != 1) return;
    if (e->mg_sensors & 2) {
        if (body && body->hidden && !(e->mg_sensors & 0x10) &&
            e->mg_seeds == 0)
            mg_mark_removed(e);
    } else if (e->mg_seeds == 0) {
        mg_mark_removed(e);
    }
}

/* sub_080CFA18: head state 3, the inert sequence under gEwramData + 0x8E
 * bit 0x40 — animation 0 looping, the step latch, then only the
 * animation advance (no collision). */
static void mg_inert(AosEnemy *e, const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->anim.id != 0) play_loop(e, kind->anims, 0, true);
        e->step = 1;
    }
    step_anim(e, kind->anims);
}

/* sub_080CF5C4: head state 1. Step 0 restarts animation 0 looping;
 * step 1 arms the sub_0806E29C(e, 0x100, 0x80, 0) proximity test — Soma
 * inside the 256 x 128 box centred on the entity; step 2 enters the
 * attack state 2. */
static void mg_watch(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        if (e->anim.id != 0) play_loop(e, kind->anims, 0, true);
        e->step = 1;
        e->substep = 0;
        break;
    case 1:
        if (player_in_centred(e, soma, 0x100, 0x80)) {
            e->step = 2;
            e->substep = 0;
        }
        break;
    case 2:
        e->state = 2;
        e->substep = 0;
        e->step = 0;
        break;
    default:
        break;
    }
}

/* sub_080CF624: head state 2, the attack. Step 0/substep 0 plays
 * animation 1 once with the 0x50 timer and song 0x119 at timer 1;
 * substep 1 advances on the animation's end. Step 1/substep 0 plays
 * animation 2 once and bumps the body's state (its + 0xA++, steps
 * cleared); substep 1 waits the animation out, sets the body's anim
 * flag bit 1 (+ 0x6C) and the direct frame + 0x65 = 0xA; substep 2
 * keeps the direct frame while the body is in state 2 with timer <= 7
 * and enters the death burst 0xF0 when the + 0x20 sensor bits 0x18 are
 * set (the attack connected, or the tail's state-3 mark). */
static void mg_attack(AosEnemy *e, const AosEnemyKind *kind) {
    AosEnemy *body = e->mg_body;
    switch (e->step) {
    case 0:
        if (e->substep == 0) {
            e->timer = 0x50;
            if (e->anim.id != 1) play_loop(e, kind->anims, 1, false);
            e->substep = 1;
        } else {
            if (e->timer == 1) aos_audio_play_song(0x119);
            if (e->timer) e->timer--;
            if (e->cycle_done) {
                e->timer = 0;
                e->step = 1;
                e->substep = 0;
            }
        }
        break;
    case 1:
        if (e->substep == 0) {
            if (e->anim.id != 2) play_loop(e, kind->anims, 2, false);
            if (body) {
                body->state++;
                body->substep = 0;
                body->step = 0;
            }
            e->substep = 1;
        } else if (e->substep == 1) {
            if (e->cycle_done) {
                if (body) body->anim.flags |= 2;
                e->static_frame = 0xA;
                e->substep = 2;
            }
        } else {
            if (body && body->state == 2 && body->timer <= 7)
                e->static_frame = 0xA;
            if (e->mg_sensors & 0x18) {
                e->state = 0xF0;
                e->substep = 0;
                e->step = 0;
            }
        }
        break;
    default:
        break;
    }
}

/* EnemyMandragoraUpdate (0x080CF99C): states 3 and 0xF0 run their own
 * sequences and return early; the other states skip everything while
 * sub_0806CC20 reports the entity outside the (-16, 0, 0x110, 0xB0)
 * activity box (animation id 2 always runs), dispatch state 1 to
 * sub_080CF5C4 and state 2 to sub_080CF624, then step the animation and
 * run the shared collision pass. The + 0x20 rewrite after the collision
 * approximates sub_0806E314's sensor byte: 0x18 while the attack box
 * touched Soma. */
AosHitReport aos_enemy_mandragora_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    (void)layer;
    (void)random;
    AosHitReport report = {0}, spawned = {0};
    if (!e->boss_parts_spawned) {
        /* sub_080CEFE8 emits the body on the first update (the native
         * create runs it inline; reports can only spawn on update). */
        if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy b = {0};
            b.id = e->id;
            b.role = AOS_ROLE_MANDRAGORA_BODY;
            b.x = e->x;
            b.y = e->y;
            b.stats = e->stats;
            b.hp = e->hp;
            b.mirrored = e->mirrored;
            b.mg_flags = 2;
            b.static_frame = -1;
            b.combat.type = AOS_TYPE_ENEMY;
            play_loop(&b, kind->anims, 3, true);
            step_anim(&b, kind->anims);
            spawned.children[spawned.child_count++] = b;
        }
        e->boss_parts_spawned = true;
    }
    if (e->state == 3) {
        mg_inert(e, kind);
        mg_merge_spawned(&report, &spawned);
        return report;
    }
    if (e->state == 0xF0) {
        mg_death(e, &report);
        mg_merge_spawned(&report, &spawned);
        return report;
    }
    {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        bool active = sx >= -16 && sx <= 0x110 && sy >= 0 && sy <= 0xB0;
        if (e->anim.id != 2 && !active) {
            mg_merge_spawned(&report, &spawned);
            return report;
        }
    }
    if (e->state == 1)
        mg_watch(e, soma, kind);
    else if (e->state == 2)
        mg_attack(e, kind);
    step_anim(e, kind->anims);
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    /* The + 0x20 byte mixes the collision's attack-landed bit 3 with
     * the family marks the body and tail OR in (bits 1, 4, 5); only
     * bit 3 is refreshed here. */
    e->mg_sensors = (uint8_t)((e->mg_sensors & (uint8_t)~0x08) |
                              (hit.soma_hit ? 0x08 : 0));
    report = hit;
    mg_merge_spawned(&report, &spawned);
    return report;
}

/* sub_080CF0E4: the body update. r5 is the head (+ 0x11), r6 the tail
 * (+ 0x12). State 0 is the dormant copy of the head's position, hidden;
 * state 1 syncs hit points, sets the head's + 0x20 bit 1, unhides and
 * plays song 0x83 before falling into state 2; state 2 reveals the tail
 * once and counts the 0x20 timer into 0xF1; state 0xF0 is the one-tick
 * death (song 0x70, sub_080683BC on the head — the runtime's reward pass
 * only pays role-ENEMY kills, so a body-only kill grants nothing here),
 * then 0xF1 emits a flame every seventh tick while the head's + 0x20
 * bit 5 holds and returns dormant when the animation loops. States <=
 * 0xEF step the animation and collide; a lethal hit enters 0xF0 through
 * the sub_080CF9FC hurt callback. */
AosHitReport aos_enemy_mandragora_body_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def,
                                           int cam_x, int cam_y,
                                           uint32_t (*random)(void)) {
    (void)layer;
    (void)cam_x;
    (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    AosEnemy *head = e->parent;
    AosEnemy *tail = e->mg_tail;
    if (head && head->mg_body != e) head->mg_body = e;
    if (!e->boss_parts_spawned) {
        /* sub_080CF2B8 emits the tail on the body's first update. */
        if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy t = {0};
            t.id = e->id;
            t.role = AOS_ROLE_MANDRAGORA_TAIL;
            t.x = e->x;
            t.y = e->y;
            t.stats = e->stats;
            t.hp = e->hp;
            t.mirrored = e->mirrored;
            t.mg_flags = 6;
            t.static_frame = -1;
            t.combat.type = AOS_TYPE_ENEMY;
            /* sub_0804277C(..., 0): a contact callback but no hurt one. */
            t.combat.hurt_off = true;
            play_loop(&t, kind->anims, 4, false);
            step_anim(&t, kind->anims);
            spawned.children[spawned.child_count++] = t;
        }
        e->boss_parts_spawned = true;
    }
    switch (e->state) {
    case 0:
        if (e->step == 0) {
            if (head) {
                e->x = head->x;
                e->y = head->y;
            }
            e->step = 1;
            e->substep = 0;
            e->hidden = true;
        }
        if (tail) tail->mg_sensors &= (uint8_t)~0x10;
        break;
    case 1:
        if (e->step == 0) {
            if (head) {
                head->mg_sensors |= 2;
                e->hp = head->hp;
            }
            if (e->anim.id != 3) play_loop(e, kind->anims, 3, true);
            e->state = 2;
            e->substep = 0;
            e->step = 0;
            e->hidden = false;
            aos_audio_play_song(0x83);
        }
        /* fallthrough */
    case 2:
        if (e->step == 0) {
            if (tail) {
                tail->state = 1;
                tail->substep = 0;
                tail->step = 0;
            }
            e->step = 1;
            e->substep = 0;
            e->timer = 0x20;
        }
        e->timer = (uint8_t)(e->timer - 1);
        if (e->timer == 0xFF) {
            e->state = 0xF1;
            e->substep = 0;
            e->step = 0;
        }
        break;
    case 0xF0:
        aos_audio_play_song(0x70);
        /* sub_080683BC(head): the head's defeat reward — not portable
         * from a child update; documented divergence. */
        e->state = 0xF1;
        e->substep = 0;
        e->step = 0;
        if (head) head->mg_sensors |= 0x20;
        /* fallthrough */
    case 0xF1:
        if (e->step == 0) {
            if (head) {
                if ((head->mg_sensors & 0x20) && tail) {
                    tail->state = 0;
                    tail->substep = 0;
                    tail->step = 0;
                }
                head->mg_sensors |= 8;
            }
            e->mg_sub = 1;
            e->step = 1;
            e->substep = 0;
        }
        {
            uint8_t t = e->timer;
            e->timer = (uint8_t)(t + 1);
            if (t % 7 == 0 && head && (head->mg_sensors & 0x20)) {
                /* sub_0806D5C0 at (-8 or +8 px by facing, -20 px), a
                 * 16 x 16 px scatter — the shared flame spawn. */
                int32_t ox = e->mirrored ? 0x80000 : (int32_t)0xFFF80000;
                spawn_flame_at(e->x + ox, e->y - 0x140000, 0x100000,
                               0x100000, random, &report);
            }
        }
        if (e->cycle_done) {
            e->state = 0;
            e->substep = 0;
            e->step = 0;
        }
        break;
    default:
        break;
    }
    if (e->state != 0) step_anim(e, kind->anims);
    if (e->state != 0 && e->state <= 0xEF) {
        AosHitReport hit = collide(e, soma, kind, weapon,
                                   weapon_frames, soma_atk, soma_def);
        /* sub_080CF9FC: a lethal blow enters state 0xF0. */
        if (hit.killed) {
            e->state = 0xF0;
            e->substep = 0;
            e->step = 0;
        }
        report = hit;
    }
    mg_merge_spawned(&report, &spawned);
    return report;
}

/* sub_080CF3BC: the tail update. It early-outs while its + 0x59 bit 2
 * (the palette group armed at create) is clear and while the body or
 * head entity records are gone. State 0 is dormant and hidden; state 1
 * shows animation 4 once and arms the palette fades into state 2's
 * 4-tick timer; state 3 loops animation 5 and marks the head's + 0x20
 * bit 4, watching the body for state > 0xEF to enter 0xF0; 0xF0 fades
 * back to dormant once the body resets. States 1-3 and 0xF0 step the
 * animation; every state but 0 and 0xF0 collides. */
AosHitReport aos_enemy_mandragora_tail_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def,
                                           int cam_x, int cam_y,
                                           uint32_t (*random)(void)) {
    (void)layer;
    (void)cam_x;
    (void)cam_y;
    (void)random;
    AosHitReport report = {0};
    AosEnemy *body = e->parent;
    AosEnemy *head = body ? body->parent : NULL;
    if (body && body->mg_tail != e) body->mg_tail = e;
    if (!(e->mg_flags & 4)) return report;
    if (!body || !head) return report;
    switch (e->state) {
    case 0:
        if (e->step == 0) {
            e->x = body->x;
            e->y = body->y;
            e->step = 1;
            e->substep = 0;
            e->hidden = true;
            head->mg_sensors &= (uint8_t)~0x10;
        }
        break;
    case 1:
        if (e->step == 0) {
            e->hidden = false;
            if (e->anim.id != 4) play_loop(e, kind->anims, 4, false);
            e->step = 1;
            e->substep = 0;
        }
        if (e->cycle_done) {
            e->timer = 4;
            e->state = 2;
            e->substep = 0;
            e->step = 0;
        }
        break;
    case 2:
        e->timer = (uint8_t)(e->timer - 1);
        if (e->timer == 0xFF) {
            e->state = 3;
            e->substep = 0;
            e->step = 0;
        }
        break;
    case 3:
        if (e->step == 0) {
            if (e->anim.id != 5) play_loop(e, kind->anims, 5, true);
            e->step = 1;
            e->substep = 0;
            head->mg_sensors |= 0x10;
        }
        if (body->state > 0xEF) e->state = 0xF0;
        break;
    case 0xF0:
        if (e->step == 0) {
            if (body->state == 0) {
                e->mg_sub = 2;
                e->timer = 0x10;
                e->step = 1;
                e->substep = 0;
            } else {
                e->mg_sub = 1;
            }
        } else {
            e->timer = (uint8_t)(e->timer - 1);
            if (e->timer == 0xFF) {
                e->state = 0;
                e->substep = 0;
                e->step = 0;
            }
        }
        break;
    default:
        break;
    }
    if (e->state != 0) {
        step_anim(e, kind->anims);
        if (e->state != 0xF0)
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
    }
    return report;
}

/* sub_080CF910: the seed update. The sub_08068AD4 probe (approximated
 * by the kind's screen margins) deletes it off the activity area; the
 * lifetime byte counts down and the + 0x46 y word over 0xAA deletes it
 * too. A live seed integrates its velocity and gains 0x3000 of gravity;
 * its death decrements the head's + 0x13 live-seed count. */
void aos_enemy_mandragora_seed_update(AosEnemy *e, const AosEnemyKind *kind,
                                      int cam_x, int cam_y) {
    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    bool dead = outside_margins(kind, 4, sx, sy);
    if (!dead) {
        e->timer = (uint8_t)(e->timer - 1);
        if (e->timer == 0xFF || (e->y >> 16) > 0xAA)
            dead = true;
        else {
            e->x += e->vx;
            e->y += e->vy;
            e->vy += 0x3000;
        }
    }
    if (dead) {
        if (e->parent) e->parent->mg_seeds--;
        e->removed = true;
    }
}
