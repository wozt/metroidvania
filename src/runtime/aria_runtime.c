/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental Aria of Sorrow room runtime: Soma in one native room.
 *
 * Loads a room exported by fusion_importer (AOSROOM-NATIVE
 * collision bytes and background.bmp) and Soma's sprite library from
 * fusion_importer --aria-export-soma, then runs the ported player frame
 * aos_soma_update at 60 Hz. The library's animation_NNN keys are the
 * native animation descriptor indices, and their durations drive the
 * animation timing. Room transitions and a portable frontend save can select
 * the active room; complete inventory, soul and native SRAM parity remain
 * separate reconstruction work. */
#include "fusion_host.h"
#include "aos/core/aos_audio.h"
#include "aos/core/aos_mp2k.h"
#include "aos/core/aos_room_music.h"
#include "aos/game/aos_door.h"
#include "aos/game/aos_object01.h"
#include "aos/game/aos_object02.h"
#include "aos/game/aos_object03.h"
#include "aos/game/aos_object04.h"
#include "aos/game/aos_object05.h"
#include "aos/game/aos_object06.h"
#include "aos/game/aos_object07.h"
#include "aos/game/aos_object0c.h"
#include "aos/game/aos_object0f.h"
#include "aos/game/aos_object0d.h"
#include "aos/game/aos_object0e.h"
#include "aos/game/aos_object10.h"
#include "aos/game/aos_object11.h"
#include "aos/game/aos_object12.h"
#include "aos/game/aos_object13.h"
#include "aos/game/aos_object14.h"
#include "aos/game/aos_object19.h"
#include "aos/game/aos_object1a.h"
#include "aos/game/aos_object1b.h"
#include "aos/game/aos_object1e.h"
#include "aos/game/aos_object15.h"
#include "aos/game/aos_object16.h"
#include "aos/game/aos_object24.h"
#include "aos/game/aos_object26.h"
#include "aos/game/aos_object27.h"
#include "aos/game/aos_object2a.h"
#include "aos/game/aos_object2b.h"
#include "aos/game/aos_object23.h"
#include "aos/game/aos_object2d.h"
#include "aos/game/aos_object2e.h"
#include "aos/game/aos_object2f.h"
#include "aos/game/aos_object36.h"
#include "aos/game/aos_object34.h"
#include "aos/game/aos_object2c.h"
#include "aos/game/aos_object17.h"
#include "aos/game/aos_object18.h"
#include "aos/game/aos_object30.h"
#include "aos/game/aos_object21.h"
#include "aos/game/aos_object25.h"
#include "aos/game/aos_object31.h"
#include "aos/game/aos_object33.h"
#include "aos/game/aos_object22.h"
#include "aos/game/aos_object29.h"
#include "aos/game/aos_object32.h"
#include "aos/game/aos_object35.h"
#include "aos/game/aos_object37.h"
#include "aos/game/aos_object1d.h"
#include "aos/game/aos_object1f.h"
#include "aos/game/aos_object1c.h"
#include "aos/game/aos_object28.h"
#include "aos/game/aos_object20.h"
#include "aos/game/aos_room_dialogue.h"
#include "aos/core/aos_wall.h"
#include "aos/core/aos_enemy.h"
#include "aos/game/aos_game_lifecycle.h"
#include "aos/intro/aos_intro_font.h"
#include "aos/intro/aos_intro_resources.h"
#include "aos/title/aos_opening_event.h"
#include "aos/menus/aos_pause_graphics.h"
#include "aos/menus/aos_portrait.h"
#include "aos/menus/aos_quick_map.h"
#include "aos/game/aos_room.h"
#include "aos/game/aos_room_overlay.h"
#include "aos/game/aos_save.h"
#include "aos/core/aos_stats.h"
#include "aos/game/aos_pickup.h"
#include "aos/souls/aos_soul.h"
#include "aos/souls/aos_soul_notice.h"
#include "aos/souls/aos_soul_menu.h"
#include "aos/red_souls/aos_soul_red_axe.h"
#include "aos/red_souls/aos_soul_red_crow.h"
#include "aos/red_souls/aos_soul_red_dagger.h"
#include "aos/red_souls/aos_soul_red_flea.h"
#include "aos/red_souls/aos_soul_red_fish_head.h"
#include "aos/red_souls/aos_soul_red_killer_fish.h"
#include "aos/red_souls/aos_soul_red_gladiator.h"
#include "aos/red_souls/aos_soul_red_werewolf.h"
#include "aos/red_souls/aos_soul_red_killer_mantle.h"
#include "aos/red_souls/aos_soul_red_biphron.h"
#include "aos/red_souls/aos_soul_red_kyoma_demon.h"
#include "aos/red_souls/aos_soul_red_student_witch.h"
#include "aos/red_souls/aos_soul_red_slime.h"
#include "aos/red_souls/aos_soul_red_ukoback.h"
#include "aos/red_souls/aos_soul_red_altair.h"
#include "aos/red_souls/aos_soul_red_une.h"
#include "aos/red_souls/aos_soul_red_mandragora.h"
#include "aos/red_souls/aos_soul_red_chronomage.h"
#include "aos/red_souls/aos_soul_red_disc_armor.h"
#include "aos/red_souls/aos_soul_red_cockatrice.h"
#include "aos/red_souls/aos_soul_red_dryad.h"
#include "aos/red_souls/aos_soul_red_arachne.h"
#include "aos/red_souls/aos_soul_red_minotaur.h"
#include "aos/red_souls/aos_soul_red_nightmare.h"
#include "aos/red_souls/aos_soul_red_man_eater.h"
#include "aos/red_souls/aos_soul_red_were_beast.h"
#include "aos/red_souls/aos_soul_red_flame_demon.h"
#include "aos/red_souls/aos_soul_red_devil.h"
#include "aos/red_souls/aos_soul_red_ghost.h"
#include "aos/red_souls/aos_soul_red_siren.h"
#include "aos/red_souls/aos_soul_red_merman.h"
#include "aos/red_souls/aos_soul_red_needles.h"
#include "aos/blue_souls/aos_soul_blue_imp.h"
#include "aos/blue_souls/aos_soul_blue_death.h"
#include "aos/blue_souls/aos_soul_blue_ghost.h"
#include "aos/blue_souls/aos_soul_blue_sky_fish.h"
#include "aos/blue_souls/aos_soul_blue_creaking_skull.h"
#include "aos/blue_souls/aos_soul_blue_alastor.h"
#include "aos/blue_souls/aos_soul_blue_giant_bat.h"
#include "aos/blue_souls/aos_soul_blue_final_guard.h"
#include "aos/red_souls/aos_soul_red_lightning.h"
#include "aos/red_souls/aos_soul_red_killer_doll.h"
#include "aos/red_souls/aos_soul_red_nemesis.h"
#include "aos/red_souls/aos_soul_red_ripper.h"
#include "aos/red_souls/aos_soul_red_millione.h"
#include "aos/red_souls/aos_soul_red_arrow.h"
#include "aos/red_souls/aos_soul_red_shot.h"
#include "aos/red_souls/aos_soul_red_skull.h"
#include "aos/red_souls/aos_soul_red_valkyrie.h"
#include "aos/red_souls/aos_soul_red_spear.h"
#include "aos/red_souls/aos_soul_red_throw.h"
#include "aos/menus/aos_hud.h"
#include "aos/intro/aos_levelup.h"
#include "aos/intro/aos_game_over.h"
#include "aos/game/aos_save_room.h"
#include "aos/core/aos_soma.h"
#include "aos/weapons/aos_weapon.h"
#include "aos/menus/aos_window_frame.h"
#include "debug_menu.h"
#include "gba_input.h"
#include "aria_sim.h"
#include "aria_wav.h"
#include "aria_runtime_types.h"
#include "aria_room_io.h"
#include "aria_tables.h"
#include "aria_assets.h"
#include "aria_draw.h"
#include "aria_entities.h"
#include "aria_skills.h"
#include "aria_opening.h"
#include "aria_menus_debug.h"
#include "aria_weapons.h"
#include "aos/core/aos_input.h"
#include "fusion/assets/rom.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Object10Create's record scan asks whether the grant item is
 * already in the inventory; the portable inventory mirrors the
 * native category bases (32 items, then 59 weapons, 45 armor).
 * update_entities has no save handle, so the room loop publishes
 * the active save through this pointer. */
static const AosSaveData *object10_save_ctx;
/* Object1C's SaveData_SaveSlotToSram needs the live save and its
 * store path; update_entities has no save handle, so the room
 * loops publish them here (same pattern as object10_save_ctx). */
AosSaveData *object1c_save_ctx;
const char *object1c_save_path;
bool object10_item_owned(void *ctx, uint16_t item) {
    const AosSaveData *save = ctx ? ctx : object10_save_ctx;
    uint8_t category, index;
    if (!save || !aos_item_decode(item, &category, &index)) return false;
    unsigned base = category == AOS_PICKUP_ITEM ? 0
        : category == AOS_PICKUP_WEAPON ? AOS_SAVE_WEAPONS
        : AOS_SAVE_WEAPONS + 59;
    return save->inventory[base + index] != 0;
}

/* sub_08011A44 .. sub_08010350: leaves the room when Soma is outside it or
 * on an exit cell; returns false when the exit or its target is missing. */
/* Leaves the room through its native entry, or through the editor's door
 * `forced`; an editor spawn point places Soma's feet there (kept inside the
 * target). 1 when the room changed, 0 when an editor door's target cannot be
 * loaded (the room stays), -1 when a native exit fails. */
/* Object20 room dialogue state — declared before take_exit, which
 * closes the window on room changes. The host adapter that binds
 * these is defined next to update_entities. */
static AosIntroResources room_dialogue_resources;
static AosIntroFont room_dialogue_font;
static AosWindowFrame room_dialogue_window;
static AosIntroTextTiles room_dialogue_tiles;
static FusionRom room_dialogue_rom;
static bool room_dialogue_rom_tried;
static const char *room_dialogue_rom_path;
static bool room_dialogue_ready;
static bool room_dialogue_input_lock;
AosRoomDialogue room_dialogue;
static AosRoomDialogueHost room_dialogue_host;

static int take_exit(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
                     AosSoma *soma, int cam_x, int cam_y, const AosProjectDoor *forced) {
    int32_t x = soma->x >> 16, y = soma->y >> 16;
    const AosTransition *exit = forced ? NULL
        : aos_room_find_exit(room->transitions, room->transition_count, room->width_screens,
                             room->height_screens, x, y);
    if (!forced && !exit) {
        fprintf(stderr, "No transition for %d/%d at %d,%d\n", room->area, room->number, x, y);
        return -1;
    }
    int target_area = forced ? forced->target_area : exit->area;
    int target_room = forced ? forced->target_room : exit->room;
    bool spawn = forced != NULL;
    int spawn_x = forced ? forced->spawn_x : 0, spawn_y = forced ? forced->spawn_y : 0;
    if (exit && room->arrivals[exit - room->transitions].set) {
        spawn = true;
        spawn_x = room->arrivals[exit - room->transitions].x;
        spawn_y = room->arrivals[exit - room->transitions].y;
    }
    char folder[512];
    room_folder(folder, sizeof folder, target_area, target_room);
    AriaRoom target = {0};
    if (!load_room_folder(folder, &target)) {
        if (!forced) return -1;
        fprintf(stderr, "Editor door %d leads to %d/%d, which is not exported\n", forced->id,
                target_area, target_room);
        return 0;
    }
    SDL_Texture *texture = load_background(renderer, folder);
    if (!texture) { free_room(&target); return forced ? 0 : -1; }
    aos_room_exit_velocity(soma, x - cam_x, y - cam_y);
    int32_t ax, ay;
    if (spawn) {
        /* Soma's y is the pixel above his feet. */
        int max_x = target.width_cells * 8 - 1, max_y = target.height_cells * 8 - 1;
        ax = spawn_x > max_x ? max_x : spawn_x;
        ay = spawn_y - 1 > max_y ? max_y : spawn_y - 1 < 0 ? 0 : spawn_y - 1;
        if (ax != spawn_x || ay != spawn_y - 1)
            fprintf(stderr, "Editor spawn point outside %d/%d, kept inside\n", target_area,
                    target_room);
    } else {
        aos_room_arrival(exit, target.width_screens, target.height_screens, x, y, &ax, &ay);
    }
    soma->x = (int32_t)((uint32_t)ax << 16) | (soma->x & 0xFFFF);
    soma->y = (int32_t)((uint32_t)ay << 16) | (soma->y & 0xFFFF);
    printf("Room %d/%d -> %d/%d, arrival %d,%d\n", room->area, room->number, target.area,
           target.number, ax, ay);
    /* A room dialogue window belongs to the room that opened it;
     * native room loads retire the text state with the entities. */
    aos_room_dialogue_close(&room_dialogue);
    free_room(room);
    *room = target;
    aos_object01_reset();   /* the + 0x1316C claim is per-room here */
    SDL_DestroyTexture(*background);
    *background = texture;
    return 1;
}


/* The homing list gEwramData + 0x13194 (sub_08021924, aged by
 * sub_08021DC4): four room entity indices with their seen and aged counts;
 * an entry not seen again since the last ageing is dropped. */
AriaHomingList homing = {{-1, -1, -1, -1}, {0}, {0}};
const AriaRoom *homing_room;

void homing_register(int index) {
    int empty = -1;
    for (int slot = 0; slot < 4; ++slot) {
        if (homing.index[slot] == index) {
            homing.seen[slot]++;
            return;
        }
        if (empty < 0 && homing.index[slot] < 0) empty = slot;
    }
    if (empty < 0) return;
    homing.index[empty] = index;
    homing.seen[empty] = 1;
    homing.aged[empty] = 0;
}

static void homing_age(void) {
    for (int slot = 0; slot < 4; ++slot) {
        if (homing.index[slot] < 0) continue;
        if (homing.aged[slot] == homing.seen[slot]) {
            homing.index[slot] = -1;
            homing.seen[slot] = homing.aged[slot] = 0;
        } else {
            homing.aged[slot] = homing.seen[slot];
        }
    }
}

void homing_clear(void) {
    for (int slot = 0; slot < 4; ++slot) {
        homing.index[slot] = -1;
        homing.seen[slot] = homing.aged[slot] = 0;
    }
}

/* The index of still frame `id` among an art's exported ones, or -1.
 * Shared with aria_draw.c's blade_frame (declared in aria_draw.h). */
int static_index(const int *ids, int count, int id) {
    for (int i = 0; i < count; ++i)
        if (ids[i] == id) return i;
    return -1;
}

/* gEwramData + 0x344: the event bits (a breakable wall's break), stored
 * in the save (schema v10). */
uint32_t aria_events[AOS_WALL_EVENT_WORDS];
/* The durable words touched by the common pickup collection tail. */
AosPickupCollectionState aria_pickup_state;

/* One shared room dialogue: the native routines never run two text
 * windows at once and sub_08067934 re-open closes the previous one.
 * The host adapter reuses the opening path's verified-ROM text
 * resources, font and window frame; the ROM is opened lazily on the
 * first resolve so --simulate without --rom fails closed (the open
 * reports failure and the entity's poll keeps it unblocked). The
 * state itself is declared before take_exit (which closes the
 * window on room changes); the host callbacks live here next to
 * update_entities. */

static bool room_dialogue_resolve(void *context, uint16_t id,
                                  const uint8_t **script,
                                  size_t *script_size) {
    (void)context;
    if (!room_dialogue_ready) {
        if (!room_dialogue_rom_tried) {
            room_dialogue_rom_tried = true;
            if (room_dialogue_rom_path) {
                char error[160];
                (void)fusion_rom_open(FUSION_ASSET_GAME_ARIA,
                                      room_dialogue_rom_path,
                                      &room_dialogue_rom, error,
                                      sizeof error);
            }
        }
        if (room_dialogue_rom.data) {
            char error[160];
            room_dialogue_ready =
                aos_intro_resources_init(&room_dialogue_resources,
                                         &room_dialogue_rom, error,
                                         sizeof error) &&
                aos_intro_font_init(&room_dialogue_font, &room_dialogue_rom,
                                    error, sizeof error) &&
                aos_window_frame_init(&room_dialogue_window,
                                      &room_dialogue_rom, 1, error,
                                      sizeof error);
        }
    }
    if (!room_dialogue_ready) return false;
    /* Same text-table fetch as resolve_opening_text, widened to the
     * uint16_t ids the room variants pass to sub_08067934. */
    return aos_intro_resources_entry(&room_dialogue_resources, 1, id,
                                     script, script_size);
}

static bool room_dialogue_open_window(void *context, uint16_t id) {
    (void)context;
    (void)id;
    /* Gap (aos_room_dialogue_host.h): sub_08067934's WIN0H/WIN0V/
     * DISPCNT writes and the sub_0805AFC0 open animation have no
     * standalone primitive; the dialogue frame is composed whenever
     * the bridge reports the window open — the same approximation
     * the opening path uses. */
    return room_dialogue_ready;
}

/* Mirrors rebuild_opening_text: speaker row through the small
 * context-B font at (40,100), body glyphs at (40,116) on 12-pixel
 * rows, into the room dialogue's own tile canvas. */
static void room_dialogue_render(void *context,
                                 const AosRoomDialogue *dlg,
                                 uint32_t frame) {
    (void)context;
    (void)frame;    /* the wait-marker blink uses the draw frame */
    const AosOpeningEvent *event = aos_room_dialogue_event(dlg);
    if (!event || !room_dialogue_ready) return;
    aos_intro_text_tiles_clear(&room_dialogue_tiles);
    const char *speaker = aos_opening_event_speaker_name(event->speaker);
    bool ok =
        aos_intro_font_render_small_span(&room_dialogue_font,
                                         &room_dialogue_tiles, 0, 48, 88,
                                         8, (const uint8_t *)speaker,
                                         strlen(speaker));
    for (unsigned line = 0; ok && line < AOS_OPENING_LINES; ++line)
        ok = aos_intro_font_render_span(&room_dialogue_font,
                                        &room_dialogue_tiles, 0,
                                        line * 12, 176, 8,
                                        event->glyphs[line],
                                        event->glyph_counts[line]);
    if (!ok)
        fprintf(stderr, "Aria room dialogue text render failed\n");
}

static void room_dialogue_close_window(void *context) {
    (void)context;
    /* Gap: no sub_08067898 primitive; the frame tiles persist until
     * the next open overwrites them, as native VRAM does. */
}

static void room_dialogue_lock(void *context, bool locked) {
    (void)context;
    room_dialogue_input_lock = locked;
}

void room_dialogue_bind(void) {
    static bool bound;
    if (bound) return;
    bound = true;
    room_dialogue_host.context = NULL;
    room_dialogue_host.resolve_text = room_dialogue_resolve;
    room_dialogue_host.open_window = room_dialogue_open_window;
    room_dialogue_host.render = room_dialogue_render;
    room_dialogue_host.close_window = room_dialogue_close_window;
    room_dialogue_host.set_input_lock = room_dialogue_lock;
    aos_room_dialogue_init(&room_dialogue);
    aos_room_dialogue_set_host(&room_dialogue, &room_dialogue_host);
}

static void free_library(AriaLibrary *lib) {
    for (int i = 0; i < MAX_ANIMS; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (lib->frames[i][j].texture) SDL_DestroyTexture(lib->frames[i][j].texture);
}

static uint16_t keyboard_buttons(void) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    uint16_t held = 0;
    if (keys[SDL_SCANCODE_RIGHT]) held |= AOS_KEY_RIGHT;
    if (keys[SDL_SCANCODE_LEFT]) held |= AOS_KEY_LEFT;
    if (keys[SDL_SCANCODE_UP]) held |= AOS_KEY_UP;
    if (keys[SDL_SCANCODE_DOWN]) held |= AOS_KEY_DOWN;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) held |= AOS_KEY_JUMP;
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_F]) held |= AOS_KEY_ATTACK;
    if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LSHIFT]) held |= AOS_KEY_ABILITY;
    if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_V]) held |= AOS_KEY_GUARDIAN;
    return held;
}

/* The keyboard and the gamepad together (Soma's key bits are the GBA
 * KEYINPUT bits). As on the GBA D-pad, opposite directions cancel out. */
_Static_assert((int)AOS_KEY_JUMP == (int)GBA_KEY_A && (int)AOS_KEY_ATTACK == (int)GBA_KEY_B &&
               (int)AOS_KEY_RIGHT == (int)GBA_KEY_RIGHT && (int)AOS_KEY_LEFT == (int)GBA_KEY_LEFT &&
               (int)AOS_KEY_UP == (int)GBA_KEY_UP && (int)AOS_KEY_DOWN == (int)GBA_KEY_DOWN &&
               (int)AOS_KEY_GUARDIAN == (int)GBA_KEY_R && (int)AOS_KEY_ABILITY == (int)GBA_KEY_L,
               "Soma's buttons are the GBA KEYINPUT bits");
static uint16_t player_buttons(const GbaInput *gamepad) {
    uint16_t held = (uint16_t)(keyboard_buttons() | gba_input_buttons(gamepad));
    if ((held & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) == (AOS_KEY_LEFT | AOS_KEY_RIGHT))
        held &= (uint16_t)~(AOS_KEY_LEFT | AOS_KEY_RIGHT);
    if ((held & (AOS_KEY_UP | AOS_KEY_DOWN)) == (AOS_KEY_UP | AOS_KEY_DOWN))
        held &= (uint16_t)~(AOS_KEY_UP | AOS_KEY_DOWN);
    return held;
}

/* State for the gameplay obtained-soul notice (sub_0800E708 +
 * sub_0800E540). The text resources are initialised lazily on the first
 * collected soul so runs without a ROM keep working. */
typedef struct {
    AosSoulNotice *notice;
    AosIntroTextTiles *tiles;
    AosIntroResources *resources;
    AosIntroFont *font;
    const FusionRom *rom;
    bool *notice_ready;
    bool *text_ready;
    int *timer;
} AriaSoulNoticeCtx;

