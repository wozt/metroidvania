/* SPDX-License-Identifier: GPL-3.0-only */
/* World-draw cluster of the Aria of Sorrow room runtime, moved verbatim
 * out of aria_runtime.c (AGENTS.md rule 8). See aria_draw.h. The shared
 * variant_keys table lives in aria_tables.c; static_index stays in
 * aria_runtime.c (the soul/skill paths there use it too) and is declared
 * in aria_draw.h. */
#include "aria_draw.h"

#include "aria_tables.h"

#include "aos/core/aos_collision.h"
#include "aos/core/aos_scroll.h"
#include "aos/core/aos_wall.h"
#include "aos/math/aos_math.h"
#include "fusion/assets/aos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The sprite art of a pickup, or -1 (items). */
int pickup_art(const AosPickup *pickup) {
    if (pickup->soul_container) return -1;
    if (pickup->type == AOS_PICKUP_HEART) return pickup->small_heart ? 7 : 8;
    if (pickup->type == AOS_PICKUP_MONEY) return pickup->variant < 7 ? pickup->variant : -1;
    if (pickup->type >= AOS_PICKUP_SOUL && pickup->type < AOS_PICKUP_SOUL + 4)
        return 9 + pickup->type - AOS_PICKUP_SOUL;
    return -1;
}

void draw_pickup_art(SDL_Renderer *renderer, const AriaObjects *objects, int index,
                            const AosPickup *pickup, int cam_x, int cam_y) {
    if (index < 0 || index >= PICKUP_ARTS) return;
    const __typeof__(objects->pickup_arts[0]) *art = &objects->pickup_arts[index];
    unsigned total = 0, frame = 0;
    for (unsigned f = 0; f < art->def.count; ++f) total += art->ticks[f];
    if (!total) return;
    unsigned t = pickup->age % total;
    while (frame + 1 < art->def.count && t >= art->ticks[frame]) t -= art->ticks[frame++];
    const AriaFrame *image = &art->frames[frame];
    if (!image->texture) return;
    SDL_FRect rect = {(float)((pickup->x >> 16) + image->offset_x - cam_x),
                      (float)((pickup->y >> 16) + pickup->offset + image->offset_y - cam_y),
                      image->w, image->h};
    SDL_RenderTexture(renderer, image->texture, NULL, &rect);
}

/* An art's exported still frame `id`, or NULL. */
const AriaFrame *blade_frame(const __typeof__(((AriaObjects *)0)->skills[0]) *art, int id) {
    int index = static_index(art->static_ids, art->static_count, id);
    return index < 0 ? NULL : &art->statics[index];
}


/* Doors use their native sprite (frame 0 or 5 by parameter 0) and palette
 * cycle; without the object library an outline marks the blocked area. */
void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
                       long frame_count, int cam_x, int cam_y) {
    const AriaObjects *objects = objects_ptr;
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL || e->id != ARIA_OBJECT_WOODEN_DOOR ||
            e->door.hidden)
            continue;
        int style = e->param0 ? 1 : 0;
        if (objects && objects->counts[style]) {
            /* sub_0803C150: the palette script loops (phase from the run start). */
            int total = 0;
            for (int k = 0; k < objects->counts[style]; ++k) total += objects->ticks[style][k];
            long t = frame_count % total;
            int k = 0;
            while (t >= objects->ticks[style][k]) t -= objects->ticks[style][k++];
            const AriaFrame *frame = &objects->frames[style][k];
            /* Mirrored when facing left, like Soma's frames; the generic draw
             * (0x03004564) is not traced, this matches the room art at both
             * room edges. */
            bool flip = e->door.facing_left;
            /* sub_0804D794's affine swing: while 0 < swing < 0x4000 the
             * leaf is squashed horizontally by cos(swing) and its x
             * shifted by 32*sin(swing) - 1 px; at 0x4000 the cosine is
             * zero and the edge-on leaf disappears into the doorway.
             * The hinge edge stays fixed (the left edge unmirrored,
             * the right edge mirrored). */
            float scale = aos_door_swing_scale(&e->door);
            float x = flip ? (float)(e->x - frame->offset_x) - frame->w
                           : (float)(e->x + frame->offset_x);
            float w = frame->w * scale;
            if (flip) x += frame->w - w;
            x += aos_door_swing_shift(&e->door) - (float)cam_x;
            SDL_FRect rect = {x, (float)(e->y + frame->offset_y - cam_y), w,
                              (float)frame->h};
            if (w >= 1.0f)
                SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0,
                                         NULL, flip ? SDL_FLIP_HORIZONTAL
                                                    : SDL_FLIP_NONE);
            continue;
        }
        SDL_FRect rect = {(float)((e->x & ~15) - cam_x), (float)(e->y - 48 - cam_y), 16, 48};
        SDL_SetRenderDrawColor(renderer, 230, 190, 120, 255);
        SDL_RenderRect(renderer, &rect);
    }
}

/* The drawing pass of an enemy: the zombie's puddles (priority + 1) first,
 * then the Peeping Eye's segments (the eye's + 2), its eyeball and pupil
 * (+ 1), then the others. */
static size_t enemy_pass(const AosEnemy *e) {
    if (e->role == AOS_ROLE_PUDDLE) return 0;
    if (e->role == AOS_ROLE_PEEPING_SEGMENT) return e->variant < 6 ? 1 : 2;
    return 3;
}

/* The breakable walls: their entry's frame at their position (behind the
 * enemies; the OBJ priorities 6 and 0x16 against the backgrounds are not
 * modelled). */
void draw_walls(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                       int cam_x, int cam_y) {
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL ||
            (e->id != ARIA_OBJECT_WALL && e->id != ARIA_OBJECT_EVENT_WALL) || e->wall.broken)
            continue;
        const AriaFrame *frame = &objects->walls[e->wall.index];
        if (!frame->texture) continue;
        SDL_FRect rect = {(float)(e->x + frame->offset_x - cam_x),
                          (float)(e->y + frame->offset_y - cam_y), frame->w, frame->h};
        SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
    }
}

/* The palette bank an enemy is drawn with. */
static int enemy_bank(const AosEnemy *enemy) {
    if (enemy->id == AOS_ENEMY_MEDUSA_HEAD) return enemy->param1 & 1;
    if (enemy->id == AOS_ENEMY_GHOST_DANCER) return enemy->param0 & 0xFF;
    if (enemy->id == AOS_ENEMY_EVIL_BUTCHER) return enemy->param1 & 0xFF;
    return 0;
}

/* The entity record +0x5C draw band an enemy entity resolves to:
 * the live +0x5C mirrors for the three kinds whose native byte is
 * dynamic (Kyoma Demon's kd_5c 0x1D/0x11, Altair's al_draw5c
 * 3/0x30 dash, Mudman's md_spawns spawned-child counter — a
 * hit.children copy carries the parent's values at spawn, like
 * the native child records), the Quezlcoatl segment's own +0x5C
 * segment number (qz_seg, 1..15 — the native byte doubles as
 * the segment's band), and the create-time kind band
 * (AriaEntity.band5c, aos_scroll_enemy_band5c) for everything
 * else. State-dependent +0x5C rewrites inside enemy updates are
 * not yet tracked — see aos_scroll_enemy_band5c. */
static uint8_t enemy_band5c(const AriaEntity *e) {
    const AosEnemy *en = &e->enemy;
    switch (en->id) {
    case AOS_ENEMY_KYOMA_DEMON: return en->kd_5c;
    case AOS_ENEMY_ALTAIR: return en->al_draw5c;
    case AOS_ENEMY_MUDMAN: return en->md_spawns;
    case AOS_ENEMY_QUEZLCOATL:
        if (en->role == AOS_ROLE_QUEZLCOATL_SEGMENT) return en->qz_seg;
        return e->band5c;
    default: return e->band5c;
    }
}

/* Enemies: sprites face left; mirrored ones are flipped around x.
 * `prio_slot` is the native OBJ-priority interleave slot (0 front
 * .. 3 back) the caller is painting; only enemies whose resolved
 * +0x5C band maps to it draw — pass -1 for the front pass, which
 * takes every enemy without a resolved band (the layered path
 * down, or a band outside the native 0x00-0x1F range). */
void draw_enemies(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                         int cam_x, int cam_y, int prio_slot) {
    for (size_t n = 0; n < 4 * room->entity_count; ++n) {
        size_t i = n % room->entity_count;
        const AriaEntity *e = &room->entities[i];
        if (enemy_pass(&e->enemy) != n / room->entity_count) continue;
        if (!e->spawned || e->kind != ARIA_KIND_ENEMY || e->id >= ENEMY_KINDS ||
            e->enemy.removed || e->enemy.hidden || e->enemy.anim.id >= AOS_ENEMY_MAX_ANIMS ||
            e->enemy.anim.frame >= MAX_FRAMES)
            continue;
        int eprio = aria_bg_band_priority(enemy_band5c(e));
        if (prio_slot >= 0 ? eprio != prio_slot : eprio >= 0)
            continue;
        const AriaFrame *frame = &objects->enemies[e->id].frames[e->enemy.anim.id][e->enemy.anim.frame];
        int bank = enemy_bank(&e->enemy);
        for (int v = 0; bank && v < VARIANT_ARTS; ++v)
            if (variant_keys[v].id == (int)e->id && variant_keys[v].bank == bank &&
                objects->variants[v].loaded)
                frame = &objects->variants[v].frames[e->enemy.anim.id][e->enemy.anim.frame];
        if (e->enemy.static_frame >= 0) {
            /* A child entity shows one sprite frame of its kind (+ 0x65). */
            frame = NULL;
            for (int k = 0; k < objects->enemies[e->id].static_count; ++k)
                if (objects->enemies[e->id].static_ids[k] == e->enemy.static_frame)
                    frame = &objects->enemies[e->id].statics[k];
        }
        if (!frame || !frame->texture) continue;
        /* The Disc Armor's + 0x18/+ 0x1C orbit draw position: the disc
         * sprite circles the walking centre (+ 0x40/+ 0x44). */
        int ex = e->enemy.x >> 16, ey = e->enemy.y >> 16;
        if (e->enemy.id == AOS_ENEMY_DISC_ARMOR &&
            e->enemy.role == AOS_ROLE_ENEMY) {
            ex = (int)(e->enemy.da_ox >> 16);
            ey = (int)(e->enemy.da_oy >> 16);
        }
        bool flip = e->enemy.mirrored;
        float x = flip ? (float)(ex - frame->offset_x) - frame->w : (float)(ex + frame->offset_x);
        SDL_FRect rect = {x - (float)cam_x, (float)(ey + frame->offset_y - cam_y), frame->w, frame->h};
        if (e->enemy.vflip) rect.y = (float)(ey - frame->offset_y - cam_y) - frame->h;
        if (e->enemy.scaled) {
            /* The owner's affine slot scales it vertically about the
             * sprite's centre; sub_0803E058 leaves a scale of 0x400 or
             * less with an out-of-range matrix (nothing shown). */
            if (e->enemy.scale <= 0x400) continue;
            float h = rect.h * (float)e->enemy.scale / 65536.0f;
            rect.y += (rect.h - h) / 2;
            rect.h = h;
        }
        if (e->enemy.affine_sx > 0 && e->enemy.affine_sy > 0) {
            /* An affine matrix of scale parameters (pa, pd: 0x10000 =
             * unit, texture from screen): drawn 0x10000 / p times as large
             * about the sprite's centre (double-size placement is not
             * modelled). */
            float w = rect.w * 65536.0f / (float)e->enemy.affine_sx;
            float h = rect.h * 65536.0f / (float)e->enemy.affine_sy;
            rect.x += (rect.w - w) / 2;
            rect.y += (rect.h - h) / 2;
            rect.w = w;
            rect.h = h;
        }
        SDL_FlipMode mode = (SDL_FlipMode)((flip ? SDL_FLIP_HORIZONTAL : 0) |
                                           (e->enemy.vflip ? SDL_FLIP_VERTICAL : 0));
        /* A spinning child (the axe): its affine angle, 0x10000 per turn.
         * sub_0803E058 builds pa = cos, pb = sin, pc = -sin, pd = cos
         * (texture from screen) and negates pa / pb for a horizontal flip:
         * the texture is flipped, then turned clockwise by the angle, as
         * SDL does with positive degrees. Rotation is about the entity
         * position (the affine double-size placement is not modelled). */
        double degrees = 0;
        SDL_FPoint centre = {(float)ex - rect.x - (float)cam_x, (float)ey - rect.y - (float)cam_y};
        if (e->enemy.role == AOS_ROLE_AXE || e->enemy.role == AOS_ROLE_FIN ||
            e->enemy.role == AOS_ROLE_WORM_SEGMENT ||
            e->enemy.role == AOS_ROLE_BUTCHER_CLEAVER ||
            e->enemy.role == AOS_ROLE_BUTCHER_GORE ||
            e->enemy.role == AOS_ROLE_MANTLE_ORBIT ||
            e->enemy.role == AOS_ROLE_DISC_BLADE ||
            e->enemy.role == AOS_ROLE_DISC_GLOW ||
            (e->enemy.id == AOS_ENEMY_DISC_ARMOR && e->enemy.state == 2) ||
            e->enemy.role == AOS_ROLE_WAITER_TRAY ||
            e->enemy.role == AOS_ROLE_COCKATRICE_ROCK ||
            e->enemy.role == AOS_ROLE_COCKATRICE_SHADOW ||
            (e->enemy.id == AOS_ENEMY_DRYAD && e->enemy.role == AOS_ROLE_ENEMY &&
             e->enemy.state == 3) ||
            /* The Slime's sub_0803E058 turn: + 0x20 is its hop angle. */
            (e->enemy.id == AOS_ENEMY_SLIME && e->enemy.role == AOS_ROLE_ENEMY) ||
            (e->enemy.role == AOS_ROLE_SKELETON_BONE && e->enemy.spinning) ||
            ((e->enemy.id == AOS_ENEMY_BUER || e->enemy.id == AOS_ENEMY_NEMESIS ||
              e->enemy.id == AOS_ENEMY_GIANT_WORM || e->enemy.id == AOS_ENEMY_POISON_WORM) &&
             e->enemy.role == AOS_ROLE_ENEMY))
            degrees = (double)(e->enemy.angle & 0xFFFF) * 360.0 / 65536.0;
        /* The Peeping Eye's tip segment: its own affine angle (sub_0803E238,
         * taken with the convention of sub_0803E058). */
        if (e->enemy.role == AOS_ROLE_PEEPING_SEGMENT && e->enemy.variant == 5)
            degrees = (double)e->enemy.pe_rotation * 360.0 / 65536.0;
        /* + 0x5A bits 1-2 = 2: a semi-transparent OBJ; the room's blend
         * coefficients are not traced, half alpha stands for them. */
        if (e->enemy.translucent) SDL_SetTextureAlphaMod(frame->texture, 128);
        SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, degrees,
                                 degrees != 0 ? &centre : NULL, mode);
        if (e->enemy.translucent) SDL_SetTextureAlphaMod(frame->texture, 255);
    }
}

