/* SPDX-License-Identifier: GPL-3.0-only */
/* Menus/debug cluster of the Aria of Sorrow room runtime, extracted
 * from aria_runtime.c (AGENTS.md rule 8, decomposition wave 4): the
 * debug-menu actions (teleport/spawn), the pause-graphics page draws
 * (hub, soul set, equip, config, sleep, ability, item use, enemy
 * detail with its sub_0804CA18 portrait probe, quick map), the
 * source-adapted draw_soul_menu fallback and the draw_debug overlay.
 * The debug-menu action ids and move tables are shared with the frame
 * loop, so they live in aria_menus_debug.h; the room-loading helpers
 * debug_teleport borrows stay owned by aria_runtime.c. */
#include "aria_menus_debug.h"

#include "aos/core/aos_collision.h"
#include "aos/core/aos_enemy.h"
#include "aos/core/aos_soma.h"
#include "aos/core/aos_stats.h"
#include "aos/game/aos_bestiary.h"
#include "aos/game/aos_object01.h"
#include "aos/game/aos_pickup.h"
#include "aos/game/aos_room_dialogue.h"
#include "aos/menus/aos_button_config.h"
#include "aos/menus/aos_pause_graphics.h"
#include "aos/menus/aos_quick_map.h"
#include "aos/souls/aos_soul_menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The shared room dialogue the debug teleport closes; the main loop
 * owns the instance (same extern pattern as aria_entities.c). */
extern AosRoomDialogue room_dialogue;

const char *const debug_move_labels[5] = {
    "Move: backdash", "Move: slide", "Move: mid-air jump", "Move: dive kick", "Move: high jump",
};
const uint32_t debug_move_bits[5] = {
    AOS_MOVE_BACKDASH, AOS_MOVE_SLIDE, AOS_MOVE_AIR_JUMP, AOS_MOVE_DIVE_KICK, AOS_MOVE_HIGH_JUMP,
};

/* An exported room at a floor found by find_spawn; Soma keeps HP, moves,
 * weapon and facing, and restarts in his normal state. */
bool debug_teleport(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
                           AosSoma *soma, const AosAnimSet *anims, int area, int number) {
    char folder[512];
    room_folder(folder, sizeof folder, area, number);
    AriaRoom target = {0};
    if (!load_room_folder(folder, &target)) {
        fprintf(stderr, "Debug teleport: room %d/%d is not exported\n", area, number);
        return false;
    }
    AosCollision layer = room_layer(&target);
    int x, y;
    SDL_Texture *texture = NULL;
    if (!find_spawn(&layer, &x, &y) || !(texture = load_background(renderer, folder))) {
        fprintf(stderr, "Debug teleport: no spawn point in room %d/%d\n", area, number);
        free_room(&target);
        return false;
    }
    AosSoma fresh = aos_soma_spawn((int32_t)((uint32_t)x << 16), (int32_t)((uint32_t)y << 16),
                                   anims);
    fresh.hp = soma->hp;
    fresh.max_hp = soma->max_hp;
    fresh.moves = soma->moves;
    fresh.weapon = soma->weapon;
    fresh.facing_left = soma->facing_left;
    *soma = fresh;
    aos_room_dialogue_close(&room_dialogue);
    free_room(room);
    *room = target;
    aos_object01_reset();
    SDL_DestroyTexture(*background);
    *background = texture;
    printf("Debug teleport: room %d/%d at %d,%d\n", area, number, x, y);
    return true;
}

/* A ported enemy 48 pixels in front of Soma, created as a room record. */
bool debug_spawn(AriaRoom *room, const AriaCombat *combat, const AosSoma *soma,
                        const AosCollision *layer, int id) {
    if (room->entity_count >= MAX_ENTITIES || !combat->kinds[id]) return false;
    int x = (soma->x >> 16) + (soma->facing_left ? -48 : 48), y = (soma->y >> 16) - 24;
    AriaEntity *e = &room->entities[room->entity_count];
    *e = (AriaEntity){.kind = ARIA_KIND_ENEMY, .id = id, .x = x, .y = y, .spawned = true,
                      .spawner = -1};
    if (!aos_enemy_create(&e->enemy, (uint8_t)id, x, y, 0, 0, soma, layer, combat->kinds[id],
                          combat->stats[id]))
        return false;
    room->entity_count++;
    return true;
}

static void debug_text(SDL_Renderer *renderer, int line, const char *text) {
    SDL_RenderDebugText(renderer, 2.0f, (float)(2 + line * 8), text);
}

