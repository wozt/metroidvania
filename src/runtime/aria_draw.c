/* SPDX-License-Identifier: GPL-3.0-only */
/* World-draw cluster of the Aria of Sorrow room runtime, moved verbatim
 * out of aria_runtime.c (AGENTS.md rule 8). See aria_draw.h. The shared
 * variant_keys table lives in aria_tables.c; static_index stays in
 * aria_runtime.c (the soul/skill paths there use it too) and is declared
 * in aria_draw.h. */
#include "aria_draw.h"

#include "aria_tables.h"

#include "aos/core/aos_collision.h"
#include "aos/math/aos_math.h"

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
            float x = flip ? (float)(e->x - frame->offset_x) - frame->w
                           : (float)(e->x + frame->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(e->y + frame->offset_y - cam_y), frame->w,
                              frame->h};
            SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                                     flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
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

/* Enemies: sprites face left; mirrored ones are flipped around x. */
void draw_enemies(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                         int cam_x, int cam_y) {
    for (size_t n = 0; n < 4 * room->entity_count; ++n) {
        size_t i = n % room->entity_count;
        const AriaEntity *e = &room->entities[i];
        if (enemy_pass(&e->enemy) != n / room->entity_count) continue;
        if (!e->spawned || e->kind != ARIA_KIND_ENEMY || e->id >= ENEMY_KINDS ||
            e->enemy.removed || e->enemy.hidden || e->enemy.anim.id >= AOS_ENEMY_MAX_ANIMS ||
            e->enemy.anim.frame >= MAX_FRAMES)
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

static void draw_object20_entity(SDL_Renderer *renderer,
                                 const AriaObjects *objects,
                                 const AriaEntity *e, int cam_x, int cam_y,
                                 unsigned step) {
    /* The whiteout bars are screen-space (no camera) and draw
     * under this entity's own sprite. */
    draw_object20_wo_bars(renderer, e);
    draw_object20_wo_bursts(renderer, objects, e, step);
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

void draw_special_objects(SDL_Renderer *renderer, const AriaRoom *room,
                                 const AriaObjects *objects, int cam_x, int cam_y,
                                 unsigned step) {
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL || e->id < 0 || e->id >= 64)
            continue;
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
 * their fixed-point positions; the later callback branches remain a follow-up. */
void draw_opening_actors(SDL_Renderer *renderer, const AriaObjects *objects,
                                const AosOpeningActors *actors, int cam_x, int cam_y) {
    if (!actors) return;
    for (int actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor) {
        if (!actors->visible[actor]) continue;
        const __typeof__(objects->opening_actors[0]) *art = &objects->opening_actors[actor];
        int animation = actors->animation[actor];
        if (!art->loaded || animation >= AOS_ENEMY_MAX_ANIMS || !art->defs[animation].count)
            continue;
        unsigned total = 0;
        for (unsigned i = 0; i < art->defs[animation].count; ++i)
            total += art->ticks[animation][i];
        if (!total) continue;
        unsigned t = (unsigned)(actors->animation_age[actor] % total), index = 0;
        while (index + 1 < art->defs[animation].count && t >= art->ticks[animation][index])
            t -= art->ticks[animation][index++];
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
        const AriaFrame *frame = NULL;
        unsigned total = 0, count = 0;
        const uint8_t *ticks = NULL;
        if (entity->entity_type == 8) {
            const __typeof__(objects->opening_actors[0]) *art =
                &objects->opening_actors[2];
            if (!art->loaded || entity->animation >= AOS_ENEMY_MAX_ANIMS)
                continue;
            count = art->defs[entity->animation].count;
            ticks = art->ticks[entity->animation];
            for (unsigned i = 0; i < count; ++i) total += ticks[i];
            if (!total) continue;
            unsigned age = entity->animation_age % total, index = 0;
            while (index + 1 < count && age >= ticks[index])
                age -= ticks[index++];
            frame = &art->frames[entity->animation][index];
        } else if (entity->entity_type == 10) {
            const __typeof__(objects->enemies[0]) *art =
                &objects->enemies[AOS_ENEMY_SKELETON_KNIGHT];
            if (!art->loaded || entity->animation >= AOS_ENEMY_MAX_ANIMS)
                continue;
            count = art->defs[entity->animation].count;
            ticks = art->ticks[entity->animation];
            for (unsigned i = 0; i < count; ++i) total += ticks[i];
            if (!total) continue;
            unsigned age = entity->animation_age % total, index = 0;
            while (index + 1 < count && age >= ticks[index])
                age -= ticks[index++];
            frame = &art->frames[entity->animation][index];
        }
        if (!frame || !frame->texture) continue;
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
}

void opening_actor_durations(
    const AriaObjects *objects, const AosOpeningActors *actors,
    uint16_t durations[AOS_OPENING_ACTOR_COUNT]) {
    memset(durations, 0, sizeof(uint16_t) * AOS_OPENING_ACTOR_COUNT);
    if (!objects || !actors) return;
    for (int actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor) {
        int animation = actors->animation[actor];
        const __typeof__(objects->opening_actors[0]) *art =
            &objects->opening_actors[actor];
        if (!art->loaded || animation < 0 || animation >= AOS_ENEMY_MAX_ANIMS)
            continue;
        unsigned total = 0;
        for (unsigned frame = 0; frame < art->defs[animation].count; ++frame)
            total += art->ticks[animation][frame];
        durations[actor] = total > UINT16_MAX ? UINT16_MAX : (uint16_t)total;
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