/* Room SPECIAL objects (kind 2): drawn from their exported sprite sheets.
 * Animation selection per object id is not yet audited against the native
 * update routines, so the first exported animation plays on a loop; ids
 * whose create exports raw still frames as separate animations show
 * animation 0's first frame for now. */
/* Object20 (SPECIAL id 32) sprite selection. The param0-dispatched
 * private 12-record table at 0x081186DC (sub_0805B754 /
 * sub_0805B5E8 record picks) maps onto the exports as: records
 * 0/1/8 -> Enemy/opening_actor_0/1/8, records 2-7/9-11 ->
 * Enemy/object20_actor_N. param0 19 (the shopkeeper) has no
 * dedicated export; its native triple resembles the Julius Belmont
 * enemy art, the closest loaded match. param0 20/21/22/30 are
 * invisible natively and param0 3 stays owned by the bespoke
 * opening path, so neither draws here. */
static int object20_sprite_record(int param0) {
    switch (param0) {
    case 4:
    case 7:  return 5;
    case 5:
    case 8:
    case 28: return 2;
    case 6:  return 6;
    case 9:  return 2;
    case 10:
    case 29:
    case 31: return 7;
    case 23:
    case 24:
    case 27: return 0;
    case 25:
    case 26: return 3;
    default: return -1;
    }
}

/* Create-time sub_0803F2C8 sequence per param0, used when no update
 * reported a pick yet (the npc_talk families never report one).
 * Sources: create9 (3,0,1), create10 (4,1,1), create19 (1,1,1),
 * the param0-26 companion pick sub_0803F2C8(1|2,0,1) — the RNG
 * branch is not persisted, arg 1 approximates — and the param0-28
 * spawn gate sub_0803F2C8(e,6,0,1); the dirs creates all open on
 * sequence 0. */
static int object20_anim_default(int param0) {
    switch (param0) {
    case 9:  return 3;
    case 10: return 4;
    case 19: return 1;
    case 26: return 1;
    case 28: return 6;
    default: return 0;
    }
}

/* Resolve the private 0x081186DC table art for a raw record
 * index (0-11): records 0/1/8 share the opening_actor exports,
 * 2-7/9-11 are Enemy/object20_actor_N. The +0x14/+0x18 child
 * sprites spawned through sub_0805B5E8(idx) use the index
 * directly as the record — unlike the parent, which maps
 * param0 through object20_sprite_record. */
static bool object20_record_art(const AriaObjects *objects, int record,
                                const AriaFrame (**frames)[MAX_FRAMES],
                                const uint8_t (**ticks)[MAX_FRAMES],
                                const AosAnimDef **defs, int *anims) {
    if (record == 0 || record == 1 || record == 8) {
        const __typeof__(objects->opening_actors[0]) *art =
            &objects->opening_actors[record == 8 ? 2 : record];
        if (!art->loaded) return false;
        *frames = art->frames;
        *ticks = art->ticks;
        *defs = art->defs;
        *anims = OBJECT_MAX_ANIMS;
        return true;
    }
    if (record < 2 || record > 11) return false;
    const AriaObject20ActorArt *art = &objects->object20_actors[record];
    if (!art->loaded) return false;
    *frames = art->frames;
    *ticks = art->ticks;
    *defs = art->defs;
    *anims = OBJECT20_ACTOR_ANIMS;
    return true;
}

/* The shared (step / 4) % total frame pick used by every
 * Object20 sprite draw (sub_0803F17C frame stepping is not
 * ported). */
static const AriaFrame *object20_anim_frame(
        const AriaFrame (*frames)[MAX_FRAMES],
        const uint8_t (*ticks)[MAX_FRAMES], const AosAnimDef *defs,
        int anim, unsigned step) {
    unsigned total = 0;
    for (unsigned f = 0; f < defs[anim].count; ++f)
        total += ticks[anim][f];
    if (!total) return NULL;
    unsigned t = (step / 4) % total, index = 0;
    while (index + 1 < defs[anim].count && t >= ticks[anim][index])
        t -= ticks[anim][index++];
    return &frames[anim][index];
}

/* Draw one Object20 child sprite at its persisted +0x42/+0x46
 * screen-pixel position (no camera subtraction — the native
 * child words are already screen-relative). `flip` is the
 * child's +0x58 bit-6 facing. */
static void draw_object20_child(SDL_Renderer *renderer,
                                const AriaObjects *objects, int record,
                                int anim, int scr_x, int scr_y,
                                bool flip, unsigned step) {
    const AriaFrame (*frames)[MAX_FRAMES];
    const uint8_t (*ticks)[MAX_FRAMES];
    const AosAnimDef *defs;
    int anims;
    if (!object20_record_art(objects, record, &frames, &ticks, &defs,
                             &anims))
        return;
    if (anim < 0 || anim >= anims || !defs[anim].count)
        anim = 0;
    if (anim < 0 || anim >= anims || !defs[anim].count) return;
    const AriaFrame *frame = object20_anim_frame(frames, ticks, defs,
                                                 anim, step);
    if (!frame || !frame->texture) return;
    SDL_FRect rect = {(float)(scr_x + frame->offset_x),
                      (float)(scr_y + frame->offset_y),
                      frame->w, frame->h};
    SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0,
                             NULL, flip ? SDL_FLIP_HORIZONTAL
                                        : SDL_FLIP_NONE);
}

/* Draw the param0-30 whiteout's persisted per-scanline records
 * (sub_08064518 -> 160x BgAffineSet -> the BG2 hblank affine
 * window). The host has no affine pipeline and the BG2 tilemap is
 * uncommitted, so this is an honest approximation, not native
 * pixels:
 *  - record i is the hblank block for scanline i (the native dest
 *    records advance 16 bytes per call in emission order), drawn
 *    as a 1px-high strip at screen row i; 160 records = VIEW_H.
 *  - the strip's left edge is (int16_t)bar.x, the record's
 *    on-screen display-center X; the case 4/5 per-bar X wedge
 *    therefore shears the wipe edge diagonally.
 *  - alpha is bar.intensity >> 8. Per the BgAffineSet source
 *    layout the +0x10 word is the rotation angle (only its high
 *    byte, field_14, is used); it is reused here as the white
 *    alpha proxy because its 0 -> 0xFF -> 0 ramp matches the
 *    whiteout's fade in/out.
 * bar.y (the per-scanline display-center Y anchor), bgx/bgy (the
 * 8.8 X/Y scale factors, not scroll offsets) and the rotation
 * itself have no screen-space meaning without the affine
 * transform and are dropped. Drawn inside the special-object
 * pass, so the bars cover the already-drawn room but stay under
 * the player sprite, HUD and later overlays — roughly the native
 * BG2-below-OBJ layering; enemies/specials already drawn are
 * covered, which the flattened pass cannot avoid. */
