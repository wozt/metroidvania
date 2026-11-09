/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Ghost Dancer (enemy 0x1D).
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* sub_080C3190: step 0 plays animation 0, stops and faces the player;
 * step 1 dances a figure of eight: vx = 2 sin(+ 0x14 + 0x4000), x += vx,
 * y += sin(4 * + 0x14) + vy, + 0x14 += 0x80; when vx is 0 the animation
 * switches between 0 and 1; every half turn (+ 0x14 % 0x8000 == 0) with
 * + 0x18 even, + 0x18 advances (the fade starts). */
static void ghost_dance(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims) {
    if (g->step == 0) {
        play(g, anims, 0);
        g->vx = g->vy = 0;
        g->mirrored = (g->x >> 16) < (target_x(soma) >> 16);
        g->phase = 0;
        g->timer = 0;
        g->step = 1;
        return;
    }
    g->vx = aos_sine((uint32_t)g->phase + 0x4000) * 2;
    g->x += g->vx;
    g->y += aos_sine((uint32_t)g->phase * 4) + g->vy;
    g->phase = (uint16_t)(g->phase + 0x80);
    if (g->vx == 0) play(g, anims, (unsigned)(1 - g->anim.id));
    if (g->phase % 0x8000 == 0 && !(g->fade & 1)) {
        g->fade++;
        g->fade_timer = 0;
    }
}

/* EnemyGhostDancerUpdate's fade (+ 0x18): 0 opaque, 1 alternating
 * semi-transparent / opaque frames for 33 frames, 2 semi-transparent, 3
 * alternating back, then 0. */
static void ghost_fade(AosEnemy *g) {
    switch (g->fade) {
    case 0: g->translucent = false; break;
    case 1:
    case 3:
        g->translucent = (g->fade == 1) == !(g->fade_timer & 1);
        if ((int8_t)g->fade_timer++ > 0x20) {
            g->fade = g->fade == 1 ? 2 : 0;
            g->fade_timer = 0;
        }
        break;
    case 2: g->translucent = true; break;
    default: break;
    }
}

/* EnemyGhostDancerUpdate: state 5 steps its animation; otherwise it is
 * hidden beyond the margins of sub_0806D128(e, 4), fades (not while dying),
 * is stunned by sub_0806AD24, dances (states 1 and 2) or dies (state 3:
 * deleted when its animation ends), then the animation step and, except
 * while dying, the collision pass (sub_080421AC: no on-screen test).
 * Contact sub_080C339C: type 0, element 0. */
static AosHitReport ghost_dancer_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (g->state == 5) {
        step_anim(g, kind->anims);
        return report;
    }
    g->hidden = outside_margins(kind, 4, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y);
    if (g->state != 3) ghost_fade(g);
    if (g->hp > 0 && g->hit_flash) {
        g->hit_flash--;
        if (g->state != 3) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&g->combat);
        return report;
    }
    if (g->state == 1 || g->state == 2) {
        ghost_dance(g, soma, kind->anims);
    } else if (g->state == 3 && g->cycle_done) {
        g->removed = true;
        return report;
    }
    step_anim(g, kind->anims);
    if (g->state != 3) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&g->combat);
    return report;
}
