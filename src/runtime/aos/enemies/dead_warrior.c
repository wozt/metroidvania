/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Dead Warrior (enemy 0x2F).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

/* sub_080B72F8: every frame the probe walker sub_0806CAF8 at + 0x18
 * (table 0x08528830, mode 4); step 1 waits for the player in the centred
 * 100 x 60 box at walk frame 1, tick 1, then charges (animation 1, step
 * 2); steps 1 and 2 raise + 0x18 by 0x4000 up to 2.0 (sound 0x7D every 25
 * frames on screen) and count + 0x1C: past 100 (sound 0x127) it brakes
 * (animation 2, step 3), losing 0x1000 per frame, raising dust every 4
 * frames (sub_08045CEC animation 4 or 5, row 6, vx 0.5 backward), then
 * turns around and walks again when the animation ends. */
static void warrior_walk(AosEnemy *w, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, int cam_x, int cam_y,
                         uint32_t (*random)(void), AosHitReport *report) {
    aos_enemy_ground_walk(w, soma, layer, &kind->probes, w->walk_timer, 4);
    switch (w->step) {
    case 0:
        w->walk_timer = 0;
        w->substep = 0;
        w->step = 1;
        /* fall through */
    case 1:
        if (aos_enemy_player_in_centred(w, soma, 0x64, 0x3C) &&
            w->anim.frame == 0 && w->anim.tick == 1) {
            aos_enemy_play(w, kind->anims, 1);
            w->step = 2;
        }
        /* fall through */
    case 2: {
        /* Sound 0x7D every 25 frames within sub_08068AD4(e, -64, -64,
         * 0x170, 0x120). */
        int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
        if (w->timer % 25 == 0 && (uint16_t)(sx + 0x40) <= 0x170 &&
            (uint16_t)(sy + 0x40) <= 0x120)
            aos_audio_play_song(0x7D);
        w->timer++;
        if (w->walk_timer <= 0x1FFFF) w->walk_timer += 0x4000;
        w->shot++;
        if (w->anim.frame == 1 && w->anim.tick == 1) w->substep++;
        if (w->shot > 0x64) {
            aos_audio_play_song(0x127);
            aos_enemy_play_loop(w, kind->anims, 2, false);
            w->step = 3;
            w->timer = 0;
        }
        return;
    }
    case 3:
        if (w->walk_timer > 0) w->walk_timer -= 0x1000;
        if ((++w->timer & 3) == 0) {
            uint8_t art = (random() & 1) ? AOS_EFFECT_SMOKE : AOS_EFFECT_DUST;
            AosEffect *dust = aos_enemy_spawn_drift(art, w->x, w->y, report);
            if (dust) dust->vx = w->mirrored ? -0x8000 : 0x8000;
        }
        if (w->anim.flags & AOS_ANIM_ENDED) {
            w->shot = 0;
            aos_enemy_play(w, kind->anims, 0);
            w->step = 0;
            w->mirrored = !w->mirrored;
        }
        return;
    default:
        return;
    }
}

/* sub_080B747C: animation 3, thrown back at 2.5 and up at 2.5 with gravity
 * 0x2000 (sub_0806D3D8(e, 8.0)), flashing for 40 frames (the palette bits
 * are not ported), sound 0x6E every 8 frames and a fire of sub_08045CEC
 * (animation 1, row 7) every 4 frames within 8 pixels of it, 32 higher;
 * deleted beyond sub_08068AD4(e, -128, -64, 0x1F0, 0x120) or when the
 * animation ended within 128 frames. Returns true when deleted. */
static bool warrior_die(AosEnemy *w, const AosEnemyKind *kind, int cam_x, int cam_y,
                        uint32_t (*random)(void), AosHitReport *report) {
    if (w->step == 0) {
        aos_enemy_play_loop(w, kind->anims, 3, false);
        w->step = 1;
        w->vx = w->mirrored ? 0x28000 : -0x28000;
        w->vy = -0x28000;
        w->ax = 0;
        w->ay = 0x2000;
        w->timer = 0;
    }
    w->timer++;
    if ((w->timer & 7) == 0) aos_audio_play_song(0x6E);
    if ((w->timer & 3) == 0) {
        int32_t x = w->x +
                    (int32_t)((uint32_t)aos_enemy_random_centred(random, 16) << 16);
        int32_t y = w->y + (int32_t)((uint32_t)(
            aos_enemy_random_centred(random, 16) - 0x20) << 16);
        aos_enemy_spawn_drift(AOS_EFFECT_EMBER, x, y, report);
    }
    aos_enemy_accel_move(w, 0x80000);
    int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
    bool on_screen = (uint16_t)(sx + 0x80) <= 0x1F0 && (uint16_t)(sy + 0x40) <= 0x120;
    return !on_screen || ((w->anim.flags & AOS_ANIM_ENDED) && w->timer <= 0x80);
}

/* EnemyDeadWarriorUpdate: inside the activity window, hidden above screen
 * Y -16 (sub_0806E4F8); state 0 walks, 1 dies, 2 only steps its
 * animation; then the collision pass before the animation step. */
AosHitReport aos_enemy_dead_warrior_update(
    AosEnemy *w, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    w->hidden = sy < -16;
    switch (w->state) {
    case 0: warrior_walk(w, soma, layer, kind, cam_x, cam_y, random, &effects); break;
    case 1:
        if (warrior_die(w, kind, cam_x, cam_y, random, &effects)) {
            w->removed = true;
            return effects;
        }
        break;
    case 2:
        aos_enemy_step_anim(w, kind->anims);
        return report;
    default:
        break;
    }
    sx = (w->x >> 16) - cam_x;
    sy = (w->y >> 16) - cam_y;
    if (!w->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = aos_enemy_collide(w, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_enemy_step_anim(w, kind->anims);
    aos_combat_tick(&w->combat);
    aos_enemy_take_effects(&report, &effects);
    return report;
}
