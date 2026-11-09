/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Sky Fish (enemy 0x51).
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Sky Fish state 0 (sub_080A7D58): the entry waits out + 0x1A while hidden,
 * then crosses the top of the screen. Step 0 plants it on the entry side
 * (+ 0x19: -16 or 256 px), a random height in [32, 64), faces the player
 * (sub_0806CF2C) and counts + 0x1A down; at 1 it shows itself and dashes
 * at 26 px/frame toward its facing. Step 1 integrates, spawns the splash
 * entity sub_080A8040 at a random offset every frame and despawns beyond
 * the margin of entry 6 (sub_0806D128). The native step-2 transition sits
 * behind the pause flag 0x4BE (always clear during updates), so the fish
 * keeps step 1; its + 0x18 gate stays 1 and it never collides. */
static void sky_fish_dash(AosEnemy *a, const AosSoma *soma, const AosEnemyKind *kind,
                          int cam_x, int cam_y, uint32_t (*random)(void),
                          AosHitReport *report) {
    if (a->step == 0) {
        a->x = a->sf_variant ? 0x1000000 : (int32_t)0xFFF00000;
        a->y = 0x200000 + (int32_t)(random() & 0x1FFFFF);
        soldier_face(a, soma);
        if (a->sf_timer-- > 1) return;
        a->step = 1;
        a->hidden = false;
        a->vx = a->mirrored ? 0x1A0000 : (int32_t)0xFFE60000;
        return;
    }
    int32_t tx = a->x + (int32_t)(random() & 0xFFFFF) - 0x80000;
    int32_t ty = a->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
    a->x += a->vx;
    a->y += a->vy;
    /* sub_080A8040's splash entity on the shared tiles is approximated by
     * a dust drift at the native random offset. */
    spawn_drift(AOS_EFFECT_DUST, tx, ty, report);
    if (outside_margins(kind, 6, (a->x >> 16) - cam_x, (a->y >> 16) - cam_y))
        a->removed = true;
}

/* Sky Fish state 1 (sub_080A7F08, reached only by the lethal hurt
 * callback): it hangs with ay 0x4000 under the sub_0806D3D8 limit 4.0,
 * blinks on sub_0806D518's timer pattern (+ 0x58 bit 5) and bursts a bubble
 * entity sub_080A80F8 every eighth timer tick at a small random offset;
 * the timer reaching 0 sets + 0x59 bit 3 and it is deleted. */
static void sky_fish_hover(AosEnemy *a, const AosEnemyKind *kind,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (a->step == 0) {
        a->step = 1;
        a->vx = a->vy = a->ax = 0;
        a->ay = 0x4000;
    }
    accel_move(a, 0x40000);
    a->hidden = a->timer <= 0x28 && (kind->blink[a->timer] & 1);
    if ((a->timer & 7) == 0) {
        int32_t bx = a->x + (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t by = a->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
        spawn_drift(AOS_EFFECT_DUST, bx, by, report);
    }
    if (--a->timer == 0) a->removed = true;
}

/* EnemySkyFishUpdate (0x080A7BD0): the sub_08068AD4 proximity check only
 * feeds the activity list of sub_08021924 (not ported); sub_0806AD24
 * stuns it while flashing. States 0/1 then step the animation in the
 * common tail, refresh the boxes on a frame change (automatic here) and
 * collide only outside state 1 with the + 0x18 gate clear — which the
 * create never clears, so a live Sky Fish is invulnerable decoration.
 * State 2 (the 0x8E & 0x40 flag path) only steps its animation. */
static AosHitReport sky_fish_update(AosEnemy *enemy, AosSoma *soma,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (enemy->hp > 0 && enemy->hit_flash) {
        enemy->hit_flash--;
    } else if (enemy->state == 2) {
        step_anim(enemy, kind->anims);
        aos_combat_tick(&enemy->combat);
        return report;
    } else {
        if (enemy->state == 0)
            sky_fish_dash(enemy, soma, kind, cam_x, cam_y, random, &spawned);
        else if (enemy->state == 1)
            sky_fish_hover(enemy, kind, random, &spawned);
        step_anim(enemy, kind->anims);
    }
    if (enemy->state != 1 && enemy->sf_gate == 0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&enemy->combat);
    take_effects(&report, &spawned);
    return report;
}