static void item_effect_text(char *text, size_t size, const AosItemUseRecord *item) {
    if (!item) snprintf(text, size, "Metadata unavailable");
    else if (item->effect == 0) snprintf(text, size, "Restore %u HP", item->amount);
    else if (item->effect == 1) snprintf(text, size, "Restore %u%% MP", item->amount);
    else if (item->effect == 2) snprintf(text, size, "Cure status mask %u", item->amount);
    else if (item->effect == 3) snprintf(text, size, "Lose %u HP", item->amount);
    else snprintf(text, size, "Key item - cannot use");
}

static const char *drop_item_name(uint16_t id) {
    uint8_t category, index;
    if (!id) return "None";
    if (!aos_item_decode(id, &category, &index)) return "Unknown";
    if (category == AOS_PICKUP_ITEM) return aos_item_name(index);
    if (category == AOS_PICKUP_WEAPON) return aos_equipment_name(AOS_EQUIP_WEAPON, index);
    if (category == AOS_PICKUP_ARMOR) return aos_equipment_name(AOS_EQUIP_ARMOR, index);
    return "Unknown";
}

bool draw_native_pause_hub(SDL_Renderer *renderer, SDL_Texture *texture,
                                  AosPauseGraphics *graphics,
                                  const AosSoulMenu *menu, const AosStats *stats,
                                  const AosSoma *soma, uint32_t play_frames) {
    if (!renderer || !texture || !graphics || !menu || !stats || !soma ||
        !menu->open || menu->page != AOS_SOUL_MENU_PAGE_HUB)
        return false;
    AosPauseHubValues values = {
        .hp = soma->hp,
        .mp = stats->mp,
        .max_hp = (uint16_t)soma->max_hp,
        .max_mp = stats->max_mp,
        .attack = stats->atk,
        .defence = stats->def,
        .level = stats->level,
        .experience = stats->experience,
        .next_experience = (uint32_t)aos_stats_experience_to_next(stats),
        .gold = stats->gold,
        .play_frames = play_frames,
        .status = stats->status,
    };
    memcpy(values.stats, stats->current, sizeof values.stats);
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_hub(graphics, &values, menu->hub_cursor,
                                       pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_soul_set(SDL_Renderer *renderer, SDL_Texture *texture,
                                 AosPauseGraphics *graphics,
                                 const AosSoulMenu *menu, const AosStats *stats) {
    if (!renderer || !texture || !graphics || !menu || !stats || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_SOUL_SET ||
        menu->category >= AOS_PAUSE_SOUL_FAMILIES)
        return false;
    AosPauseSoulSetValues values = {
        .attack = stats->atk,
        .defence = stats->def,
        .status = stats->status,
        .category = menu->category,
        .equipped = {stats->red_soul, stats->blue_soul, stats->yellow_soul},
        .recent_souls = menu->recent_souls,
    };
    memcpy(values.stats, stats->current, sizeof values.stats);
    memcpy(values.cursor, menu->cursor, sizeof values.cursor);
    static const uint8_t counts[] = {55, 24, 35};
    for (unsigned category = 0; category < AOS_PAUSE_SOUL_FAMILIES; ++category)
        for (unsigned soul = 0; soul < counts[category]; ++soul)
            values.counts[category][soul] =
                aos_souls_total(stats, (int32_t)category, (int32_t)soul);

    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_soul_set(graphics, &values,
                                            pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_equip(SDL_Renderer *renderer, SDL_Texture *texture,
                              AosPauseGraphics *graphics,
                              const AosSoulMenu *menu, const AosStats *stats,
                              const AosEquipmentTables *equipment) {
    if (!renderer || !texture || !graphics || !menu || !stats || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_EQUIP ||
        menu->equip_slot >= AOS_EQUIP_SLOTS)
        return false;
    AosPauseEquipValues values = {
        .current = {stats->atk, stats->def,
                    stats->current[AOS_STAT_STR], stats->current[AOS_STAT_CON],
                    stats->current[AOS_STAT_INT], stats->current[AOS_STAT_LCK]},
        .equipped = {stats->weapon, stats->armor, stats->accessory},
        .slot = menu->equip_slot,
        .selected_item = menu->equip_selecting
                             ? menu->equip_choice
                             : aos_equipment_current(stats, menu->equip_slot),
        .preview_active = menu->equip_selecting && equipment,
        .selecting = menu->equip_selecting,
    };
    memcpy(values.preview, values.current, sizeof values.preview);
    if (values.preview_active) {
        AosStats preview = *stats;
        uint8_t *slot = menu->equip_slot == AOS_EQUIP_WEAPON ? &preview.weapon
                        : menu->equip_slot == AOS_EQUIP_ARMOR ? &preview.armor
                                                              : &preview.accessory;
        *slot = values.selected_item;
        aos_stats_refresh(&preview, equipment);
        values.preview[0] = preview.atk;
        values.preview[1] = preview.def;
        memcpy(values.preview + 2, preview.current, sizeof preview.current);
    }

    int choices = aos_equipment_choice_count(stats, menu->equip_slot);
    int cursor = 0;
    for (int ordinal = 0; ordinal < choices; ++ordinal)
        if (aos_equipment_ordered_choice_at(stats, menu->recent_items,
                                            menu->equip_slot, ordinal) ==
            values.selected_item) {
            cursor = ordinal;
            break;
        }
    int top = cursor - 3;
    if (top > choices - AOS_PAUSE_EQUIP_VISIBLE)
        top = choices - AOS_PAUSE_EQUIP_VISIBLE;
    if (top < 0) top = 0;
    for (int entry = 0;
         entry < AOS_PAUSE_EQUIP_VISIBLE && top + entry < choices; ++entry) {
        uint8_t item = aos_equipment_ordered_choice_at(
            stats, menu->recent_items, menu->equip_slot, top + entry);
        values.items[values.item_count] = item;
        values.quantities[values.item_count] =
            aos_equipment_quantity(stats, menu->equip_slot, item);
        ++values.item_count;
    }

    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_equip(graphics, &values,
                                         pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_config(SDL_Renderer *renderer, SDL_Texture *texture,
                               AosPauseGraphics *graphics,
                               const AosSoulMenu *menu) {
    if (!renderer || !texture || !graphics || !menu || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_CONFIG)
        return false;
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_config(graphics, menu->config_buttons,
                                          menu->config_cursor,
                                          pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_sleep(SDL_Renderer *renderer, SDL_Texture *texture,
                              AosPauseGraphics *graphics,
                              const AosSoulMenu *menu) {
    if (!renderer || !texture || !graphics || !menu || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_SLEEP)
        return false;
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_sleep(graphics, menu->sleep_available,
                                         pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_ability(SDL_Renderer *renderer, SDL_Texture *texture,
                                AosPauseGraphics *graphics,
                                const AosSoulMenu *menu, const AosStats *stats) {
    if (!renderer || !texture || !graphics || !menu || !stats || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_ABILITY)
        return false;
    AosPauseAbilityValues values = {
        .cursor = menu->cursor[AOS_SOUL_MENU_ABILITY],
    };
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        values.owned[i] =
            aos_souls_total(stats, AOS_SOUL_ABILITY, (int32_t)i) != 0;
        values.enabled[i] =
            (stats->abilities[i >> 3] & (1u << (i & 7))) != 0;
    }
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_ability(graphics, &values,
                                           pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_item_use(SDL_Renderer *renderer, SDL_Texture *texture,
                                 AosPauseGraphics *graphics,
                                 const AosSoulMenu *menu, const AosStats *stats,
                                 const AosItemUseRecord items[AOS_ITEM_RECORDS]) {
    if (!renderer || !texture || !graphics || !menu || !stats || !menu->open ||
        !items || menu->page != AOS_SOUL_MENU_PAGE_ITEM_USE)
        return false;
    AosPauseItemUseValues values = {.selected = 0xFF};
    memcpy(values.counts, stats->item_counts, sizeof values.counts);
    for (unsigned item = 0; item < AOS_PAUSE_ITEM_USE_RECORDS; ++item)
        values.icons[item] = items[item].icon;
    int choices = aos_item_choice_count(stats);
    int cursor = 0;
    for (int ordinal = 0; ordinal < choices; ++ordinal)
        if (aos_item_choice_at(stats, ordinal) == menu->item_cursor) {
            cursor = ordinal;
            break;
        }
    int top = cursor - 3;
    if (top > choices - AOS_PAUSE_ITEM_USE_VISIBLE)
        top = choices - AOS_PAUSE_ITEM_USE_VISIBLE;
    if (top < 0) top = 0;
    values.top = (uint8_t)top;
    if (menu->item_cursor < AOS_PAUSE_ITEM_USE_RECORDS)
        values.selected = menu->item_cursor;
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_pause_graphics_render_item_use(graphics, &values,
                                            pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

/* sub_0804CA18: the detail page's enemy portrait is a real enemy entity —
 * the record's +0x00 create fn runs and +0x04 becomes the entity update,
 * but the main entity loop skips slots 0x00-0xCF while unk_A074_0 is set
 * (the pause menu), so the portrait keeps the frame the create leaves.
 * The create runs once per enemy with the shared RNG state preserved; a
 * far decoy makes every enemy face right, matching the native case where
 * the player entity sits far from the menu anchor. */
static void enemy_portrait_init(AriaObjects *objects, int id) {
    __typeof__(objects->enemies[0]) *art = &objects->enemies[id];
    if (art->portrait_state) return;
    art->portrait_state = -1;
    if (!art->loaded) return;
    uint32_t rng = aos_random_get();
    aos_enemy_set_decoy(true, 0x40000000, 0);
    AosSoma soma;
    memset(&soma, 0, sizeof soma);
    soma.x = 0x40000000;
    static const uint8_t empty_cells[1] = {0};
    AosCollision layer = {.width_cells = 1, .height_cells = 1, .cells = empty_cells};
    AosEnemy probe;
    bool created = aos_enemy_create(&probe, (uint8_t)id, 64, 88, 0, 0,
                                    &soma, &layer, &art->kind, &art->stats);
    aos_enemy_set_decoy(false, 0, 0);
    aos_random_seed(rng);
    if (!created) return;
    const char *path = NULL;
    const AriaFrame *frame = NULL;
    if (probe.static_frame >= 0) {
        for (int k = 0; k < art->static_count; ++k)
            if (art->static_ids[k] == probe.static_frame) {
                path = art->static_paths[k];
                frame = &art->statics[k];
                break;
            }
    } else if (probe.anim.id < AOS_ENEMY_MAX_ANIMS && probe.anim.frame < MAX_FRAMES) {
        path = art->paths[probe.anim.id][probe.anim.frame];
        frame = &art->frames[probe.anim.id][probe.anim.frame];
    }
    if (!path || !frame) return;
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) return;
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!rgba) return;
    int w = rgba->w, h = rgba->h;
    uint8_t *pixels = malloc((size_t)w * h * 4);
    if (!pixels) {
        SDL_DestroySurface(rgba);
        return;
    }
    for (int y = 0; y < h; ++y)
        memcpy(pixels + (size_t)y * w * 4,
               (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch, (size_t)w * 4);
    SDL_DestroySurface(rgba);
    art->portrait = pixels;
    art->portrait_w = w;
    art->portrait_h = h;
    art->portrait_frame = frame;
    art->portrait_flip = probe.mirrored;
    art->portrait_state = 1;
}

/* The entity anchor is (0x40, 0x58) plus the record's +0x1E/+0x20 pixel
 * offsets; the sprite hangs off it through the frame's exported offsets,
 * flipped horizontally when the create mirrored the enemy (draw_enemies
 * uses the same convention). */
static void enemy_portrait_blit(AriaObjects *objects, int id, uint8_t *pixels) {
    if (id < 0 || id >= ENEMY_KINDS) return;
    enemy_portrait_init(objects, id);
    const __typeof__(objects->enemies[0]) *art = &objects->enemies[id];
    if (art->portrait_state != 1 || !art->portrait || !art->portrait_frame) return;
    const AriaFrame *frame = art->portrait_frame;
    int anchor_x = 0x40 + art->portrait_x, anchor_y = 0x58 + art->portrait_y;
    int dx = art->portrait_flip ? anchor_x - frame->offset_x - art->portrait_w
                                : anchor_x + frame->offset_x;
    int dy = anchor_y + frame->offset_y;
    for (int sy = 0; sy < art->portrait_h; ++sy) {
        int y = dy + sy;
        if (y < 0 || y >= VIEW_H) continue;
        for (int sx = 0; sx < art->portrait_w; ++sx) {
            int x = dx + sx;
            if (x < 0 || x >= VIEW_W) continue;
            const uint8_t *src = art->portrait + ((size_t)sy * art->portrait_w + sx) * 4;
            if (!src[3]) continue;
            memcpy(pixels + ((size_t)y * VIEW_W + x) * 4, src, 4);
        }
    }
}

bool draw_native_enemy(SDL_Renderer *renderer, SDL_Texture *texture,
                              AosPauseGraphics *graphics,
                              const AosSoulMenu *menu, const AosStats *stats,
                              AriaObjects *objects) {
    if (!renderer || !texture || !graphics || !menu || !stats || !menu->open ||
        menu->page != AOS_SOUL_MENU_PAGE_ENEMY)
        return false;
    AosPauseEnemyValues values = {
        .top = menu->enemy.top,
        .cursor = menu->enemy.cursor,
        .detail = menu->enemy.detail,
        .intro_phase = menu->enemy.intro,
    };
    memcpy(values.defeated, stats->defeated, sizeof values.defeated);
    memcpy(values.dropped, stats->dropped, sizeof values.dropped);
    if (menu->enemy.detail && objects && menu->enemy.cursor < ENEMY_KINDS) {
        const AosEnemyStats *es = &objects->enemies[menu->enemy.cursor].stats;
        const AosEnemyDrops *ed = &objects->enemies[menu->enemy.cursor].drops;
        values.hp = es->hp;
        values.experience = es->experience;
        values.weak = es->weak;
        values.resist = es->resist;
        values.common_item = ed->common_item;
        values.rare_item = ed->rare_item;
        values.soul_type = ed->soul_type;
        values.soul_index = ed->soul_index;
        values.soul_owned =
            ed->soul_index != 0 &&
            aos_souls_total(stats, ed->soul_type, ed->soul_index - 1) != 0;
        values.soul_view = menu->enemy.soul_view;
    }
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    bool ok = menu->enemy.detail
                  ? aos_pause_graphics_render_enemy_detail(
                        graphics, &values, pixels, sizeof pixels)
                  : aos_pause_graphics_render_enemy(graphics, &values,
                                                    pixels, sizeof pixels);
    if (ok && menu->enemy.detail && objects)
        enemy_portrait_blit(objects, menu->enemy.cursor, pixels);
    if (!ok || !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

bool draw_native_quick_map(SDL_Renderer *renderer, SDL_Texture *texture,
                                  AosQuickMap *map, const AosStats *stats,
                                  uint32_t animation_frame) {
    if (!renderer || !texture || !map || !stats) return false;
    static uint8_t pixels[VIEW_W * VIEW_H * 4];
    if (!aos_quick_map_render(map, stats->item_counts[0x1D] != 0,
                              stats->item_counts[0x1E] != 0,
                              stats->item_counts[0x1F] != 0,
                              animation_frame, pixels, sizeof pixels) ||
        !SDL_UpdateTexture(texture, NULL, pixels, VIEW_W * 4))
        return false;
    SDL_FRect destination = {0, 0, VIEW_W, VIEW_H};
    return SDL_RenderTexture(renderer, texture, NULL, &destination);
}

/* Source-adapted slices of sub_080473F4 / sub_08047764. Native tile, icon,
 * description and transition rendering remain separate future work. */
void draw_soul_menu(SDL_Renderer *renderer, const AosSoulMenu *menu,
                           const AosStats *stats, const AosEquipmentTables *equipment,
                           const AosItemUseRecord items[AOS_ITEM_RECORDS],
                           const AosSoma *soma, const AriaObjects *objects) {
    if (!menu->open) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 7, 12, 24, 238);
    SDL_FRect whole = {0, 0, VIEW_W, VIEW_H};
    SDL_RenderFillRect(renderer, &whole);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    char line[64];
    if (menu->page == AOS_SOUL_MENU_PAGE_HUB) {
        debug_text(renderer, 0, "STATUS");
        snprintf(line, sizeof line, "LV %u   HP %d/%d   MP %d/%u", stats->level,
                 soma->hp, soma->max_hp, stats->mp, stats->max_mp);
        debug_text(renderer, 1, line);
        snprintf(line, sizeof line, "ATK %d  DEF %d  GOLD %u", stats->atk, stats->def,
                 stats->gold);
        debug_text(renderer, 2, line);
        for (int entry = 0; entry < AOS_PAUSE_ENTRIES; ++entry) {
            snprintf(line, sizeof line, "%c %-10s %s", entry == menu->hub_cursor ? '>' : ' ',
                     aos_pause_entry_name((uint8_t)entry),
                     entry == AOS_PAUSE_SOUL_SET || entry == AOS_PAUSE_EQUIP ||
                             entry == AOS_PAUSE_ITEM_USE ||
                             entry == AOS_PAUSE_ABILITY || entry == AOS_PAUSE_SLEEP ||
                             entry == AOS_PAUSE_CONFIG || entry == AOS_PAUSE_ENEMY
                         ? ""
                         : "[pending]");
            debug_text(renderer, 4 + entry, line);
        }
        debug_text(renderer, 13, "A select  B/Start close");
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_ENEMY) {
        uint8_t enemy = menu->enemy.cursor;
        if (!menu->enemy.detail) {
            unsigned discovered = aos_bestiary_discovered_count(stats);
            snprintf(line, sizeof line, "ENEMY  %u/%u DISCOVERED", discovered,
                     AOS_BESTIARY_MENU_RECORDS);
            debug_text(renderer, 0, line);
            for (int row = 0; row < AOS_BESTIARY_PAGE_RECORDS / 2; ++row) {
                unsigned left = menu->enemy.top + (unsigned)row * 2;
                unsigned right = left + 1;
                if (left >= AOS_BESTIARY_MENU_RECORDS) break;
                const char *left_name = aos_stats_enemy_defeated(stats, (uint8_t)left)
                                            ? aos_bestiary_name((uint8_t)left)
                                            : "---------";
                const char *right_name = right < AOS_BESTIARY_MENU_RECORDS &&
                                                  aos_stats_enemy_defeated(stats, (uint8_t)right)
                                             ? aos_bestiary_name((uint8_t)right)
                                             : "---------";
                if (right < AOS_BESTIARY_MENU_RECORDS)
                    snprintf(line, sizeof line, "%c%03u %-9.9s %c%03u %-9.9s",
                             left == enemy ? '>' : ' ', left + 1, left_name,
                             right == enemy ? '>' : ' ', right + 1, right_name);
                else
                    snprintf(line, sizeof line, "%c%03u %-9.9s",
                             left == enemy ? '>' : ' ', left + 1, left_name);
                debug_text(renderer, 2 + row * 2, line);
            }
            debug_text(renderer, 14, "D-pad move  L/R page  A details");
            debug_text(renderer, 15, "B back  Start close");
            return;
        }
        const AosEnemyStats *record = &objects->enemies[enemy].stats;
        const AosEnemyDrops *drops = &objects->enemies[enemy].drops;
        snprintf(line, sizeof line, "#%03u %s%s", enemy + 1,
                 aos_bestiary_name(enemy), aos_bestiary_is_boss(enemy) ? " [BOSS]" : "");
        debug_text(renderer, 0, line);
        snprintf(line, sizeof line, "HP %d   EXP %u", record->hp, record->experience);
        debug_text(renderer, 2, line);
        snprintf(line, sizeof line, "ATK %u   DEF %u", record->contact, record->defence);
        debug_text(renderer, 3, line);
        snprintf(line, sizeof line, "WEAK %04X   RESIST %04X", record->weak, record->resist);
        debug_text(renderer, 4, line);
        snprintf(line, sizeof line, "COMMON %-19s %s", drop_item_name(drops->common_item),
                 stats->dropped[0][enemy >> 5] & (1u << (enemy & 31)) ? "*" : "");
        debug_text(renderer, 6, line);
        snprintf(line, sizeof line, "RARE   %-19s %s", drop_item_name(drops->rare_item),
                 stats->dropped[1][enemy >> 5] & (1u << (enemy & 31)) ? "*" : "");
        debug_text(renderer, 7, line);
        bool has_soul = drops->soul_index != 0;
        bool owns_soul = has_soul &&
            aos_souls_total(stats, drops->soul_type, drops->soul_index - 1) != 0;
        if (!has_soul) {
            debug_text(renderer, 9, "SOUL NONE");
        } else if (!owns_soul) {
            debug_text(renderer, 9, "SOUL ???");
        } else {
            snprintf(line, sizeof line, "SOUL TYPE %u  ID %u%s", drops->soul_type,
                     drops->soul_index, menu->enemy.soul_view ? " [DETAIL]" : "");
            debug_text(renderer, 9, line);
        }
        debug_text(renderer, 12, "Left/Right previous/next known");
        debug_text(renderer, 13, "Up/Down soul view  B list");
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_CONFIG) {
        static const uint8_t rows[AOS_BUTTON_ACTIONS] = {
            AOS_BUTTON_ATTACK, AOS_BUTTON_JUMP, AOS_BUTTON_GUARDIAN, AOS_BUTTON_ABILITY,
        };
        debug_text(renderer, 0, "CONFIG - BUTTON ASSIGN");
        for (int row = 0; row < AOS_BUTTON_ACTIONS; ++row) {
            uint8_t action = rows[row];
            snprintf(line, sizeof line, "%c %-10s %s",
                     row == menu->config_cursor ? '>' : ' ',
                     aos_button_config_action_name(action),
                     aos_button_config_button_name(menu->config_buttons[action]));
            debug_text(renderer, 3 + row * 2, line);
        }
        debug_text(renderer, 12, "A/Right next  Left previous");
        debug_text(renderer, 13, "Select defaults");
        debug_text(renderer, 14, "B apply/back  Start apply/close");
        if (!aos_pause_config_unique(menu))
            debug_text(renderer, 16, "Duplicate assignment - change before exit");
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_SLEEP) {
        debug_text(renderer, 0, "SLEEP");
        if (!menu->sleep_available) {
            debug_text(renderer, 3, "No file slot is active.");
            debug_text(renderer, 5, "A/B return");
        } else if (menu->sleep_stage == AOS_SLEEP_WAIT_SAVE) {
            debug_text(renderer, 4, "Writing quicksave...");
        } else if (menu->sleep_stage == AOS_SLEEP_FADING) {
            debug_text(renderer, 4, "Quicksave complete");
        } else {
            debug_text(renderer, 3, "Create a one-use quicksave?");
            debug_text(renderer, 6,
                       menu->sleep_choice == 0 ? "> YES      NO" : "  YES    > NO");
            debug_text(renderer, 9, "Left/Right choose  A confirm");
            debug_text(renderer, 10, "B return");
        }
        if (menu->sleep_stage == AOS_SLEEP_FADING) {
            unsigned fade = menu->sleep_timer < 64 ? menu->sleep_timer : 64;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, (uint8_t)(fade * 255 / 64));
            SDL_RenderFillRect(renderer, &whole);
        }
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_ITEM_USE) {
        debug_text(renderer, 0, "ITEM USE");
        int choices = aos_item_choice_count(stats);
        int cursor = 0;
        for (int ordinal = 0; ordinal < choices; ++ordinal)
            if (aos_item_choice_at(stats, ordinal) == menu->item_cursor) cursor = ordinal;
        int top = cursor - 5;
        if (top > choices - 11) top = choices - 11;
        if (top < 0) top = 0;
        if (!choices) debug_text(renderer, 4, "No items owned");
        for (int row = 0; row < 11 && top + row < choices; ++row) {
            uint8_t item = aos_item_choice_at(stats, top + row);
            snprintf(line, sizeof line, "%c %02u %-20s x%u", top + row == cursor ? '>' : ' ',
                     item + 1, aos_item_name(item), stats->item_counts[item]);
            debug_text(renderer, 2 + row, line);
        }
        snprintf(line, sizeof line, "HP %d/%d  MP %d/%u", soma->hp, soma->max_hp,
                 stats->mp, stats->max_mp);
        debug_text(renderer, 14, line);
        item_effect_text(line, sizeof line,
                         items && menu->item_cursor < AOS_ITEM_RECORDS
                             ? &items[menu->item_cursor]
                             : NULL);
        debug_text(renderer, 15, line);
        debug_text(renderer, 17, "A use  Up/Down select");
        debug_text(renderer, 18, "B back  Start close");
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_EQUIP) {
        debug_text(renderer, 0, menu->equip_selecting ? "EQUIP - SELECT" : "EQUIP");
        if (!menu->equip_selecting) {
            for (int slot = 0; slot < AOS_EQUIP_SLOTS; ++slot) {
                uint8_t item = aos_equipment_current(stats, (uint8_t)slot);
                snprintf(line, sizeof line, "%c %-9s %-20s",
                         slot == menu->equip_slot ? '>' : ' ',
                         aos_equipment_slot_name((uint8_t)slot),
                         aos_equipment_name((uint8_t)slot, item));
                debug_text(renderer, 2 + slot, line);
            }
            snprintf(line, sizeof line, "ATK %d  DEF %d", stats->atk, stats->def);
            debug_text(renderer, 7, line);
            snprintf(line, sizeof line, "STR %d CON %d INT %d LCK %d",
                     stats->current[AOS_STAT_STR], stats->current[AOS_STAT_CON],
                     stats->current[AOS_STAT_INT], stats->current[AOS_STAT_LCK]);
            debug_text(renderer, 8, line);
            debug_text(renderer, 11, "Up/Down slot  A choose");
            debug_text(renderer, 12, "B back  Start close");
            return;
        }
        snprintf(line, sizeof line, "%s", aos_equipment_slot_name(menu->equip_slot));
        debug_text(renderer, 1, line);
        int choices = aos_equipment_choice_count(stats, menu->equip_slot);
        int cursor = 0;
        for (int ordinal = 0; ordinal < choices; ++ordinal)
            if (aos_equipment_ordered_choice_at(stats, menu->recent_items,
                                                menu->equip_slot, ordinal) ==
                menu->equip_choice)
                cursor = ordinal;
        int top = cursor - 5;
        if (top > choices - 11) top = choices - 11;
        if (top < 0) top = 0;
        for (int row = 0; row < 11 && top + row < choices; ++row) {
            uint8_t item = aos_equipment_ordered_choice_at(
                stats, menu->recent_items, menu->equip_slot, top + row);
            snprintf(line, sizeof line, "%c %02u %-20s x%u", top + row == cursor ? '>' : ' ',
                     item == AOS_EQUIPMENT_NONE ? 0 : item + 1,
                     aos_equipment_name(menu->equip_slot, item),
                     aos_equipment_quantity(stats, menu->equip_slot, item));
            debug_text(renderer, 3 + row, line);
        }
        if (equipment) {
            AosStats preview = *stats;
            uint8_t *slot = menu->equip_slot == AOS_EQUIP_WEAPON ? &preview.weapon
                            : menu->equip_slot == AOS_EQUIP_ARMOR ? &preview.armor
                                                                  : &preview.accessory;
            *slot = menu->equip_choice;
            aos_stats_refresh(&preview, equipment);
            snprintf(line, sizeof line, "ATK %d>%d DEF %d>%d", stats->atk, preview.atk,
                     stats->def, preview.def);
            debug_text(renderer, 14, line);
        }
        debug_text(renderer, 16, "A equip  B cancel  Start close");
        return;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_UNAVAILABLE) {
        snprintf(line, sizeof line, "%s", aos_pause_entry_name(menu->hub_cursor));
        debug_text(renderer, 0, line);
        debug_text(renderer, 2, "This native page is not reconstructed yet.");
        debug_text(renderer, 4, "B return  Start close");
        return;
    }
    snprintf(line, sizeof line, "SOUL EQUIPMENT  <%s>",
             aos_soul_menu_category_name(menu->category));
    debug_text(renderer, 0, line);
    debug_text(renderer, 1, menu->page == AOS_SOUL_MENU_PAGE_SOUL_SET
                                ? "L/R family  Up/Down select"
                                : "ABILITY  Up/Down select");
    debug_text(renderer, 2, "A equip/toggle  B back  Start close");

    int choices = aos_soul_menu_count(menu->category) +
                  (menu->category == AOS_SOUL_MENU_ABILITY ? 0 : 1);
    int cursor = menu->cursor[menu->category];
    int top = cursor - 6;
    if (top > choices - 13) top = choices - 13;
    if (top < 0) top = 0;
    for (int row = 0; row < 13 && top + row < choices; ++row) {
        AosSoulMenu item = *menu;
        item.cursor[item.category] = (uint8_t)(top + row);
        int index = aos_soul_menu_inventory_index(&item);
        uint8_t quantity = aos_soul_menu_quantity(&item, stats);
        bool enabled = aos_soul_menu_enabled(&item, stats);
        snprintf(line, sizeof line, "%c%c %2d %-18s x%u",
                 top + row == cursor ? '>' : ' ', enabled ? '*' : ' ',
                 index < 0 ? 0 : index + 1,
                 aos_soul_menu_name(item.category, item.cursor[item.category]), quantity);
        debug_text(renderer, 4 + row, line);
    }
}

void draw_debug(SDL_Renderer *renderer, const DebugMenu *menu, bool paused,
                       const AriaRoom *room, const AosSoma *soma, long step) {
    if (!menu->open && !paused) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_FRect panel = {0, 0, VIEW_W, menu->open ? VIEW_H : 12};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    char line[64];
    if (!menu->open) {
        debug_text(renderer, 0, "PAUSED  F2 step  F1 menu");
        return;
    }
    snprintf(line, sizeof line, "DEBUG room %d/%d frame %ld", room->area, room->number, step);
    debug_text(renderer, 0, line);
    snprintf(line, sizeof line, "x%d y%d st%u an%u fr%u", soma->x >> 16, soma->y >> 16,
             soma->state, soma->anim.id, soma->anim.frame);
    debug_text(renderer, 1, line);
    int alive = 0;
    for (size_t i = 0; i < room->entity_count; ++i)
        alive += room->entities[i].kind == ARIA_KIND_ENEMY && room->entities[i].spawned &&
                 !room->entities[i].enemy.removed;
    snprintf(line, sizeof line, "HP %d/%d enemies %d", soma->hp, soma->max_hp, alive);
    debug_text(renderer, 2, line);
    enum { FIRST = 4, ROWS = 15 };
    int top = menu->cursor - ROWS / 2;
    if (top > menu->count - ROWS) top = menu->count - ROWS;
    if (top < 0) top = 0;
    for (int i = 0; i < ROWS && top + i < menu->count; ++i) {
        debug_menu_line(menu, top + i, line, sizeof line);
        debug_text(renderer, FIRST + i, line);
    }
}
