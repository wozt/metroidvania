/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Curly (enemy 0x2B) and Durga (0x11, the same code).
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Curly's walk speed (0.5) toward its facing, slowed uphill. */
static void curly_pace(AosEnemy *c, const AosCollision *layer) {
    c->vx = slope_speed(c, layer, c->mirrored ? 0x8000 : -0x8000);
}

/* sub_08093218: step 0 (unhurt) paces, turning at walls, drops and the
 * room's edges; step 1 (hurt, sub_0806D54C) faces the player at every
 * frame start. Outside walk frames 4 to 7 each frame start sets the pace
 * and on frame 1, with the player in (x - 50, y - 64, 100, 70), three
 * chances in four (r & 0x7F <= 0x5F) start the attack (animation 1); frames
 * 4 to 7 stand still. It moves with sub_08069A00(e, 1.0, -32, 8.0, 8). */
static void curly_walk(AosEnemy *c, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (c->step == 0 && c->hp < c->stats.hp) {
        c->step = 1;
        return;
    }
    uint8_t frame = c->anim.frame;
    bool start = c->anim.tick == 0;
    if ((uint8_t)(frame - 4) > 3) {
        if (start) {
            if (c->step == 1) soldier_face(c, soma);
            curly_pace(c, layer);
            int32_t x = c->x >> 16, y = c->y >> 16;
            if (frame == 1 &&
                player_near(soma, (int16_t)(x - 0x32), (int16_t)(y - 0x40), 0x64, 0x46) &&
                (random() & 0x7F) <= 0x5F) {
                c->state = 1;
                c->step = 0;
                c->timer = 0;
                play_loop(c, anims, 1, false);
                return;
            }
        }
    } else if (start) {
        c->vx = 0;
    }
    /* A step: sound 0x85 at the start of walk frame 4. */
    if (frame == 4 && start) aos_audio_play_song(0x85);
    int result = walk_collide(c, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    if (c->step != 0 || !(result & 4)) return;
    int32_t ahead = (c->x >> 16) + (c->mirrored ? 1 : -1);
    if ((result & 3) || blocked_ahead(layer, ahead, c->y >> 16) || at_room_edge(c, layer)) {
        c->mirrored = !c->mirrored;
        curly_pace(c, layer);
    }
}

/* sub_08093460: a lunge at 1.0 for 8 frames, then still; back to the walk
 * when animation 1 ends (sound 0x85 at frames 4, 7, 10 and 13). */
static void curly_attack(AosEnemy *c, const AosCollision *layer, const AosAnimSet *anims) {
    if (c->step != 0) return;
    if (c->timer <= 7) {
        c->timer++;
        c->vx = slope_speed(c, layer, c->mirrored ? 0x10000 : -0x10000);
        c->vy = 0x10000;
    } else {
        c->vx = 0;
    }
    c->ay = 0x2800;
    walk_collide(c, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    if (c->cycle_done) {
        play(c, anims, 0);
        c->state = c->step = 0;
        curly_pace(c, layer);
        c->vy = 0x10000;
        c->ay = 0x2800;
    }
    /* Sound 0x85 at the start of attack frames 4, 7, 10 and 13. */
    if (c->state == 1 && c->anim.tick == 0 &&
        (c->anim.frame == 4 || c->anim.frame == 7 || c->anim.frame == 10 || c->anim.frame == 13))
        aos_audio_play_song(0x85);
}

/* EnemyCurlyUpdate: inside the activity window, state 0 walks, 1 attacks,
 * 2 runs the generic death, 3 only steps its animation; then the
 * animation step and the collision pass. Contact sub_08093674: type 0,
 * element 0. */
static AosHitReport curly_update(AosEnemy *c, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (c->x >> 16) - cam_x, sy = (c->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (c->state) {
    case 0: curly_walk(c, soma, layer, kind->anims, random); break;
    case 1: curly_attack(c, layer, kind->anims); break;
    case 2:
        generic_death(c, kind, random, &effects);
        if (c->removed) return effects;
        break;
    case 3:
        step_anim(c, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(c, kind->anims);
    sx = (c->x >> 16) - cam_x;
    sy = (c->y >> 16) - cam_y;
    if (!c->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&c->combat);
    take_effects(&report, &effects);
    return report;
}