static void draw_object20_wo_bars(SDL_Renderer *renderer,
                                  const AriaEntity *e) {
    if (!e->wo_bars_count) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (unsigned i = 0; i < e->wo_bars_count && i < VIEW_H; ++i) {
        const AosWoBar *b = &e->wo_bars[i];
        uint8_t alpha = (uint8_t)(b->intensity >> 8);
        if (!alpha) continue;
        int x0 = (int16_t)b->x;
        if (x0 >= VIEW_W) continue;
        /* x0 < 0 still needs to reach the right edge. */
        float w = x0 < 0 ? (float)(VIEW_W - x0) : (float)VIEW_W;
        SDL_FRect strip = {(float)x0, (float)i, w, 1.0f};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
        SDL_RenderFillRect(renderer, &strip);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* Whiteout burst carriers (sub_08063168/sub_080632B0/
 * sub_08065834) and particles (sub_080630E8) drawn from the
 * wo_bursts host pool. Carrier positions are the native +0x42/+0x46
 * screen coords; its animation is anim_<variant> advanced by wall
 * clock through the exported tick durations (the native
 * sub_0803F17C step is approximated like the other SPECIAL draws —
 * the +0x65 frame byte is not tracked on the carrier). Particles
 * draw their +0x65 still frame at the 16.16 position high words.
 * Palette cycle banks 5-9 of 0x08119FDC are unexported, so both
 * draw bank 0 — documented gap. */
static void draw_object20_wo_bursts(SDL_Renderer *renderer,
                                    const AriaObjects *objects,
                                    const AriaEntity *e, unsigned step) {
    if (!e->wo_burst_count) return;
    const __typeof__(objects->special_objects[0]) *carrier =
        &objects->wo_burst_carrier;
    const __typeof__(objects->special_objects[0]) *particle =
        &objects->wo_burst_particle;
    for (uint8_t i = 0; i < e->wo_burst_count; ++i) {
        const AosWoBurst *b = &e->wo_bursts[i];
        if (b->carrier_valid && carrier->loaded &&
            b->carrier.variant >= 0 &&
            b->carrier.variant < OBJECT_MAX_ANIMS &&
            carrier->defs[b->carrier.variant].count) {
            unsigned total = 0;
            for (unsigned f = 0;
                 f < carrier->defs[b->carrier.variant].count; ++f)
                total += carrier->ticks[b->carrier.variant][f];
            if (total) {
                unsigned t = (step / 4) % total, index = 0;
                while (index + 1 <
                           carrier->defs[b->carrier.variant].count &&
                       t >= carrier->ticks[b->carrier.variant][index])
                    t -= carrier->ticks[b->carrier.variant][index++];
                const AriaFrame *frame =
                    &carrier->frames[b->carrier.variant][index];
                if (frame->texture) {
                    SDL_FRect rect = {
                        (float)(b->carrier.scr_42 + frame->offset_x),
                        (float)(b->carrier.scr_46 + frame->offset_y),
                        frame->w, frame->h};
                    SDL_RenderTexture(renderer, frame->texture, NULL,
                                      &rect);
                }
            }
        }
        for (uint8_t p = 0; p < b->particle_count; ++p) {
            const AosWoBurstParticle *pt = &b->particles[p];
            if (!particle->loaded || pt->field_65 >= MAX_FRAMES)
                continue;
            const AriaFrame *frame = &particle->frames[0][pt->field_65];
            if (!frame->texture) continue;
            SDL_FRect rect = {
                (float)((pt->field_40 >> 16) + frame->offset_x),
                (float)((pt->field_44 >> 16) + frame->offset_y),
                frame->w, frame->h};
            SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
        }
    }
}

/* One whiteout still-frame particle (sub_08063024 sparkle /
 * sub_08063AEC burst particle / sub_080630E8 burst particle —
 * all share graphics 0x08260390, frames 0x08267EE4 and draw
 * sub_0803B9D0): +0x65 indexes the still frame inside the
 * whiteout_burst_particle export (native record index; only
 * frames 0xB/0xC are exported — 0xA picks draw nothing,
 * documented gap), position is the +0x40/+0x44 16.16 pair's
 * high words (+0x42/+0x46 aliases). */
static void draw_object20_wo_particle(
        SDL_Renderer *renderer, const AriaObjects *objects,
        uint8_t field_65, int32_t fx, int32_t fy) {
    const __typeof__(objects->special_objects[0]) *particle =
        &objects->wo_burst_particle;
    if (!particle->loaded || field_65 >= MAX_FRAMES)
        return;
    const AriaFrame *frame = &particle->frames[0][field_65];
    if (!frame->texture) return;
    SDL_FRect rect = {
        (float)((fx >> 16) + frame->offset_x),
        (float)((fy >> 16) + frame->offset_y),
        frame->w, frame->h};
    SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
}

/* The story family's host pools drawn with the shared particle
 * art: wo_sparks (sub_08063024/sub_08065858) and wo_63aec
 * (sub_08063AEC/sub_08063944). Screen-space positions — no
 * camera, like the bars and bursts. */
static void draw_object20_wo_particles(SDL_Renderer *renderer,
                                       const AriaObjects *objects,
                                       const AriaEntity *e) {
    if (e->object20.family != AOS_OBJECT20_FAMILY_STORY)
        return;
    const AosObject20Story *s = &e->object20.v.story;
    for (uint8_t i = 0; i < s->wo_spark_count; ++i) {
        const AosWoSpark *sp = &s->wo_sparks[i];
        draw_object20_wo_particle(renderer, objects, sp->field_65,
                                  sp->field_40, sp->field_44);
    }
    for (uint8_t i = 0; i < s->wo_63aec_count; ++i) {
        const AosWo63AEC *p = &s->wo_63aec[i];
        draw_object20_wo_particle(renderer, objects, p->field_65,
                                  p->field_40, p->field_44);
    }
}

static void draw_object20_entity(SDL_Renderer *renderer,
                                 const AriaObjects *objects,
                                 const AriaEntity *e, int cam_x, int cam_y,
                                 unsigned step) {
    /* The whiteout bars are screen-space (no camera) and draw
     * under this entity's own sprite. */
    draw_object20_wo_bars(renderer, e);
    draw_object20_wo_bursts(renderer, objects, e, step);
    draw_object20_wo_particles(renderer, objects, e);
    /* Child sprites draw under the parent (native entity order is
     * unverified): the story family's +0x14 child14 and +0x18
     * link18 (param0 9/23/31) and the param0-26 companion's +0x18
     * child18. Positions/anims are the last values the ported
     * variants persisted — see the variant headers: the param0-31
     * child29 chain has ported update vectors while child14/
     * link18/child18 keep only the native sub_0805B748 anim step
     * and intentionally stay at create-time placement. */
    switch (e->object20.family) {
    case AOS_OBJECT20_FAMILY_STORY: {
        const AosObject20Story *s = &e->object20.v.story;
        if (s->child14)
            draw_object20_child(renderer, objects, s->child14_record,
                                s->child14_anim6d, s->child14_scr42,
                                s->child14_scr46,
                                (s->child14_flags58 & 0x40) != 0, step);
        if (s->link18)
            /* The +0x18 follower's sub_0805B5E8 spawn index is
             * always record 1 (param0 9 only). */
            draw_object20_child(renderer, objects, 1, s->link18_anim6d,
                                s->link18_scr42, s->link18_scr46,
                                (s->link18_flags58 & 0x40) != 0, step);
        break;
    }
    case AOS_OBJECT20_FAMILY_LOOP: {
        const AosNpcLoop *g = &e->object20.v.npc_loop;
        if (g->child18)
            /* sub_0805B5E8(3) -> record 3; the child's own +0x58
             * is not persisted — the parent's facing byte is the
             * approximation. */
            draw_object20_child(renderer, objects, 3, g->child18_anim,
                                g->child18_scr42, g->child18_scr46,
                                (g->flags58 & 0x40) != 0, step);
        break;
    }
    default:
        break;
    }
    const AriaFrame (*frames)[MAX_FRAMES] = NULL;
    const uint8_t (*ticks)[MAX_FRAMES] = NULL;
    const AosAnimDef *defs = NULL;
    int anims = 0;
    if (e->param0 == 19) {
        const __typeof__(objects->enemies[0]) *art =
            &objects->enemies[AOS_ENEMY_BELMONT];
        if (!art->loaded) return;
        frames = art->frames;
        ticks = art->ticks;
        defs = art->defs;
        anims = AOS_ENEMY_MAX_ANIMS;
    } else {
        int record = object20_sprite_record(e->param0);
        if (record < 0) return;
        if (record == 0 || record == 1 || record == 8) {
            const __typeof__(objects->opening_actors[0]) *art =
                &objects->opening_actors[record == 8 ? 2 : record];
            if (!art->loaded) return;
            frames = art->frames;
            ticks = art->ticks;
            defs = art->defs;
            anims = OBJECT_MAX_ANIMS;
        } else {
            const AriaObject20ActorArt *art =
                &objects->object20_actors[record];
            if (!art->loaded) return;
            frames = art->frames;
            ticks = art->ticks;
            defs = art->defs;
            anims = OBJECT20_ACTOR_ANIMS;
        }
    }
    int anim = e->object20_anim;
    if (anim < 0 || anim >= anims || !defs[anim].count)
        anim = object20_anim_default(e->param0);
    if (anim < 0 || anim >= anims || !defs[anim].count) return;
    const AriaFrame *frame = object20_anim_frame(frames, ticks, defs,
                                                 anim, step);
    if (!frame || !frame->texture) return;
    /* +0x58 bit 6 is the native facing byte the npc creates mirror
     * from the player compare; flip like the emitted opening
     * entities do. dirs/story keep their copy inside the variant
     * state, the other families report it on the flat field. */
    uint8_t f58 = e->object20.flags58;
    switch (e->object20.family) {
    case AOS_OBJECT20_FAMILY_DIRS:
        f58 = e->object20.v.dirs.flags58;
        break;
    case AOS_OBJECT20_FAMILY_STORY:
        f58 = e->object20.v.story.flags58;
        break;
    default:
        break;
    }
    SDL_FRect rect = {(float)(e->x + frame->offset_x - cam_x),
                      (float)(e->y + frame->offset_y - cam_y),
                      frame->w, frame->h};
    SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                             (f58 & 0x40) ? SDL_FLIP_HORIZONTAL
                                         : SDL_FLIP_NONE);
}

/* `prio_slot` is the native OBJ-priority interleave slot the
 * caller is drawing (0 front .. 3 back): only drawable units
 * whose retained +0x5C band resolves to that priority through
 * aria_bg_band_priority draw — today that is Object1F's
 * per-child band5c alone. prio_slot < 0 is the front pass:
 * every unit without a resolved band draws there, preserving
 * the pre-priority all-on-top order. */
void draw_special_objects(SDL_Renderer *renderer, const AriaRoom *room,
                                 const AriaObjects *objects, int cam_x, int cam_y,
                                 unsigned step, int prio_slot) {
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL || e->id < 0 || e->id >= 64)
            continue;
        /* Every special object but Object1F lacks a retained
         * +0x5C band, so a priority slot skips them outright;
         * they all draw in the front pass. */
        if (prio_slot >= 0 && e->id != 31) continue;
        if (e->id == 2 && e->object02.deleted) continue;
        if (e->id == 4 && e->object04.deleted) continue;
        if (e->id == 15 && e->object0f.finished) continue;
        if (e->id == 14 && e->object0e.finished) continue;
        if (e->id == 19 && e->object13.deleted) continue;
        if (e->id == 47 && e->object2f.deleted) continue;
        if (e->id == 36 && e->object24.hidden) continue;
        if (e->id == 32) {
            /* Object20: the param0-selected record of its private
             * sprite table (records 0/1/8 share the opening_actor
             * exports) plus the child sprites the ported variants
             * persist — story child14/link18 and the param0-26
             * companion child18 (see draw_object20_entity). */
            if (!e->object20.deleted)
                draw_object20_entity(renderer, objects, e, cam_x, cam_y,
                                     step);
            continue;
        }
        if (e->id == 1) {
            /* Object01's children are the sprites: draw the exported
             * still at each record coordinate (room pixels). */
            if (!e->spawned || !e->object01.claimed) continue;
            const __typeof__(objects->special_objects[0]) *child_art =
                &objects->special_objects[1];
            if (!child_art->loaded || !child_art->defs[0].count ||
                !child_art->frames[0][0].texture) continue;
            const AriaFrame *frame = &child_art->frames[0][0];
            for (int c = 0; c < e->object01.child_count; ++c) {
                SDL_FRect rect = {(float)(e->object01.child_x[c] +
                                          frame->offset_x - cam_x),
                                  (float)(e->object01.child_y[c] +
                                          frame->offset_y - cam_y),
                                  frame->w, frame->h};
                SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
            }
            continue;
        }
        if (e->id == 28) {
            /* Object1C children drawFunc pass: sub_080544E8 (marker)
             * commits three sprite positions per frame, sparkle/beam
             * sub_0803B9D0 one, spark sub_080554BC two. Every kind
             * shares frame records 0x08257964 / graphics 0x082052E8;
             * the palette bank is the spawn-fixed +0x5A bits 3-6
             * nibble minus 0xA (OBJ row 0xA+N = bank N), exported as
             * special_28_v<bank>: marker param 0xE -> bank 4,
             * sparkle records 0xC/0xB -> banks 2/1 (unchanged by the
             * state-1 sub_0803F2C8 anim swap), beam and spark 0xD ->
             * bank 3. commit x/y are the native +0x42/+0x46 screen
             * positions — no camera offset. +0x65 is the flat
             * frame-record index (0-25); the variant's anims are
             * contiguous ranges of it, so it decomposes by cumulative
             * count. */
            if (e->object1c.deleted) continue;
            /* children_draw recomputes the draw-side position
             * fields like the native drawFunc does; the writes
             * are idempotent per frame, so the const entity is
             * cast for the module's non-const API. */
            AosObject1CChildrenOut dout;
            aos_object1c_children_draw(
                (AosObject1C *)&e->object1c, &dout);
            if (!dout.commits) continue;
            for (unsigned c = 0; c < dout.commits; ++c) {
                const AosObject1CCommit *cm = &dout.commit[c];
                if (cm->child >= e->object1c.child_count) continue;
                const AosObject1CChild *ch =
                    &e->object1c.children[cm->child];
                int bank = (int)((ch->field5a >> 3) & 0xF) - 0xA;
                const __typeof__(objects->special_objects[0]) *art28 =
                    NULL;
                for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                    if (objects->special_variants[v].id == 28 &&
                        objects->special_variants[v].param0 == bank &&
                        objects->special_variants[v].loaded) {
                        art28 = &objects->special_variants[v];
                        break;
                    }
                if (!art28) continue;
                /* Flat +0x65 -> (anim, index): the descriptor's
                 * sequences cover contiguous flat ranges in anim
                 * order (anim0 0-3, anim1 4-7, anim2 8-9, anim3
                 * 10-11, anim4 12-15, anim5 16-19, anims 6-11
                 * 20-25). */
                const AriaFrame *frame28 = NULL;
                unsigned flat = cm->frame, base = 0;
                for (unsigned a = 0; a < OBJECT_MAX_ANIMS && !frame28;
                     ++a) {
                    unsigned n = art28->defs[a].count;
                    if (flat >= base && flat < base + n)
                        frame28 = &art28->frames[a][flat - base];
                    base += n;
                }
                if (!frame28 || !frame28->texture) continue;
                SDL_FRect rect28 = {
                    (float)(cm->x + frame28->offset_x),
                    (float)(cm->y + frame28->offset_y),
                    frame28->w, frame28->h};
                SDL_RenderTexture(renderer, frame28->texture, NULL,
                                  &rect28);
            }
            continue;
        }
        if (e->id == 29) {
            /* Object1D: +0x65 indexes the still frame inside the
             * variant's frame descriptor (0x08257BD8 = 12 records
             * for v0, 0x08257964 = 26 for v1 — see aos_object1d.h),
             * exported as the flat frame list across anim_N. Walk
             * the anims in order to that index; there is no anim
             * stepping (update is the shared sub_0803F17C no-op
             * for this record shape). */
            const __typeof__(objects->special_objects[0]) *art29 = NULL;
            for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                if (objects->special_variants[v].id == 29 &&
                    objects->special_variants[v].param0 == e->param0 &&
                    objects->special_variants[v].loaded) {
                    art29 = &objects->special_variants[v];
                    break;
                }
            if (!art29) continue;
            unsigned want = e->object1d.sel, seen = 0;
            const AriaFrame *frame29 = NULL;
            for (unsigned a = 0; a < AOS_ENEMY_MAX_ANIMS && !frame29; ++a)
                for (unsigned f = 0; f < art29->defs[a].count; ++f, ++seen)
                    if (seen == want) {
                        frame29 = &art29->frames[a][f];
                        break;
                    }
            if (!frame29 || !frame29->texture) continue;
            SDL_FRect rect29 = {(float)(e->x + frame29->offset_x - cam_x),
                                (float)(e->y + frame29->offset_y - cam_y),
                                frame29->w, frame29->h};
            SDL_RenderTexture(renderer, frame29->texture, NULL, &rect29);
            continue;
        }
        if (e->id == 31) {
            /* Object1F's eleven children are the sprites (the
             * parent draws nothing natively). +0x42/+0x46 are
             * screen-space positions — the gate is a fixed
             * cinematic element, so no camera offset. Ring
             * children (0-8) draw special_31_v2 frame record
             * +0x65 through anim_<idx65>; flanks (9-10) draw
             * +0x65 = 9 on the palette bank cycled by the
             * 0x081185F8 table — anim_id holds the last bank
             * once state 1 runs, bank 4 before. */
            if (!e->spawned || e->object1f.deleted) continue;
            for (int ci = 0; ci < 11; ++ci) {
                const AosObject1FChild *ch = &e->object1f.children[ci];
                /* +0x5C draw band -> this room's OBJ priority
                 * (sub_0803E528 bucket -> sub_0803DE10's OAM
                 * attribute-2 stamp). An unresolvable band
                 * (no layered BG path) keeps the child in the
                 * front pass like every other unbanded sprite. */
                int cprio = aria_bg_band_priority(ch->band5c);
                if (prio_slot >= 0 ? cprio != prio_slot : cprio >= 0)
                    continue;
                int bank = ci < 9 ? 2
                        : (ch->state == 1 ? ch->anim_id : 4);
                const __typeof__(objects->special_objects[0]) *art31 = NULL;
                for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                    if (objects->special_variants[v].id == 31 &&
                        objects->special_variants[v].param0 == bank &&
                        objects->special_variants[v].loaded) {
                        art31 = &objects->special_variants[v];
                        break;
                    }
                if (!art31) continue;
                unsigned anim = ch->idx65;
                if (anim >= OBJECT_MAX_ANIMS ||
                    !art31->defs[anim].count) continue;
                const AriaFrame *frame31 = &art31->frames[anim][0];
                if (!frame31->texture) continue;
                SDL_FRect rect31 = {
                    (float)(ch->pos42 + frame31->offset_x),
                    (float)(ch->pos46 + frame31->offset_y),
                    frame31->w, frame31->h};
                SDL_RenderTexture(renderer, frame31->texture, NULL,
                                  &rect31);
            }
            continue;
        }
        if (e->id == 7) {
            /* Object07's children are the sprites: ambient motes and the
             * four anchored records share the exported mote art; parent
             * param0 == 2 selects the alternate set (v101). Positions
             * are fixed-point screen pixels (fx >> 16). */
            const __typeof__(objects->special_objects[0]) *child_art = NULL;
            for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                if (objects->special_variants[v].id == 7 &&
                    objects->special_variants[v].param0 ==
                        (e->param0 == 2 ? 101 : 100) &&
                    objects->special_variants[v].loaded) {
                    child_art = &objects->special_variants[v];
                    break;
                }
            if (!child_art || !child_art->defs[0].count) continue;
            unsigned total = 0;
            for (unsigned f = 0; f < child_art->defs[0].count; ++f)
                total += child_art->ticks[0][f];
            if (!total) continue;
            unsigned t = (step / 4) % total, index = 0;
            while (index + 1 < child_art->defs[0].count &&
                   t >= child_art->ticks[0][index])
                t -= child_art->ticks[0][index++];
            const AriaFrame *frame = &child_art->frames[0][index];
            if (!frame->texture) continue;
            for (int c = 0; c < e->object07.ambient_count; ++c) {
                const AosObject07Mote *m = &e->object07.motes[c];
                if (!m->alive) continue;
                SDL_FRect rect = {(float)((m->fx >> 16) + frame->offset_x),
                                  (float)((m->fy >> 16) + frame->offset_y),
                                  frame->w, frame->h};
                SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
            }
            for (int c = 0; c < e->object07.anchor_count; ++c) {
                const AosObject07Anchor *a = &e->object07.anchors[c];
                if (!a->alive) continue;
                SDL_FRect rect = {(float)((a->fx >> 16) + frame->offset_x),
                                  (float)((a->fy >> 16) + frame->offset_y),
                                  frame->w, frame->h};
                SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
            }
            continue;
        }
        const __typeof__(objects->special_objects[0]) *art = NULL;
        /* Object0FCreate (id 15) selects the OBJ palette row param0 >> 8,
         * defaulting to 7; its variant suffix carries that row. Other
         * variant ids match the raw param0. */
        int variant_key = e->param0;
        if (e->id == 15) {
            variant_key = e->param0 >> 8;
            if (!variant_key) variant_key = 7;
        } else if (e->id == 14 && variant_key > 14) {
            /* Object0E's 15 config records map onto special_14_v00-v14. */
            variant_key = 14;
        } else if (e->id == 42) {
            /* Object2A's 0x08526510 records index param1 (+ 0x32),
             * not param0 — the _vP suffix carries param1. */
            variant_key = e->param1;
        } else if (e->id == 41) {
            /* Object29's 0x085264D0 records repeat every four
             * entries and share the special_42_v00-v03 triples;
             * the + 0x32 param selects the record. */
            variant_key = e->param1 & 3;
        }
        for (int v = 0; v < SPECIAL_VARIANTS; ++v)
            if ((objects->special_variants[v].id == e->id ||
                 (e->id == 41 &&
                  objects->special_variants[v].id == 42)) &&
                objects->special_variants[v].param0 == variant_key &&
                objects->special_variants[v].loaded) {
                art = &objects->special_variants[v];
                break;
            }
        if (!art) {
            /* Object26 and its Object27/Object28 orb children share
             * the chandelier tiles/frames of special_19 but palette
             * slot 2 — exported as special_38. */
            if (e->id == 38 || e->id == 39 || e->id == 40)
                art = &objects->special_objects[38];
            else if (e->id == 48 || e->id == 49)
                /* Object30's sub_08058FE8 and Object31's
                 * sub_080593C0 load the identical triple as
                 * Object2F — special_47. */
                art = &objects->special_objects[47];
            else
                art = &objects->special_objects[e->id];
        }
        if (!art->loaded || !art->defs[0].count) continue;
        unsigned total = 0;
        for (unsigned f = 0; f < art->defs[0].count; ++f) total += art->ticks[0][f];
        if (!total) continue;
        unsigned t = (step / 4) % total, index = 0;
        while (index + 1 < art->defs[0].count && t >= art->ticks[0][index])
            t -= art->ticks[0][index++];
        const AriaFrame *frame = &art->frames[0][index];
        if (!frame->texture) continue;
        int ex = e->x, ey = e->y;
        SDL_FRect rect = {(float)(ex + frame->offset_x - cam_x),
                          (float)(ey + frame->offset_y - cam_y), frame->w, frame->h};
        SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
    }
}