static void aria_gameplay_soul_notice(void *opaque, unsigned category,
                                      unsigned soul_index, bool player_low,
                                      bool fresh) {
    AriaSoulNoticeCtx *ctx = opaque;
    /* sub_0800E708's per-kind bases: names 0xE3/0x11B/0x133/0x156 and
     * sub_0800E540's descriptions 0x1E4/0x21C/0x234/0x257, both indexed
     * by the 0-based soul index carried in the pickup variant. */
    static const uint16_t name_bases[] = {0xE3, 0x11B, 0x133, 0x156};
    static const uint16_t desc_bases[] = {0x1E4, 0x21C, 0x234, 0x257};
    if (!ctx || category > 3 || !ctx->rom || !ctx->rom->data) return;
    if (!*ctx->text_ready) {
        char error[128];
        *ctx->text_ready =
            aos_intro_resources_init(ctx->resources, ctx->rom, error,
                                     sizeof error) &&
            aos_intro_font_init(ctx->font, ctx->rom, error, sizeof error);
        if (!*ctx->text_ready) return;
    }
    if (!*ctx->notice_ready) {
        char error[128];
        *ctx->notice_ready =
            aos_soul_notice_init(ctx->notice, ctx->rom, error, sizeof error);
        if (!*ctx->notice_ready) return;
    }
    AosOpeningEvent name, acquired, description;
    uint8_t name_glyphs[256], acquired_glyphs[256];
    if (!decode_opening_soul_entry(ctx->resources,
                                   (uint16_t)(name_bases[category] + soul_index),
                                   &name) ||
        !decode_opening_soul_entry(ctx->resources, 0x35E, &acquired) ||
        (fresh &&
         !decode_opening_soul_entry(ctx->resources,
                                    (uint16_t)(desc_bases[category] + soul_index),
                                    &description)))
        return;
    aos_intro_text_tiles_clear(ctx->tiles);
    size_t name_count = flatten_opening_text(&name, name_glyphs,
                                             sizeof name_glyphs);
    size_t acquired_count = flatten_opening_text(&acquired, acquired_glyphs,
                                                 sizeof acquired_glyphs);
    unsigned name_width = aos_intro_font_measure_small(ctx->font, name_glyphs,
                                                       name_count);
    bool ready =
        aos_intro_font_render_small_span(ctx->font, ctx->tiles, 0, 0, 240, 15,
                                         name_glyphs, name_count) &&
        aos_intro_font_render_small_span(
            ctx->font, ctx->tiles, name_width, 0, 240 - name_width, 15,
            acquired_glyphs, acquired_count);
    for (unsigned line = 0; ready && fresh; ++line)
        ready = aos_intro_font_render_span(
            ctx->font, ctx->tiles, 42, 24 + line * 12, 180, 15,
            description.glyphs[line], description.glyph_counts[line]);
    if (!ready) return;
    aos_soul_notice_build(ctx->notice, ctx->tiles, (name_width + 7) / 8,
                          player_low, fresh);
    /* sub_0800EFD4 counts unk_422 down each HUD tick: 0xF0 on the
     * name-only path (a repeated soul), 2 on the description path (a
     * new soul, units unresolved — the shared banner lifetime stands
     * in for both). */
    *ctx->timer = 0xF0;
}

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s [--check] [--simulate N [--simulate-hold MASK] [--simulate-script FILE]] [--save slot.sav] [--save-target slot.sav]"
            " [--consume-save] [--rom aria.gba] [--library index.tsv]\n"
            "       [--spawn X Y] [--moves MASK]\n"
            "       [--weapon none|INDEX] [--red-soul N] [--blue-soul N] [--yellow-soul N] [--hitboxes] [--no-overlays | --overlay-dir DIR] [--atk N] [--def N] [--hp N]\n"
            "       [--repeat N: release the capture buttons one frame in N]\n"
            "       [--capture out.bmp FRAMES BUTTONS] [--input-map map.txt] [--debug-menu]\n"
            "       [--debug-input MASK,MASK,...: menu key edges, one per captured frame]\n"
            "       [--play-song N: PlaySong at the start] [--audio-wav out.wav] [--mute]\n"
            "       (--area A --room R | room-folder)\n"
            "       --simulate-script FILE: per-frame held input; lines are\n"
            "         hold MASK [FRAMES] | tap MASK FRAMES | release | wait FRAMES\n"
            "         ('#' comments; MASK = AOS_KEY_* bits, strtoul base 0)\n",
            name);
}

int aria_runtime_main(int argc, char **argv) {
    const char *folder = NULL, *library_path = DEFAULT_LIBRARY, *capture_path = NULL;
    const char *audio_wav_path = NULL;
    long start_song = -1;
    bool mute = false;
    const char *weapon_name = "none", *load_save_path = NULL, *save_path = NULL;
    const char *rom_path = NULL;
    bool consume_save = false;
    long area = -1, room_number = -1, capture_frames = 0;
    long simulate_frames = 0;
    unsigned simulate_hold = 0;   /* --simulate-hold: AOS_KEY_* bits held every frame */
    const char *simulate_script_path = NULL;   /* --simulate-script: per-frame held input */
    unsigned long capture_buttons = 0;
    /* A new file owns no ability soul. In particular, Malphas' mid-air jump
     * must be collected before AOS_MOVE_AIR_JUMP becomes available. Saves and
     * the explicit --moves diagnostic override this zero mask below. */
    unsigned long moves = 0;
    int spawn_x = -1, spawn_y = -1;
    bool project_door_latch = false;   /* inside an editor door since arriving */
    bool check = false, show_hitboxes = false, debug_at_start = false;
    bool hp_override = false, moves_override = false, spawn_override = false;
    bool atk_override = false, def_override = false, weapon_override = false;
    long red_soul_override = -1;   /* --red-soul: equippedRedSoul diagnostic override */
    long blue_soul_override = -1;  /* --blue-soul: equippedBlueSoul diagnostic override */
    long yellow_soul_override = -1; /* --yellow-soul: equippedYellowSoul diagnostic override */
    /* --frontend FILE and --frontend-arg ARG...: the frontend to return to
     * with --game-over once Soma's death fade has ended. */
    const char *frontend_command[64] = {NULL};
    int frontend_count = 0;
    const char *debug_input = NULL;
    /* Diagnostic combat stats (new-game values are not traced). */
    AriaPlayerStats player_stats = {10, 4, 320, 320};
    AosSaveData active_save = {0};
    bool has_active_save = false;
    long repeat = 0;
    GbaPadMap pad_map;
    gba_pad_map_default(&pad_map);
    GbaInput gamepad = {0};
    for (int i = 1; i < argc; ++i) {
        char *end = NULL;
        if (!strcmp(argv[i], "--check")) check = true;
        else if (!strcmp(argv[i], "--simulate") && i + 1 < argc)
            simulate_frames = atol(argv[++i]);
        else if (!strcmp(argv[i], "--simulate-hold") && i + 1 < argc)
            simulate_hold = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--simulate-script") && i + 1 < argc)
            simulate_script_path = argv[++i];
        else if (!strcmp(argv[i], "--audit-transitions")) {
            aria_overlays_enabled = false;
            return audit_transitions();
        }
        else if (!strcmp(argv[i], "--no-overlays")) aria_overlays_enabled = false;
        else if (!strcmp(argv[i], "--overlay-dir") && i + 1 < argc)
            aria_overlay_directory = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) load_save_path = argv[++i];
        else if (!strcmp(argv[i], "--save-target") && i + 1 < argc) save_path = argv[++i];
        else if (!strcmp(argv[i], "--consume-save")) consume_save = true;
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_path = argv[++i];
        else if (!strcmp(argv[i], "--library") && i + 1 < argc) library_path = argv[++i];
        else if (!strcmp(argv[i], "--area") && i + 1 < argc) area = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--room") && i + 1 < argc) room_number = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--weapon") && i + 1 < argc) {
            weapon_name = argv[++i];
            weapon_override = true;
        } else if (!strcmp(argv[i], "--frontend") && i + 1 < argc) {
            frontend_command[0] = argv[++i];
            if (!frontend_count) frontend_count = 1;
        } else if (!strcmp(argv[i], "--frontend-arg") && i + 1 < argc) {
            if (!frontend_count) frontend_count = 1;
            if (frontend_count < 60) frontend_command[frontend_count++] = argv[i + 1];
            ++i;
        } else if (!strcmp(argv[i], "--red-soul") && i + 1 < argc) {
            red_soul_override = strtol(argv[++i], &end, 10);
        } else if (!strcmp(argv[i], "--blue-soul") && i + 1 < argc) {
            blue_soul_override = strtol(argv[++i], &end, 10);
        } else if (!strcmp(argv[i], "--yellow-soul") && i + 1 < argc) {
            yellow_soul_override = strtol(argv[++i], &end, 10);
        }
        else if (!strcmp(argv[i], "--hitboxes")) show_hitboxes = true;
        else if (!strcmp(argv[i], "--debug-menu")) debug_at_start = true;
        else if (!strcmp(argv[i], "--debug-input") && i + 1 < argc) debug_input = argv[++i];
        else if (!strcmp(argv[i], "--input-map") && i + 1 < argc) {
            char error[256];
            if (!gba_pad_map_load(&pad_map, argv[++i], error, sizeof error)) {
                fprintf(stderr, "Input map: %s\n", error);
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--repeat") && i + 1 < argc) repeat = atol(argv[++i]);
        else if (!strcmp(argv[i], "--atk") && i + 1 < argc) {
            player_stats.atk = atoi(argv[++i]);
            atk_override = true;
        } else if (!strcmp(argv[i], "--def") && i + 1 < argc) {
            player_stats.def = atoi(argv[++i]);
            def_override = true;
        }
        else if (!strcmp(argv[i], "--hp") && i + 1 < argc) {
            player_stats.hp = player_stats.max_hp = atoi(argv[++i]);
            hp_override = true;
        }
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) {
            moves = strtoul(argv[++i], &end, 0);
            if (*end || moves > 0x1F) { usage(argv[0]); return 2; }
            moves_override = true;
            end = NULL;
        } else if (!strcmp(argv[i], "--spawn") && i + 2 < argc) {
            spawn_x = atoi(argv[++i]);
            spawn_y = atoi(argv[++i]);
            spawn_override = true;
        } else if (!strcmp(argv[i], "--play-song") && i + 1 < argc) {
            start_song = strtol(argv[++i], &end, 0);
            if (*end || start_song < 0 || start_song > 0xFFFF) { usage(argv[0]); return 2; }
        } else if (!strcmp(argv[i], "--audio-wav") && i + 1 < argc) {
            audio_wav_path = argv[++i];
        } else if (!strcmp(argv[i], "--mute")) {
            mute = true;
        } else if (!strcmp(argv[i], "--capture") && i + 3 < argc) {
            capture_path = argv[++i];
            capture_frames = strtol(argv[++i], &end, 10);
            if (*end || capture_frames < 1 || capture_frames > 36000) { usage(argv[0]); return 2; }
            capture_buttons = strtoul(argv[++i], &end, 0);
            if (*end || capture_buttons > 0xFFFFu) { usage(argv[0]); return 2; }
            end = NULL;
        } else if (argv[i][0] != '-' && !folder) folder = argv[i];
        else { usage(argv[0]); return 2; }
        if (end && *end) { usage(argv[0]); return 2; }
    }
    if (!save_path) save_path = load_save_path;
    if (load_save_path) {
        char error[256];
        if (!aos_save_load(load_save_path, &active_save, error, sizeof error)) {
            fprintf(stderr, "Cannot load Aria save: %s\n", error);
            return 1;
        }
        if (consume_save && remove(load_save_path) != 0 && errno != ENOENT) {
            fprintf(stderr, "Cannot consume Aria quicksave %s: %s\n",
                    load_save_path, strerror(errno));
            return 1;
        }
        has_active_save = true;
        if (area < 0) area = active_save.area;
        if (room_number < 0) room_number = active_save.room;
        if (!spawn_override && active_save.spawn_x >= 0 && active_save.spawn_y >= 0) {
            spawn_x = active_save.spawn_x;
            spawn_y = active_save.spawn_y;
        }
        if (!hp_override) {
            player_stats.hp = active_save.hp;
            player_stats.max_hp = active_save.max_hp;
        }
        if (!moves_override) moves = active_save.moves;
    }
    char folder_buffer[512], room_path[600];
    if (!folder) {
        if (area < 0 || area > 99 || room_number < 0 || room_number > 999) { usage(argv[0]); return 2; }
        room_folder(folder_buffer, sizeof folder_buffer, (int)area, (int)room_number);
        folder = folder_buffer;
    }
    snprintf(room_path, sizeof room_path, "%s/room.tsv", folder);

    AriaRoom room = {0};
    if (!load_room(room_path, &room)) {
        fprintf(stderr, "Export the rooms first: ./build/fusion_rebuild "
                        "--task aria_rooms\n");
        return 1;
    }
    apply_room_overlay(&room);
    AosCollision layer = room_layer(&room);
    if (spawn_x < 0 && !find_spawn(&layer, &spawn_x, &spawn_y)) {
        fprintf(stderr, "No floor with headroom found in %s\n", room_path);
        free_room(&room);
        return 1;
    }
    char audio_error[256];
    aos_audio_reset();
    bool have_audio_table = aos_audio_load_song_table(DEFAULT_AUDIO_TABLE,
                                                       audio_error, sizeof audio_error);
    if (!have_audio_table)
        fprintf(stderr, "No Aria song table: ./build/fusion_rebuild --task aria_sound (%s)\n",
                audio_error);
    AosMp2kEngine mp2k;
    aos_mp2k_engine_init(&mp2k);
    bool have_audio_bank = aos_mp2k_engine_load_bank(&mp2k, DEFAULT_AUDIO_BANK,
                                                      audio_error, sizeof audio_error);
    if (!have_audio_bank)
        fprintf(stderr, "No Aria MP2K bank: ./build/fusion_rebuild --task aria_sound (%s)\n",
                audio_error);
    if (have_audio_table && have_audio_bank)
        aos_audio_set_start_sink(start_mp2k_song, &mp2k);
    static AriaLibrary library;
    if (check) {
        bool ok = load_library(library_path, &library, NULL);
        size_t initialized_headers = 0, failed_songs = 0, ended_songs = 0;
        if (have_audio_table && have_audio_bank) {
            for (size_t song = 0; song < aos_audio_song_count(); ++song) {
                AosAudioSongInfo info;
                if (aos_audio_song_info(song, &info) &&
                    aos_mp2k_player_start(&mp2k, info.player, info.header_pointer)) {
                    initialized_headers++;
                    /* Ten seconds of sequencer clocks: streams the decoder does
                     * not follow yet (PORT, XCMD) fail closed and are counted. */
                    AosMp2kPlayer *player = &mp2k.players[info.player];
                    for (int frame = 0; frame < 600 && !player->ended && !player->failed; ++frame)
                        aos_mp2k_engine_player_frame(&mp2k, info.player);
                    if (player->failed) failed_songs++;
                    else if (player->ended) ended_songs++;
                }
            }
            if (initialized_headers != aos_audio_song_count()) ok = false;
        }
        if (ok) printf("Aria room %s: %d x %d cells, Soma library %d animations, spawn %d,%d\n",
                       folder, room.width_cells, room.height_cells, library.set.count,
                       spawn_x, spawn_y);
        if (have_audio_table)
            printf("Aria audio table: %zu songs\n", aos_audio_song_count());
        if (have_audio_bank)
            printf("Aria MP2K bank: %zu bytes\n", mp2k.bank_size);
        if (have_audio_table && have_audio_bank)
            printf("Aria MP2K headers: %zu/%zu initialized; over 600 frames %zu ended, %zu "
                   "stopped on an unsupported or invalid stream, %u channel notes (%zu on "
                   "CGB channels), %zu dropped, %zu sample reads outside the bank\n",
                   initialized_headers, aos_audio_song_count(), ended_songs, failed_songs,
                   mp2k.serial, mp2k.cgb_notes, mp2k.dropped_notes, mp2k.missing_samples);
        aos_audio_set_start_sink(NULL, NULL);
        aos_mp2k_engine_destroy(&mp2k);
        free_room(&room);
        return ok ? 0 : 1;
    }

    /* The Object20 room dialogue resolves its text through the same
     * verified-ROM resources as the opening; the ROM path is kept
     * for the lazy open inside room_dialogue_resolve so both the
     * SDL and --simulate paths can bind it. */
    room_dialogue_rom_path = rom_path;

    if (simulate_frames > 0) {
        /* Headless gameplay tick: the same data loads as the SDL runtime
         * (renderer NULL, so no textures) and the player, stats, save-room
         * and entity updates run with neutral input for N frames. This is
         * the only automated exercise of update_entities' SPECIAL dispatch.
         * Out of scope: room transitions, pause/save menus, rendering,
         * audio mixing, guardian and soul-skill attacks, the opening
         * scene and death/game-over handling. */
        if (!load_library(library_path, &library, NULL)) {
            fprintf(stderr, "Cannot load the Soma library: %s\n", library_path);
            aos_mp2k_engine_destroy(&mp2k);
            free_room(&room);
            return 1;
        }
        static AriaObjects objects;
        bool objects_ok = aria_load_objects(DEFAULT_OBJECTS, &objects, NULL);
        /* Bind whatever special_28 variants actually loaded: the
         * loader is all-or-nothing on its return but marks each
         * variant loaded per frame, and the variant table can fill
         * before later objects (special_31_v6+) — id 28's art is
         * complete either way. bind() itself checks ->loaded. */
        object1c_anim_bind(&objects);
        bool enemies_ok = objects_ok && aria_load_enemy_data(&objects);
        if (!objects_ok)
            fprintf(stderr, "No object library: ./build/fusion_rebuild --task aria_objects\n");
        else if (!enemies_ok)
            fprintf(stderr, "No enemy data: ./build/fusion_rebuild --task aria_objects\n");
        static AosStats progress;
        progress.character = 0;
        aos_stats_init(&progress);
        aos_button_config_default(&progress.buttons);
        memset(aria_events, 0, sizeof aria_events);
        aos_object01_reset();
        memset(&aria_pickup_state, 0, sizeof aria_pickup_state);
        aos_stats_new_game_equipment(&progress);
        aos_stats_new_game_inventory(&progress);
        aos_stats_level_up(&progress);
        static AosEquipmentTables equipment;
        bool have_equipment = aria_load_equipment(DEFAULT_EQUIPMENT, &equipment);
        static AosItemUseRecord item_use[AOS_ITEM_RECORDS];
        bool have_item_use = aria_load_item_use(DEFAULT_ITEM_USE, item_use);
        if (!have_item_use)
            fprintf(stderr, "No item-use table: ./build/fusion_rebuild --task aria_objects\n");
        if (have_equipment) {
            aos_stats_refresh(&progress, &equipment);
            if (!atk_override) player_stats.atk = progress.atk;
            if (!def_override) player_stats.def = progress.def;
        }
        if (!hp_override && progress.max_hp > player_stats.max_hp) {
            player_stats.max_hp = progress.max_hp;
            if (player_stats.hp > player_stats.max_hp) player_stats.hp = player_stats.max_hp;
        }
        if (red_soul_override >= 0 && red_soul_override < AOS_RED_SOUL_RECORDS) {
            progress.red_soul = (uint8_t)red_soul_override;
            if (red_soul_override > 0 &&
                !aos_souls_total(&progress, AOS_SOUL_RED, red_soul_override - 1))
                aos_souls_add(&progress, AOS_SOUL_RED, red_soul_override - 1, 1);
        }
        if (blue_soul_override >= 0 && blue_soul_override < AOS_BLUE_SOUL_RECORDS) {
            progress.blue_soul = (uint8_t)blue_soul_override;
            if (blue_soul_override > 0 &&
                !aos_souls_total(&progress, AOS_SOUL_BLUE, blue_soul_override - 1))
                aos_souls_add(&progress, AOS_SOUL_BLUE, blue_soul_override - 1, 1);
        }
        if (yellow_soul_override >= 0 && yellow_soul_override < AOS_YELLOW_SOUL_RECORDS) {
            progress.yellow_soul = (uint8_t)yellow_soul_override;
            if (yellow_soul_override > 0 &&
                !aos_souls_total(&progress, AOS_SOUL_YELLOW, yellow_soul_override - 1))
                aos_souls_add(&progress, AOS_SOUL_YELLOW, yellow_soul_override - 1, 1);
        }
        progress.mp = (int16_t)progress.max_mp;
        static AosRedSoulRecord red_souls[AOS_RED_SOUL_RECORDS];
        aria_load_red_souls(DEFAULT_RED_SOULS, red_souls);
        static AosThrowRecord skill_throws[AOS_THROW_RECORDS];
        aria_load_skill_throws(DEFAULT_SKILL_THROWS, skill_throws);
        aria_load_bursts(DEFAULT_BURSTS);
        static AosYellowSoulRecord yellow_souls[AOS_YELLOW_SOUL_RECORDS];
        bool have_yellow_souls = aria_load_yellow_souls(DEFAULT_YELLOW_SOULS, yellow_souls);
        AosYellowSoul yellow_soul = {0};
        static char sim_weapon_name[8];
        if (weapon_override)
            progress.weapon = strcmp(weapon_name, "none") ? (uint8_t)atoi(weapon_name)
                                                          : AOS_EQUIPMENT_NONE;
        else {
            snprintf(sim_weapon_name, sizeof sim_weapon_name, "%u", progress.weapon);
            weapon_name = sim_weapon_name;
        }
        AosSoma soma = aos_soma_spawn(spawn_x << 16, spawn_y << 16, &library.set);
        soma.moves = (uint32_t)moves;
        soma.stats = &progress;
        soma.random = aos_random;
        soma.hp = (int16_t)player_stats.hp;
        soma.max_hp = (int16_t)player_stats.max_hp;
        if (!load_weapon(DEFAULT_WEAPONS, weapon_name, &soma.weapon))
            fprintf(stderr, "Export the weapons first: ./build/fusion_rebuild --task aria_weapons\n");
        AosPickupRecent recent_items;
        aos_pickup_recent_reset(&recent_items);
        AosRecentSouls recent_souls;
        aos_pickup_recent_soul_reset(&recent_souls);
        AosWeaponEntity weapon_entity = {0};
        AriaCombat combat = {
            .weapon = &weapon_entity,
            .weapon_frames = NULL,
            .player = &player_stats,
            .progress = &progress,
            .equipment = have_equipment ? &equipment : NULL,
            .item_icons = objects_ok ? &objects.item_icon[0][0] : NULL,
            .recent_items = &recent_items,
            .recent_souls = &recent_souls,
            .hud = NULL,
            .hud_font = NULL,
            .derive_atk = !atk_override,
            .derive_def = !def_override,
        };
        if (objects.money_loaded) combat.money = objects.money;
        for (int k = 0; objects_ok && k < AOS_EFFECT_KINDS; ++k)
            if (objects.effects[k].def.count) combat.effects[k] = &objects.effects[k].set;
        for (int id = 0; enemies_ok && id < ENEMY_KINDS; ++id) {
            if (!objects.enemies[id].loaded) continue;
            combat.kinds[id] = &objects.enemies[id].kind;
            combat.stats[id] = &objects.enemies[id].stats;
            combat.drops[id] = &objects.enemies[id].drops;
        }
        int cam_x = 0, cam_y = 0;
        follow_camera(&room, spawn_x, spawn_y, &cam_x, &cam_y);
        AosForcedInput forced = {0};
        AosSaveRoom save_room = {0};
        homing_room = &room;
        object10_save_ctx = NULL;
        object1c_save_ctx = NULL;
        object1c_save_path = NULL;
        /* --simulate-script replaces the constant --simulate-hold mask with a
         * per-frame held value walked from the parsed directives. */
        AriaSimDirective *sim_dirs = NULL;
        size_t sim_dir_count = 0;
        if (simulate_script_path &&
            aria_load_sim_script(simulate_script_path, &sim_dirs,
                                 &sim_dir_count) != 0) {
            aos_mp2k_engine_destroy(&mp2k);
            free_room(&room);
            return 2;
        }
        const bool use_sim_script = simulate_script_path != NULL;
        size_t sim_next = 0;
        long sim_timed_end = -1;
        unsigned sim_timed_held = 0, sim_base = 0, sim_held = 0;
        unsigned sim_prev_held = 0;
        for (long frame = 0; frame < simulate_frames; ++frame) {
            if (!moves_override) soma.moves = progress.abilities[0] & 0x1Fu;
            soma.abilities = progress.flags;
            soma.guardian_alive = false;
            unsigned held = simulate_hold;
            if (use_sim_script) {
                while (sim_next < sim_dir_count &&
                       sim_dirs[sim_next].pos <= frame) {
                    const AriaSimDirective *d = &sim_dirs[sim_next++];
                    if (d->timed) {
                        sim_timed_held = d->held;
                        sim_timed_end = d->end;
                    } else {
                        sim_base = d->held;
                    }
                }
                held = frame < sim_timed_end ? sim_timed_held : sim_base;
            }
            sim_held = held;
            /* Edge input for the room-dialogue poll: raw held
             * transitions, computed before the dialogue input
             * lock zeroes the gameplay mask below. */
            uint16_t sim_new_input = (uint16_t)(held & ~sim_prev_held);
            sim_prev_held = held;
            unsigned held_eff = room_dialogue_input_lock ? 0 : held;
            aos_soma_update(&soma, &layer, (uint16_t)held_eff, 0);
            bool door_held = false;
            for (size_t k = 0; k < room.entity_count && !door_held; ++k)
                door_held = room.entities[k].kind == ARIA_KIND_SPECIAL &&
                            room.entities[k].spawned &&
                            ((room.entities[k].id == ARIA_OBJECT_WOODEN_DOOR &&
                              room.entities[k].door.holding) ||
                             (room.entities[k].id == 2 &&
                              room.entities[k].object02.holding));
            if (progress.accessory == 0x2A && !door_held &&
                !(soma.flags & (AOS_FLAG_AIRBORNE | AOS_FLAG_WALL)) &&
                (held_eff & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) &&
                ((uint16_t)(soma.ticks - 1) & 7) == 0) {
                if (++progress.experience > AOS_STATS_MAX_EXPERIENCE)
                    progress.experience = AOS_STATS_MAX_EXPERIENCE;
                int32_t gained = aos_stats_level_up(&progress);
                if (gained > 0) {
                    soma.max_hp = (int16_t)progress.max_hp;
                    aos_levelup_start(&combat.levelup);
                    if (combat.equipment) {
                        aos_stats_refresh(&progress, combat.equipment);
                        if (combat.derive_atk) combat.player->atk = progress.atk;
                        if (combat.derive_def) combat.player->def = progress.def;
                    }
                }
            }
            aos_stats_frame_reset(&progress);
            aos_stats_status_update(&progress, 0, (uint16_t)(soma.ticks - 1));
            aos_stats_derive(&progress);
            aria_pickup_state.global_flags &= ~0x101u;
            if (door_held) aria_pickup_state.global_flags |= 0x101u;
            if (have_yellow_souls) {
                aos_yellow_soul_update(&yellow_soul, &progress, yellow_souls, soma.hp,
                                       soma.max_hp);
                aos_yellow_soul_regen(&yellow_soul, &progress, yellow_souls, &soma.hp,
                                      soma.max_hp, soma.vx, soma.flags, door_held);
            }
            if (aos_save_room_update(&save_room, room_has_save_point(&room), soma.x >> 16,
                                     soma.y >> 16, 0)) {
                soma.hp = soma.max_hp;
                player_stats.hp = soma.hp;
            }
            aos_combat_tick(&soma.combat);
            follow_camera(&room, soma.x >> 16, soma.y >> 16, &cam_x, &cam_y);
            homing_room = &room;
            object10_save_ctx = NULL;
            object1c_save_ctx = NULL;
            object1c_save_path = NULL;
            update_entities(&room, &layer, &soma, cam_x, cam_y, &forced, &combat,
                            sim_new_input);
            homing_age();
            aos_enemy_set_soul_attacks(NULL, 0);
        }
        int spawned_special = 0, active_special = 0, enemies_alive = 0;
        printf("Aria simulate: %ld frames, room %d/%d, spawn %d,%d, Soma %d,%d hp %d/%d\n",
               simulate_frames, room.area, room.number, spawn_x, spawn_y,
               (int)(soma.x >> 16), (int)(soma.y >> 16), soma.hp, soma.max_hp);
        if (use_sim_script)
            printf("Aria simulate: script %s: %zu directives, final held 0x%x\n",
                   simulate_script_path, sim_dir_count, sim_held);
        printf("Aria simulate: %zu entities, %zu effects, %zu pickups; SPECIAL coverage:\n",
               room.entity_count, room.effect_count, room.pickup_count);
        for (int id = 0; id < 64; ++id) {
            int total = 0, spawned = 0;
            for (size_t k = 0; k < room.entity_count; ++k) {
                const AriaEntity *e = &room.entities[k];
                if (e->kind != ARIA_KIND_SPECIAL || e->id != id) continue;
                ++total;
                if (e->spawned) ++spawned;
            }
            if (!total) continue;
            spawned_special += spawned;
            active_special += total;
            printf("  id %2d: %d record(s), %d spawned\n", id, total, spawned);
        }
        for (size_t k = 0; k < room.entity_count; ++k) {
            const AriaEntity *e = &room.entities[k];
            if (e->kind == ARIA_KIND_ENEMY && e->spawned) ++enemies_alive;
        }
        printf("Aria simulate: SPECIAL %d/%d spawned, %d enemies alive\n",
               spawned_special, active_special, enemies_alive);
        free(sim_dirs);
        aos_mp2k_engine_destroy(&mp2k);
        free_room(&room);
        return 0;
    }

    if (!fusion_host_init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | (mute ? 0 : SDL_INIT_AUDIO))) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        aos_audio_set_start_sink(NULL, NULL);
        aos_mp2k_engine_destroy(&mp2k);
        return 1;
    }
    gba_input_init(&gamepad, &pad_map);
    /* SoundMainRAM's output: signed 8-bit mono at the mode's 10512 Hz. */
    SDL_AudioStream *audio_stream = NULL;
    if (!mute && have_audio_bank) {
        SDL_AudioSpec spec = {SDL_AUDIO_S8, 1, AOS_MP2K_HOST_RATE};
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL,
                                                 NULL);
        if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);
        else fprintf(stderr, "No audio output: %s\n", SDL_GetError());
    }
    AriaWav wav = {0};
    if (audio_wav_path && !aria_wav_open(&wav, audio_wav_path))
        fprintf(stderr, "Cannot write %s\n", audio_wav_path);
    if (start_song >= 0) aos_audio_play_song((uint16_t)start_song);
    /* sub_08013960 runs at each room load (the start counts as one). */
    bool have_room_music = load_room_music(DEFAULT_ROOM_MUSIC);
    if (!have_room_music)
        fprintf(stderr, "No Aria room music flags: ./build/fusion_rebuild --task aria_sound\n");
    AosRoomMusic room_music = {0};
    int music_area = -1, music_room = -1;
    FusionRom opening_rom = {0};
    AosPauseGraphics pause_graphics = {0};
    bool have_pause_graphics = false;
    SDL_Texture *pause_texture = NULL;
    AosQuickMap quick_map = {0};
    bool have_quick_map = false;
    bool quick_map_open = false;
    /* sub_080110E4's BLDY stages: 16->0 over four frames on open, then
     * 0->16 over four frames on close before the map hands back. */
    int quick_map_bldY = 0;
    bool quick_map_closing = false;
    SDL_Texture *quick_map_texture = NULL;
    AosIntroResources opening_resources = {0};
    AosIntroFont opening_font = {0};
    AosOpeningEvent opening = {0};
    AosOpeningActors opening_actors = {0};
    AosWindowFrame opening_window = {0};
    AosSoulNotice opening_soul_notice = {0};
    AosSoulNotice gameplay_soul_notice = {0};
    static AosIntroTextTiles opening_tiles;
    static AosIntroTextTiles opening_soul_tiles;
    static AosIntroTextTiles gameplay_soul_tiles;
    bool gameplay_soul_notice_ready = false;
    bool gameplay_soul_text_ready = false;
    int gameplay_soul_notice_timer = 0;
    AriaSoulNoticeCtx soul_notice_ctx = {
        .notice = &gameplay_soul_notice,
        .tiles = &gameplay_soul_tiles,
        .resources = &opening_resources,
        .font = &opening_font,
        .rom = &opening_rom,
        .notice_ready = &gameplay_soul_notice_ready,
        .text_ready = &gameplay_soul_text_ready,
        .timer = &gameplay_soul_notice_timer,
    };
    bool opening_text_ready = false;
    bool opening_window_ready = false;
    bool opening_epilogue = false;
    bool opening_talk_armed = true;
    bool opening_soul_text_ready = false;
    SDL_Texture *opening_portrait_texture = NULL;
    uint8_t opening_portrait_id = 0xFF;
    int rc = 1;
    SDL_Window *window = fusion_host_create_window("Metroid Vania - experimental Aria runtime",
                                          VIEW_W * 4, VIEW_H * 4, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    SDL_Texture *background = NULL;
    if (!renderer) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); goto cleanup; }
    SDL_SetRenderLogicalPresentation(renderer, VIEW_W, VIEW_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    background = load_background(renderer, folder);
    if (!background || !load_library(library_path, &library, renderer)) goto cleanup;
    static AriaObjects objects;
    bool objects_ok = aria_load_objects(DEFAULT_OBJECTS, &objects, renderer);
    /* Same partial-load tolerance as the --simulate path: bind the
     * special_28 variants that loaded regardless of the aggregate
     * result (the variant table can fill before later objects). */
    object1c_anim_bind(&objects);
    bool enemies_ok = objects_ok && aria_load_enemy_data(&objects);
    if (!enemies_ok) fprintf(stderr, "No object library: ./build/fusion_rebuild --task aria_objects\n");
    int32_t cam_x = 0;
    int cam_y = 0;

    /* The level and its stats follow from the saved experience
     * (sub_08033DA4 then sub_08033CAC, as sub_08033E38 rebuilds them). */
    static AosStats progress;
    progress.character = has_active_save ? active_save.character : 0;
    aos_stats_init(&progress);
    aos_button_config_default(&progress.buttons);
    memset(aria_events, 0, sizeof aria_events);
    aos_object01_reset();   /* the + 0x1316C claim is per-room here */
    memset(&aria_pickup_state, 0, sizeof aria_pickup_state);
    if (has_active_save) {
        progress.experience = active_save.experience;
        progress.gold = active_save.gold;
        memcpy(progress.defeated, active_save.defeated, sizeof progress.defeated);
        memcpy(progress.dropped, active_save.dropped, sizeof progress.dropped);
        memcpy(aria_events, active_save.events, sizeof aria_events);
        memcpy(aria_pickup_state.placed, active_save.placed_items,
               sizeof aria_pickup_state.placed);
        aria_pickup_state.special = active_save.special_souls;
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            progress.buttons.action[action] =
                aos_button_config_button(active_save.buttons[action]);
    }
    aos_stats_level_up(&progress);
    /* Equipment: the file's (sub_08014548's new-game set for older files),
     * or the --weapon choice. The equipment records give ATK and DEF
     * (sub_0804AD9C) unless --atk / --def ask for diagnostic values. */
    aos_stats_new_game_equipment(&progress);
    aos_stats_new_game_inventory(&progress);
    if (has_active_save) {
        progress.weapon = active_save.weapon;
        progress.armor = active_save.armor;
        progress.accessory = active_save.accessory;
        memcpy(progress.item_counts, active_save.inventory, sizeof progress.item_counts);
        memcpy(progress.weapon_counts, active_save.inventory + AOS_SAVE_WEAPONS,
               sizeof progress.weapon_counts);
        memcpy(progress.armor_counts, active_save.inventory + AOS_SAVE_ARMOR,
               sizeof progress.armor_counts);
        aria_souls_from_save(&progress, &active_save);
        progress.red_soul = active_save.red_soul;
        progress.blue_soul = active_save.blue_soul;
        progress.yellow_soul = active_save.yellow_soul;
        progress.challenge_flags = active_save.challenge_flags & 0x300000u;
    }
    static char save_weapon_name[8];
    if (weapon_override)
        progress.weapon = strcmp(weapon_name, "none") ? (uint8_t)atoi(weapon_name)
                                                      : AOS_EQUIPMENT_NONE;
    else if (has_active_save) {
        if (progress.weapon == AOS_EQUIPMENT_NONE)
            weapon_name = "none";
        else {
            snprintf(save_weapon_name, sizeof save_weapon_name, "%u", progress.weapon);
            weapon_name = save_weapon_name;
        }
    }
    static AosEquipmentTables equipment;
    bool have_equipment = aria_load_equipment(DEFAULT_EQUIPMENT, &equipment);
    static AosItemUseRecord item_use[AOS_ITEM_RECORDS];
    bool have_item_use = aria_load_item_use(DEFAULT_ITEM_USE, item_use);
    if (!have_item_use)
        fprintf(stderr, "No item-use table: ./build/fusion_rebuild --task aria_objects\n");
    if (have_equipment) {
        aos_stats_refresh(&progress, &equipment);
        if (!atk_override) player_stats.atk = progress.atk;
        if (!def_override) player_stats.def = progress.def;
    } else {
        /* Without the records the effective stats are the base ones. */
        static const AosEquipmentTables no_equipment;
        aos_stats_refresh(&progress, &no_equipment);
        fprintf(stderr, "No equipment table: ./build/fusion_rebuild --task aria_objects "
                        "(ATK and DEF stay diagnostic)\n");
    }
    if (has_active_save)
        printf("Aria file %u: level %u, experience %u, max HP %u, ATK %d, DEF %d\n",
               active_save.slot + 1u, progress.level, progress.experience, progress.max_hp,
               player_stats.atk, player_stats.def);
    if (!hp_override && progress.max_hp > player_stats.max_hp) {
        player_stats.max_hp = progress.max_hp;
        if (player_stats.hp > player_stats.max_hp) player_stats.hp = player_stats.max_hp;
    }
    if (red_soul_override >= 0 && red_soul_override < AOS_RED_SOUL_RECORDS) {
        progress.red_soul = (uint8_t)red_soul_override;
        /* A diagnostic equip behaves like an owned soul in the menu. */
        if (red_soul_override > 0 &&
            !aos_souls_total(&progress, AOS_SOUL_RED, red_soul_override - 1))
            aos_souls_add(&progress, AOS_SOUL_RED, red_soul_override - 1, 1);
    }
    if (blue_soul_override >= 0 && blue_soul_override < AOS_BLUE_SOUL_RECORDS) {
        progress.blue_soul = (uint8_t)blue_soul_override;
        if (blue_soul_override > 0 &&
            !aos_souls_total(&progress, AOS_SOUL_BLUE, blue_soul_override - 1))
            aos_souls_add(&progress, AOS_SOUL_BLUE, blue_soul_override - 1, 1);
    }
    if (yellow_soul_override >= 0 && yellow_soul_override < AOS_YELLOW_SOUL_RECORDS) {
        progress.yellow_soul = (uint8_t)yellow_soul_override;
        if (yellow_soul_override > 0 &&
            !aos_souls_total(&progress, AOS_SOUL_YELLOW, yellow_soul_override - 1))
            aos_souls_add(&progress, AOS_SOUL_YELLOW, yellow_soul_override - 1, 1);
    }
    /* currentMP: the file's, full for a new or older file (sub_08014548). */
    progress.mp = has_active_save && active_save.mp >= 0 ? active_save.mp
                                                         : (int16_t)progress.max_mp;
    if (progress.mp > (int16_t)progress.max_mp) progress.mp = (int16_t)progress.max_mp;
    static AosRedSoulRecord red_souls[AOS_RED_SOUL_RECORDS];
    bool have_red_souls = aria_load_red_souls(DEFAULT_RED_SOULS, red_souls);
    static AosThrowRecord skill_throws[AOS_THROW_RECORDS];
    bool have_skill_throws = aria_load_skill_throws(DEFAULT_SKILL_THROWS, skill_throws);
    if (!aria_load_bursts(DEFAULT_BURSTS)) fprintf(stderr, "No Aria burst list: %s\n", DEFAULT_BURSTS);
    size_t burst_count = 0;
    const AosEffect *bursts = aria_burst_effects(&burst_count);
    aos_enemy_set_burst_source(bursts, burst_count);
    AosSoma soma = aos_soma_spawn(spawn_x << 16, spawn_y << 16, &library.set);
    soma.moves = (uint32_t)moves;
    if (rom_path) {
        char error[256];
        if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, rom_path, &opening_rom,
                             error, sizeof error)) {
            fprintf(stderr, "Cannot open the verified Aria ROM: %s\n", error);
            goto cleanup;
        }
        have_pause_graphics = aos_pause_graphics_init(
            &pause_graphics, &opening_rom, 1, error, sizeof error);
        if (!have_pause_graphics) {
            fprintf(stderr, "Cannot decode the native Aria pause graphics: %s\n",
                    error);
            goto cleanup;
        }
        pause_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          VIEW_W, VIEW_H);
        if (!pause_texture) {
            fprintf(stderr, "Cannot create the native Aria pause texture: %s\n",
                    SDL_GetError());
            goto cleanup;
        }
        SDL_SetTextureScaleMode(pause_texture, SDL_SCALEMODE_NEAREST);
        have_quick_map = aos_quick_map_init(&quick_map, &opening_rom,
                                            error, sizeof error);
        if (!have_quick_map) {
            fprintf(stderr, "Cannot decode the native Aria quick map: %s\n", error);
            goto cleanup;
        }
        if (has_active_save) {
            aos_quick_map_unpack_explored(&quick_map, active_save.map_explored);
            quick_map.secret_rooms = active_save.secret_rooms;
        }
        quick_map_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              VIEW_W, VIEW_H);
        if (!quick_map_texture) {
            fprintf(stderr, "Cannot create the native Aria quick-map texture: %s\n",
                    SDL_GetError());
            goto cleanup;
        }
        SDL_SetTextureScaleMode(quick_map_texture, SDL_SCALEMODE_NEAREST);
    }
    bool opening_needed = has_active_save &&
        aos_opening_event_should_start((uint8_t)room.area, (uint16_t)room.number,
                                       progress.character, aria_events,
                                       room_has_opening_controller(&room));
    if (opening_needed) {
        char error[256];
        const uint8_t *script = NULL;
        size_t script_size = 0;
        if (!opening_rom.data) {
            fprintf(stderr, "The unfinished Aria opening scene requires --rom\n");
            goto cleanup;
        }
        if (!aos_intro_resources_init(&opening_resources, &opening_rom,
                                      error, sizeof error) ||
            !aos_intro_font_init(&opening_font, &opening_rom,
                                 error, sizeof error) ||
            !aos_window_frame_init(&opening_window, &opening_rom, 1,
                                   error, sizeof error) ||
            !aos_intro_resources_entry(&opening_resources, 1,
                                       AOS_OPENING_TEXT_ID, &script,
                                       &script_size) ||
            !aos_opening_event_init(&opening, script, script_size)) {
            fprintf(stderr, "Cannot start the Aria opening scene: %s\n", error);
            goto cleanup;
        }
        opening_window_ready = true;
        aos_opening_event_set_resolver(&opening, resolve_opening_text,
                                       &opening_resources);
        aos_opening_actors_init(&opening_actors);
        /* sub_0805CED0 case 0 opens with PlaySong(0x1000): the AudioCommand
         * STOP that silences whatever the introduction left playing before
         * the scene's own cues take over. */
        aos_audio_play_song(0x1000);
        aos_opening_actors_set_motion(&opening_actors, opening_random,
                                      opening_sine, opening_angle,
                                      opening_floor_depth, opening_wall_push,
                                      &layer);
        soma.x = AOS_OPENING_SOMA_X << 16;
        soma.y = AOS_OPENING_SOMA_Y << 16;
        soma.anim.id = AOS_SOMA_ANIM_OPENING_LYING;
        soma.anim.frame = 0;
        soma.anim.tick = 0;
        spawn_x = AOS_OPENING_SOMA_X;
        spawn_y = AOS_OPENING_SOMA_Y;
        opening_text_ready = rebuild_opening_text(&opening, &opening_font,
                                                  &opening_tiles);
        if (!opening_text_ready) {
            fprintf(stderr, "Cannot render the Aria opening dialogue\n");
            goto cleanup;
        }
        AosOpeningEvent soul_name, acquired, soul_description;
        uint8_t name_glyphs[256], acquired_glyphs[256];
        opening_soul_text_ready = aos_soul_notice_init(
                                      &opening_soul_notice, &opening_rom,
                                      error, sizeof error) &&
                                  decode_opening_soul_entry(
                                      &opening_resources, 0xE3, &soul_name) &&
                                  decode_opening_soul_entry(
                                      &opening_resources, 0x35E, &acquired) &&
                                  decode_opening_soul_entry(
                                      &opening_resources, 0x1E4, &soul_description);
        if (opening_soul_text_ready) {
            size_t name_count = flatten_opening_text(
                &soul_name, name_glyphs, sizeof name_glyphs);
            size_t acquired_count = flatten_opening_text(
                &acquired, acquired_glyphs, sizeof acquired_glyphs);
            unsigned name_width = aos_intro_font_measure_small(
                &opening_font, name_glyphs, name_count);
            aos_intro_text_tiles_clear(&opening_soul_tiles);
            opening_soul_text_ready =
                aos_intro_font_render_small_span(
                    &opening_font, &opening_soul_tiles, 0, 0, 240, 15,
                    name_glyphs, name_count) &&
                aos_intro_font_render_small_span(
                    &opening_font, &opening_soul_tiles, name_width, 0,
                    240 - name_width, 15, acquired_glyphs, acquired_count);
            for (unsigned line = 0;
                 opening_soul_text_ready && line < AOS_OPENING_LINES; ++line)
                opening_soul_text_ready = aos_intro_font_render_span(
                    &opening_font, &opening_soul_tiles, 42, 24 + line * 12,
                    180, 15, soul_description.glyphs[line],
                    soul_description.glyph_counts[line]);
            if (opening_soul_text_ready)
                aos_soul_notice_build(&opening_soul_notice,
                                      &opening_soul_tiles,
                                      (name_width + 7) / 8, true, true);
        }
        if (!opening_soul_text_ready)
            fprintf(stderr, "Aria opening soul notice text unavailable\n");
        opening.dirty = false;
        aos_audio_play_song(0x1000);
        if (start_song < 0) {
            room_music.current_song = 0;
            /* sub_0805CED0 starts MUS_28 after Object20's room reveal. The
             * compact host has no reveal stage yet, so it starts with the
             * dialogue instead of letting the room loop overwrite it. */
            aos_room_music_change(&room_music, 0x1C);
        }
        printf("Aria opening story started from Object20 parameter 3\n");
    }
    static AriaSouls souls;
    souls = (AriaSouls){.stats = &progress, .records = have_red_souls ? red_souls : NULL,
                        .throws = have_skill_throws ? skill_throws : NULL, .soma = &soma,
                        .burst = bursts, .burst_count = burst_count};
    soma.cast = cast_red_soul;
    soma.cast_context = &souls;
    soma.stats = &progress;
    soma.random = aos_random;      /* the random voices */
    static AosYellowSoulRecord yellow_souls[AOS_YELLOW_SOUL_RECORDS];
    bool have_yellow_souls = aria_load_yellow_souls(DEFAULT_YELLOW_SOULS, yellow_souls);
    if (!have_yellow_souls)
        fprintf(stderr, "No Aria yellow soul records: %s\n", DEFAULT_YELLOW_SOULS);
    AosYellowSoul yellow_soul = {0};
    static AosBlueSoulRecord blue_souls[AOS_BLUE_SOUL_RECORDS];
    static AriaGuardian guardian;
    guardian = (AriaGuardian){.stats = &progress, .soma = &soma};
    if (objects_ok && objects.skills[SKILL_ART_BIG_GOLEM].loaded &&
        objects.skills[SKILL_ART_BIG_GOLEM].defs[1].count) {
        guardian.big_golem_art = &objects.skills[SKILL_ART_BIG_GOLEM].set;
        guardian.big_golem_kind = &objects.skills[SKILL_ART_BIG_GOLEM].kind;
    }
    if (objects_ok && objects.skills[SKILL_ART_ALURA_UNE].loaded &&
        objects.skills[SKILL_ART_ALURA_UNE].defs[0].count)
        guardian.alura_une_art = &objects.skills[SKILL_ART_ALURA_UNE].set;
    if (objects_ok && objects.skills[SKILL_ART_SKY_FISH].loaded &&
        objects.skills[SKILL_ART_SKY_FISH].defs[0].count)
        guardian.sky_fish_art = &objects.skills[SKILL_ART_SKY_FISH].set;
    if (objects_ok && objects.skills[SKILL_ART_CREAKING_SKULL].loaded &&
        objects.skills[SKILL_ART_CREAKING_SKULL].defs[0].count)
        guardian.creaking_skull_art = &objects.skills[SKILL_ART_CREAKING_SKULL].set;
    if (objects_ok && objects.skills[SKILL_ART_ALASTOR].loaded &&
        objects.skills[SKILL_ART_ALASTOR].defs[0].count)
        guardian.alastor_art = &objects.skills[SKILL_ART_ALASTOR].set;
    if (objects_ok && objects.skills[SKILL_ART_FINAL_GUARD].loaded &&
        objects.skills[SKILL_ART_FINAL_GUARD].defs[0].count)
        guardian.final_guard_art = &objects.skills[SKILL_ART_FINAL_GUARD].set;
    if (objects_ok && objects.skills[SKILL_ART_SHADOW_KNIGHT].loaded &&
        objects.skills[SKILL_ART_SHADOW_KNIGHT].defs[1].count) {
        guardian.shadow_knight_art = &objects.skills[SKILL_ART_SHADOW_KNIGHT].set;
        guardian.shadow_knight_kind = &objects.skills[SKILL_ART_SHADOW_KNIGHT].kind;
    }
    if (objects_ok && objects.skills[SKILL_ART_DEVIL_SPARK].loaded &&
        objects.skills[SKILL_ART_DEVIL_SPARK].defs[3].count)
        guardian.spark_art = &objects.skills[SKILL_ART_DEVIL_SPARK].set;
    if (objects_ok && objects.skills[SKILL_ART_GREAT_ARMOR].loaded &&
        objects.skills[SKILL_ART_GREAT_ARMOR].defs[1].count)
        guardian.great_armor_art = &objects.skills[SKILL_ART_GREAT_ARMOR].set;
    if (objects_ok && objects.skills[SKILL_ART_PERSEPHONE].loaded &&
        objects.skills[SKILL_ART_PERSEPHONE].defs[0].count) {
        guardian.persephone_art = &objects.skills[SKILL_ART_PERSEPHONE].set;
        guardian.persephone_kind = &objects.skills[SKILL_ART_PERSEPHONE].kind;
    }
    if (objects_ok && objects.skills[SKILL_ART_DEATH].loaded &&
        objects.skills[SKILL_ART_DEATH].defs[0].count)
        guardian.death_art = &objects.skills[SKILL_ART_DEATH].set;
    if (objects_ok && objects.skills[SKILL_ART_FLYING_ARMOR].loaded &&
        objects.skills[SKILL_ART_FLYING_ARMOR].defs[0].count)
        guardian.flying_armor_art = &objects.skills[SKILL_ART_FLYING_ARMOR].set;
    if (objects_ok && objects.skills[SKILL_ART_CAGNAZZO].loaded &&
        objects.skills[SKILL_ART_CAGNAZZO].defs[0].count)
        guardian.cagnazzo_art = &objects.skills[SKILL_ART_CAGNAZZO].set;
    if (objects_ok && objects.skills[SKILL_ART_BUER].loaded &&
        objects.skills[SKILL_ART_BUER].defs[0].count)
        guardian.buer_art = &objects.skills[SKILL_ART_BUER].set;
    if (objects_ok && objects.skills[SKILL_ART_CATOBLEPAS].loaded &&
        objects.skills[SKILL_ART_CATOBLEPAS].defs[0].count)
        guardian.catoblepas_art = &objects.skills[SKILL_ART_CATOBLEPAS].set;
    if (objects_ok && objects.skills[SKILL_ART_PILLAR].loaded &&
        objects.skills[SKILL_ART_PILLAR].defs[0].count)
        guardian.pillar_art = &objects.skills[SKILL_ART_PILLAR].set;
    if (objects_ok && objects.skills[SKILL_ART_CURLY].loaded &&
        objects.skills[SKILL_ART_CURLY].defs[0].count)
        guardian.curly_art = &objects.skills[SKILL_ART_CURLY].set;
    if (objects_ok && objects.skills[SKILL_ART_GIANT_GHOST].loaded &&
        objects.skills[SKILL_ART_GIANT_GHOST].set.count > 1 &&
        objects.skills[SKILL_ART_GIANT_GHOST].defs[1].count)
        guardian.ghost_art = &objects.skills[SKILL_ART_GIANT_GHOST].set;
    if (objects_ok && objects.skills[SKILL_ART_WITCH].loaded &&
        objects.skills[SKILL_ART_WITCH].defs[0].count)
        guardian.witch_art = &objects.skills[SKILL_ART_WITCH].set;
    soma.guardian_cancel = cancel_transform;
    if (objects_ok && objects.skills[SKILL_ART_IMP].loaded &&
        objects.skills[SKILL_ART_IMP].defs[0].count) {
        guardian.imp_art = &objects.skills[SKILL_ART_IMP].set;
        guardian.imp_kind = &objects.skills[SKILL_ART_IMP].kind;
    }
    if (aria_load_blue_souls(DEFAULT_BLUE_SOULS, blue_souls)) {
        guardian.records = blue_souls;
        soma.guardian = start_guardian;
        soma.guardian_context = &guardian;
    } else {
        fprintf(stderr, "No Aria blue soul records: %s\n", DEFAULT_BLUE_SOULS);
    }
    /* The in-game HUD (sub_0800DA50), drawn from the exported resources. */
    static uint8_t hud_bytes[AOS_HUD_ASSET_BYTES];
    static AosHud hud;
    AosHudFont hud_font = {0};
    bool have_hud = false;
    {
        FILE *f = fopen(DEFAULT_HUD, "rb");
        if (f) {
            have_hud = fread(hud_bytes, 1, sizeof hud_bytes, f) == sizeof hud_bytes;
            fclose(f);
        }
    }
    SDL_Texture *hud_texture = have_hud ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                            SDL_TEXTUREACCESS_STREAMING, 240,
                                                            160)
                                        : NULL;
    if (hud_texture) {
        SDL_SetTextureBlendMode(hud_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(hud_texture, SDL_SCALEMODE_NEAREST);
    }
    static uint16_t hud_drawn[AOS_HUD_MAP_SIZE * AOS_HUD_MAP_SIZE];
    uint32_t hud_drawn_revision = 0;
    bool hud_dirty = true;
    soma.hp = (int16_t)player_stats.hp;
    soma.max_hp = (int16_t)player_stats.max_hp;
    if (have_hud) {
        aos_hud_init(&hud, soma.hp, soma.max_hp, progress.mp, (int16_t)progress.max_mp);
        hud_font = (AosHudFont){hud_bytes + AOS_HUD_FONT_METRIC_OFFSET,
                                hud_bytes + AOS_HUD_FONT_BITMAP_OFFSET};
    }
    if (!load_weapon(DEFAULT_WEAPONS, weapon_name, &soma.weapon)) {
        fprintf(stderr, "Export the weapons first: ./build/fusion_rebuild --task aria_weapons\n");
        goto cleanup;
    }
    /* Only the class 0, 2 and 3 entity (sub_080221CC) is ported. */
    static AriaWeaponSprite weapon_sprite;
    bool weapon_ported = soma.weapon.weapon_class <= 4;
    bool weapon_loaded = weapon_ported && load_weapon_sprite(weapon_name, &weapon_sprite, renderer);
    AosWeaponEntity weapon_entity = {0};
    AosPickupRecent recent_items;
    aos_pickup_recent_reset(&recent_items);
    AosRecentSouls recent_souls;
    aos_pickup_recent_soul_reset(&recent_souls);
    AriaCombat combat = {
        .weapon = &weapon_entity,
        .weapon_frames = weapon_loaded ? &weapon_sprite.data : NULL,
        .player = &player_stats,
        .progress = &progress,
        .equipment = have_equipment ? &equipment : NULL,
        .item_icons = objects_ok ? &objects.item_icon[0][0] : NULL,
        .recent_items = &recent_items,
        .recent_souls = &recent_souls,
        .hud = have_hud ? &hud : NULL,
        .hud_font = have_hud ? &hud_font : NULL,
        .derive_atk = !atk_override,
        .derive_def = !def_override,
        .soul_notice = aria_gameplay_soul_notice,
        .soul_notice_ctx = &soul_notice_ctx,
    };
    if (objects.money_loaded) combat.money = objects.money;
    for (int k = 0; objects_ok && k < AOS_EFFECT_KINDS; ++k)
        if (objects.effects[k].def.count) combat.effects[k] = &objects.effects[k].set;
    for (int id = 0; enemies_ok && id < ENEMY_KINDS; ++id) {
        if (!objects.enemies[id].loaded) continue;
        combat.kinds[id] = &objects.enemies[id].kind;
        combat.stats[id] = &objects.enemies[id].stats;
        combat.drops[id] = &objects.enemies[id].drops;
    }
    follow_camera(&room, spawn_x, spawn_y, &cam_x, &cam_y);
    /* Native Aria input state is tracked separately from SDL. */
    AosForcedInput forced = {0};
    AosSaveRoom save_room = {0};
    bool announced_death = false;
    static AosGameOver death;
    long step = 0;
    AosInput aria_input = {0};
    AosGameLifecycle lifecycle;
    /* This executable still requires a room folder, --area/--room or --save,
     * so its established development entry starts directly in IN_GAME. The
     * separate title launcher remains unchanged until MAIN_MENU is native. */
    aos_game_lifecycle_init(&lifecycle, true);
    Uint64 last = SDL_GetTicksNS(), accumulator = 0;
    const Uint64 frame_ns = 1000000000ull / 60;
    bool running = true;
    /* The debug menu's view of the engine state. */
    static DebugMenu debug;
    bool paused = false, step_once = false, chord_before = false;
    AosSoulMenu soul_menu = {0};
    soul_menu.recent_items = &recent_items;
    soul_menu.recent_souls = &recent_souls;
    bool debug_moves[5] = {false};
    int debug_hp = soma.hp, debug_max_hp = soma.max_hp, debug_enemy = 0;
    int debug_area = room.area, debug_room = room.number;
    int enemy_ids[ENEMY_KINDS], enemy_count = 0;
    const char *enemy_labels[ENEMY_KINDS];
    for (int id = 0; id < ENEMY_KINDS; ++id)
        if (combat.kinds[id]) {
            enemy_ids[enemy_count] = id;
            enemy_labels[enemy_count++] = enemy_names[id];
        }
    debug_menu_clear(&debug);
    debug_menu_toggle(&debug, "Paused", &paused, DEBUG_PAUSE);
    debug_menu_action(&debug, "Step one frame", DEBUG_STEP);
    debug_menu_toggle(&debug, "Hitboxes", &show_hitboxes, DEBUG_HITBOXES);
    debug_menu_value(&debug, "HP", &debug_hp, 1, 9999, 1, NULL, DEBUG_HP);
    debug_menu_value(&debug, "Max HP", &debug_max_hp, 1, 9999, 1, NULL, DEBUG_MAX_HP);
    debug_menu_action(&debug, "Refill HP", DEBUG_REFILL);
    debug_menu_value(&debug, "ATK (override)", &player_stats.atk, 0, 999, 1, NULL, DEBUG_ATK);
    debug_menu_value(&debug, "DEF (override)", &player_stats.def, 0, 999, 1, NULL, DEBUG_DEF);
    for (int i = 0; i < 5; ++i)
        debug_menu_toggle(&debug, debug_move_labels[i], &debug_moves[i], DEBUG_MOVES);
    if (enemy_count) {
        debug_menu_value(&debug, "Enemy", &debug_enemy, 0, enemy_count - 1, 1, enemy_labels,
                         DEBUG_ENEMY);
        debug_menu_action(&debug, "Spawn enemy ahead", DEBUG_SPAWN);
    }
    debug_menu_action(&debug, "Remove all enemies", DEBUG_CLEAR);
    debug_menu_value(&debug, "Area", &debug_area, 0, 99, 1, NULL, DEBUG_AREA);
    debug_menu_value(&debug, "Room", &debug_room, 0, 999, 1, NULL, DEBUG_ROOM);
    debug_menu_action(&debug, "Teleport", DEBUG_TELEPORT);
    debug.open = debug_at_start;
    uint16_t menu_previous = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            gba_input_handle_event(&gamepad, &event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_F1)
                debug.open = !debug.open;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F2 && paused)
                step_once = true;
        }
        bool chord = gba_input_debug_chord(&gamepad);
        if (chord && !chord_before) debug.open = !debug.open;
        chord_before = chord;
        if (debug.open) {
            /* Mirror the engine before reading the menu's input. */
            debug_hp = soma.hp;
            debug_max_hp = soma.max_hp;
            for (int i = 0; i < 5; ++i) debug_moves[i] = soma.moves & debug_move_bits[i];
            uint16_t menu_held = chord ? 0 : player_buttons(&gamepad), pressed;
            if (capture_path) {
                /* Scripted menu edges: the next mask of --debug-input. */
                char *end = NULL;
                pressed = debug_input && *debug_input
                              ? (uint16_t)strtoul(debug_input, &end, 0) : 0;
                if (end) debug_input = *end == ',' ? end + 1 : end;
            } else {
                pressed = (uint16_t)(menu_held & ~menu_previous);
            }
            switch (debug_menu_input(&debug, pressed)) {
            case DEBUG_STEP: step_once = true; break;
            case DEBUG_HP: soma.hp = (int16_t)(debug_hp > soma.max_hp ? soma.max_hp : debug_hp); break;
            case DEBUG_MAX_HP:
                soma.max_hp = (int16_t)debug_max_hp;
                if (soma.hp > soma.max_hp) soma.hp = soma.max_hp;
                player_stats.max_hp = debug_max_hp;
                break;
            case DEBUG_REFILL: soma.hp = soma.max_hp; break;
            case DEBUG_MOVES:
                soma.moves = 0;
                for (int i = 0; i < 5; ++i)
                    if (debug_moves[i]) soma.moves |= debug_move_bits[i];
                moves_override = true;
                break;
            case DEBUG_SPAWN:
                if (!debug_spawn(&room, &combat, &soma, &layer, enemy_ids[debug_enemy]))
                    fprintf(stderr, "Debug spawn failed\n");
                break;
            case DEBUG_CLEAR:
                for (size_t i = 0; i < room.entity_count; ++i)
                    if (room.entities[i].kind == ARIA_KIND_ENEMY) room.entities[i].enemy.removed = true;
                break;
            case DEBUG_TELEPORT:
                if (debug_teleport(renderer, &room, &background, &soma, &library.set, debug_area,
                                   debug_room)) {
                    layer = room_layer(&room);
                    save_room = (AosSaveRoom){0};
                    follow_camera(&room, soma.x >> 16, soma.y >> 16, &cam_x, &cam_y);
                }
                break;
            default: break;
            }
            menu_previous = menu_held;
        } else {
            menu_previous = 0xFFFF;     /* no stray edge when the menu opens */
        }
        Uint64 now = SDL_GetTicksNS();
        accumulator += capture_path ? frame_ns : now - last;
        last = now;
        if (accumulator > frame_ns * 5) accumulator = frame_ns * 5;
        /* The menu and the pause freeze the game (not in captures). */
        if ((debug.open || paused) && !capture_path) {
            accumulator = step_once ? frame_ns : 0;
            step_once = false;
        }
        while (accumulator >= frame_ns) {
            accumulator -= frame_ns;
            uint16_t held = capture_path ? (uint16_t)capture_buttons
                                         : player_buttons(&gamepad);
            if (capture_path && repeat > 0 && step % repeat == repeat - 1) held = 0;
            if (debug.open && !capture_path) held = 0;     /* a menu frame step */
            bool forced_frame = forced.active;
            if (forced.active) held = forced.held;
            forced.active = false;
            aos_input_update(&aria_input, held);
            lifecycle.input = aria_input;
            aos_game_lifecycle_update(&lifecycle);
            /* The room's area music enters with the room like the native
             * flow; Object20's story cues (MUS_16 confrontation, MUS_23
             * after the battle) override it mid-scene. While the opening
             * runs, sub_0805CED0 owns the song: the area theme must not
             * fire on the first frame and overwrite MUS_28, and it enters
             * only once the state-8 return pan hands control back. */
            if (have_room_music && start_song < 0 &&
                !(opening.active || opening_actors.camera_active) &&
                (room.area != music_area || room.number != music_room)) {
                music_area = room.area;
                music_room = room.number;
                const AosRoomMusicFlags *flags =
                    music_area >= 0 && music_area < MUSIC_AREAS && music_room >= 0 &&
                            music_room < MUSIC_ROOMS
                        ? &room_music_flags[music_area][music_room]
                        : NULL;
                aos_room_music_enter(&room_music, music_area, music_room, flags);
            }
            /* AudioVSync advances the native volume commands every frame. */
            aos_audio_volume_update();
            /* m4aSoundMain: advance every player, retire players whose final
             * track ended, then mix the active DirectSound voices. */
            if (have_audio_bank) {
                const AosAudioState *audio_state = aos_audio_state();
                for (unsigned player = 0; player < AOS_AUDIO_PLAYER_COUNT; ++player) {
                    aos_mp2k_player_pause(&mp2k, (uint8_t)player,
                                          audio_state->players[player].paused);
                    aos_mp2k_player_volume(
                        &mp2k, (uint8_t)player,
                        (uint8_t)(audio_state->players[player].volume_current / 0x10));
                }
                aos_mp2k_engine_frame(&mp2k);
                for (unsigned player = 0; player < AOS_AUDIO_PLAYER_COUNT; ++player)
                    if (mp2k.players[player].ended || mp2k.players[player].failed)
                        aos_audio_player_finished(player);
                int8_t pcm[AOS_MP2K_SAMPLES_PER_FRAME];
                aos_mp2k_mix(&mp2k, pcm, sizeof pcm);
                if (audio_stream &&
                    SDL_GetAudioStreamQueued(audio_stream) < (int)sizeof pcm * AOS_MP2K_HOST_QUEUE_FRAMES)
                    SDL_PutAudioStreamData(audio_stream, pcm, sizeof pcm);
                aria_wav_write(&wav, pcm, sizeof pcm);
            }
            if (!lifecycle.in_game_frame_requested) continue;
            /* +0xb==8 keeps Object20 alive for the mode-4 return pan after
             * the script's end command; the branch stays open until the
             * camera finishes recentring on Soma. */
            if (opening.active || opening_actors.camera_active) {
                /* The scene owns Soma's pose until Object20 commits. */
                soma.anim.id = AOS_SOMA_ANIM_OPENING_LYING;
                soma.anim.frame = 0;
                /* The modal obtained-soul window consumes its own A presses;
                 * they must not also advance the blocked text script. */
                aos_opening_actors_input(&opening_actors, aria_input.new_input);
                AosOpeningCue script_cue = AOS_OPENING_CUE_NONE;
                /* sub_0805CED0 case 0 holds the script until the spawn
                 * camera phase (mode 5) finishes its reveal pan; the text
                 * window only opens at case 1. */
                if (opening_actors.reveal_done &&
                    !aos_opening_actors_blocks_script(&opening_actors)) {
                    aos_opening_event_update(&opening, aria_input.new_input);
                    if (opening.advanced) aos_audio_play_song(0x132);
                    script_cue = opening.cue;
                }
                aos_opening_actors_trigger(&opening_actors, script_cue);
                uint16_t actor_durations[AOS_OPENING_ACTOR_COUNT];
                opening_actor_durations(objects_ok ? &objects : NULL,
                                        &opening_actors, actor_durations);
                AosOpeningCue actor_cue = aos_opening_actors_update(
                    &opening_actors, actor_durations, &cam_x, cam_y);
                soma.anim.id = opening_actors.soma_animation;
                for (unsigned actor = 0;
                     actor < opening_actors.emitted_count; ++actor)
                    if (opening_actors.emitted[actor].events &
                        AOS_OPENING_ENTITY_EVENT_SOUND_0X6B) {
                        aos_audio_play_song(0x6B);
                        break;
                    }
                for (unsigned projectile = 0;
                     projectile < opening_actors.projectile_count; ++projectile)
                    if (opening_actors.projectiles[projectile].events &
                        AOS_OPENING_ENTITY_EVENT_SOUND_0XBC) {
                        aos_audio_play_song(0xBC);
                        break;
                    }
                if (start_song < 0) play_opening_cue(actor_cue, &room_music);
                if (opening.malformed) {
                    fprintf(stderr, "Malformed Aria opening text entry 0x%04X\n",
                            AOS_OPENING_TEXT_ID);
                    goto cleanup;
                }
                if (opening.dirty) {
                    opening_text_ready = rebuild_opening_text(
                        &opening, &opening_font, &opening_tiles);
                    opening.dirty = false;
                    if (!opening_text_ready) {
                        fprintf(stderr, "Cannot render the Aria opening dialogue\n");
                        goto cleanup;
                    }
                }
                if (opening.complete &&
                    aos_opening_event_commit(&opening, aria_events, &progress)) {
                    printf("Aria opening story completed; event 0x20 set and "
                           "Winged Skeleton soul granted and equipped\n");
                }
                if (opening.complete && have_room_music && start_song < 0 &&
                    (room.area != music_area || room.number != music_room)) {
                    const AosRoomMusicFlags *flags =
                        room.area < MUSIC_AREAS && room.number < MUSIC_ROOMS
                            ? &room_music_flags[room.area][room.number]
                            : NULL;
                    aos_room_music_enter(&room_music, room.area, room.number,
                                         flags);
                    music_area = room.area;
                    music_room = room.number;
                }
                ++step;
                if (has_active_save) active_save.play_frames++;
                continue;
            }
            uint8_t previous_weapon = progress.weapon;
            AosItemVitals item_vitals = {soma.hp, soma.max_hp};
            soul_menu.sleep_available = has_active_save;
            unsigned soul_menu_event = 0;
            if (quick_map_open) {
                /* BLDY ramps 4/frame like sub_080110E4's stages. */
                if (quick_map_closing) {
                    quick_map_bldY += 4;
                    if (quick_map_bldY >= 16) {
                        quick_map_bldY = 16;
                        quick_map_open = false;
                        quick_map_closing = false;
                        printf("Quick map closed\n");
                    }
                } else if (quick_map_bldY > 0) {
                    quick_map_bldY -= 4;
                    if (quick_map_bldY < 0) quick_map_bldY = 0;
                }
                if (quick_map_open && quick_map_bldY == 0 && !quick_map_closing &&
                    (aria_input.new_input & (AOS_KEY_SELECT | AOS_KEY_B))) {
                    quick_map_closing = true;
                    aos_audio_play_song(AOS_AUDIO_SONG_LOUD);
                }
                ++step;
                if (has_active_save) active_save.play_frames++;
                continue;
            }
            if (!soul_menu.open && have_quick_map &&
                (aria_input.new_input & AOS_KEY_SELECT)) {
                char error[160];
                if (aos_quick_map_visit(&quick_map, &opening_rom,
                                        (unsigned)room.area, (unsigned)room.number,
                                        soma.x >> 16, soma.y >> 16,
                                        error, sizeof error)) {
                    quick_map_open = true;
                    quick_map_bldY = 16;
                    quick_map_closing = false;
                    aos_audio_play_song(AOS_AUDIO_SONG_QUIET);
                    printf("Quick map opened\n");
                } else {
                    fprintf(stderr, "Cannot place the quick-map cursor: %s\n", error);
                }
                ++step;
                if (has_active_save) active_save.play_frames++;
                continue;
            }
            soul_menu_event = progress.character == 0
                    ? aos_pause_menu_update(&soul_menu, &progress,
                                            have_equipment ? &equipment : NULL,
                                            have_item_use ? item_use : NULL, &item_vitals,
                                            aria_input.new_input, aria_input.repeated_input)
                    : 0;
            soma.hp = item_vitals.hp;
            if ((soul_menu_event & AOS_SOUL_MENU_CHANGED) &&
                progress.weapon != previous_weapon &&
                !reload_equipped_weapon(progress.weapon, &soma, &weapon_sprite,
                                        &weapon_loaded, &weapon_entity, &combat, renderer)) {
                fprintf(stderr, "Cannot load equipped weapon %u; keeping %u\n",
                        progress.weapon, previous_weapon);
                progress.weapon = previous_weapon;
                if (have_equipment) aos_stats_refresh(&progress, &equipment);
            }
            if ((soul_menu_event & AOS_SOUL_MENU_CHANGED) &&
                soul_menu.hub_cursor == AOS_PAUSE_EQUIP) {
                if (combat.derive_atk) player_stats.atk = progress.atk;
                if (combat.derive_def) player_stats.def = progress.def;
            }
            if (soul_menu_event & AOS_SOUL_MENU_OPENED) {
                aos_pickup_icon_backup(&room.pickup_icons);
                aos_audio_play_song(AOS_AUDIO_SONG_QUIET);
                printf("Pause hub opened\n");
            }
            if (soul_menu_event & AOS_SOUL_MENU_MOVED) aos_audio_play_song(0xF1);
            if (soul_menu_event & AOS_SOUL_MENU_CONFIG_EDITED) aos_audio_play_song(0xF4);
            if (soul_menu_event & AOS_SOUL_MENU_ACCEPTED) aos_audio_play_song(0xF3);
            if (soul_menu_event & AOS_SOUL_MENU_BACK) aos_audio_play_song(0xF0);
            if (soul_menu_event & AOS_SOUL_MENU_ITEM_USED) {
                if (item_use[soul_menu.last_item].effect == 3) {
                    aos_audio_play_song(0x198);
                    aos_audio_play_song(0xEF);
                } else {
                    aos_audio_play_song(0xF8);
                }
                printf("Used %s: HP %d/%d, MP %d/%u, remaining %u\n",
                       aos_item_name(soul_menu.last_item), soma.hp, soma.max_hp,
                       progress.mp, progress.max_mp,
                       progress.item_counts[soul_menu.last_item]);
            }
            if (soul_menu_event & AOS_SOUL_MENU_REFUSED) aos_audio_play_song(0xEF);
            if (soul_menu_event & AOS_SOUL_MENU_CONFIG_APPLIED)
                printf("Buttons: Attack %s, Jump %s, Ability %s, Guardian %s\n",
                       aos_button_config_button_name(aos_button_config_index(
                           progress.buttons.action[AOS_BUTTON_ATTACK])),
                       aos_button_config_button_name(aos_button_config_index(
                           progress.buttons.action[AOS_BUTTON_JUMP])),
                       aos_button_config_button_name(aos_button_config_index(
                           progress.buttons.action[AOS_BUTTON_ABILITY])),
                       aos_button_config_button_name(aos_button_config_index(
                           progress.buttons.action[AOS_BUTTON_GUARDIAN])));
            if (soul_menu_event & AOS_SOUL_MENU_SLEEP_SAVE_REQUESTED) {
                char quick_path[1024], error[256];
                bool saved = has_active_save && save_path &&
                    aos_save_quick_path(quick_path, sizeof quick_path, save_path);
                if (saved) {
                    aria_runtime_to_save(&active_save, &room, &soma, &progress);
                    if (have_quick_map) {
                        aos_quick_map_pack_explored(&quick_map,
                                                    active_save.map_explored);
                        active_save.secret_rooms = quick_map.secret_rooms;
                    }
                    saved = aos_save_store(quick_path, &active_save, error, sizeof error);
                } else {
                    snprintf(error, sizeof error, "no active file slot");
                }
                aos_pause_sleep_result(&soul_menu, saved);
                if (saved) {
                    aos_audio_play_song(0x1010);
                    printf("Created one-use Aria quicksave for file %u (frame %ld)\n",
                           active_save.slot + 1u, step);
                } else {
                    aos_audio_play_song(0xEF);
                    fprintf(stderr, "Cannot write Aria quicksave: %s\n", error);
                }
            }
            if (soul_menu_event & AOS_SOUL_MENU_SLEEP_FINISHED) {
                printf("Sleep reset completed (frame %ld)\n", step);
                if (frontend_command[0]) {
                    if (!fusion_host_launch(frontend_command))
                        fprintf(stderr, "Cannot return to the frontend: %s\n",
                                SDL_GetError());
                }
                running = false;
                break;
            }
            if (soul_menu_event & AOS_SOUL_MENU_UNAVAILABLE)
                printf("Pause page pending: %s\n", aos_pause_entry_name(soul_menu.hub_cursor));
            if (soul_menu_event & AOS_SOUL_MENU_CHANGED) {
                if (soul_menu.page == AOS_SOUL_MENU_PAGE_EQUIP ||
                    soul_menu.hub_cursor == AOS_PAUSE_EQUIP)
                    printf("Equipment: weapon %u, armor %u, accessory %u, ATK %d, DEF %d\n",
                           progress.weapon, progress.armor, progress.accessory,
                           progress.atk, progress.def);
                else
                    printf("Equipped souls: red %u, blue %u, yellow %u, abilities %02x%02x\n",
                           progress.red_soul, progress.blue_soul, progress.yellow_soul,
                           progress.abilities[1], progress.abilities[0]);
            }
            if (soul_menu_event & AOS_SOUL_MENU_CLOSED) {
                aos_pickup_icon_restore(&room.pickup_icons);
                aos_audio_play_song(AOS_AUDIO_SONG_LOUD);
                printf("Pause hub closed\n");
            }
            if (soul_menu.open || soul_menu_event) {
                ++step;
                continue;
            }
            /* Object20 +0xa==10: once the story commits and the return pan
             * ends, Mina and Arikado stay in the room and repeat short
             * proximity conversations - Arikado (entity +0x18) plays text
             * 0xE, Mina (+0x14) text 0xD, inside a 0x40px box test
             * (sub_08068AD4) offset by -0x20 from each actor's screen
             * position, gated on the player being controllable. */
            opening_epilogue = opening_needed && opening.complete &&
                               !opening_actors.camera_active &&
                               room.area == 0 && room.number == 0;
            if (opening_epilogue && !opening.active &&
                soma.hp > 0 && soma.anim.id != AOS_SOMA_ANIM_DEATH) {
                int psx = (int)(soma.x >> 16) - cam_x;
                int psy = (int)(soma.y >> 16) - cam_y;
                bool near_any = false;
                int talk = -1;
                /* Arikado (+0x18) is tested before Mina (+0x14), matching
                 * sub_0805D076's order. */
                static const struct { unsigned actor; uint16_t text; } talks[] = {
                    {1, 0xE}, {0, 0xD},
                };
                for (unsigned t = 0; t < 2; ++t) {
                    int ax = (int)(opening_actors.x[talks[t].actor] >> 16) - cam_x - 0x20;
                    int ay = (int)(opening_actors.y[talks[t].actor] >> 16) - cam_y - 0x20;
                    if ((uint16_t)(psx - ax) <= 0x40 &&
                        (uint16_t)(psy - ay) <= 0x40) {
                        near_any = true;
                        if (talk < 0) talk = (int)talks[t].text;
                    }
                }
                /* Re-arm only after Soma leaves both boxes so a finished
                 * conversation cannot retrigger on the next frame. */
                if (!near_any) opening_talk_armed = true;
                if (talk >= 0 && opening_talk_armed) {
                    opening_talk_armed = false;
                    const uint8_t *script = NULL;
                    size_t script_size = 0;
                    if (opening_rom.data &&
                        aos_intro_resources_entry(&opening_resources, 1,
                                                   (uint16_t)talk, &script,
                                                   &script_size) &&
                        aos_opening_event_init(&opening, script,
                                               script_size)) {
                        aos_opening_event_set_resolver(
                            &opening, resolve_opening_text, &opening_resources);
                        opening_actors.soma_animation = AOS_SOMA_ANIM_IDLE;
                    }
                }
                if (opening.active) {
                    ++step;
                    if (has_active_save) active_save.play_frames++;
                    continue;
                }
            }
            uint16_t player_held = forced_frame
                                       ? aria_input.player_held_input
                                       : aos_button_config_remap(&progress.buttons,
                                                                 aria_input.player_held_input);
            uint16_t player_new = forced_frame
                                      ? aria_input.player_new_input
                                      : aos_button_config_remap(&progress.buttons,
                                                                aria_input.player_new_input);
            /* Object20 room dialogue locks gameplay input while its
             * window is open (the host's set_input_lock callback);
             * A/START still reach the poll through
             * aria_input.new_input inside update_entities. */
            if (room_dialogue_input_lock) {
                player_held = 0;
                player_new = 0;
            }
            /* Ability souls 0-4 map directly to Soma's five movement flags.
             * A newly collected ability is enabled by aos_pickup_collect_soul;
             * the pause Ability page can then toggle it. Diagnostic --moves
             * and F1 movement edits deliberately retain their explicit mask. */
            if (!moves_override) soma.moves = progress.abilities[0] & 0x1Fu;
            /* Soma reads gEwramData + 0x13260 (the yellow souls' bits). */
            soma.abilities = progress.flags;
            soma.guardian_alive = guardian.core.active;
            aos_soma_update(&soma, &layer, player_held, player_new);
            /* A door walking Soma through owns the shared bits 0 and 8
             * (unk_42C & 1 and the 0xA074 bit 1 the native check reads). */
            bool door_held = false;
            for (size_t k = 0; k < room.entity_count && !door_held; ++k)
                door_held = room.entities[k].kind == ARIA_KIND_SPECIAL &&
                            room.entities[k].spawned &&
                            ((room.entities[k].kind == ARIA_KIND_SPECIAL &&
                              room.entities[k].id == ARIA_OBJECT_WOODEN_DOOR &&
                              room.entities[k].door.holding) ||
                             (room.entities[k].kind == ARIA_KIND_SPECIAL &&
                              room.entities[k].id == 2 &&
                              room.entities[k].object02.holding));
            /* _0801CCE2, the tail of sub_0801B0D8 before +0x22 advances: the
             * Sherman Ring (accessory 0x2A) grants one experience every
             * eighth tick while LEFT or RIGHT is held, on the ground, not
             * wall-pushed and not door-held (unk_42C bit 0 and the 0xA074
             * bit 1 are clear natively; the pause/menu and transition paths
             * never reach this update). The point goes straight into
             * +0x1328C capped at 0x05F5E0FF, then sub_08033CAC's level loop;
             * a gained level calls sub_080455BC(0) like an enemy reward. */
            if (progress.accessory == 0x2A && !door_held &&
                !(soma.flags & (AOS_FLAG_AIRBORNE | AOS_FLAG_WALL)) &&
                (player_held & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) &&
                ((uint16_t)(soma.ticks - 1) & 7) == 0) {
                if (++progress.experience > AOS_STATS_MAX_EXPERIENCE)
                    progress.experience = AOS_STATS_MAX_EXPERIENCE;
                int32_t gained = aos_stats_level_up(&progress);
                if (gained > 0) {
                    soma.max_hp = (int16_t)progress.max_hp;
                    aos_levelup_start(&combat.levelup);
                    aos_audio_play_song(0xBC);
                    if (combat.equipment) {
                        aos_stats_refresh(&progress, combat.equipment);
                        if (combat.derive_atk) combat.player->atk = progress.atk;
                        if (combat.derive_def) combat.player->def = progress.def;
                    }
                    printf("Level up: level %u, max HP %u\n", progress.level,
                           progress.max_hp);
                }
            }
            /* sub_08021FA4 and sub_08021C44 at the end of Soma's update,
             * then the yellow soul entity (slot 0x11). */
            aos_stats_frame_reset(&progress);
            /* sub_08021994 runs inside the update, before + 0x22 advances:
             * the statuses (stone presses, timers, poison, curse) and the
             * MP regeneration, then sub_08021C44. */
            aos_stats_status_update(&progress, player_new, (uint16_t)(soma.ticks - 1));
            aos_stats_derive(&progress);
            aria_pickup_state.global_flags &= ~0x101u;
            if (door_held) aria_pickup_state.global_flags |= 0x101u;
            if (have_yellow_souls) {
                aos_yellow_soul_update(&yellow_soul, &progress, yellow_souls, soma.hp,
                                       soma.max_hp);
                aos_yellow_soul_regen(&yellow_soul, &progress, yellow_souls, &soma.hp,
                                      soma.max_hp, soma.vx, soma.flags, door_held);
            }
            if (have_equipment && !atk_override) player_stats.atk = progress.atk;
            if (have_equipment && !def_override) player_stats.def = progress.def;
            /* sub_0800DC70: the HUD chases the current values. */
            if (have_hud) {
                aos_hud_update(&hud, soma.hp, soma.max_hp, progress.mp,
                               (int16_t)progress.max_mp);
                aos_hud_tick(&hud);
            }
            /* sub_0800EFD4's default case: unk_422 counts down once per
             * HUD tick and sub_0800EB04 clears the banner at zero. */
            if (gameplay_soul_notice_timer > 0) --gameplay_soul_notice_timer;
            const __typeof__(objects.skills[0]) *bat = &objects.skills[SKILL_ART_BAT],
                                                *archer = &objects.skills[SKILL_ART_ARCHER];
            souls.bat = objects_ok && bat->loaded ? &bat->set : NULL;
            souls.bow = objects_ok && archer->loaded && archer->static_count ? &archer->set : NULL;
            souls.bone = objects_ok && objects.skills[SKILL_ART_BONE].static_count;
            souls.millione = objects_ok && objects.skills[SKILL_ART_BONE].loaded &&
                                      objects.skills[SKILL_ART_BONE].defs[0].count
                                  ? &objects.skills[SKILL_ART_BONE].set
                                  : NULL;
            const __typeof__(objects.skills[0]) *blades = &objects.skills[SKILL_ART_BLADES];
            souls.durga = objects_ok && blade_frame(blades, 2);
            const __typeof__(objects.skills[0]) *flea = &objects.skills[SKILL_ART_FLEA];
            souls.flea = objects_ok && flea->static_count == 3 && flea->kind.probes.count &&
                                 flea->set.count > AOS_FLEA_END_SEQUENCE
                             ? &flea->set
                             : NULL;
            souls.flea_kind = souls.flea ? &flea->kind : NULL;
            souls.flea_still_attack = flea->static_attack;
            const __typeof__(objects.skills[0]) *needles = &objects.skills[SKILL_ART_NEEDLES];
            souls.needles = objects_ok && needles->loaded && needles->static_count == 4 &&
                                    needles->defs[0].count
                                ? &needles->set
                                : NULL;
            const __typeof__(objects.skills[0]) *ripper = &objects.skills[SKILL_ART_RIPPER];
            souls.ripper = objects_ok && ripper->loaded &&
                           blade_frame(ripper, 0) && blade_frame(ripper, 1);
            souls.ripper_kind = objects_ok ? &ripper->kind : NULL;
            souls.ripper_ids = ripper->static_ids;
            souls.ripper_count = objects_ok ? ripper->static_count : 0;
            const __typeof__(objects.skills[0]) *fish_head =
                &objects.skills[SKILL_ART_FISH_HEAD];
            souls.fish_head = objects_ok && fish_head->loaded && fish_head->defs[0].count
                                  ? &fish_head->set
                                  : NULL;
            const __typeof__(objects.skills[0]) *killer_fish =
                &objects.skills[SKILL_ART_KILLER_FISH];
            souls.killer_fish = objects_ok && killer_fish->loaded &&
                                        killer_fish->set.count > 1 &&
                                        killer_fish->defs[0].count && killer_fish->defs[1].count
                                    ? &killer_fish->set
                                    : NULL;
            souls.gladiator = objects_ok && blades->loaded && blades->defs[0].count
                                  ? &blades->set
                                  : NULL;
            souls.gladiator_kind = souls.gladiator ? &blades->kind : NULL;
            const __typeof__(objects.skills[0]) *valkyrie = &objects.skills[SKILL_ART_VALKYRIE];
            souls.valkyrie = objects_ok && valkyrie->loaded && valkyrie->defs[0].count
                                 ? &valkyrie->set
                                 : NULL;
            souls.valkyrie_kind = souls.valkyrie ? &valkyrie->kind : NULL;
            const __typeof__(objects.skills[0]) *skull = &objects.skills[SKILL_ART_SKULL];
            souls.skull = objects_ok && skull->loaded && skull->defs[0].count ? &skull->set : NULL;
            souls.skull_kind = souls.skull ? &skull->kind : NULL;
            const __typeof__(objects.skills[0]) *lightning = &objects.skills[SKILL_ART_LIGHTNING];
            souls.lightning = objects_ok && lightning->loaded && lightning->defs[0].count
                                  ? &lightning->set
                                  : NULL;
            const __typeof__(objects.skills[0]) *werewolf = &objects.skills[SKILL_ART_WEREWOLF];
            souls.werewolf = objects_ok && werewolf->loaded && werewolf->defs[0].count
                                 ? &werewolf->set
                                 : NULL;
            souls.werewolf_kind = souls.werewolf ? &werewolf->kind : NULL;
            const __typeof__(objects.skills[0]) *biphron =
                &objects.skills[SKILL_ART_BIPHRON];
            souls.biphron = objects_ok && biphron->loaded && biphron->defs[0].count
                                ? &biphron->set
                                : NULL;
            souls.biphron_kind = souls.biphron ? &biphron->kind : NULL;
            const __typeof__(objects.skills[0]) *balore =
                &objects.skills[SKILL_ART_BALORE];
            souls.balore = objects_ok && balore->loaded && balore->defs[0].count
                               ? &balore->set
                               : NULL;
            const __typeof__(objects.skills[0]) *kyoma =
                &objects.skills[SKILL_ART_KYOMA_DEMON];
            souls.kyoma_demon = objects_ok && kyoma->loaded && kyoma->defs[0].count
                                    ? &kyoma->set
                                    : NULL;
            const __typeof__(objects.skills[0]) *legion =
                &objects.skills[SKILL_ART_LEGION];
            souls.legion = objects_ok && legion->loaded && legion->defs[0].count
                               ? &legion->set
                               : NULL;
            const __typeof__(objects.skills[0]) *s_witch =
                &objects.skills[SKILL_ART_STUDENT_WITCH];
            souls.student_witch = objects_ok && s_witch->loaded && s_witch->defs[0].count
                                      ? &s_witch->set
                                      : NULL;
            souls.witch_kind = souls.student_witch ? &s_witch->kind : NULL;
            const __typeof__(objects.skills[0]) *slime =
                &objects.skills[SKILL_ART_SLIME];
            souls.slime = objects_ok && slime->loaded && slime->defs[0].count
                              ? &slime->set
                              : NULL;
            const __typeof__(objects.skills[0]) *ukoback =
                &objects.skills[SKILL_ART_UKOBACK];
            souls.ukoback = objects_ok && ukoback->loaded && ukoback->defs[0].count
                                ? &ukoback->set
                                : NULL;
            const __typeof__(objects.skills[0]) *altair =
                &objects.skills[SKILL_ART_ALTAIR];
            souls.altair = objects_ok && altair->loaded && altair->defs[0].count
                               ? &altair->set
                               : NULL;
            const __typeof__(objects.skills[0]) *une =
                &objects.skills[SKILL_ART_UNE];
            souls.une = objects_ok && une->loaded && une->defs[0].count
                            ? &une->set
                            : NULL;
            souls.une_kind = souls.une ? &une->kind : NULL;
            const __typeof__(objects.skills[0]) *mandragora =
                &objects.skills[SKILL_ART_MANDRAGORA];
            souls.mandragora = objects_ok && mandragora->loaded &&
                                       mandragora->defs[0].count
                                   ? &mandragora->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *chronomage =
                &objects.skills[SKILL_ART_CHRONOMAGE];
            souls.chronomage = objects_ok && chronomage->loaded &&
                                       chronomage->defs[0].count
                                   ? &chronomage->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *disc_armor =
                &objects.skills[SKILL_ART_DISC_ARMOR];
            souls.disc_armor = objects_ok && disc_armor->loaded &&
                                       disc_armor->defs[0].count
                                   ? &disc_armor->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *cockatrice =
                &objects.skills[SKILL_ART_COCKATRICE];
            souls.cockatrice = objects_ok && cockatrice->loaded &&
                                       cockatrice->defs[0].count
                                   ? &cockatrice->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *dryad =
                &objects.skills[SKILL_ART_DRYAD];
            souls.dryad = objects_ok && dryad->loaded &&
                                   dryad->defs[0].count
                               ? &dryad->set
                               : NULL;
            const __typeof__(objects.skills[0]) *arachne =
                &objects.skills[SKILL_ART_ARACHNE];
            souls.arachne = objects_ok && arachne->loaded &&
                                    arachne->defs[0].count
                                ? &arachne->set
                                : NULL;
            const __typeof__(objects.skills[0]) *red_minotaur =
                &objects.skills[SKILL_ART_RED_MINOTAUR];
            souls.red_minotaur = objects_ok && red_minotaur->loaded &&
                                       red_minotaur->defs[0].count
                                   ? &red_minotaur->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *nightmare =
                &objects.skills[SKILL_ART_NIGHTMARE];
            souls.nightmare = objects_ok && nightmare->loaded &&
                                      nightmare->defs[0].count
                                  ? &nightmare->set
                                  : NULL;
            const __typeof__(objects.skills[0]) *man_eater =
                &objects.skills[SKILL_ART_MAN_EATER];
            souls.man_eater = objects_ok && man_eater->loaded &&
                                      man_eater->static_count >= 15
                                  ? &man_eater->set
                                  : NULL;
            const __typeof__(objects.skills[0]) *mantle =
                &objects.skills[SKILL_ART_KILLER_MANTLE];
            souls.killer_mantle = objects_ok && mantle->loaded && mantle->defs[0].count
                                      ? &mantle->set
                                      : NULL;
            const __typeof__(objects.skills[0]) *beast =
                &objects.skills[SKILL_ART_WERE_BEAST];
            souls.were_beast = objects_ok && beast->loaded && beast->defs[0].count
                                   ? &beast->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *lord =
                &objects.skills[SKILL_ART_DEMON_LORD];
            souls.demon_lord = objects_ok && lord->loaded && lord->defs[0].count
                                   ? &lord->set
                                   : NULL;
            const __typeof__(objects.skills[0]) *killer_doll =
                &objects.skills[SKILL_ART_KILLER_DOLL];
            souls.killer_doll = objects_ok && killer_doll->loaded &&
                                        killer_doll->set.count > 1 &&
                                        killer_doll->defs[0].count && killer_doll->defs[1].count
                                    ? &killer_doll->set
                                    : NULL;
            const __typeof__(objects.skills[0]) *crow = &objects.skills[SKILL_ART_CROW];
            souls.crow = objects_ok && crow->set.count > AOS_CROW_SEQUENCE &&
                                 crow->defs[AOS_CROW_SEQUENCE].count
                             ? &crow->set
                             : NULL;
            souls.crow_kind = souls.crow ? &crow->kind : NULL;
            const __typeof__(objects.skills[0]) *devil = &objects.skills[SKILL_ART_DEVIL];
            souls.devil = objects_ok && devil->loaded && devil->static_count == 16 ? &devil->set
                                                                                   : NULL;
            souls.harpy = objects_ok && blade_frame(blades, AOS_DAGGER_FRAME) &&
                          blade_frame(blades, 3) && blade_frame(blades, 4) && blade_frame(blades, 5);
            souls.durga_attack = objects.skills[SKILL_ART_BLADES].static_attack[2];
            const __typeof__(objects.skills[0]) *merman = &objects.skills[SKILL_ART_SHARED];
            souls.shared_ids = merman->static_ids;
            souls.shared_count = objects_ok ? merman->static_count : 0;
            souls.archer_ids = archer->static_ids;
            souls.archer_count = objects_ok ? archer->static_count : 0;
            souls.archer_attacks = objects.skills[SKILL_ART_ARCHER].static_attack;
            souls.merman = objects_ok && merman->set.count > AOS_MERMAN_DROP_SEQUENCE &&
                                   merman->defs[AOS_MERMAN_SPOUT_SEQUENCE].count &&
                                   merman->defs[AOS_MERMAN_DROP_SEQUENCE].count
                               ? &merman->set
                               : NULL;
            souls.margins = objects_ok ? (const int16_t(*)[2])objects.enemies[0].kind.margins : NULL;
            guardian.margins = souls.margins;
            souls.cam_x = cam_x;
            souls.cam_y = cam_y;
            souls.layer = &layer;
            souls.effects = room.effects;
            souls.effect_count = &room.effect_count;
            souls.bone_kind = &objects.skills[SKILL_ART_BONE].kind;
            souls.bone_ids = objects.skills[SKILL_ART_BONE].static_ids;
            souls.bone_count = objects.skills[SKILL_ART_BONE].static_count;
            update_skills(&souls);
            guardian_cam_x = cam_x;
            guardian_cam_y = cam_y;
            {
                bool was_active = guardian.core.active;
                guardian.effect_count = 0;
                update_guardian(&guardian);
                for (int k = 0; k < guardian.effect_count && room.effect_count < MAX_EFFECTS; ++k)
                    if (combat.effects[guardian.effects[k].kind])
                        room.effects[room.effect_count++] = guardian.effects[k];
                /* A guardian changing the current stats (the Great Armor)
                 * reaches ATK and DEF after its update. */
                if (have_equipment && !atk_override) player_stats.atk = progress.atk;
                if (have_equipment && !def_override) player_stats.def = progress.def;
                /* sub_0802DD84: an ending guardian clears 0x13260 bits 8-13. */
                if (was_active && !guardian.core.active) progress.flags &= 0xFFFFC0FFu;
            }
            ++step;
            if (has_active_save) active_save.play_frames++;
            if (soma.state == 16 && !announced_death) {
                /* sub_0801AF20 enters the death sub-state (unk_64 = 2):
                 * sub_0800C8C8 fades BG and OBJ to black and its case 0
                 * plays SONG_MUTE. */
                printf("Soma died (frame %ld)\n", step);
                announced_death = true;
                aos_audio_play_song(0x1010);
                aos_death_start(&death);
            }
            if (announced_death && aos_death_update(&death)) {
                printf("Game over (frame %ld)\n", step);
                if (frontend_command[0]) {
                    frontend_command[frontend_count] = "--game-over";
                    frontend_command[frontend_count + 1] = NULL;
                    if (!fusion_host_launch(frontend_command))
                        fprintf(stderr, "Cannot return to the frontend: %s\n", SDL_GetError());
                }
                running = false;
                break;
            }
            /* sub_0801B03C: while Soma dies, a drifting sparkle entity
             * (callback sub_08045C9C) emits one sub_08045CEC(x, y, 0xD, 8)
             * effect every four frames inside an 8x8 pixel box 16..8
             * pixels above the corpse: animation 13, palette nibble 8. */
            if (soma.state == 16 && (step & 3) == 0 &&
                room.effect_count < MAX_EFFECTS &&
                combat.effects[AOS_EFFECT_DEATH_SPARKLE]) {
                room.effects[room.effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_DEATH_SPARKLE, .drift = true,
                    .x = soma.x + (int32_t)(aos_random() % 0x80000u),
                    .y = soma.y + (int32_t)(aos_random() % 0x80000u) - 0x100000,
                };
            }
            /* The editor's doors take Soma once he steps in, then wait for
             * him to step out of every door (no bounce on arrival). */
            const AosProjectDoor *project_door =
                project_door_touched(&room, soma.x >> 16, soma.y >> 16);
            if (!project_door) project_door_latch = false;
            if (project_door_latch) project_door = NULL;
            bool leaving = aos_room_outside(&layer, room.width_screens, room.height_screens,
                                            soma.x >> 16, soma.y >> 16);
            int exited = leaving || project_door
                ? take_exit(renderer, &room, &background, &soma, cam_x, cam_y,
                            leaving ? NULL : project_door)
                : 0;
            if (exited < 0) goto cleanup;
            if (project_door && !exited) project_door_latch = true;
            if (exited > 0) {
                project_door_latch = true;
                /* sub_0800EB04: transitions clear the HUD notice; the
                 * level-up entity dies with the room's entityData. */
                aos_hud_clear_notice(&hud);
                combat.levelup = (AosLevelUp){0};
                clear_skills(&souls);
                layer = room_layer(&room);
                save_room = (AosSaveRoom){0};
            }
            if (aos_save_room_update(&save_room, room_has_save_point(&room), soma.x >> 16,
                                     soma.y >> 16, aria_input.player_held_input)) {
                soma.hp = soma.max_hp;
                player_stats.hp = soma.hp;
                if (has_active_save) {
                    aria_runtime_to_save(&active_save, &room, &soma, &progress);
                    if (have_quick_map) {
                        aos_quick_map_pack_explored(&quick_map,
                                                    active_save.map_explored);
                        active_save.secret_rooms = quick_map.secret_rooms;
                    }
                    char error[256];
                    if (aos_save_store(save_path, &active_save, error, sizeof error))
                        printf("Saved Aria file %u in room %d/%d at %d,%d (frame %ld)\n",
                               active_save.slot + 1u, room.area, room.number, soma.x >> 16,
                               soma.y >> 16, step);
                    else
                        fprintf(stderr, "Cannot write Aria save: %s\n", error);
                } else {
                    printf("Save point restored HP; launch from a file slot to persist it\n");
                }
            }
            /* sub_080426B0 after the player update. */
            aos_combat_tick(&soma.combat);
            follow_camera(&room, soma.x >> 16, soma.y >> 16, &cam_x, &cam_y);
            /* Entity slots: the weapon entity updates before the enemies. */
            if (weapon_loaded) {
                aos_weapon_update(&weapon_entity, &soma, &weapon_sprite.data, &layer, aos_random);
                /* Its impact and casing join the room's effects. */
                for (int k = 0; k < weapon_entity.effect_count && room.effect_count < MAX_EFFECTS;
                     ++k)
                    if (combat.effects[weapon_entity.effects[k].kind])
                        room.effects[room.effect_count++] = weapon_entity.effects[k];
            }
            AosSoulAttack attacks[MAX_SKILLS + GUARDIAN_ATTACKS];
            AriaSkill *owners[MAX_SKILLS + GUARDIAN_ATTACKS];
            int attack_count = soul_attacks(&souls, souls.bat ? &objects.skills[SKILL_ART_BAT].kind : NULL,
                                            attacks, owners);
            int guardian_first = attack_count;
            for (int k = guardian_attacks(&guardian, &attacks[attack_count]); k > 0; --k)
                owners[attack_count++] = NULL;
            aos_enemy_set_soul_attacks(attacks, attack_count);
            /* gEwramData + 0x13168: the Killer Doll soul's decoy, else Soma. */
            {
                const AriaSkill *decoy = NULL;
                for (int k = 0; k < MAX_SKILLS && !decoy; ++k)
                    if (souls.skills[k].alive && souls.skills[k].kind == SKILL_DECOY &&
                        souls.skills[k].started)
                        decoy = &souls.skills[k];
                aos_enemy_set_decoy(decoy != NULL, decoy ? decoy->x : 0, decoy ? decoy->y : 0);
                /* The Nemesis soul clears it. */
                bool none = false;
                for (int k = 0; k < MAX_SKILLS && !none; ++k)
                    none = souls.skills[k].alive && souls.skills[k].kind == SKILL_NEMESIS &&
                           souls.skills[k].started;
                aos_enemy_set_no_target(none);
            }
            homing_room = &room;
            object10_save_ctx = has_active_save ? &active_save : NULL;
            object1c_save_ctx = has_active_save ? &active_save : NULL;
            object1c_save_path = has_active_save ? save_path : NULL;
            update_entities(&room, &layer, &soma, cam_x, cam_y, &forced, &combat,
                            aria_input.new_input);
            if (have_quick_map)
                for (size_t wi = 0; wi < room.entity_count; ++wi) {
                    AriaEntity *wall_e = &room.entities[wi];
                    if (!wall_e->secret_mark) continue;
                    wall_e->secret_mark = false;
                    /* Object08Update / Object09Update: the broken wall's
                     * room position marks its secret-room cell
                     * (sub_08012154 + sub_08012048); record byte 9 is
                     * the direction. */
                    FusionAosRoomDescriptor wall_room;
                    char wall_error[160];
                    if (fusion_aos_read_room_descriptor(
                            &opening_rom, &quick_map.directory,
                            (size_t)room.area, (size_t)room.number, &wall_room,
                            wall_error, sizeof wall_error))
                        aos_quick_map_secret_mark(
                            &quick_map, wall_room.map_x, wall_room.map_y,
                            wall_e->x >> 16, wall_e->y >> 16,
                            (unsigned)((wall_e->param0 >> 8) & 0xFF));
                }
            homing_age();
            aos_enemy_set_soul_attacks(NULL, 0);
            /* sub_0802F8CC: each touch of the ghost's body throws a spirit
             * from the contact point. */
            for (int k = guardian_first; k < attack_count; ++k)
                if (attacks[k].struck && attacks[k].type == AOS_GHOST_ATTACK_TYPE &&
                    guardian.routine == GUARDIAN_GHOST_ROUTINE)
                {
                    int cx, cy;
                    aos_enemy_soul_attack_contact(k, &cx, &cy);
                    aos_ghost_guardian_touch(&guardian.ghost, cx, cy, aos_random);
                }
            /* sub_08031F28: a struck enemy heals Soma 5 HP (once a frame
             * here: the struck flag does not count the enemies). */
            for (int k = guardian_first; k < attack_count; ++k)
                if (attacks[k].struck && guardian.routine == GUARDIAN_PERSEPHONE_ROUTINE &&
                    soma.hp > 0) {
                    soma.hp = (int16_t)(soma.hp + AOS_PERSEPHONE_HEAL);
                    if (soma.hp > soma.max_hp) soma.hp = soma.max_hp;
                }
            for (int k = 0; k < attack_count; ++k)
                if (attacks[k].struck && owners[k]) {
                    owners[k]->struck = true;
                    if (owners[k]->kind == SKILL_RIPPER && !owners[k]->ripper.attached &&
                        attacks[k].target) {
                        owners[k]->ripper_target = attacks[k].target;
                        aos_ripper_soul_attach(&owners[k]->ripper, attacks[k].target->x,
                                               attacks[k].target->y);
                    }
                    if (owners[k]->kind == SKILL_DRYAD) {
                        /* sub_0802D9BC: the first strike spawns the child
                         * (+ 0x20), sets + 0x0D = 0x78 and the 0x1014F8F6
                         * box; later strikes only reset + 0x26. */
                        if (!owners[k]->dryad.planted) {
                            owners[k]->dryad.planted = true;
                            owners[k]->dryad.timer = 0x78;
                        }
                        owners[k]->dryad.counter = 0;
                        owners[k]->dryad_host = attacks[k].target;
                    }
                    if ((owners[k]->kind == SKILL_ARACHNE ||
                         owners[k]->kind == SKILL_MUDMAN) &&
                        !owners[k]->arachne.planted) {
                        /* sub_0802DB84: the first strike spawns the
                         * sub_080239E4 child (+ 0x20) and sets + 0x72
                         * bit 1; later strikes are no-ops. */
                        owners[k]->arachne.planted = true;
                        owners[k]->arachne_host = attacks[k].target;
                    }
                    if (owners[k]->kind == SKILL_MAN_EATER)
                        owners[k]->man_eater.hit = true;
                }
        }

        int sx = soma.x >> 16, sy = soma.y >> 16;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src = {(float)cam_x, (float)cam_y, VIEW_W, VIEW_H}, dst = {0, 0, VIEW_W, VIEW_H};
        {
            /* The BG fade of sub_0800C8C8, relative to the OBJ veil. */
            float bg = announced_death && death.obj.weight > 0
                           ? (float)death.bg.weight / (float)death.obj.weight : 1.0f;
            uint8_t mod = (uint8_t)((bg > 1.0f ? 1.0f : bg) * 255.0f);
            SDL_SetTextureColorMod(background, mod, mod, mod);
            if (hud_texture) SDL_SetTextureColorMod(hud_texture, mod, mod, mod);
        }
        SDL_RenderTexture(renderer, background, &src, &dst);
        /* The save point draws through its SPECIAL id-28 entity
         * (Object1C marker child art) in draw_special_objects —
         * the old fixed-position placeholder is retired. */
        draw_doors(renderer, &room, objects_ok ? &objects : NULL, step, cam_x, cam_y);
        if (objects_ok) draw_walls(renderer, &room, &objects, cam_x, cam_y);
        if (objects_ok) draw_effects(renderer, &room, &objects, AOS_EFFECT_BLOOD, cam_x, cam_y);
        if (objects_ok) draw_enemies(renderer, &room, &objects, cam_x, cam_y);
        if (objects_ok) draw_special_objects(renderer, &room, &objects, cam_x, cam_y, step);
        /* Object20's actors stay in the room for the proximity chatter. */
        if ((opening.active || opening_epilogue) && objects_ok)
            draw_opening_actors(renderer, &objects, &opening_actors, cam_x, cam_y);
        if (opening.active && opening_actors.soul_active && objects_ok) {
            AosPickup soul = {
                .type = AOS_PICKUP_SOUL,
                .x = opening_actors.soul_x,
                .y = opening_actors.soul_y,
                .age = opening_actors.soul_age,
            };
            draw_pickup_art(renderer, &objects, 9, &soul, cam_x, cam_y);
        }
        if (objects_ok) draw_levelup(renderer, &objects, &combat.levelup, &soma, cam_x, cam_y);
        for (int k = AOS_EFFECT_SMOKE; objects_ok && k < AOS_EFFECT_KINDS; ++k)
            draw_effects(renderer, &room, &objects, k, cam_x, cam_y);
        if (objects_ok) draw_effects(renderer, &room, &objects, AOS_EFFECT_FLAME, cam_x, cam_y);
        for (size_t k = 0; objects_ok && k < room.damage_popup_count; ++k)
            draw_damage_popup(renderer, &objects, &room.damage_popups[k], cam_x, cam_y);
        /* Pickups: their sequence from creation, looping; a blinking pickup
         * shows on even frames (the native blink of + 0x0F is not traced). */
        for (size_t k = 0; objects_ok && k < room.pickup_count; ++k) {
            const AosPickup *pickup = &room.pickups[k];
            if (pickup->blinking && (pickup->age & 1)) continue;
            if (pickup->soul_container) {
                unsigned type = pickup->type - AOS_PICKUP_SOUL;
                if (type < 4) {
                    draw_pickup_art(renderer, &objects, PICKUP_ART_SOUL_CONTAINER + (int)type,
                                    pickup, cam_x, cam_y);
                    if (!pickup->container_released)
                        draw_pickup_art(renderer, &objects,
                                        PICKUP_ART_SOUL_CONTAINER_FOLLOWER + (int)type, pickup,
                                        cam_x, cam_y);
                }
                continue;
            }
            if (pickup->type >= AOS_PICKUP_ITEM && pickup->type <= AOS_PICKUP_ARMOR) {
                /* sub_0801232C's icon on the frames of 0x080E10F0. */
                unsigned icon = pickup->icon_slot >= 0 &&
                                        pickup->icon_slot < AOS_PICKUP_ICON_SLOTS &&
                                        room.pickup_icons.active[pickup->icon_slot] ==
                                            pickup->icon_id
                                    ? pickup->icon_id : 0;
                const AriaFrame *image = icon < ITEM_ICONS ? &objects.icons[icon] : NULL;
                if (!icon || !image || !image->texture) continue;
                SDL_FRect rect = {(float)((pickup->x >> 16) + image->offset_x - cam_x),
                                  (float)((pickup->y >> 16) + image->offset_y - cam_y), image->w,
                                  image->h};
                SDL_RenderTexture(renderer, image->texture, NULL, &rect);
                continue;
            }
            int index = pickup_art(pickup);
            draw_pickup_art(renderer, &objects, index, pickup, cam_x, cam_y);
        }
        for (int k = 0; objects_ok && k < MAX_SKILLS; ++k) {
            const AriaSkill *skill = &souls.skills[k];
            if (!skill->alive || !skill->started || skill->anim.frame >= MAX_FRAMES) continue;
            /* The arrow blinks (+ 0x0F) while it counts down. */
            if (skill->kind == SKILL_ARROW && skill->state == 2 && (step & 1)) continue;
            const __typeof__(objects.skills[0]) *shared = &objects.skills[SKILL_ART_SHARED];
            int still = static_index(shared->static_ids, shared->static_count,
                                     skill->kind == SKILL_SPEAR ? 0 : skill->parameter);
            if (skill->kind == SKILL_SPEAR_TIP || skill->kind == SKILL_HARPY ||
                skill->kind == SKILL_DEVIL || skill->kind == SKILL_MILLIONE_EMITTER)
                continue;
            if (skill->kind == SKILL_NEEDLES && skill->needles.struck) {
                /* sub_08028018: the four pieces, alternating by parity. */
                for (int k = 0; k < AOS_NEEDLES_PIECES; ++k) {
                    int32_t px, py;
                    const AriaFrame *image = blade_frame(&objects.skills[SKILL_ART_NEEDLES], k);
                    if (!image || !image->texture || !aos_needles_piece(&skill->needles, k, &px, &py))
                        continue;
                    float ix = skill->facing_left
                                   ? (float)((px >> 16) - image->offset_x) - image->w
                                   : (float)((px >> 16) + image->offset_x);
                    SDL_FRect piece = {ix - (float)cam_x,
                                       (float)((py >> 16) + image->offset_y - cam_y), image->w,
                                       image->h};
                    SDL_RenderTextureRotated(renderer, image->texture, NULL, &piece, 0, NULL,
                                             skill->facing_left ? SDL_FLIP_HORIZONTAL
                                                                : SDL_FLIP_NONE);
                }
                continue;
            }
            if (skill->kind == SKILL_DAGGER) {
                /* sub_0802DC60: frames 3-5 at the history's entries 1-3
                 * (the trail entity's own facing is the dagger's). */
                for (int t = AOS_DAGGER_TRAIL_IMAGES - 1; t >= 0; --t) {
                    const AriaFrame *image = blade_frame(&objects.skills[SKILL_ART_BLADES],
                                                         AOS_DAGGER_TRAIL_FIRST_FRAME + t);
                    if (!image || !image->texture) continue;
                    int tx = skill->dagger_trail.history[t + 1][0];
                    int ty = skill->dagger_trail.history[t + 1][1];
                    float ix = skill->facing_left ? (float)(tx - image->offset_x) - image->w
                                                  : (float)(tx + image->offset_x);
                    SDL_FRect trail_rect = {ix - (float)cam_x, (float)(ty + image->offset_y - cam_y),
                                            image->w, image->h};
                    SDL_RenderTextureRotated(renderer, image->texture, NULL, &trail_rect, 0, NULL,
                                             skill->facing_left ? SDL_FLIP_HORIZONTAL
                                                                : SDL_FLIP_NONE);
                }
                if (skill->dagger.blinking && (step & 1)) continue;
            }
            if (skill->kind == SKILL_SPEAR && skill->spear.blinking && (step & 1)) continue;
            if (skill->kind == SKILL_FLEA && skill->flea.blinking && (step & 1)) continue;
            if (skill->kind == SKILL_VALKYRIE && skill->valkyrie.blinking && (step & 1)) continue;
            if (skill->kind == SKILL_GLADIATOR && skill->gladiator.blinking && (step & 1))
                continue;
            if (skill->kind == SKILL_KILLER_FISH && skill->killer_fish.hidden) continue;
            if (skill->kind == SKILL_RIPPER && skill->ripper.blink == 2 && (step & 1)) continue;
            if (skill->kind == SKILL_THROW && skill->thrown.blinking && (step & 1)) continue;
            if (skill->kind == SKILL_DECOY && skill->decoy.blinking && (step & 1)) continue;
            if (skill->kind == SKILL_NEMESIS && skill->nemesis.blinking && (step & 1)) continue;
            const AriaFrame *art =
                skill->kind == SKILL_BAT     ? &objects.skills[SKILL_ART_BAT].frames[0][skill->anim.frame]
                : skill->kind == SKILL_BOW   ? &objects.skills[SKILL_ART_ARCHER].frames[0][skill->anim.frame]
                : skill->kind == SKILL_ARROW ? &objects.skills[SKILL_ART_ARCHER].statics[0]
                : skill->kind == SKILL_THROW
                    ? (skill->thrown.hidden
                           ? NULL
                           : blade_frame(&objects.skills[
                                             souls.throws[skill->parameter].resource == 0
                                                 ? SKILL_ART_SHARED
                                                 : souls.throws[skill->parameter].resource == 1
                                                       ? SKILL_ART_RIPPER
                                                       : souls.throws[skill->parameter].resource ==
                                                                 18
                                                             ? SKILL_ART_ARCHER
                                                             : SKILL_ART_BONE],
                                         souls.throws[skill->parameter].frame))
                : skill->kind == SKILL_SHOT || skill->kind == SKILL_DAGGER
                    ? blade_frame(&objects.skills[SKILL_ART_BLADES],
                                  skill->kind == SKILL_SHOT ? 2 : AOS_DAGGER_FRAME)
                : skill->kind == SKILL_NEEDLES
                    ? &objects.skills[SKILL_ART_NEEDLES].frames[0][skill->anim.frame]
                : skill->kind == SKILL_RIPPER
                    ? blade_frame(&objects.skills[SKILL_ART_RIPPER], skill->parameter)
                : skill->kind == SKILL_MILLIONE_CHILD
                    ? &objects.skills[SKILL_ART_BONE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_FISH_HEAD
                    ? &objects.skills[SKILL_ART_FISH_HEAD].frames[0][skill->anim.frame]
                : skill->kind == SKILL_KILLER_FISH
                    ? &objects.skills[SKILL_ART_KILLER_FISH]
                           .frames[skill->anim.id][skill->anim.frame]
                : skill->kind == SKILL_GLADIATOR
                    ? &objects.skills[SKILL_ART_BLADES].frames[0][skill->anim.frame]
                : skill->kind == SKILL_VALKYRIE
                    ? (skill->valkyrie.hidden
                           ? NULL
                           : &objects.skills[SKILL_ART_VALKYRIE].frames[0][skill->anim.frame])
                : skill->kind == SKILL_WEREWOLF
                    ? &objects.skills[SKILL_ART_WEREWOLF].frames[0][skill->anim.frame]
                : skill->kind == SKILL_LIGHTNING
                    ? &objects.skills[SKILL_ART_LIGHTNING].frames[0][skill->anim.frame]
                : skill->kind == SKILL_DECOY
                    ? &objects.skills[SKILL_ART_KILLER_DOLL]
                           .frames[skill->anim.id][skill->anim.frame]
                : skill->kind == SKILL_SKULL
                    ? &objects.skills[SKILL_ART_SKULL].frames[0][skill->anim.frame]
                : skill->kind == SKILL_CROW
                    ? &objects.skills[SKILL_ART_CROW].frames[AOS_CROW_SEQUENCE][skill->anim.frame]
                : skill->kind == SKILL_FLEA
                    ? (skill->flea.state == 1
                           ? blade_frame(&objects.skills[SKILL_ART_FLEA], skill->flea.frame)
                           : &objects.skills[SKILL_ART_FLEA]
                                  .frames[AOS_FLEA_END_SEQUENCE][skill->anim.frame])
                : skill->kind == SKILL_GHOST ? NULL
                : skill->kind == SKILL_FLAME_DEMON || skill->kind == SKILL_FLAME_BOLT
                    ? &objects.skills[SKILL_ART_FISH_HEAD].frames[0][skill->anim.frame]
                : skill->kind == SKILL_DEMON_LORD || skill->kind == SKILL_FLAME_SPIRIT
                    ? &objects.skills[SKILL_ART_DEMON_LORD].frames[0][skill->anim.frame]
                : skill->kind == SKILL_BIPHRON || skill->kind == SKILL_BIPHRON_TOTEM
                    ? &objects.skills[SKILL_ART_BIPHRON].frames[0][skill->anim.frame]
                : skill->kind == SKILL_BALORE
                    ? &objects.skills[SKILL_ART_BALORE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_KYOMA_DEMON
                    ? &objects.skills[SKILL_ART_KYOMA_DEMON].frames[0][skill->anim.frame]
                : skill->kind == SKILL_LEGION
                    ? &objects.skills[SKILL_ART_LEGION].frames[0][skill->anim.frame]
                : skill->kind == SKILL_STUDENT_WITCH
                    ? &objects.skills[SKILL_ART_STUDENT_WITCH].frames[0][skill->anim.frame]
                : skill->kind == SKILL_SLIME || skill->kind == SKILL_SLIME_TRAIL
                    ? &objects.skills[SKILL_ART_SLIME].frames[0][skill->anim.frame]
                : skill->kind == SKILL_UKOBACK
                    ? &objects.skills[SKILL_ART_UKOBACK].frames[0][skill->anim.frame]
                : skill->kind == SKILL_ALTAIR
                    ? &objects.skills[SKILL_ART_ALTAIR].frames[0][skill->anim.frame]
                : skill->kind == SKILL_UNE
                    ? &objects.skills[SKILL_ART_UNE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_MANDRAGORA
                    ? &objects.skills[SKILL_ART_MANDRAGORA].frames[0][skill->anim.frame]
                : skill->kind == SKILL_CHRONOMAGE
                    ? &objects.skills[SKILL_ART_CHRONOMAGE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_DISC_ARMOR
                    ? &objects.skills[SKILL_ART_DISC_ARMOR].frames[0][skill->anim.frame]
                : skill->kind == SKILL_COCKATRICE
                    ? &objects.skills[SKILL_ART_COCKATRICE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_DRYAD
                    ? &objects.skills[SKILL_ART_DRYAD].frames[0][0]
                : skill->kind == SKILL_ARACHNE || skill->kind == SKILL_MUDMAN
                    ? &((skill->kind == SKILL_ARACHNE
                             ? &objects.skills[SKILL_ART_ARACHNE]
                             : &objects.skills[SKILL_ART_FISH_HEAD])
                           ->frames[skill->anim.id][skill->anim.frame])
                : skill->kind == SKILL_ARACHNE_SHARD
                    ? &objects.skills[SKILL_ART_FISH_HEAD].frames[1][0]
                : skill->kind == SKILL_RED_MINOTAUR
                    ? &objects.skills[SKILL_ART_RED_MINOTAUR].frames[0][skill->anim.frame]
                : skill->kind == SKILL_MINOTAUR_BLADE
                    ? &objects.skills[SKILL_ART_RED_MINOTAUR].frames[0][0]
                : skill->kind == SKILL_NIGHTMARE
                    ? &objects.skills[SKILL_ART_NIGHTMARE].frames[0][skill->anim.frame]
                : skill->kind == SKILL_MAN_EATER
                    ? (skill->man_eater.frame <
                               objects.skills[SKILL_ART_MAN_EATER].static_count
                           ? &objects.skills[SKILL_ART_MAN_EATER]
                                  .statics[skill->man_eater.frame]
                           : NULL)
                : skill->kind == SKILL_NEMESIS
                    ? &objects.skills[SKILL_ART_DEVIL]
                           .frames[AOS_NEMESIS_SOUL_SEQUENCE][skill->anim.frame]
                : skill->kind == SKILL_GHOST_SOUL
                    ? &objects.skills[SKILL_ART_DEVIL]
                           .frames[AOS_GHOST_SEQUENCE][skill->anim.frame]
                : skill->kind == SKILL_SIREN
                    ? blade_frame(shared, skill->siren.frame)
                : skill->kind == SKILL_DEVIL_FIRE
                    ? (skill->fire.state <= 1
                           ? &objects.skills[SKILL_ART_DEVIL].frames[0][skill->anim.frame]
                           : blade_frame(&objects.skills[SKILL_ART_DEVIL],
                                         aos_devil_fire_frame(&skill->fire)))
                : skill->kind == SKILL_AXE || skill->kind == SKILL_AXE_TRAIL ||
                        skill->kind == SKILL_SPEAR
                    ? (still < 0 ? NULL : &shared->statics[still])
                    : &shared->frames[skill->anim.id][skill->anim.frame];
            if (!art || !art->texture) continue;
            int px = skill->x >> 16, py = skill->y >> 16;
            float x = skill->facing_left ? (float)(px - art->offset_x) - art->w
                                         : (float)(px + art->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(py + art->offset_y - cam_y), art->w,
                              art->h};
            /* The throw's affine matrix (sub_0803E058: pa = cos, pb = sin,
             * pc = -sin) turns the sprite clockwise for a positive angle, as
             * SDL does; the turn is about the frame's centre. */
            double angle = skill->kind == SKILL_THROW   ? skill->thrown.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_AXE ? skill->axe.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_SPEAR
                               ? (uint16_t)skill->spear.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_AXE_TRAIL
                               ? (uint16_t)skill->trail.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_RIPPER
                               ? (uint16_t)skill->ripper.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_GLADIATOR
                               ? skill->gladiator.angle * 360.0 / 65536.0
                           : skill->kind == SKILL_RED_MINOTAUR ||
                                     skill->kind == SKILL_MINOTAUR_BLADE
                               ? (skill->kind == SKILL_RED_MINOTAUR
                                      ? skill->red_minotaur.angle
                                      : skill->minotaur_blade.angle) *
                                     360.0 / 65536.0
                               : 0;
            /* The afterimages are semi-transparent objects (+ 0x5A mode 1);
             * the blend weights in force are not traced: half here. */
            if (skill->kind == SKILL_AXE_TRAIL) SDL_SetTextureAlphaMod(art->texture, 128);
            bool vertical_flip = (skill->kind == SKILL_KILLER_FISH &&
                                  skill->killer_fish.vertical_flip) ||
                                 (skill->kind == SKILL_LIGHTNING && skill->lightning.vflip);
            if (vertical_flip)
                rect.y = (float)(py - art->offset_y - cam_y) - art->h;
            SDL_FlipMode flip = (SDL_FlipMode)(
                (skill->facing_left ? SDL_FLIP_HORIZONTAL : 0) |
                (vertical_flip ? SDL_FLIP_VERTICAL : 0));
            SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, angle, NULL, flip);
            if (skill->kind == SKILL_AXE_TRAIL) SDL_SetTextureAlphaMod(art->texture, 255);
        }
        /* The guardian entity (Cagnazzo's sequence 0). */
        if (guardian.core.active && guardian.routine == GUARDIAN_CAGNAZZO_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_CAGNAZZO].frames[0][guardian.anim.frame];
            if (art->texture) {
                const AosCagnazzoGuardian *cagnazzo = &guardian.cagnazzo;
                float x = cagnazzo->facing_left ? (float)(cagnazzo->x - art->offset_x) - art->w
                                                : (float)(cagnazzo->x + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x,
                                  (float)(cagnazzo->y + art->offset_y - cam_y), art->w, art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         cagnazzo->facing_left ? SDL_FLIP_HORIZONTAL
                                                               : SDL_FLIP_NONE);
            }
        }
        /* Death's sickles: translucent, their affine scale and spin
         * (sub_0803E058, about their position). */
        for (int k = 0; guardian.core.active && guardian.routine == GUARDIAN_DEATH_ROUTINE &&
                        k < AOS_DEATH_SICKLES;
             ++k) {
            const AosDeathSickle *s = &guardian.death.sickles[k];
            if (!s->alive || s->state == 0 || s->scale <= 0x400 ||
                s->anim.frame >= AOS_ENEMY_MAX_FRAMES)
                continue;
            const AriaFrame *art = &objects.skills[SKILL_ART_DEATH].frames[0][s->anim.frame];
            if (!art->texture) continue;
            float zoom = (float)s->scale / 65536.0f;
            int sx = s->x >> 16, sy = s->y >> 16;
            float ox = s->mirrored ? -(float)art->offset_x - art->w : (float)art->offset_x;
            SDL_FRect rect = {(float)(sx - cam_x) + ox * zoom,
                              (float)(sy - cam_y) + (float)art->offset_y * zoom, art->w * zoom,
                              art->h * zoom};
            SDL_FPoint centre = {(float)(sx - cam_x) - rect.x, (float)(sy - cam_y) - rect.y};
            SDL_SetTextureAlphaMod(art->texture, 128);
            SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect,
                                     (double)s->angle * 360.0 / 65536.0, &centre,
                                     s->mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            SDL_SetTextureAlphaMod(art->texture, 255);
        }
        /* Alura Une (translucent, its affine scale about its position). */
        if (guardian.core.active && guardian.routine == GUARDIAN_ALURA_UNE_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES &&
            guardian.alura_une.scale_x > 0x400) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_ALURA_UNE].frames[0][guardian.anim.frame];
            const AosAluraUneGuardian *fairy = &guardian.alura_une;
            if (art->texture) {
                float zx = (float)fairy->scale_x / 65536.0f, zy = (float)fairy->scale_y / 65536.0f;
                float ox = fairy->facing_left ? -(float)art->offset_x - art->w : (float)art->offset_x;
                SDL_FRect rect = {(float)(fairy->x - cam_x) + ox * zx,
                                  (float)(fairy->y - cam_y) + (float)art->offset_y * zy,
                                  art->w * zx, art->h * zy};
                SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         fairy->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* The Shadow Knight (translucent; stretched in by its affine scale
         * about its position while + 0x28 / + 0x2C are set). */
        if (guardian.core.active &&
            (guardian.routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE ||
             guardian.routine == GUARDIAN_BIG_GOLEM_ROUTINE) &&
            guardian.core.state && guardian.anim.id < AOS_ENEMY_MAX_ANIMS &&
            guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            int a = guardian.routine == GUARDIAN_BIG_GOLEM_ROUTINE ? SKILL_ART_BIG_GOLEM
                                                                  : SKILL_ART_SHADOW_KNIGHT;
            const AriaFrame *art = &objects.skills[a].frames[guardian.anim.id][guardian.anim.frame];
            const AosShadowKnightGuardian *knight = &guardian.shadow_knight;
            float zx = knight->scale_x ? (float)knight->scale_x / 65536.0f : 1.0f;
            float zy = knight->scale_y ? (float)knight->scale_y / 65536.0f : 1.0f;
            if (art->texture && (!knight->scale_x || knight->scale_x > 0x400)) {
                float ox = knight->mirrored ? -(float)art->offset_x - art->w : (float)art->offset_x;
                SDL_FRect rect = {(float)(knight->x - cam_x) + ox * zx,
                                  (float)(knight->y - cam_y) + (float)art->offset_y * zy,
                                  art->w * zx, art->h * zy};
                SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         knight->mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* The Great Armor (translucent, sequence 1). */
        if (guardian.core.active && guardian.routine == GUARDIAN_GREAT_ARMOR_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_GREAT_ARMOR].frames[1][guardian.anim.frame];
            if (art->texture) {
                const AosGreatArmorGuardian *armor = &guardian.great_armor;
                float x = armor->facing_left ? (float)(armor->x - art->offset_x) - art->w
                                             : (float)(armor->x + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(armor->y + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         armor->facing_left ? SDL_FLIP_HORIZONTAL
                                                            : SDL_FLIP_NONE);
                SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* Persephone (translucent; blinking on odd frames while leaving). */
        if (guardian.core.active && guardian.routine == GUARDIAN_PERSEPHONE_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES &&
            !(guardian.persephone.blinking && (step & 1))) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_PERSEPHONE].frames[0][guardian.anim.frame];
            if (art->texture) {
                const AosPersephoneGuardian *maid = &guardian.persephone;
                float x = maid->facing_left ? (float)(maid->x - art->offset_x) - art->w
                                            : (float)(maid->x + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(maid->y + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         maid->facing_left ? SDL_FLIP_HORIZONTAL
                                                           : SDL_FLIP_NONE);
                SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* The Flying Armor (translucent, + 0x59 bit 1: half alpha). */
        if (guardian.core.active && guardian.routine == GUARDIAN_FLYING_ARMOR_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_FLYING_ARMOR].frames[0][guardian.anim.frame];
            if (art->texture) {
                const AosFlyingArmorGuardian *armor = &guardian.flying_armor;
                float x = armor->facing_left ? (float)(armor->x - art->offset_x) - art->w
                                             : (float)(armor->x + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(armor->y + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         armor->facing_left ? SDL_FLIP_HORIZONTAL
                                                            : SDL_FLIP_NONE);
                SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* The Devil and Manticore sparks (translucent). */
        for (int k = 0; guardian.routine == GUARDIAN_TRANSFORM_ROUTINE && k < 7; ++k) {
            const __typeof__(guardian.sparks[0]) *spark = &guardian.sparks[k];
            if (!spark->alive || spark->anim.frame >= AOS_ENEMY_MAX_FRAMES) continue;
            const AriaFrame *art =
                &objects.skills[SKILL_ART_DEVIL_SPARK].frames[spark->sequence][spark->anim.frame];
            if (!art->texture) continue;
            int sx = spark->x >> 16, sy = spark->y >> 16;
            float x = spark->facing_left ? (float)(sx - art->offset_x) - art->w
                                         : (float)(sx + art->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(sy + art->offset_y - cam_y), art->w,
                              art->h};
            SDL_SetTextureAlphaMod(art->texture, 128);
            SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                     spark->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            SDL_SetTextureAlphaMod(art->texture, 255);
        }
        /* Curly's shell around the transformed Soma. */
        if (guardian.core.active && guardian.routine == GUARDIAN_TRANSFORM_ROUTINE &&
            guardian.curly.child && guardian.curly.variant == AOS_CURLY_VARIANT &&
            guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_CURLY].frames[0][guardian.anim.frame];
            if (art->texture) {
                int cx = guardian.curly.x >> 16, cy = guardian.curly.y >> 16;
                float x = guardian.curly.facing_left ? (float)(cx - art->offset_x) - art->w
                                                     : (float)(cx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(cy + art->offset_y - cam_y), art->w,
                                  art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         guardian.curly.facing_left ? SDL_FLIP_HORIZONTAL
                                                                    : SDL_FLIP_NONE);
            }
        }
        /* The Giant Ghost (translucent, its affine scale + 0x28: drawn
         * that many times as large about its centre) or the Student Witch,
         * on Soma with his facing. */
        if (guardian.core.active && guardian.routine == GUARDIAN_GHOST_ROUTINE &&
            guardian.core.state && guardian.ghost.scale > 0 &&
            guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            int a = guardian.ghost.giant ? SKILL_ART_GIANT_GHOST : SKILL_ART_WITCH;
            int seq = guardian.ghost.giant ? 1 : 0;
            const AriaFrame *art = &objects.skills[a].frames[seq][guardian.anim.frame];
            if (art->texture) {
                int gx = guardian.ghost.x >> 16, gy = guardian.ghost.y >> 16;
                bool right = !guardian.ghost.facing_left;
                float x = right ? (float)(gx - art->offset_x) - art->w : (float)(gx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(gy + art->offset_y - cam_y), art->w,
                                  art->h};
                float k = (float)guardian.ghost.scale / 65536.0f;
                float w = rect.w * k, h = rect.h * k;
                rect.x += (rect.w - w) / 2;
                rect.y += (rect.h - h) / 2;
                rect.w = w;
                rect.h = h;
                if (guardian.ghost.giant) SDL_SetTextureAlphaMod(art->texture, 128);
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                if (guardian.ghost.giant) SDL_SetTextureAlphaMod(art->texture, 255);
            }
        }
        /* The Imp familiar (blinking while it leaves). */
        if (guardian.core.active && guardian.routine == GUARDIAN_IMP_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES &&
            !(guardian.imp.blinking && (step & 1))) {
            const AriaFrame *art = &objects.skills[SKILL_ART_IMP].frames[0][guardian.anim.frame];
            if (art->texture) {
                int ix = guardian.imp.x >> 16, iy = guardian.imp.y >> 16;
                float x = guardian.imp.mirrored ? (float)(ix - art->offset_x) - art->w
                                                : (float)(ix + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(iy + art->offset_y - cam_y), art->w,
                                  art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         guardian.imp.mirrored ? SDL_FLIP_HORIZONTAL
                                                               : SDL_FLIP_NONE);
            }
        }
        /* SkillSkyFishUse's shoulder pet: on Soma with his facing. */
        if (guardian.core.active && guardian.routine == GUARDIAN_SKY_FISH_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_SKY_FISH].frames[0][guardian.anim.frame];
            if (art->texture) {
                int fx = guardian.sky_fish.x >> 16, fy = guardian.sky_fish.y >> 16;
                float x = guardian.sky_fish.facing_left
                              ? (float)(fx - art->offset_x) - art->w
                              : (float)(fx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(fy + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         guardian.sky_fish.facing_left ? SDL_FLIP_HORIZONTAL
                                                                        : SDL_FLIP_NONE);
            }
        }
        /* SkillCreakingSkullUse: the anchor above Soma and its three
         * skulls on the shared record; children 0/1 draw the
         * 0x080E1AF8 sequence pick {2, 1}, child 2 sequence 0. */
        if (guardian.core.active && guardian.routine == GUARDIAN_CREAKING_SKULL_ROUTINE) {
            const AriaGuardian *g = &guardian;
            for (int k = -1; k < AOS_CREAKING_SKULL_CHILDREN; ++k) {
                int32_t px;
                int32_t py;
                bool flip;
                int seq;
                if (k < 0) {
                    if (!g->core.state || g->anim.frame >= AOS_ENEMY_MAX_FRAMES) continue;
                    px = g->creaking_skull.x; py = g->creaking_skull.y;
                    flip = g->creaking_skull.facing_left; seq = 0;
                } else {
                    const AosCreakingChild *child = &g->creaking_skull.children[k];
                    if (child->dead || child->state == 0) continue;
                    px = child->x; py = child->y; flip = child->facing_left;
                    seq = k == 2 ? 0 : (k == 0 ? 2 : 1);
                }
                const AriaFrame *art =
                    &objects.skills[SKILL_ART_CREAKING_SKULL].frames[seq]
                         [k < 0 ? g->anim.frame : 0];
                if (!art->texture) continue;
                int cx = px >> 16, cy = py >> 16;
                float x = flip ? (float)(cx - art->offset_x) - art->w
                               : (float)(cx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(cy + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            }
        }
        /* SkillAlastorUse: the parked guardian whirls in place — the
         * native sub_0803E058 affine turns it by + 0x2C and scales it
         * by + 0x28; approximated by a rotated, scaled blit. */
        if (guardian.core.active && guardian.routine == GUARDIAN_ALASTOR_ROUTINE &&
            guardian.core.state && guardian.alastor.state &&
            guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_ALASTOR].frames[0][guardian.anim.frame];
            if (art->texture) {
                int ax = guardian.alastor.x >> 16, ay = guardian.alastor.y >> 16;
                float x = guardian.alastor.facing_left
                              ? (float)(ax - art->offset_x) - art->w
                              : (float)(ax + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x,
                                  (float)(ay + art->offset_y - cam_y), art->w, art->h};
                float k = (float)guardian.alastor.scale / 65536.0f;
                float w = rect.w * k, h = rect.h * k;
                rect.x += (rect.w - w) / 2;
                rect.y += (rect.h - h) / 2;
                rect.w = w;
                rect.h = h;
                float deg = (float)(uint16_t)guardian.alastor.angle * 360.0f / 65536.0f;
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, deg, NULL,
                                         guardian.alastor.facing_left ? SDL_FLIP_HORIZONTAL
                                                                      : SDL_FLIP_NONE);
            }
        }
        /* SkillFinalGuardUse: the guard trailing Soma at y + 4 with
         * his facing (sub_0803F17C steps its sequence). */
        if (guardian.core.active && guardian.routine == GUARDIAN_FINAL_GUARD_ROUTINE &&
            guardian.final_guard.phase == 2 &&
            guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art =
                &objects.skills[SKILL_ART_FINAL_GUARD].frames[0][guardian.anim.frame];
            if (art->texture) {
                int gx = guardian.final_guard.x >> 16, gy = guardian.final_guard.y >> 16;
                float x = guardian.final_guard.facing_left
                              ? (float)(gx - art->offset_x) - art->w
                              : (float)(gx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(gy + art->offset_y - cam_y),
                                  art->w, art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         guardian.final_guard.facing_left
                                             ? SDL_FLIP_HORIZONTAL
                                             : SDL_FLIP_NONE);
            }
        }
        /* Catoblepas' clouds (sub_08030D6C's sequence 0). */
        if (guardian.core.active && (guardian.routine == GUARDIAN_CATOBLEPAS_ROUTINE ||
                                     guardian.routine == GUARDIAN_PILLAR_ROUTINE)) {
            int gas_art = guardian.routine == GUARDIAN_PILLAR_ROUTINE ? SKILL_ART_PILLAR
                                                                      : SKILL_ART_CATOBLEPAS;
            for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
                const AosCatoblepasGas *gas = &guardian.catoblepas.gases[k];
                if (!gas->alive || guardian.gas_anims[k].frame >= AOS_ENEMY_MAX_FRAMES) continue;
                const AriaFrame *art = &objects.skills[gas_art]
                                            .frames[0][guardian.gas_anims[k].frame];
                if (!art->texture) continue;
                int gx = gas->x >> 16, gy = gas->y >> 16;
                float x = gas->facing_left ? (float)(gx - art->offset_x) - art->w
                                           : (float)(gx + art->offset_x);
                SDL_FRect rect = {x - (float)cam_x, (float)(gy + art->offset_y - cam_y), art->w,
                                  art->h};
                SDL_RenderTextureRotated(renderer, art->texture, NULL, &rect, 0, NULL,
                                         gas->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            }
        }
        /* Buer's two flames (sub_0802FDE4's sequence 0, unmirrored). */
        if (guardian.core.active && guardian.routine == GUARDIAN_BUER_ROUTINE &&
            guardian.core.state && guardian.anim.frame < AOS_ENEMY_MAX_FRAMES) {
            const AriaFrame *art = &objects.skills[SKILL_ART_BUER].frames[0][guardian.anim.frame];
            for (int k = 0; art->texture && k < AOS_BUER_FLAMES; ++k) {
                const AosBuerFlame *flame = &guardian.buer.flames[k];
                SDL_FRect rect = {(float)((flame->x >> 16) + art->offset_x - cam_x),
                                  (float)((flame->y >> 16) + art->offset_y - cam_y), art->w,
                                  art->h};
                SDL_RenderTexture(renderer, art->texture, NULL, &rect);
            }
        }
        if (show_hitboxes) {
            /* The pickups' sprites are not exported: their boxes only. */
            SDL_SetRenderDrawColor(renderer, 240, 210, 60, 255);
            for (size_t i = 0; i < room.pickup_count; ++i) {
                const AosPickup *pickup = &room.pickups[i];
                if (pickup->blinking && (step & 1)) continue;
                AosBox shape = pickup->soul_container ? aos_soul_container_box
                                                       : aos_pickup_box_of(pickup);
                SDL_FRect box = {(float)((pickup->x >> 16) + shape.x - cam_x),
                                 (float)((pickup->y >> 16) + shape.y - cam_y), shape.w, shape.h};
                SDL_RenderRect(renderer, &box);
            }
            AosRect hurt = aos_player_rect(soma.hurtbox, soma.x >> 16, soma.y >> 16,
                                           soma.facing_left, false);
            SDL_FRect r = {(float)(hurt.x1 - cam_x), (float)(hurt.y1 - cam_y),
                           (float)(hurt.x2 - hurt.x1), (float)(hurt.y2 - hurt.y1)};
            SDL_SetRenderDrawColor(renderer, 80, 230, 120, 255);
            SDL_RenderRect(renderer, &r);
            for (size_t i = 0; enemies_ok && i < room.entity_count; ++i) {
                const AriaEntity *e = &room.entities[i];
                if (!e->spawned || e->kind != ARIA_KIND_ENEMY || e->enemy.removed ||
                    e->id >= ENEMY_KINDS)
                    continue;
                if (e->enemy.own_boxes) {
                    /* Code-driven boxes (grenades): the attack box in red. */
                    AosRect a = aos_entity_rect(e->enemy.own_attack, e->enemy.x >> 16,
                                                e->enemy.y >> 16, e->enemy.mirrored, false);
                    SDL_FRect ar = {(float)(a.x1 - cam_x), (float)(a.y1 - cam_y),
                                    (float)(a.x2 - a.x1 + 1), (float)(a.y2 - a.y1 + 1)};
                    SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
                    if (!e->enemy.combat.attack_off) SDL_RenderRect(renderer, &ar);
                    continue;
                }
                if (e->enemy.anim.id >= AOS_ENEMY_MAX_ANIMS ||
                    e->enemy.anim.frame >= AOS_ENEMY_MAX_FRAMES ||
                    !objects.enemies[e->id].kind.modes[e->enemy.anim.id][e->enemy.anim.frame])
                    continue;
                AosRect b = aos_entity_rect(objects.enemies[e->id].kind.hurt[e->enemy.anim.id][e->enemy.anim.frame],
                                            e->enemy.x >> 16, e->enemy.y >> 16, e->enemy.mirrored,
                                            e->enemy.vflip);
                SDL_FRect er = {(float)(b.x1 - cam_x), (float)(b.y1 - cam_y),
                                (float)(b.x2 - b.x1 + 1), (float)(b.y2 - b.y1 + 1)};
                SDL_SetRenderDrawColor(renderer, 240, 220, 60, 255);
                SDL_RenderRect(renderer, &er);
            }
        }
        const AriaFrame *frame = NULL;
        if (soma.anim.frame < MAX_FRAMES)
            frame = &library.frames[soma.anim.id][soma.anim.frame];
        if (frame && frame->texture &&
            !(opening.active && opening_actors.soma_hidden)) {
            /* Library offsets face right; facing left mirrors around x. */
            float x = soma.facing_left ? (float)(sx - frame->offset_x) - frame->w
                                       : (float)(sx + frame->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(sy + frame->offset_y - cam_y), frame->w,
                              frame->h};
            SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                                     soma.facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (weapon_loaded && weapon_entity.active && weapon_entity.anim.frame < weapon_sprite.count) {
            const AriaFrame *blade = &weapon_sprite.frames[weapon_entity.anim.frame];
            int wx = sx + weapon_entity.x_offset, wy = sy + weapon_entity.y_offset;
            float x = weapon_entity.facing_left ? (float)(wx - blade->offset_x) - blade->w
                                                : (float)(wx + blade->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(wy + blade->offset_y - cam_y), blade->w,
                              blade->h};
            /* Class 1: the affine angle of sub_0803E058 (clockwise after the
             * flip, see draw_enemies) about the entity position. */
            double degrees = weapon_entity.weapon_class == 1
                                 ? (double)(weapon_entity.angle & 0xFFFF) * 360.0 / 65536.0
                                 : 0;
            SDL_FPoint centre = {(float)(wx - cam_x) - rect.x, (float)(wy - cam_y) - rect.y};
            SDL_RenderTextureRotated(renderer, blade->texture, NULL, &rect, degrees,
                                     degrees != 0 ? &centre : NULL,
                                     weapon_entity.facing_left ? SDL_FLIP_HORIZONTAL
                                                               : SDL_FLIP_NONE);
            int hx, hy, hw, hh;
            if (show_hitboxes && aos_weapon_hitbox(&weapon_entity, &soma, &weapon_sprite.data,
                                                   &hx, &hy, &hw, &hh)) {
                SDL_FRect box = {(float)(hx - cam_x), (float)(hy - cam_y), (float)hw, (float)hh};
                SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
                SDL_RenderRect(renderer, &box);
            }
        }
        /* Object20 owns BG0/BG1 and suppresses the gameplay HUD while its
         * dialogue or obtained-soul window is active. */
        if (hud_texture && !opening.active) {
            if (hud_dirty || hud_drawn_revision != hud.revision ||
                memcmp(hud_drawn, hud.map, sizeof hud_drawn)) {
                static uint8_t pixels[240 * 160 * 4];
                for (unsigned y = 0; y < 160; ++y)
                    for (unsigned x = 0; x < 240; ++x) {
                        uint8_t *out = pixels + (y * 240 + x) * 4;
                        uint16_t color;
                        if (!aos_hud_pixel(&hud, hud_bytes + AOS_HUD_TILE_OFFSET,
                                           hud_bytes, x, y, &color)) {
                            memset(out, 0, 4);
                            continue;
                        }
                        out[0] = (uint8_t)(((color & 31) * 255) / 31);
                        out[1] = (uint8_t)((((color >> 5) & 31) * 255) / 31);
                        out[2] = (uint8_t)((((color >> 10) & 31) * 255) / 31);
                        out[3] = 255;
                    }
                SDL_UpdateTexture(hud_texture, NULL, pixels, 240 * 4);
                memcpy(hud_drawn, hud.map, sizeof hud_drawn);
                hud_drawn_revision = hud.revision;
                hud_dirty = false;
            }
            SDL_FRect whole = {0, 0, 240, 160};
            SDL_RenderTexture(renderer, hud_texture, NULL, &whole);
        }
        /* sub_0800E708's banner rides the HUD BG0 like the opening notice. */
        if (!opening.active && gameplay_soul_notice_timer > 0 &&
            gameplay_soul_notice_ready)
            draw_opening_soul_notice(renderer, &gameplay_soul_notice,
                                     (uint32_t)step);
        /* sub_0800C8C8's fades: BG at its own weight (the room and the HUD
         * scaled so that the black veil below completes it), everything at
         * the OBJ weight. */
        if (announced_death) {
            float obj = (float)death.obj.weight / 65536.0f;
            if (obj < 1.0f) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, (uint8_t)((1.0f - obj) * 255.0f));
                SDL_FRect whole = {0, 0, 240, 160};
                SDL_RenderFillRect(renderer, &whole);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        }
        if (quick_map_open) {
            draw_native_quick_map(renderer, quick_map_texture, &quick_map,
                                  &progress, (uint32_t)step);
            /* BLDY brightness stages: black veil weight = bldY/16. */
            if (quick_map_bldY > 0) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0,
                                       (uint8_t)(quick_map_bldY * 255 / 16));
                SDL_FRect whole = {0, 0, VIEW_W, VIEW_H};
                SDL_RenderFillRect(renderer, &whole);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        } else if (!draw_native_equip(renderer, pause_texture, &pause_graphics,
                                    &soul_menu, &progress,
                                    have_equipment ? &equipment : NULL) &&
                 !draw_native_soul_set(renderer, pause_texture, &pause_graphics,
                                       &soul_menu, &progress) &&
                 !draw_native_pause_hub(renderer, pause_texture, &pause_graphics,
                                   &soul_menu, &progress, &soma,
                                   has_active_save ? active_save.play_frames : (uint32_t)step) &&
                 !draw_native_config(renderer, pause_texture, &pause_graphics,
                                     &soul_menu) &&
                 !draw_native_ability(renderer, pause_texture, &pause_graphics,
                                      &soul_menu, &progress) &&
                 !draw_native_item_use(renderer, pause_texture, &pause_graphics,
                                       &soul_menu, &progress,
                                       have_item_use ? item_use : NULL) &&
                 !draw_native_enemy(renderer, pause_texture, &pause_graphics,
                                    &soul_menu, &progress, &objects) &&
                 !draw_native_sleep(renderer, pause_texture, &pause_graphics,
                                    &soul_menu))
            draw_soul_menu(renderer, &soul_menu, &progress,
                           have_equipment ? &equipment : NULL,
                           have_item_use ? item_use : NULL, &soma, &objects);
        /* Object20's dialogue effect is applied after every targeted scene
         * layer. BG0's portrait and glyphs are then composed above it. */
        if (opening.active && opening_actors.soul_notice == 0 &&
            opening_actors.reveal_done)
            draw_opening_backdrop(renderer, &opening, &opening_rom,
                                  &opening_portrait_texture,
                                  &opening_portrait_id);
        if (opening_soul_text_ready &&
            (opening_actors.soul_notice == 1 || opening_actors.soul_notice == 2))
            draw_opening_soul_notice(renderer, &opening_soul_notice,
                                     (uint32_t)step);
        /* sub_08067A04's close states retire the window once the script's
         * end command lands; the backdrop gate above already hides it, so
         * the glyph pass must not linger through the return pan. */
        else if (opening.active && opening_text_ready &&
                 opening_actors.reveal_done)
            draw_opening_text(renderer, &opening, &opening_tiles,
                              opening_window_ready ? &opening_window : NULL,
                              (uint32_t)step);
        /* Object20 room dialogue: the shared bridge's window uses
         * the same font/frame stack as the opening text. The render
         * callback only refreshes the tile canvas while dirty, and
         * the draw reuses draw_opening_text's pixel pass — both
         * no-op without a ready ROM in --simulate (which never
         * reaches this path anyway). */
        if (aos_room_dialogue_active(&room_dialogue)) {
            aos_room_dialogue_render(&room_dialogue, (uint32_t)step);
            const AosOpeningEvent *rd_event =
                aos_room_dialogue_event(&room_dialogue);
            if (rd_event)
                draw_opening_text(renderer, rd_event, &room_dialogue_tiles,
                                  room_dialogue_ready
                                      ? &room_dialogue_window
                                      : NULL,
                                  (uint32_t)step);
        }
        draw_debug(renderer, &debug, paused, &room, &soma, step);
        if (capture_path && step >= capture_frames) {
            SDL_Surface *shot = SDL_RenderReadPixels(renderer, NULL);
            bool saved = shot && SDL_SaveBMP(shot, capture_path);
            if (shot) SDL_DestroySurface(shot);
            if (!saved) { fprintf(stderr, "Capture failed: %s\n", SDL_GetError()); goto cleanup; }
            printf("Captured frame %ld: %s (x=%d y=%d state=%u anim=%u frame=%u flags=%08x "
                   "effects=%zu opening=%u/%u notice=%u blocked=%u text=%zu waiting=%u)\n",
                   step, capture_path, sx, sy, soma.state, soma.anim.id, soma.anim.frame,
                   (unsigned)soma.flags, room.effect_count,
                   opening_actors.scene_state, opening_actors.scene_step,
                   opening_actors.soul_notice,
                   aos_opening_actors_blocks_script(&opening_actors) ? 1u : 0u,
                   opening.position, opening.waiting ? 1u : 0u);
            running = false;
        }
        SDL_RenderPresent(renderer);
        if (!capture_path) SDL_Delay(1);
    }
    rc = 0;
cleanup:
    if (quick_map_texture) SDL_DestroyTexture(quick_map_texture);
    if (pause_texture) SDL_DestroyTexture(pause_texture);
    aos_pause_graphics_free(&pause_graphics);
    if (opening_portrait_texture) SDL_DestroyTexture(opening_portrait_texture);
    fusion_rom_close(&opening_rom);
    if (audio_stream) SDL_DestroyAudioStream(audio_stream);
    aria_wav_close(&wav);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&mp2k);
    gba_input_close(&gamepad);
    free_library(&library);
    free_weapon_sprite(&weapon_sprite);
    aria_free_objects(&objects);
    if (background) SDL_DestroyTexture(background);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) fusion_host_destroy_window(window);
    fusion_host_quit();
    free_room(&room);
    return rc;
}

#ifndef FUSION_EMBEDDED
int main(int argc, char **argv) {
    return aria_runtime_main(argc, argv);
}
#endif