/* Object20's first-room actors are ordinary object entities, not enemies.
 * sub_0805D198 starts Mina at animation 4, Arikado at animation 1 and the
 * distant winged skeleton at animation 4. The compact actor controller keeps
 * their fixed-point positions; the later callback branches remain a follow-up.
 * `prio_slot` is the native OBJ-priority interleave slot (0 front .. 3
 * back) the caller is painting: sub_0805B5E8 stamps every child +0x5C =
 * 0x11 (parameter-bit-2 Skeleton Knights get 0x17 from sub_0805E528),
 * debris copies its parent's band, sub_0806D578 flames draw at band-1,
 * soul orbs and sub_0805ED80 bolts carry 0x13 and sub_08045B44 type-8
 * sparkles 0x0C — pass -1 for the front pass, which takes every element
 * whose band did not resolve (the layered path down or a band outside
 * the native 0x00-0x1F range). */
void draw_opening_actors(SDL_Renderer *renderer, const AriaObjects *objects,
                                const AosOpeningActors *actors, int cam_x,
                                int cam_y, int prio_slot) {
    if (!actors) return;
    for (int actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor) {
        if (!actors->visible[actor]) continue;
        int band_prio = aria_bg_band_priority(0x11);
        if (prio_slot >= 0 ? band_prio != prio_slot : band_prio >= 0)
            continue;
        const __typeof__(objects->opening_actors[0]) *art = &objects->opening_actors[actor];
        int animation = actors->animation[actor];
        /* The export keeps OBJECT_MAX_ANIMS slots; the wider
         * AOS_ENEMY_MAX_ANIMS bound would index past defs. */
        if (!art->loaded || animation >= OBJECT_MAX_ANIMS ||
            !art->defs[animation].count)
            continue;
        unsigned index;
        /* +0x6E is the sequence entry index itself: when the host
         * bound the flattened {entry,ticks} sequence the tick chain
         * in aos_opening_actors_update already advanced it (loop
         * wrap or the +0x6C-bit-2 last-entry pin keep it inside
         * [0,count); the clamp only guards a hand-built state). */
        const AosOpeningAnimSeq *seq =
            &actors->anim_seqs[actor][animation];
        if (seq->ticks && seq->count) {
            index = actors->anim_subframe[actor];
            if (index >= seq->count) index = seq->count - 1;
        } else {
            unsigned total = 0;
            for (unsigned i = 0; i < art->defs[animation].count; ++i)
                total += art->ticks[animation][i];
            if (!total) continue;
            unsigned t = (unsigned)(actors->animation_age[actor] % total);
            index = 0;
            while (index + 1 < art->defs[animation].count &&
                   t >= art->ticks[animation][index])
                t -= art->ticks[animation][index++];
        }
        const AriaFrame *frame = &art->frames[animation][index];
        if (!frame->texture) continue;
        int x = actors->x[actor] >> 16, y = actors->y[actor] >> 16;
        SDL_FRect rect = {(float)(x + frame->offset_x - cam_x),
                          (float)(y + frame->offset_y - cam_y), frame->w,
                          frame->h};
        SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
    }
    for (unsigned actor = 0; actor < actors->emitted_count; ++actor) {
        const AosOpeningEntity *entity = &actors->emitted[actor];
        if (!entity->active || entity->hidden) continue;
        const uint8_t entity_band =
            entity->entity_type == 10 && (entity->parameter & 4)
                ? 0x17
                : 0x11;
        int band_prio = aria_bg_band_priority(entity_band);
        if (prio_slot >= 0 ? band_prio != prio_slot : band_prio >= 0)
            continue;
        /* Type 8 draws Enemy/opening_actor_8 (the actor slot-2 export)
         * and type 10 draws the Skeleton Knight enemy art; both carry
         * exported {frame,ticks} rows, so a bound sequence selects the
         * frame from the child's real +0x6E subframe and only an
         * unbound animation falls back to the age walk. */
        const AriaFrame *row = NULL;
        const uint8_t *ticks = NULL;
        unsigned count = 0;
        const AosOpeningAnimSeq *seq = NULL;
        int animation = entity->animation;
        if (entity->entity_type == 8) {
            const __typeof__(objects->opening_actors[0]) *art =
                &objects->opening_actors[2];
            if (!art->loaded || animation >= OBJECT_MAX_ANIMS ||
                !art->defs[animation].count)
                continue;
            row = art->frames[animation];
            ticks = art->ticks[animation];
            count = art->defs[animation].count;
            seq = &actors->anim_seqs[2][animation];
        } else if (entity->entity_type == 10) {
            const __typeof__(objects->enemies[0]) *art =
                &objects->enemies[AOS_ENEMY_SKELETON_KNIGHT];
            if (!art->loaded || animation >= AOS_ENEMY_MAX_ANIMS ||
                !art->defs[animation].count)
                continue;
            row = art->frames[animation];
            ticks = art->ticks[animation];
            count = art->defs[animation].count;
            if (animation < AOS_OPENING_ACTOR_ANIMS)
                seq = &actors->emitted_seqs[animation];
        }
        if (!row) continue;
        unsigned index;
        if (seq && seq->ticks && seq->count) {
            index = entity->anim_subframe;
            if (index >= seq->count) index = seq->count - 1;
        } else {
            unsigned total = 0;
            for (unsigned i = 0; i < count; ++i) total += ticks[i];
            if (!total) continue;
            unsigned age = entity->animation_age % total;
            index = 0;
            while (index + 1 < count && age >= ticks[index])
                age -= ticks[index++];
        }
        const AriaFrame *frame = &row[index];
        if (!frame->texture) continue;
        int x = entity->x >> 16, y = entity->y >> 16;
        float draw_x = entity->mirrored
                           ? (float)(x - frame->offset_x) - frame->w
                           : (float)(x + frame->offset_x);
        SDL_FRect rect = {draw_x - (float)cam_x,
                          (float)(y + frame->offset_y - cam_y), frame->w,
                          frame->h};
        SDL_RenderTextureRotated(
            renderer, frame->texture, NULL, &rect, 0, NULL,
            entity->mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
    for (unsigned piece = 0; piece < actors->debris_count; ++piece) {
        const AosOpeningDebris *debris = &actors->debris[piece];
        if (!debris->active) continue;
        /* sub_0805E8C8/sub_0805E9F0 copy the dying entity's +0x5C band
         * onto each piece; every debris source here is a band-0x11
         * type-8 body. */
        int band_prio = aria_bg_band_priority(0x11);
        if (prio_slot >= 0 ? band_prio != prio_slot : band_prio >= 0)
            continue;
        const __typeof__(objects->enemies[0]) *art =
            &objects->enemies[debris->source_type == 8
                                  ? AOS_ENEMY_WINGED_SKELETON
                                  : AOS_ENEMY_SKELETON];
        const AriaFrame *frame = blade_frame(art, debris->frame);
        if (!frame || !frame->texture) continue;
        int x = debris->x >> 16, y = debris->y >> 16;
        float draw_x = debris->mirrored
                           ? (float)(x - frame->offset_x) - frame->w
                           : (float)(x + frame->offset_x);
        SDL_FRect rect = {draw_x - (float)cam_x,
                          (float)(y + frame->offset_y - cam_y), frame->w,
                          frame->h};
        SDL_RenderTextureRotated(
            renderer, frame->texture, NULL, &rect, 0, NULL,
            debris->mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
    const __typeof__(objects->effects[0]) *flame_art =
        &objects->effects[AOS_EFFECT_FLAME];
    unsigned flame_total = 0;
    for (unsigned frame = 0; frame < flame_art->def.count; ++frame)
        flame_total += flame_art->ticks[frame];
    if (flame_total) {
        for (unsigned index = 0; index < actors->flame_count; ++index) {
            const AosOpeningFlame *flame = &actors->flames[index];
            if (!flame->active) continue;
            int band_prio = aria_bg_band_priority(flame->band);
            if (prio_slot >= 0 ? band_prio != prio_slot
                               : band_prio >= 0)
                continue;
            unsigned age = flame->age % flame_total;
            unsigned frame = 0;
            while (frame + 1 < flame_art->def.count &&
                   age >= flame_art->ticks[frame])
                age -= flame_art->ticks[frame++];
            const AriaFrame *art = &flame_art->frames[frame];
            if (!art->texture) continue;
            int x = flame->x >> 16, y = flame->y >> 16;
            float draw_x = flame->mirrored
                               ? (float)(x - art->offset_x) - art->w
                               : (float)(x + art->offset_x);
            SDL_FRect rect = {draw_x - (float)cam_x,
                              (float)(y + art->offset_y - cam_y),
                              art->w, art->h};
            SDL_RenderTextureRotated(
                renderer, art->texture, NULL, &rect, 0, NULL,
                flame->mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
    }
    const __typeof__(objects->effects[0]) *projectile_art =
        &objects->effects[AOS_EFFECT_OPENING_PROJECTILE];
    unsigned projectile_total = 0;
    for (unsigned frame = 0; frame < projectile_art->def.count; ++frame)
        projectile_total += projectile_art->ticks[frame];
    if (projectile_total) {
        for (unsigned index = 0; index < actors->projectile_count; ++index) {
            const AosOpeningProjectile *projectile = &actors->projectiles[index];
            if (!projectile->active) continue;
            /* sub_0805ED80 stamps the bolt's +0x5C with 0x13. */
            int band_prio = aria_bg_band_priority(0x13);
            if (prio_slot >= 0 ? band_prio != prio_slot
                               : band_prio >= 0)
                continue;
            unsigned age = projectile->animation_age % projectile_total;
            unsigned frame = 0;
            while (frame + 1 < projectile_art->def.count &&
                   age >= projectile_art->ticks[frame])
                age -= projectile_art->ticks[frame++];
            const AriaFrame *art = &projectile_art->frames[frame];
            if (!art->texture) continue;
            SDL_FRect rect = {
                (float)((projectile->x >> 16) + art->offset_x - cam_x),
                (float)((projectile->y >> 16) + art->offset_y - cam_y),
                art->w, art->h};
            SDL_RenderTexture(renderer, art->texture, NULL, &rect);
        }
    }
    /* sub_08045B44 type-8 sparkles: the entry index is the +0x6E
     * analogue of the 0x08525194 sequence {0x49,0x36,0x35,0x34}, which
     * the exported soul_container_spark arts already store in order;
     * their {3,7,7,3} tick table plays the type-4 timing instead of
     * the native type-8 {2,4,4,2}, the same documented divergence the
     * soul-pickup trail carries. kind selects the palette bank. */
    for (unsigned index = 0; index < actors->spark_count; ++index) {
        const AosOpeningSpark *spark = &actors->sparks[index];
        if (!spark->active || spark->kind >= AOS_EFFECT_KINDS) continue;
        /* sub_08045B44 stamps the type-8 sparkle's +0x5C with 0x0C. */
        int band_prio = aria_bg_band_priority(0x0C);
        if (prio_slot >= 0 ? band_prio != prio_slot : band_prio >= 0)
            continue;
        const __typeof__(objects->effects[0]) *art =
            &objects->effects[spark->kind];
        if (spark->entry >= art->def.count) continue;
        const AriaFrame *frame = &art->frames[spark->entry];
        if (!frame->texture) continue;
        SDL_FRect rect = {
            (float)((spark->x >> 16) + frame->offset_x - cam_x),
            (float)((spark->y >> 16) + frame->offset_y - cam_y),
            frame->w, frame->h};
        SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
    }
}

void opening_actor_durations(
    const AriaObjects *objects, AosOpeningActors *actors,
    uint16_t durations[AOS_OPENING_ACTOR_COUNT]) {
    memset(durations, 0, sizeof(uint16_t) * AOS_OPENING_ACTOR_COUNT);
    if (!actors) return;
    for (int actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor) {
        int animation = actors->animation[actor];
        const __typeof__(objects->opening_actors[0]) *art =
            objects ? &objects->opening_actors[actor] : NULL;
        /* Rebind the flattened {entry,ticks} sequences the tick chain
         * in aos_opening_actors_update consumes. The runtime art stores
         * one rendered frame and one u8 duration per sequence entry, so
         * the entry index is the +0x65 analogue; binding every animation
         * slot (not just the current one) keeps actor_animation switches
         * covered. Entries without exported frames stay NULL so the
         * duration fallback applies; a missing objects library clears
         * every binding rather than leaving stale pointers. */
        for (unsigned anim = 0; anim < AOS_OPENING_ACTOR_ANIMS; ++anim) {
            AosOpeningAnimSeq *seq = &actors->anim_seqs[actor][anim];
            seq->ticks = NULL;
            seq->count = 0;
            if (!art || !art->loaded || anim >= OBJECT_MAX_ANIMS ||
                !art->defs[anim].count)
                continue;
            seq->ticks = art->ticks[anim];
            seq->count = art->defs[anim].count;
        }
        if (!art || !art->loaded || animation < 0 ||
            animation >= AOS_ENEMY_MAX_ANIMS)
            continue;
        unsigned total = 0;
        for (unsigned frame = 0; frame < art->defs[animation].count; ++frame)
            total += art->ticks[animation][frame];
        durations[actor] = total > UINT16_MAX ? UINT16_MAX : (uint16_t)total;
    }
    /* The emitted Skeleton Knight children (entity_type 10) tick
     * through the same chain against Enemy/skeleton_knight's exported
     * sequences; type-8 children reuse actor slot 2's anim_seqs
     * (Enemy/opening_actor_8) and need no binding here. Animations
     * past AOS_OPENING_ACTOR_ANIMS stay unbound and keep the age
     * fallback in both the tick step and the draw. */
    const __typeof__(objects->enemies[0]) *knight =
        objects ? &objects->enemies[AOS_ENEMY_SKELETON_KNIGHT] : NULL;
    for (unsigned anim = 0; anim < AOS_OPENING_ACTOR_ANIMS; ++anim) {
        AosOpeningAnimSeq *seq = &actors->emitted_seqs[anim];
        seq->ticks = NULL;
        seq->count = 0;
        if (!knight || !knight->loaded || anim >= AOS_ENEMY_MAX_ANIMS ||
            !knight->defs[anim].count)
            continue;
        seq->ticks = knight->ticks[anim];
        seq->count = knight->defs[anim].count;
    }
}

uint32_t opening_random(void *context) {
    (void)context;
    return aos_random();
}

int32_t opening_sine(uint32_t angle, void *context) {
    (void)context;
    return aos_sine(angle);
}

uint16_t opening_angle(int16_t x, int16_t y, void *context) {
    (void)context;
    return aos_arctan2(x, y);
}

int opening_floor_depth(int32_t x, int32_t y, void *context) {
    const AosCollision *layer = context;
    return layer ? aos_floor_depth(layer, x, y, 0, false) : 0;
}

int opening_wall_push(int32_t x, int32_t y, bool moving_left,
                             void *context) {
    const AosCollision *layer = context;
    if (!layer) return 0;
    return moving_left ? aos_wall_push_right(layer, x, y)
                       : aos_wall_push_left(layer, x, y);
}

/* Shared effects of one kind, flipped when + 0x58 bit 0x40 (drawn at
 * random) is set. Flames and smoke (priority + 0x5C or the creator's) are
 * drawn in front of the enemies, blood (priority 3) behind them; OBJ
 * blending is not modelled. */
static void draw_effect(SDL_Renderer *renderer, const AriaObjects *objects, const AosEffect *f,
                        int cam_x, int cam_y) {
    if (f->hidden || f->anim.frame >= objects->effects[f->kind].def.count) return;
    const AriaFrame *frame = &objects->effects[f->kind].frames[f->anim.frame];
    if (!frame->texture) return;
    int fx = f->x >> 16, fy = f->y >> 16;
    float x = f->mirrored ? (float)(fx - frame->offset_x) - frame->w : (float)(fx + frame->offset_x);
    SDL_FRect rect = {x - (float)cam_x, (float)(fy + frame->offset_y - cam_y), frame->w, frame->h};
    if (f->vflip) rect.y = (float)(fy - frame->offset_y - cam_y) - frame->h;
    /* + 0x59 bit 1: semi-transparent, half alpha standing for the blend. */
    if (f->translucent) SDL_SetTextureAlphaMod(frame->texture, 128);
    SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                             (SDL_FlipMode)((f->mirrored ? SDL_FLIP_HORIZONTAL : 0) |
                                            (f->vflip ? SDL_FLIP_VERTICAL : 0)));
    if (f->translucent) SDL_SetTextureAlphaMod(frame->texture, 255);
}

void draw_effects(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                         int kind, int cam_x, int cam_y) {
    for (size_t i = 0; i < room->effect_count; ++i)
        if (room->effects[i].kind == kind) draw_effect(renderer, objects, &room->effects[i], cam_x, cam_y);
    if (kind != AOS_EFFECT_FLAME) return;
    /* The glow of a Lightning Doll's piece (sub_080C9058), its own entity
     * in the game. */
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AosEnemy *e = &room->entities[i].enemy;
        if (room->entities[i].kind == ARIA_KIND_ENEMY && e->role == AOS_ROLE_DOLL_PIECE &&
            !e->removed && e->glow.started)
            draw_effect(renderer, objects, &e->glow, cam_x, cam_y);
    }
}

/* sub_080455BC's display: still frame 0x48 at the player's x and
 * y - 0x20, drawn through the sub_0803E058 affine slot of
 * sub_08045454 — rotation = 0x10000 - sine(frame * 0x400 + 0x4000)
 * turns for frames <= 0xF (else one full turn), uniform scale
 * 0x2000 for frames <= 7, 0x10000 - sine(frame * 0x800) for 8-15,
 * unit after; scale and rotation about the entity anchor. */
void draw_levelup(SDL_Renderer *renderer, const AriaObjects *objects,
                         const AosLevelUp *levelup, const AosSoma *soma,
                         int cam_x, int cam_y) {
    if (!levelup->active) return;
    const __typeof__(objects->effects[0]) *art = &objects->effects[AOS_EFFECT_LEVEL_UP];
    if (!art->def.count || !art->frames[0].texture) return;
    const AriaFrame *frame = &art->frames[0];
    /* The entity anchor: player x, player y - 0x20 (drawFunc
     * sub_08045454 writes the player record's + 0x42/+ 0x46 - 0x20). */
    float ax = (float)((soma->x >> 16) - cam_x);
    float ay = (float)(((soma->y >> 16) - 0x20) - cam_y);
    float scale = (float)aos_levelup_scale(levelup->env_frame, aos_sine) / 65536.0f;
    if (scale <= 0.0f) return;
    /* The unscaled top-left is anchor + frame offset; the affine scale
     * grows it about the anchor, so the scaled offset is offset*scale. */
    SDL_FRect rect = {ax + (float)frame->offset_x * scale,
                      ay + (float)frame->offset_y * scale,
                      (float)frame->w * scale, (float)frame->h * scale};
    double degrees =
        (double)aos_levelup_angle(levelup->env_frame, aos_sine) * 360.0 / 65536.0;
    /* The pivot is the entity anchor: (-offset * scale) from the
     * scaled top-left, in rect-relative units like the enemy spins. */
    SDL_FPoint pivot = {-(float)frame->offset_x * scale,
                        -(float)frame->offset_y * scale};
    SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, degrees,
                             &pivot, SDL_FLIP_NONE);
}

/* sub_0804301C: skip blank entries, drawing every visible glyph five
 * pixels after the previous one. The frame's native anchor still applies. */
void draw_damage_popup(SDL_Renderer *renderer, const AriaObjects *objects,
                              const AosDamagePopup *popup, int cam_x, int cam_y) {
    if (popup->hidden || !popup->active) return;
    if (popup->guard) {
        const AriaFrame *frame = &objects->damage_guard.frames[0];
        if (!frame->texture) return;
        SDL_FRect rect = {(float)((popup->x >> 16) + frame->offset_x - cam_x),
                          (float)((popup->y >> 16) + frame->offset_y - cam_y), frame->w, frame->h};
        SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
        return;
    }
    int x = popup->x >> 16;
    for (int i = 0; i < AOS_DAMAGE_POPUP_DIGITS; ++i) {
        uint8_t glyph = popup->glyphs[i];
        if (glyph == AOS_DAMAGE_POPUP_BLANK) continue;
        if (glyph >= AOS_DAMAGE_POPUP_GLYPHS) continue;
        const AriaFrame *frame = &objects->damage[glyph].frames[0];
        if (frame->texture) {
            SDL_FRect rect = {(float)(x + frame->offset_x - cam_x),
                              (float)((popup->y >> 16) + frame->offset_y - cam_y),
                              frame->w, frame->h};
            SDL_RenderTexture(renderer, frame->texture, NULL, &rect);
        }
        x += 5;
    }
}

/* ================= BG1-BG3 layered draw =================
 * The composite background.bmp flattens every decoded BG layer at
 * its static origin, so BG2/BG3 parallax (and the scanline /
 * hblank / affine modes sub_0800D288 reports through
 * AosScrollOut::fx) had no draw consumer even though the scroll
 * state kept the faithful bgOfs shadows. When a ROM is bound
 * (aria_bg_layers_bind — the runtime opens it for the room
 * dialogue and scroll tilemap) this path decodes the room's three
 * background tile arrays straight from the ROM — the same
 * fusion_aos_read_background_tiles + fusion_aos_render_background
 * pipeline the exporter composites — and keeps one SDL texture
 * per decoded layer. Each frame the layer is drawn at its own
 * gDisplayRegisters.bgOfs shadow, fed by a display-side
 * AosScroll that mirrors object20_scroll_ensure's room bind and
 * runs the same sub_0803FBBC request + sub_0800D288 apply chain
 * the native per-frame camera update runs, so parallax modes
 * track BG1 exactly like the hardware.
 *
 * Honest limits, matching the scroll port's documented gaps:
 *  - affine tile arrays now decode through the sub_0803F7C8
 *    4x4-block/8-bit-entry tiling (BG2 only, like the native
 *    routine); the draw runs the load-time identity transform
 *    plus the frame's winning sub_0800D288 affine program —
 *    mode 18's per-scanline hblank records and modes 21/27/32's
 *    per-frame BgAffineSet — CPU-warped through warp_tex with
 *    the BGxCNT area-overflow wrap honoured. Non-BG2 affine
 *    metadata still produces no texture.
 *  - the text-layer hblank/scanline wave modes update the
 *    layer's base bgOfs and draw per scanline through
 *    aria_bg_draw_hblank; the scanline/window FX writers are
 *    ported too — sub_0800CB8C (modes 15/23), sub_0800CC90
 *    (16/24/25), sub_0800CED4 (28/29), sub_0800CDAC (BG1 mode
 *    0x1A, the pre-loop emitter that wins only when no BG2/BG3
 *    mode claims the buffer) and the inline mode 20/30/31/36
 *    programs all evaluate into the same AosScrollHBlank ofs[]
 *    table (sub_0800D0F8 is mode 32's affine arm, consumed by
 *    the warp below). The CPU offset
 *    per row replaces the native hblank DMA register writes —
 *    documented, never approximated with invented data.
 * OBJ-vs-BG priority now follows the native tables: the room
 * load's sub_0803DD14/sub_0803FD60 chain turns each BG
 * record's field_0 layer byte into a BGxCNT priority (kept in
 * AriaBgRoomLayers.depth.bg_priority) and sub_0803DC18's unk_E
 * table maps an entity record's +0x5C draw band to the OBJ
 * priority sub_0803DE10 stamps into OAM (depth.band_priority,
 * exposed through aria_bg_band_priority). The runtime draws
 * back-to-front per priority level — BG layers first, then the
 * sprites whose retained band maps to it — so a sprite can sit
 * behind a foreground layer. Bands the port does not retain
 * (enemies, effects, pickups, skills, guardians, walls, most
 * specials — see PROJECT_STATUS) still draw in the front pass,
 * the same all-on-top order as before; Object20's persisted
 * +0x5C copies are animation-derived approximations
 * (aria_entities.c feeds soma->anim.id as player_anim5c), not
 * the native band chain, so they stay unbanded too.
 * Without a bound ROM, a failed decode, or zero decoded layers
 * aria_bg_room_begin returns false and the caller falls back
 * to the static composite — single-layer rooms are pixel-
 * identical either way. */

typedef struct {
    SDL_Texture *texture;   /* NULL: layer absent or undecoded */
    int width, height;      /* layer bitmap size in pixels */
    uint8_t rank;           /* background record field_0 — also
                               the sub_0803DD14 layer byte the
                               depth.bg_priority tables derive
                               from (kept for provenance) */
    bool enabled;           /* DISPCNT BG-enable bit at load */
    bool affine;            /* tilemap flags bit 0: BG2 affine
                               8-bit tilemap (sub_0803F7C8) */
    bool wrap;              /* BGxCNT bit 13 area-overflow: affine
                               texel coords wrap the map instead
                               of going transparent (background
                               record control bit 13) */
    /* CPU copy of the decoded RGBA for the affine warp — SDL
     * textures are not readable back, and the per-frame
     * BgAffineSet transform needs texel access. Affine layers
     * only (256x256x4 = 256 KiB); NULL for text layers. */
    uint8_t *affine_pixels;
} AriaBgLayerDraw;

typedef struct {
    int area, number;
    bool ready;             /* >=1 layer decoded for this room */
    AriaBgLayerDraw layer[3];   /* [0]=BG1 [1]=BG2 [2]=BG3 */
    /* Display-side scroll state: mirrors object20_scroll_ensure's
     * bind (aria_entities.c) but is fed the per-frame camera delta
     * every rendered frame — the entity transaction state there
     * only binds on the first sub_0800ED24 call and stays dormant
     * for rooms without Object20 scrolls. */
    AosScroll scroll;
    bool scroll_bound;
    int32_t prev_bg1_x, prev_bg1_y; /* last drawn BG1 offset (px) */
    uint32_t phase;         /* unk_60.unk_4C8 stand-in, +1/frame */
    /* gEwramData->frameCounter stand-in for the CB8C/CC90/31
     * wave phase terms: global like the native, never reset on a
     * room change (unk_4C8/phase is the one the transition
     * clears). */
    uint32_t frame_counter;
    /* This frame's winning sub_0800D288 hBlankEffect program
     * (aos_scroll_hblank): per-scanline absolute BGxHOFS/BGxVOFS
     * register values for the target BG, or target_bg 0. */
    AosScrollHBlank hblank;
    /* This frame's winning sub_0800D288 affine program
     * (aos_scroll_affine): the BgAffineSet parameters modes
     * 18/21/27/32 write to BG2PA..BG2PG — per-scanline records
     * for mode 18, one frame transform for 21/27/32 — or
     * target_bg 0. */
    AosScrollAffine affine;
    /* Streaming texture + pixel buffer the affine warp uploads
     * through (created lazily on the first warped frame). */
    SDL_Texture *warp_tex;
    uint8_t *warp_buf;
    bool bg2_disabled;      /* mode 19/21 dispCnt &= ~DCNT_BG2 */
    /* This room's sub_0803DC18/3DD14 depth tables: BGxCNT
     * priorities for BG0..BG3 and the +0x5C band -> OBJ priority
     * map sub_0803DE10 stamps into OAM. Computed once per room
     * from the background records' field_0 layer bytes. */
    AosDepthPriorities depth;
    bool depth_ok;
    int32_t ofs_bx, ofs_by; /* this frame's drawn BG1 offset */
} AriaBgRoomLayers;

static const FusionRom *aria_bg_rom;
static FusionAosAreaDirectory aria_bg_areas;
static bool aria_bg_areas_tried, aria_bg_areas_ok;
/* gEwramData +0x344 event bitfield — the same aria_events word
 * array aria_entities.c feeds sub_08012078/sub_08012048. Word 3
 * is unk_60.unk_33C[3], the flag word sub_0800D288's modes
 * 10/19/21 gates read (mode 21's 0x02000000 is event flag 0x79). */
extern uint32_t aria_events[AOS_WALL_EVENT_WORDS];
/* area/number start at -1 so the empty cache can never alias a
 * real room (0/0 is a valid room). */
static AriaBgRoomLayers aria_bg_room = { .area = -1, .number = -1 };

static void aria_bg_room_clear(void) {
    for (int i = 0; i < FUSION_AOS_BACKGROUND_COUNT; ++i) {
        if (aria_bg_room.layer[i].texture)
            SDL_DestroyTexture(aria_bg_room.layer[i].texture);
        aria_bg_room.layer[i].texture = NULL;
        aria_bg_room.layer[i].width = aria_bg_room.layer[i].height = 0;
        aria_bg_room.layer[i].rank = 0;
        aria_bg_room.layer[i].enabled = false;
        aria_bg_room.layer[i].affine = false;
        aria_bg_room.layer[i].wrap = false;
        free(aria_bg_room.layer[i].affine_pixels);
        aria_bg_room.layer[i].affine_pixels = NULL;
    }
    if (aria_bg_room.warp_tex)
        SDL_DestroyTexture(aria_bg_room.warp_tex);
    aria_bg_room.warp_tex = NULL;
    free(aria_bg_room.warp_buf);
    aria_bg_room.warp_buf = NULL;
    memset(&aria_bg_room.affine, 0, sizeof aria_bg_room.affine);
    memset(&aria_bg_room.scroll, 0, sizeof aria_bg_room.scroll);
    aria_bg_room.area = aria_bg_room.number = -1;
    aria_bg_room.ready = false;
    aria_bg_room.scroll_bound = false;
    aria_bg_room.prev_bg1_x = aria_bg_room.prev_bg1_y = 0;
    aria_bg_room.phase = 0;
    memset(&aria_bg_room.hblank, 0, sizeof aria_bg_room.hblank);
    aria_bg_room.bg2_disabled = false;
    memset(&aria_bg_room.depth, 0, sizeof aria_bg_room.depth);
    aria_bg_room.depth_ok = false;
    aria_bg_room.ofs_bx = aria_bg_room.ofs_by = 0;
}

/* Bind the ROM the layered draw decodes room backgrounds from.
 * `rom` is borrowed — it must outlive the runtime (the
 * room-dialogue ROM handle qualifies). NULL unbinds: the draw
 * path then always falls back to the composite. */
void aria_bg_layers_bind(const FusionRom *rom) {
    if (aria_bg_rom == rom) return;
    aria_bg_rom = rom;
    aria_bg_areas_tried = aria_bg_areas_ok = false;
    aria_bg_room_clear();
}

/* Decode the room's three background layers once per room and
 * bind the display-side scroll state. Returns false (caller falls
 * back to background.bmp) when no ROM is bound, the records fail
 * to read, or no layer decoded. */
static bool aria_bg_room_ensure(SDL_Renderer *renderer,
                                const AriaRoom *room,
                                int cam_x, int cam_y) {
    if (aria_bg_room.area == room->area &&
        aria_bg_room.number == room->number)
        return aria_bg_room.ready;
    aria_bg_room_clear();
    aria_bg_room.area = room->area;
    aria_bg_room.number = room->number;
    if (!renderer || !aria_bg_rom) return false;
    if (!aria_bg_areas_tried) {
        aria_bg_areas_tried = true;
        char error[160];
        aria_bg_areas_ok =
            fusion_aos_read_area_directory(aria_bg_rom, &aria_bg_areas,
                                           error, sizeof error);
    }
    if (!aria_bg_areas_ok || room->area < 0 ||
        room->area >= FUSION_AOS_AREA_COUNT || room->number < 0 ||
        (size_t)room->number >=
            aria_bg_areas.areas[room->area].room_count)
        return false;
    char error[160];
    FusionAosRoomDescriptor descriptor;
    FusionAosBackgroundList backgrounds;
    if (!fusion_aos_read_room_descriptor(
            aria_bg_rom, &aria_bg_areas, (size_t)room->area,
            (size_t)room->number, &descriptor, error, sizeof error) ||
        !fusion_aos_read_room_backgrounds(
            aria_bg_rom, &descriptor, &backgrounds, error, sizeof error))
        return false;
    /* sub_0800F9EC's depth setup: sub_0803DD14(layer, bg) per BG
     * record then sub_0803DD14(0, 0) for BG0 and sub_0803FD60 —
     * the room record's field_0 byte is the unk_0 layer value
     * (EwramData_unk_88_8). The port keeps the resulting
     * unk_A/unk_E tables so the draw can interleave OBJ and BG
     * by the hardware priorities instead of painting every
     * sprite over every layer. */
    {
        uint8_t layer_bytes[4] = { 0,
            backgrounds.entries[0].field_0,
            backgrounds.entries[1].field_0,
            backgrounds.entries[2].field_0 };
        aos_scroll_depth_priorities(layer_bytes, &aria_bg_room.depth);
        aria_bg_room.depth_ok = true;
    }
    FusionAosVideoMemory *memory = malloc(sizeof *memory);
    if (!memory) return false;
    bool mem_ok = fusion_aos_load_video_memory(
        aria_bg_rom, &descriptor, memory, error, sizeof error);
    if (!mem_ok) {
        free(memory);
        return false;
    }
    for (int i = 0; i < FUSION_AOS_BACKGROUND_COUNT; ++i) {
        AriaBgLayerDraw *layer = &aria_bg_room.layer[i];
        const FusionAosBackground *rec = &backgrounds.entries[i];
        layer->rank = rec->field_0;
        layer->affine = false;
        layer->wrap = false;
        /* BGxCNT bit 13 (the record's control halfword): affine
         * area-overflow — out-of-map texel coords wrap instead of
         * sampling transparent. */
        layer->wrap = (rec->control & 0x2000) != 0;
        /* DISPCNT bits 9..11 enable BG1..BG3 (DCNT_BG2 = 0x0400);
         * display_control is sub_0800F9EC's gUnk_03002CB0.dispCnt
         * seed. */
        layer->enabled =
            (descriptor.display_control >> (9 + i)) & 1;
        FusionAosBackgroundTilemap tilemap;
        memset(&tilemap, 0, sizeof tilemap);
        if (!fusion_aos_read_background_tiles(aria_bg_rom, rec,
                                              &tilemap, error,
                                              sizeof error))
            continue;
        if (tilemap.status == FUSION_AOS_TILEMAP_DECODED) {
            layer->affine = (tilemap.flags & 1) != 0;
            FusionAosRenderedBackground rendered;
            memset(&rendered, 0, sizeof rendered);
            if (fusion_aos_render_background(rec, &tilemap, memory,
                                             &rendered, error,
                                             sizeof error)) {
                SDL_Texture *tex = SDL_CreateTexture(
                    renderer, SDL_PIXELFORMAT_RGBA32,
                    SDL_TEXTUREACCESS_STATIC,
                    (int)rendered.width_pixels,
                    (int)rendered.height_pixels);
                if (tex) {
                    SDL_UpdateTexture(
                        tex, NULL, rendered.rgba,
                        (int)(rendered.width_pixels * 4));
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(tex,
                                            SDL_SCALEMODE_NEAREST);
                    layer->texture = tex;
                    layer->width = (int)rendered.width_pixels;
                    layer->height = (int)rendered.height_pixels;
                    aria_bg_room.ready = true;
                    if (layer->affine && rendered.rgba) {
                        /* The affine warp samples texels per
                         * frame; SDL textures are write-only, so
                         * keep the decoded RGBA beside them. */
                        layer->affine_pixels = malloc(
                            rendered.pixel_count * 4);
                        if (layer->affine_pixels)
                            memcpy(layer->affine_pixels, rendered.rgba,
                                   rendered.pixel_count * 4);
                    }
                }
                fusion_aos_free_rendered_background(&rendered);
            }
        }
        fusion_aos_free_background_tilemap(&tilemap);
    }
    free(memory);
    if (!aria_bg_room.ready) return false;

    /* Display-side scroll bind, mirroring object20_scroll_ensure
     * (aria_entities.c): BG1 keeps the room screen counts as its
     * pBgMetadata->unk_0/unk_1 gates and the camera position; BG2/
     * BG3 bind their full records with the sub_0800ED5C/sub_0800EE54
     * initial positions (the incoming scroll approximated by the
     * camera, y minus 0x30). The room-load tail — sub_0800F9EC's
     * dispCnt store plus the first sub_0800D288 with BG1 velocity
     * still 0 — runs once through aos_scroll_apply so the
     * meta<=1 pins, the mode 8/9/10 direct bgOfs writes and the
     * mode 19/21 DCNT_BG2 clear land exactly like the native load. */
    memset(&aria_bg_room.scroll, 0, sizeof aria_bg_room.scroll);
    const FusionAosBackground *bg1 = &backgrounds.entries[0];
    aos_scroll_bind(&aria_bg_room.scroll, 1,
                    (uint8_t)room->width_screens,
                    (uint8_t)room->height_screens,
                    bg1->field_1, bg1->field_4, bg1->field_6,
                    (int32_t)cam_x << 16, (int32_t)cam_y << 16);
    for (int bg = 2; bg < 4; ++bg) {
        const FusionAosBackground *rec = &backgrounds.entries[bg - 1];
        uint8_t mode = rec->field_1;
        uint16_t range_x = rec->field_4;
        uint16_t range_y = rec->field_6;
        uint8_t meta_0 = rec->has_dimensions ? rec->width_screens : 0;
        uint8_t meta_1 = rec->has_dimensions ? rec->height_screens : 0;
        uint16_t origin_x = aos_scroll_origin_x(
            mode, meta_0, range_x, aria_bg_room.scroll.bg[1].unk_14,
            (uint16_t)cam_x);
        uint16_t origin_y = aos_scroll_origin_y(
            mode, meta_1, range_y, aria_bg_room.scroll.bg[1].unk_16,
            (uint16_t)(cam_y - 0x30));
        aos_scroll_bind(&aria_bg_room.scroll, bg, meta_0, meta_1,
                        mode, range_x, range_y,
                        (int32_t)origin_x << 16,
                        (int32_t)origin_y << 16);
    }
    {
        AosScrollOut load_out;
        memset(&load_out, 0, sizeof load_out);
        aos_scroll_apply(&aria_bg_room.scroll, NULL, &load_out);
        if (load_out.fx & AOS_SCROLL_FX_BG2_DISABLE)
            aria_bg_room.bg2_disabled = true;
    }
    aria_bg_room.prev_bg1_x = cam_x;
    aria_bg_room.prev_bg1_y = cam_y;
    aria_bg_room.scroll_bound = true;
    if (getenv("ARIA_BG_DEBUG"))
        fprintf(stderr, "aria_bg: room %d/%d layers=%d,%d,%d modes=%u,%u,%u disp2=(%u,%u) disp3=(%u,%u)\n",
                room->area, room->number,
                aria_bg_room.layer[0].texture != NULL,
                aria_bg_room.layer[1].texture != NULL,
                aria_bg_room.layer[2].texture != NULL,
                backgrounds.entries[0].field_1, backgrounds.entries[1].field_1,
                backgrounds.entries[2].field_1,
                aria_bg_room.scroll.disp_h[2], aria_bg_room.scroll.disp_v[2],
                aria_bg_room.scroll.disp_h[3], aria_bg_room.scroll.disp_v[3]);
    return true;
}

/* Begin the layered BG pass for this frame: ensures the room's
 * decoded layers (aria_bg_room_ensure) and feeds this frame's
 * BG1 motion — the camera delta plus the transaction delta —
 * through the same request/apply chain the native per-frame
 * camera update runs, so the BG2/BG3 parallax modes
 * (request-scaled, interpolated, direct-write) track BG1
 * exactly. unk_60.unk_4C8 ticks once per rendered frame like
 * aria_scroll_frame_phase. Returns false when the layered path
 * is unavailable — the caller then draws the composite
 * background.bmp exactly as before. `bg1_ox`/`bg1_oy` is the
 * accumulated sub_0800ED24 transaction delta
 * (aria_scroll_bg1_offset). */
bool aria_bg_room_begin(SDL_Renderer *renderer, const AriaRoom *room,
                        int cam_x, int cam_y,
                        int32_t bg1_ox, int32_t bg1_oy) {
    if (!aria_bg_room_ensure(renderer, room, cam_x, cam_y))
        return false;
    ++aria_bg_room.frame_counter;
    const int32_t bx = (int32_t)cam_x + bg1_ox;
    const int32_t by = (int32_t)cam_y + bg1_oy;
    const int32_t dx = bx - aria_bg_room.prev_bg1_x;
    const int32_t dy = by - aria_bg_room.prev_bg1_y;
    aria_bg_room.prev_bg1_x = bx;
    aria_bg_room.prev_bg1_y = by;
    ++aria_bg_room.phase;
    /* unk_60.unk_33C[3] is the gEwramData +0x344 event bitfield's
     * word 3 — aria_events[3] (mode 21's 0x02000000 gate is event
     * flag 0x79); unk_4CC_0 stays unmodelled like the entity-side
     * world. room_index feeds mode 32's GetRoomFromMapPosition
     * switch. */
    AosScrollWorld world = { aria_events[3], false,
                             aria_bg_room.phase, NULL, 0,
                             aria_bg_room.number };
    AosScrollOut out;
    memset(&out, 0, sizeof out);
    aos_scroll_request(&aria_bg_room.scroll, 1,
                       (int32_t)((int64_t)dx << 16),
                       (int32_t)((int64_t)dy << 16), NULL);
    /* Resync BG1's own record onto the drawn position: the request
     * integrated last frame's pos and the draw offset is the
     * authoritative bgOfs[1] anyway. */
    aria_bg_room.scroll.bg[1].x_pos = (int32_t)((int64_t)bx << 16);
    aria_bg_room.scroll.bg[1].y_pos = (int32_t)((int64_t)by << 16);
    aria_bg_room.scroll.disp_h[1] = (uint16_t)bx;
    aria_bg_room.scroll.disp_v[1] = (uint16_t)by;
    aos_scroll_apply(&aria_bg_room.scroll, &world, &out);
    if (out.fx & AOS_SCROLL_FX_BG2_DISABLE)
        aria_bg_room.bg2_disabled = true;
    /* The hBlankEffect program sub_0800D288 armed this frame:
     * evaluated after apply so the bgOfs shadows the wave
     * routines fold in as their base are final. The affine
     * winner (mode 18) is reported through hblank.affine and
     * simply suppresses the flat-wave draw below. */
    aos_scroll_hblank(&aria_bg_room.scroll, &world,
                      aria_bg_room.frame_counter,
                      &aria_bg_room.hblank);
    if (aria_bg_room.hblank.target_bg && getenv("ARIA_BG_DEBUG"))
        fprintf(stderr, "aria_bg: hblank program bg=%d %s frame=%u\n",
                aria_bg_room.hblank.target_bg,
                aria_bg_room.hblank.affine ? "affine"
                : aria_bg_room.hblank.vertical ? "vOfs" : "hOfs",
                aria_bg_room.frame_counter);
    /* The affine sibling of the hblank eval: modes 18/21/27/32's
     * BgAffineSet parameters for this frame, consumed by the
     * affine-layer warp in aria_bg_draw_priority. Evaluated after
     * apply like the hblank pass; hblank.affine decides whether a
     * mode-18 program actually reached the screen (it loses the
     * single hBlankEffect buffer to a later wave emitter). */
    aos_scroll_affine(&aria_bg_room.scroll, &world,
                      &aria_bg_room.affine);
    if (aria_bg_room.affine.target_bg && getenv("ARIA_BG_DEBUG"))
        fprintf(stderr, "aria_bg: affine program bg=%d %s%s frame=%u\n",
                aria_bg_room.affine.target_bg,
                aria_bg_room.affine.per_scanline ? "per-scanline " : "",
                aria_bg_room.affine.has_frame ? "+frame" : "",
                aria_bg_room.frame_counter);
    aria_bg_room.ofs_bx = bx;
    aria_bg_room.ofs_by = by;
    return true;
}

/* Draw one layer under the frame's winning hBlankEffect program:
 * the ISR rewrites the target BG's BGxHOFS (hblank.vertical =
 * false) or BGxVOFS (true) every scanline, so screen row y
 * samples map column (h_ofs[y] + x) and row (v_ofs[y] + y),
 * wrapped at the layer bitmap size like the flat blit's
 * bgOfs wrap. ofs[] already carries each routine's absolute
 * register value; the untouched axis keeps the layer's bgOfs
 * shadow. Emitted as 1-px-high strips (<= 2 SDL draws per row
 * for the horizontal wrap) — the same modulo-and-continue
 * segmentation aos_scroll_bgofs_blits uses, per scanline. */
static void aria_bg_draw_hblank(SDL_Renderer *renderer,
                                const AriaBgLayerDraw *layer, int bg,
                                const AosScrollHBlank *hb) {
    const AosScroll *sc = &aria_bg_room.scroll;
    for (int y = 0; y < VIEW_H; ++y) {
        int32_t h = hb->vertical ? (int32_t)sc->disp_h[bg]
                                 : (int32_t)hb->ofs[y];
        int32_t v = hb->vertical ? (int32_t)hb->ofs[y]
                                 : (int32_t)sc->disp_v[bg];
        int sx = (int)(((int64_t)h % layer->width + layer->width) %
                       layer->width);
        int sy = (int)(((int64_t)(v + y) % layer->height +
                        layer->height) % layer->height);
        if (sx + VIEW_W <= layer->width) {
            SDL_FRect src = {(float)sx, (float)sy, (float)VIEW_W, 1.0f};
            SDL_FRect dst = {0.0f, (float)y, (float)VIEW_W, 1.0f};
            SDL_RenderTexture(renderer, layer->texture, &src, &dst);
            continue;
        }
        /* Horizontal wrap: the row continues at source 0 like
         * the tilemap wrap (map width >= 256 > VIEW_W, so at most
         * one split is ever needed). */
        int w0 = layer->width - sx;
        SDL_FRect src0 = {(float)sx, (float)sy, (float)w0, 1.0f};
        SDL_FRect dst0 = {0.0f, (float)y, (float)w0, 1.0f};
        SDL_RenderTexture(renderer, layer->texture, &src0, &dst0);
        SDL_FRect src1 = {0.0f, (float)sy,
                          (float)(VIEW_W - w0), 1.0f};
        SDL_FRect dst1 = {(float)w0, (float)y,
                          (float)(VIEW_W - w0), 1.0f};
        SDL_RenderTexture(renderer, layer->texture, &src1, &dst1);
    }
}

/* CPU warp of an affine BG layer through this frame's winning
 * sub_0800D288 affine program (aria_bg_room.affine, evaluated in
 * begin()). The native writes BGxPA..BG2PG — per scanline for
 * mode 18's hBlankEffect records, once per frame for modes
 * 21/27/32 — and the hardware then samples
 *   texel = ((PH + PA*x + PB*y) >> 8, (PG + PC*x + PD*y) >> 8)
 * per screen pixel; the port reproduces that fetch texel-by-texel
 * into warp_buf (VIEW_W x VIEW_H RGBA) and streams it through
 * warp_tex. Program selection mirrors the hardware precedence:
 * a mode-18 program only reaches the screen when it won the
 * hBlankEffect buffer (hblank.affine — a later wave emitter on
 * the other BG overwrites it natively); when it lost, any
 * per-frame write (has_frame) is what stays in the registers,
 * and with neither the load-time identity transform applies —
 * the caller's static draw. Out-of-map texels sample transparent
 * unless BGxCNT bit 13 (layer->wrap, area overflow) wraps them.
 * Returns false when no affine program applies or the warp
 * buffers could not be allocated — the caller keeps the static
 * frame-0 draw. */
static bool aria_bg_draw_affine_warp(SDL_Renderer *renderer,
                                     AriaBgLayerDraw *layer, int bg,
                                     uint8_t fade_mod) {
    const AosScrollAffine *af = &aria_bg_room.affine;
    if (bg != af->target_bg || !layer->affine_pixels)
        return false;
    const AosScrollAffineSample *scan = NULL;
    const AosScrollAffineSample *frame = NULL;
    if (af->per_scanline) {
        if (aria_bg_room.hblank.affine)
            scan = af->scan;          /* row y uses record y */
        else if (af->has_frame)
            frame = &af->frame;       /* lost the buffer: the
                                          per-frame write stands */
        else
            return false;             /* neither: load-time
                                          identity stays */
    } else {
        frame = &af->frame;
    }
    if (!aria_bg_room.warp_buf) {
        aria_bg_room.warp_buf =
            malloc((size_t)VIEW_W * VIEW_H * 4);
        if (!aria_bg_room.warp_buf) return false;
    }
    if (!aria_bg_room.warp_tex) {
        aria_bg_room.warp_tex = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, VIEW_W, VIEW_H);
        if (!aria_bg_room.warp_tex) return false;
        SDL_SetTextureBlendMode(aria_bg_room.warp_tex,
                                SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(aria_bg_room.warp_tex,
                                SDL_SCALEMODE_NEAREST);
    }
    const int mw = layer->width, mh = layer->height;
    const uint8_t *src = layer->affine_pixels;
    uint8_t *dst = aria_bg_room.warp_buf;
    for (int y = 0; y < VIEW_H; ++y) {
        const AosScrollAffineSample *sm =
            scan ? &scan[y] : frame;
        for (int x = 0; x < VIEW_W; ++x) {
            int32_t tx, ty;
            aos_scroll_affine_texel(sm, x, y, &tx, &ty);
            const uint8_t *px = NULL;
            if (layer->wrap) {
                /* BGxCNT area overflow: texel coords wrap the
                 * map like the native tilemap fetch. */
                tx %= mw; if (tx < 0) tx += mw;
                ty %= mh; if (ty < 0) ty += mh;
                px = src + ((size_t)ty * mw + tx) * 4;
            } else if (tx >= 0 && tx < mw && ty >= 0 && ty < mh) {
                px = src + ((size_t)ty * mw + tx) * 4;
            }
            /* Out-of-map texels are transparent on hardware. */
            if (px) memcpy(dst, px, 4);
            else memset(dst, 0, 4);
            dst += 4;
        }
    }
    SDL_UpdateTexture(aria_bg_room.warp_tex, NULL, aria_bg_room.warp_buf,
                      VIEW_W * 4);
    SDL_SetTextureColorMod(aria_bg_room.warp_tex, fade_mod, fade_mod,
                           fade_mod);
    SDL_FRect dst_rect = {0.0f, 0.0f, (float)VIEW_W, (float)VIEW_H};
    SDL_RenderTexture(renderer, aria_bg_room.warp_tex, NULL,
                      &dst_rect);
    return true;
}

/* Draw the decoded BG layers whose sub_0803FD60 priority equals
 * `priority` (0 front .. 3 back), highest BG number first — the
 * hardware's same-priority BG tie-break (lower BG number wins,
 * so the higher one paints first). Only layers that decoded,
 * are DISPCNT-enabled and were not killed by the DCNT_BG2 clear
 * draw; BG1 samples the begin()'d camera+delta offset, BG2/BG3
 * their faithful bgOfs shadows. `fade_mod` is the sub_0800C8C8
 * BG fade the composite path already applies. No-op when the
 * layered path is not bound. */
void aria_bg_draw_priority(SDL_Renderer *renderer, int priority,
                           uint8_t fade_mod) {
    if (!aria_bg_room.ready || !aria_bg_room.depth_ok || !renderer)
        return;
    for (int bg = FUSION_AOS_BACKGROUND_COUNT; bg >= 1; --bg) {
        AriaBgLayerDraw *layer = &aria_bg_room.layer[bg - 1];
        if (!layer->texture || !layer->enabled ||
            aria_bg_room.depth.bg_priority[bg] != (uint8_t)priority ||
            (bg == 2 && aria_bg_room.bg2_disabled))
            continue;
        /* bgOfs shadow: BG1 is the drawn camera+delta position;
         * BG2/BG3 are the scroll state's faithful integer offsets
         * (absolute positions in each layer's own map space). */
        SDL_SetTextureColorMod(layer->texture, fade_mod, fade_mod,
                               fade_mod);
        if (layer->affine) {
            /* Affine BG (BG2 only, sub_0803F7C8 + the load-time
             * BgAffineSet): the hardware ignores bgOfs — the
             * transform is texX=texY=0x8000, scr=(0x78,0x50),
             * unit scale, alpha 0, i.e. screen pixel (x,y) samples
             * texel (x+8, y+48) of the 256x256 tilemap. When this
             * frame's sub_0800D288 affine program targets BG2 the
             * warp consumer reproduces it: mode 18's per-scanline
             * hblank records, or modes 21/27/32's per-frame
             * BgAffineSet. Rooms with no affine program — and the
             * mode-18-lost-the-buffer case with no per-frame
             * write — keep this static frame-0 transform, the
             * documented approximation. */
            if (aria_bg_draw_affine_warp(renderer, layer, bg,
                                         fade_mod))
                continue;
            SDL_FRect src = {8.0f, 48.0f, (float)VIEW_W,
                             (float)VIEW_H};
            SDL_FRect dst = {0.0f, 0.0f, (float)VIEW_W,
                             (float)VIEW_H};
            SDL_RenderTexture(renderer, layer->texture, &src, &dst);
            continue;
        }
        /* The frame's hBlankEffect program, when this layer is its
         * target: per-scanline BGxHOFS/BGxVOFS strips. hblank.affine
         * (mode 18's sub_0800D000 per-scanline BgAffineSet) and
         * affine tilemaps (bgOfs ignored by the hardware) fall
         * through to the flat draw — documented, never approximated
         * with invented transform data. */
        if (aria_bg_room.hblank.target_bg == bg && !layer->affine &&
            !aria_bg_room.hblank.affine) {
            aria_bg_draw_hblank(renderer, layer, bg,
                                &aria_bg_room.hblank);
            continue;
        }
        int32_t h_ofs, v_ofs;
        if (bg == 1) {
            h_ofs = aria_bg_room.ofs_bx; v_ofs = aria_bg_room.ofs_by;
        } else {
            h_ofs = aria_bg_room.scroll.disp_h[bg];
            v_ofs = aria_bg_room.scroll.disp_v[bg];
        }
        AosScrollBlit blits[4];
        size_t count = aos_scroll_bgofs_blits(
            h_ofs, v_ofs, layer->width, layer->height,
            VIEW_W, VIEW_H, blits);
        for (size_t b = 0; b < count; ++b) {
            SDL_FRect src = {(float)blits[b].src_x,
                             (float)blits[b].src_y,
                             (float)blits[b].w,
                             (float)blits[b].h};
            SDL_FRect dst = {(float)blits[b].dst_x,
                             (float)blits[b].dst_y,
                             (float)blits[b].w,
                             (float)blits[b].h};
            SDL_RenderTexture(renderer, layer->texture, &src, &dst);
        }
    }
}

/* The OBJ priority an entity record +0x5C draw band maps to
 * through this room's sub_0803DC18/unk_E table (the value
 * sub_0803DE10 stamps into the OAM attribute-2 priority bits),
 * or -1 when the layered path is unavailable or the band is
 * outside the native 0x00-0x1F range — the caller then keeps
 * the sprite in the front pass like every unbanded draw. */
int aria_bg_band_priority(uint8_t band) {
    if (!aria_bg_room.ready || !aria_bg_room.depth_ok || band >= 0x20)
        return -1;
    return aria_bg_room.depth.band_priority[band];
}
