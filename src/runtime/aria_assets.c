/* SPDX-License-Identifier: GPL-3.0-only */
/* Asset and save-data loaders of the Aria of Sorrow room runtime, moved
 * verbatim out of aria_runtime.c (AGENTS.md rule 8). See aria_assets.h. */
#include "aos/title/aos_opening_event.h" /* AOS_OPENING_ACTOR_COUNT, before the types */
#include "aria_assets.h"
#include "aria_tables.h"

#include "aos/core/aos_wall.h"
#include "aos/game/aos_pickup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runtime state the save marshalling reads; owned by aria_runtime.c. */
extern uint32_t aria_events[AOS_WALL_EVENT_WORDS];
extern AosPickupCollectionState aria_pickup_state;

/* enemy_frames.tsv (boxes, death blink, margins) and enemies.tsv (stats). */
bool aria_load_enemy_data(AriaObjects *objects) {
    FILE *f = fopen(DEFAULT_ENEMY_FRAMES, "rb");
    if (!f) return false;
    char line[256];
    uint8_t blink[40] = {0};
    int margins[14] = {0};
    while (fgets(line, sizeof line, f)) {
        char name[16], text[64];
        int anim, index, frame, ticks, mode, h[4], a[4];
        if (sscanf(line, "blink\t%63s", text) == 1) {
            for (int i = 0; i < 40 && text[i]; ++i) blink[i] = (uint8_t)(text[i] == '1');
            continue;
        }
        if (!strncmp(line, "anchor\t", 7)) {
            char anchor_name[16];
            int anim, index, ax, ay;
            if (sscanf(line + 7, "%15s %d %d %d %d", anchor_name, &anim, &index, &ax, &ay) != 5 ||
                anim < 0 || anim >= AOS_ENEMY_MAX_ANIMS || index < 0 ||
                index >= AOS_ENEMY_MAX_FRAMES)
                continue;
            for (int id = 0; id < ENEMY_KINDS; ++id)
                if (enemy_names[id] && !strcmp(anchor_name, enemy_names[id])) {
                    objects->enemies[id].kind.anchors[anim][index][0] = (int8_t)ax;
                    objects->enemies[id].kind.anchors[anim][index][1] = (int8_t)ay;
                }
            continue;
        }
        if (!strncmp(line, "edge\t", 5)) {
            char edge_name[16];
            int anim, index, ex, ey;
            if (sscanf(line + 5, "%15s %d %d %d %d", edge_name, &anim, &index, &ex, &ey) != 5 ||
                anim < 0 || anim >= AOS_ENEMY_MAX_ANIMS || index < 0 ||
                index >= AOS_ENEMY_MAX_FRAMES)
                continue;
            for (int id = 0; id < ENEMY_KINDS; ++id)
                if (enemy_names[id] && !strcmp(edge_name, enemy_names[id])) {
                    objects->enemies[id].kind.edges[anim][index][0] = (int8_t)ex;
                    objects->enemies[id].kind.edges[anim][index][1] = (int8_t)ey;
                }
            continue;
        }
        if (!strncmp(line, "static_attack\t", 14)) {
            char attack_name[16];
            int sprite, box[4];
            if (sscanf(line + 14, "%15s %d %d %d %d %d", attack_name, &sprite, &box[0], &box[1],
                       &box[2], &box[3]) != 6 ||
                sprite < 0 || sprite >= 64)
                continue;
            for (int k = 0; k < SKILL_ARTS; ++k)
                if (!strcmp(attack_name, skill_art_names[k]))
                    objects->skills[k].static_attack[sprite] =
                        (AosBox){(int8_t)box[0], (int8_t)box[1], (uint8_t)box[2], (uint8_t)box[3]};
            continue;
        }
        if (!strncmp(line, "static_anchor\t", 14)) {
            char anchor_name[16];
            int sprite, ax, ay;
            if (sscanf(line + 14, "%15s %d %d %d", anchor_name, &sprite, &ax, &ay) != 4 ||
                sprite < 0 || sprite >= 64)
                continue;
            for (int id = 0; id < ENEMY_KINDS; ++id)
                if (enemy_names[id] && !strcmp(anchor_name, enemy_names[id])) {
                    objects->enemies[id].kind.static_anchors[sprite][0] = (int8_t)ax;
                    objects->enemies[id].kind.static_anchors[sprite][1] = (int8_t)ay;
                }
            continue;
        }
        if (!strncmp(line, "volley\t", 7)) {
            /* A shooter's volley: number, then (frame, y offset) pairs. */
            char volley_name[16];
            int used = 0, number;
            char *cursor = line + 7;
            if (sscanf(cursor, "%15s %d%n", volley_name, &number, &used) != 2 || number < 0 ||
                number > 3)
                continue;
            cursor += used;
            for (int id = 0; id < ENEMY_KINDS; ++id) {
                if (!enemy_names[id] || strcmp(volley_name, enemy_names[id])) continue;
                AosEnemyKind *kind = &objects->enemies[id].kind;
                int count = 0;
                char *end;
                for (long frame; count < 8 && (frame = strtol(cursor, &end, 10), end != cursor);) {
                    cursor = end;
                    long offset = strtol(cursor, &end, 10);
                    if (end == cursor) break;
                    cursor = end;
                    kind->volleys[number][count][0] = (int8_t)frame;
                    kind->volleys[number][count][1] = (int8_t)offset;
                    ++count;
                }
                kind->volley_sizes[number] = (uint8_t)count;
            }
            continue;
        }
        /* Patch 0667: extra locally exported ROM table, eight signed (X,Y)
         * pairs. Refuse partial/malformed rows, preserving old import data. */
        if (!strncmp(line, "buer_death_offsets\tbuer\t", 24)) {
            int16_t offsets[8][2] = {{0}};
            char *cursor = line + 23;
            bool valid = true;
            for (int i = 0; i < 8 && valid; ++i) {
                for (int axis = 0; axis < 2; ++axis) {
                    if (*cursor != '\t') { valid = false; break; }
                    ++cursor;
                    char *end = NULL;
                    long value = strtol(cursor, &end, 10);
                    if (end == cursor || value < -32768 || value > 32767) {
                        valid = false;
                        break;
                    }
                    offsets[i][axis] = (int16_t)value;
                    cursor = end;
                }
            }
            if (*cursor && *cursor != '\r' && *cursor != '\n') valid = false;
            if (valid) {
                AosEnemyKind *buer = &objects->enemies[AOS_ENEMY_BUER].kind;
                memcpy(buer->buer_death_offsets, offsets, sizeof offsets);
                buer->buer_death_offsets_valid = true;
            }
            continue;
        }
        if (!strncmp(line, "probes\t", 7)) {
            /* The walker probe table: count, ceiling, floor, half width,
             * then count wall offsets. */
            char *cursor = line + 7;
            char probe_name[16];
            int used = 0;
            if (sscanf(cursor, "%15s%n", probe_name, &used) != 1) continue;
            cursor += used;
            for (int id = 0; id < ENEMY_KINDS + SKILL_ARTS; ++id) {
                if (id >= ENEMY_KINDS ? strcmp(probe_name, skill_art_names[id - ENEMY_KINDS])
                                      : !enemy_names[id] || strcmp(probe_name, enemy_names[id]))
                    continue;
                AosProbes *probes = id >= ENEMY_KINDS ? &objects->skills[id - ENEMY_KINDS].kind.probes
                                                      : &objects->enemies[id].kind.probes;
                long count = strtol(cursor, &cursor, 10);
                if (count < 1 || count > 8) break;
                probes->count = (int16_t)count;
                probes->ceiling = (int16_t)strtol(cursor, &cursor, 10);
                probes->floor = (int16_t)strtol(cursor, &cursor, 10);
                probes->half_width = (int16_t)strtol(cursor, &cursor, 10);
                for (long i = 0; i < count; ++i)
                    probes->wall_y[i] = (int16_t)strtol(cursor, &cursor, 10);
                /* Enable the mode-2/mode-6 native integration only
                 * when Buer's table came from a local ROM export.
                 * Runtime still validates every signed offset. */
                if (id == AOS_ENEMY_BUER)
                    objects->enemies[id].kind.buer_native_motion = true;
            }
            continue;
        }
        if (!strncmp(line, "margins\t", 8)) {
            char *cursor = line + 8;
            for (int i = 0; i < 14; ++i) margins[i] = (int)strtol(cursor, &cursor, 10);
            continue;
        }
        if (sscanf(line, "%15s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", name, &anim,
                   &index, &frame, &ticks, &mode, &h[0], &h[1], &h[2], &h[3], &a[0], &a[1], &a[2],
                   &a[3]) != 14 ||
            anim < 0 || anim >= AOS_ENEMY_MAX_ANIMS || index < 0 || index >= AOS_ENEMY_MAX_FRAMES)
            continue;
        for (int id = 0; id < ENEMY_KINDS + SKILL_ARTS; ++id) {
            AosEnemyKind *kind;
            if (id >= ENEMY_KINDS) {
                if (strcmp(name, skill_art_names[id - ENEMY_KINDS])) continue;
                kind = &objects->skills[id - ENEMY_KINDS].kind;
            } else {
                if (!enemy_names[id] || strcmp(name, enemy_names[id])) continue;
                kind = &objects->enemies[id].kind;
            }
            kind->modes[anim][index] = (uint8_t)mode;
            kind->sprites[anim][index] = (uint8_t)frame;
            kind->hurt[anim][index] = (AosBox){(int8_t)h[0], (int8_t)h[1], (uint8_t)h[2], (uint8_t)h[3]};
            kind->attack[anim][index] = (AosBox){(int8_t)a[0], (int8_t)a[1], (uint8_t)a[2],
                                                 (uint8_t)a[3]};
        }
    }
    fclose(f);
    for (int id = 0; id < ENEMY_KINDS; ++id) {
        AosEnemyKind *kind = &objects->enemies[id].kind;
        kind->anims = &objects->enemies[id].set;
        kind->flame_anims = objects->effects[AOS_EFFECT_FLAME].def.count
                                ? &objects->effects[AOS_EFFECT_FLAME].set
                                : NULL;
        memcpy(kind->blink, blink, sizeof blink);
        for (int i = 0; i < 7; ++i) {
            kind->margins[i][0] = (int16_t)margins[2 * i];
            kind->margins[i][1] = (int16_t)margins[2 * i + 1];
        }
    }
    f = fopen(DEFAULT_ENEMY_STATS, "rb");
    if (!f) return false;
    while (fgets(line, sizeof line, f)) {
        int id, hp, field_e, contact, defence;
        unsigned weak, resist, experience = 0, common = 0, rare = 0, rarity = 0;
        unsigned common_rate = 0, rare_rate = 0, soul_type = 0, soul_index = 0;
        int portrait_x = 0, portrait_y = 0;
        /* Schema v1 exports lack the experience column, v2 the drops, v4 the
         * portrait anchor offsets of sub_0804CA18. */
        int fields = sscanf(line, "%d\t%d\t%d\t%d\t%d\t%x\t%x\t%u\t%x\t%x\t%u\t%u\t%u\t%u\t%u\t%d\t%d",
                            &id, &hp, &field_e, &contact, &defence, &weak, &resist,
                            &experience, &common, &rare, &rarity, &common_rate, &rare_rate,
                            &soul_type, &soul_index, &portrait_x, &portrait_y);
        if (fields >= 15 && id >= 0 && id < ENEMY_KINDS)
            objects->enemies[id].drops = (AosEnemyDrops){
                (uint16_t)common, (uint16_t)rare, (uint8_t)rarity, (uint8_t)common_rate,
                (uint8_t)rare_rate, (uint8_t)soul_type, (uint8_t)soul_index};
        if (fields >= 17 && id >= 0 && id < ENEMY_KINDS) {
            objects->enemies[id].portrait_x = (int16_t)portrait_x;
            objects->enemies[id].portrait_y = (int16_t)portrait_y;
        }
        if (fields >= 7 && id >= 0 && id < ENEMY_KINDS)
            objects->enemies[id].stats = (AosEnemyStats){(int16_t)hp, (uint8_t)contact,
                                                         (uint8_t)defence, (uint16_t)weak,
                                                         (uint16_t)resist,
                                                         (uint16_t)experience};
    }
    fclose(f);
    /* metadata/item_icons.tsv: the icon each item record shows. */
    f = fopen(DEFAULT_ITEM_ICONS, "rb");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char kind[16];
            unsigned index, icon;
            if (sscanf(line, "%15s\t%u\t%u", kind, &index, &icon) != 3 || index >= 59 ||
                icon >= ITEM_ICONS)
                continue;
            int table = !strcmp(kind, "item") ? 0 : !strcmp(kind, "weapon") ? 1
                        : !strcmp(kind, "armor") ? 2 : -1;
            if (table >= 0) objects->item_icon[table][index] = (uint8_t)icon;
        }
        fclose(f);
    }
    /* metadata/money.tsv (older exports lack it: money then never drops). */
    f = fopen(DEFAULT_MONEY, "rb");
    if (f) {
        unsigned loaded = 0;
        while (fgets(line, sizeof line, f)) {
            unsigned variant, value, text, field_8, field_9, lifted;
            if (sscanf(line, "%u\t%u\t%u\t%u\t%u\t%u", &variant, &value, &text, &field_8,
                       &field_9, &lifted) == 6 && variant < AOS_MONEY_KINDS) {
                objects->money[variant] = (AosMoneyKind){value, text, (uint8_t)field_8,
                                                         (uint8_t)field_9, (uint16_t)lifted};
                loaded |= 1u << variant;
            }
        }
        fclose(f);
        objects->money_loaded = loaded == (1u << AOS_MONEY_KINDS) - 1;
    }
    return true;
}

/* metadata/yellow_souls.tsv: Soma's yellow soul records; the routines the
 * runtime does not run are cleared so that they do nothing. */
bool aria_load_yellow_souls(const char *path,
                              AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned loaded = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned soul, routine, refresh, parameter, argument;
        if (sscanf(line, "%u\t%x\t%u\t%u\t%x", &soul, &routine, &refresh, &parameter,
                   &argument) != 5 ||
            soul >= AOS_YELLOW_SOUL_RECORDS)
            continue;
        records[soul] = (AosYellowSoulRecord){aos_yellow_soul_ported(routine) ? routine : 0,
                                              (uint16_t)refresh, (uint16_t)parameter, argument};
        loaded++;
    }
    fclose(f);
    return loaded == AOS_YELLOW_SOUL_RECORDS;
}

/* metadata/blue_souls.tsv: Soma's blue soul records. */
bool aria_load_blue_souls(const char *path, AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned loaded = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned soul, routine, mp, flags, parameter, argument;
        if (sscanf(line, "%u\t%x\t%u\t%x\t%u\t%x", &soul, &routine, &mp, &flags, &parameter,
                   &argument) != 6 ||
            soul >= AOS_BLUE_SOUL_RECORDS)
            continue;
        records[soul] = (AosBlueSoulRecord){routine, (uint8_t)mp, (uint8_t)flags,
                                            (uint16_t)parameter, argument};
        loaded++;
    }
    fclose(f);
    return loaded == AOS_BLUE_SOUL_RECORDS;
}

/* metadata/red_souls.tsv: Soma's red soul records. */
bool aria_load_red_souls(const char *path, AosRedSoulRecord records[AOS_RED_SOUL_RECORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned loaded = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned soul, skill, pose, maximum, power, parameter, elements;
        int mp;
        if (sscanf(line, "%u\t%x\t%u\t%d\t%u\t%u\t%u\t%x", &soul, &skill, &pose, &mp,
                   &maximum, &power, &parameter, &elements) != 8 ||
            soul >= AOS_RED_SOUL_RECORDS)
            continue;
        records[soul] = (AosRedSoulRecord){skill, (uint16_t)pose, (int16_t)mp,
                                           (uint8_t)maximum, (uint16_t)parameter,
                                           (uint16_t)power, (uint16_t)elements};
        loaded++;
    }
    fclose(f);
    return loaded == AOS_RED_SOUL_RECORDS;
}

/* metadata/bursts.tsv: burst list 0 as ready burst effects (offsets in the
 * whole pixels of x and y). Its arts are those of their animation and row. */
#define MAX_BURST 32
static AosEffect burst_list[MAX_BURST];
static size_t burst_size;

bool aria_load_bursts(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    burst_size = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned list, anim, row, priority, delay;
        int dx, dy, vx, vy, ax, ay;
        if (sscanf(line, "%u\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d", &list, &anim, &row,
                   &priority, &delay, &dx, &dy, &vx, &vy, &ax, &ay) != 11 ||
            list != 0 || burst_size >= MAX_BURST)
            continue;
        int kind = anim == 0 && row == 7    ? AOS_EFFECT_FLAME
                   : anim == 1 && row == 7  ? AOS_EFFECT_BURST_1
                   : anim == 4 && row == 6  ? AOS_EFFECT_DUST
                   : anim == 5 && row == 6  ? AOS_EFFECT_SMOKE
                   : anim == 12 && row == 6 ? AOS_EFFECT_BURST_12
                   : anim == 15 && row == 7 ? AOS_EFFECT_BURST_15
                                            : -1;
        if (kind < 0) continue;
        burst_list[burst_size++] = (AosEffect){
            .kind = (uint8_t)kind, .burst = true, .x = (int32_t)((uint32_t)dx << 16),
            .y = (int32_t)((uint32_t)dy << 16), .vx = vx, .vy = vy, .ax = ax, .ay = ay,
            .timer = (uint8_t)delay};
    }
    fclose(f);
    return burst_size > 0;
}

/* metadata/skill_throws.tsv: the shared throw routine's records. */
bool aria_load_skill_throws(const char *path, AosThrowRecord records[AOS_THROW_RECORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned loaded = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned index, resource, frame, sequence, palette, song, flags, spin, field_16;
        int vx, vy, gravity;
        if (sscanf(line, "%u\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t%x\t%x\t%x\t%x", &index, &vx, &vy,
                   &gravity, &resource, &frame, &sequence, &palette, &song, &flags, &spin,
                   &field_16) != 12 ||
            index >= AOS_THROW_RECORDS)
            continue;
        records[index] = (AosThrowRecord){vx, vy, gravity, (uint8_t)resource, (uint8_t)frame,
                                          (uint8_t)sequence, (uint8_t)palette, (uint16_t)song,
                                          (uint16_t)flags, (uint16_t)spin, (uint16_t)field_16};
        loaded++;
    }
    fclose(f);
    return loaded == AOS_THROW_RECORDS;
}

/* The soul inventories and abilities as the file stores them. */
_Static_assert(sizeof ((AosStats *)0)->red_souls + sizeof ((AosStats *)0)->blue_souls +
                       sizeof ((AosStats *)0)->yellow_souls +
                       sizeof ((AosStats *)0)->ability_souls +
                       sizeof ((AosStats *)0)->abilities ==
                   AOS_SAVE_SOUL_BYTES,
               "the save's soul bytes match the stats");
void aria_souls_from_save(AosStats *stats, const AosSaveData *save) {
    const uint8_t *bytes = save->souls;
    memcpy(stats->red_souls, bytes, sizeof stats->red_souls);
    bytes += sizeof stats->red_souls;
    memcpy(stats->blue_souls, bytes, sizeof stats->blue_souls);
    bytes += sizeof stats->blue_souls;
    memcpy(stats->yellow_souls, bytes, sizeof stats->yellow_souls);
    bytes += sizeof stats->yellow_souls;
    memcpy(stats->ability_souls, bytes, sizeof stats->ability_souls);
    bytes += sizeof stats->ability_souls;
    memcpy(stats->abilities, bytes, sizeof stats->abilities);
    stats->souls_collected = save->souls_collected;
}

static void souls_to_save(AosSaveData *save, const AosStats *stats) {
    uint8_t *bytes = save->souls;
    memcpy(bytes, stats->red_souls, sizeof stats->red_souls);
    bytes += sizeof stats->red_souls;
    memcpy(bytes, stats->blue_souls, sizeof stats->blue_souls);
    bytes += sizeof stats->blue_souls;
    memcpy(bytes, stats->yellow_souls, sizeof stats->yellow_souls);
    bytes += sizeof stats->yellow_souls;
    memcpy(bytes, stats->ability_souls, sizeof stats->ability_souls);
    bytes += sizeof stats->ability_souls;
    memcpy(bytes, stats->abilities, sizeof stats->abilities);
    save->souls_collected = stats->souls_collected;
}

void aria_runtime_to_save(AosSaveData *save, const AriaRoom *room,
                            const AosSoma *soma, const AosStats *progress) {
    save->area = (uint8_t)room->area;
    save->room = (uint16_t)room->number;
    save->spawn_x = (int16_t)(soma->x >> 16);
    save->spawn_y = (int16_t)(soma->y >> 16);
    save->hp = soma->hp;
    save->max_hp = soma->max_hp;
    save->moves = soma->moves;
    save->experience = progress->experience;
    save->gold = progress->gold;
    memcpy(save->defeated, progress->defeated, sizeof save->defeated);
    memcpy(save->dropped, progress->dropped, sizeof save->dropped);
    memcpy(save->events, aria_events, sizeof save->events);
    memcpy(save->placed_items, aria_pickup_state.placed, sizeof save->placed_items);
    save->special_souls = aria_pickup_state.special;
    save->weapon = progress->weapon;
    save->armor = progress->armor;
    save->accessory = progress->accessory;
    save->red_soul = progress->red_soul;
    save->blue_soul = progress->blue_soul;
    save->yellow_soul = progress->yellow_soul;
    save->challenge_flags = progress->challenge_flags;
    save->mp = progress->mp;
    for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
        save->buttons[action] = aos_button_config_index(
            progress->buttons.action[action]);
    memcpy(save->inventory, progress->item_counts, sizeof progress->item_counts);
    memcpy(save->inventory + AOS_SAVE_WEAPONS, progress->weapon_counts,
           sizeof progress->weapon_counts);
    memcpy(save->inventory + AOS_SAVE_ARMOR, progress->armor_counts,
           sizeof progress->armor_counts);
    souls_to_save(save, progress);
}

bool aria_load_equipment(const char *path, AosEquipmentTables *tables) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned weapons = 0, armor = 0;
    while (fgets(line, sizeof line, f)) {
        char kind[16];
        unsigned index, atk, def, resist;
        int str, con, intelligence, lck;
        if (sscanf(line, "%15s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%x", kind, &index, &atk, &def,
                   &str, &con, &intelligence, &lck, &resist) != 9)
            continue;
        AosEquipmentRecord record = {(uint8_t)atk, (uint8_t)def,
                                     {(int8_t)str, (int8_t)con, (int8_t)intelligence,
                                      (int8_t)lck},
                                     (uint16_t)resist};
        if (!strcmp(kind, "weapon") && index < AOS_WEAPON_RECORDS) {
            tables->weapons[index] = record;
            weapons++;
        } else if (!strcmp(kind, "armor") && index < AOS_ARMOR_RECORDS) {
            tables->armor[index] = record;
            armor++;
        }
    }
    fclose(f);
    return weapons == AOS_WEAPON_RECORDS && armor == AOS_ARMOR_RECORDS;
}

bool aria_load_item_use(const char *path, AosItemUseRecord items[AOS_ITEM_RECORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    unsigned loaded = 0;
    bool seen[AOS_ITEM_RECORDS] = {false};
    while (fgets(line, sizeof line, f)) {
        unsigned item, icon, effect, amount;
        if (sscanf(line, "%u\t%u\t%u\t%u", &item, &icon, &effect, &amount) != 4 ||
            item >= AOS_ITEM_RECORDS || icon > UINT8_MAX || effect > UINT8_MAX ||
            amount > UINT16_MAX)
            continue;
        items[item] = (AosItemUseRecord){(uint8_t)icon, (uint8_t)effect, (uint16_t)amount};
        if (!seen[item]) {
            seen[item] = true;
            loaded++;
        }
    }
    fclose(f);
    return loaded == AOS_ITEM_RECORDS;
}

static bool load_frame(AriaFrame *out, const char *path, int ox, int oy, SDL_Renderer *renderer) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) return false;
    out->w = (float)surface->w;
    out->h = (float)surface->h;
    out->offset_x = ox;
    out->offset_y = oy;
    if (!renderer) {
        /* Headless loads (--simulate): the frame's metrics count as
         * loaded but no texture exists; nothing draws in that mode. */
        out->texture = NULL;
        SDL_DestroySurface(surface);
        return true;
    }
    out->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!out->texture) return false;
    SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
    return true;
}

bool aria_load_objects(const char *path, AriaObjects *objects, SDL_Renderer *renderer) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    for (int v = 0; v < SPECIAL_VARIANTS; ++v) objects->special_variants[v].id = -1;
    char line[1024];
    bool ok = fgets(line, sizeof line, f) && !strncmp(line, INDEX_SCHEMA, strlen(INDEX_SCHEMA));
    while (ok && fgets(line, sizeof line, f)) {
        char frame_path[512];
        int style, frame, ticks, ox, oy;
        char enemy_name[24];
        if (sscanf(line, "Enemy/%15[^/]/frame_%d\t%d\t%d\t%d\t%d\t%511s", enemy_name, &style,
                   &frame, &ticks, &ox, &oy, frame_path) == 7) {
            int wall = -1;
            if (sscanf(enemy_name, "wall_%d", &wall) == 1) {
                if (wall < 0 || wall >= AOS_WALL_ENTRIES || frame != 0 ||
                    !load_frame(&objects->walls[wall], frame_path, ox, oy, renderer))
                    ok = false;
                continue;
            }
            for (int id = 0; id < ENEMY_KINDS + SKILL_ARTS; ++id) {
                if (id >= ENEMY_KINDS ? strcmp(enemy_name, skill_art_names[id - ENEMY_KINDS])
                                      : !enemy_names[id] || strcmp(enemy_name, enemy_names[id]))
                    continue;
                __typeof__(objects->enemies[0]) *art =
                    id >= ENEMY_KINDS ? &objects->skills[id - ENEMY_KINDS] : &objects->enemies[id];
                if (frame != 0 || art->static_count >= ENEMY_STATIC_FRAMES ||
                    !load_frame(&art->statics[art->static_count], frame_path, ox, oy, renderer)) {
                    ok = false;
                    break;
                }
                if (id < ENEMY_KINDS) {
                    size_t length = strlen(frame_path) + 1;
                    art->static_paths[art->static_count] = malloc(length);
                    if (art->static_paths[art->static_count])
                        memcpy(art->static_paths[art->static_count], frame_path, length);
                }
                art->static_ids[art->static_count++] = style;
            }
            continue;
        }
        if (sscanf(line, "Effect/%23[^/]/anim_%d\t%d\t%d\t%d\t%d\t%511s", enemy_name, &style,
                   &frame, &ticks, &ox, &oy, frame_path) == 7) {
            int damage = -1;
            bool damage_guard = !strcmp(enemy_name, "damage_guard");
            if (sscanf(enemy_name, "damage_%d", &damage) == 1 || damage_guard) {
                __typeof__(objects->damage[0]) *art = damage_guard
                    ? &objects->damage_guard
                    : damage >= 0 && damage < AOS_DAMAGE_POPUP_GLYPHS
                        ? &objects->damage[damage] : NULL;
                if (!art || style != 0 || frame != 0 || ticks < 1 || ticks > 255 ||
                    !load_frame(&art->frames[0], frame_path, ox, oy, renderer)) {
                    ok = false;
                    break;
                }
                art->ticks[0] = (uint8_t)ticks;
                art->def = (AosAnimDef){1, art->ticks};
                art->set = (AosAnimSet){&art->def, 1};
                continue;
            }
            /* One animation per effect kind or pickup sprite. */
            for (int k = 0; k < AOS_EFFECT_KINDS + PICKUP_ARTS; ++k) {
                if (strcmp(enemy_name, k < AOS_EFFECT_KINDS ? effect_names[k]
                                                            : pickup_art_names[k - AOS_EFFECT_KINDS]))
                    continue;
                __typeof__(objects->effects[0]) *art =
                    k < AOS_EFFECT_KINDS ? &objects->effects[k]
                                         : &objects->pickup_arts[k - AOS_EFFECT_KINDS];
                if (frame != art->def.count || frame >= MAX_FRAMES || ticks < 1 || ticks > 255 ||
                    !load_frame(&art->frames[frame], frame_path, ox, oy, renderer)) {
                    ok = false;
                    break;
                }
                art->ticks[frame] = (uint8_t)ticks;
                art->def.durations = art->ticks;
                art->def.count++;
                art->set = (AosAnimSet){&art->def, 1};
            }
            continue;
        }
        /* Object20's first-room actors share the object exporter format but
         * are not enemies. Keep their animation tables separate so the
         * actor sheets cannot be selected by an ordinary enemy id. */
        int actor, actor_key;
        if (sscanf(line, "Enemy/opening_actor_%d/anim_%d\t%d\t%d\t%d\t%d\t%511s", &actor_key,
                   &style, &frame, &ticks, &ox, &oy, frame_path) == 7) {
            /* The third record is index 8 in the native twelve-entry table;
             * it occupies slot 2 in the compact runtime array. */
            actor = actor_key == 8 ? 2 : actor_key;
            if (actor < 0 || actor >= AOS_OPENING_ACTOR_COUNT || style < 0 ||
                style >= OBJECT_MAX_ANIMS || frame != objects->opening_actors[actor].defs[style].count ||
                frame >= MAX_FRAMES || ticks < 1 || ticks > 255 ||
                !load_frame(&objects->opening_actors[actor].frames[style][frame], frame_path,
                            ox, oy, renderer)) {
                ok = false;
                break;
            }
            objects->opening_actors[actor].ticks[style][frame] = (uint8_t)ticks;
            objects->opening_actors[actor].defs[style].durations =
                objects->opening_actors[actor].ticks[style];
            objects->opening_actors[actor].defs[style].count++;
            objects->opening_actors[actor].loaded = true;
            if (style + 1 > objects->opening_actors[actor].set.count)
                objects->opening_actors[actor].set.count = (uint16_t)(style + 1);
            objects->opening_actors[actor].set.anims = objects->opening_actors[actor].defs;
            continue;
        }
        /* Object20's private table records 2-7/9-11
         * (Enemy/object20_actor_N); records 0/1/8 load above as
         * opening_actor_*. Indexed by the native record number so
         * the param0 -> record map stays a plain table lookup. */
        int actor20, actor20_style;
        if (sscanf(line, "Enemy/object20_actor_%d/anim_%d\t%d\t%d\t%d\t%d\t%511s",
                   &actor20, &actor20_style, &frame, &ticks, &ox, &oy,
                   frame_path) == 7) {
            if (actor20 < 0 || actor20 >= 12 || actor20_style < 0 ||
                actor20_style >= OBJECT20_ACTOR_ANIMS ||
                frame != objects->object20_actors[actor20].defs[actor20_style].count ||
                frame >= MAX_FRAMES || ticks < 1 || ticks > 255 ||
                !load_frame(&objects->object20_actors[actor20]
                                .frames[actor20_style][frame],
                            frame_path, ox, oy, renderer)) {
                ok = false;
                break;
            }
            objects->object20_actors[actor20].ticks[actor20_style][frame] =
                (uint8_t)ticks;
            objects->object20_actors[actor20].defs[actor20_style].durations =
                objects->object20_actors[actor20].ticks[actor20_style];
            objects->object20_actors[actor20].defs[actor20_style].count++;
            objects->object20_actors[actor20].loaded = true;
            if (actor20_style + 1 > objects->object20_actors[actor20].set.count)
                objects->object20_actors[actor20].set.count =
                    (uint16_t)(actor20_style + 1);
            objects->object20_actors[actor20].set.anims =
                objects->object20_actors[actor20].defs;
            continue;
        }
        if (sscanf(line, "Enemy/%23[^/]/anim_%d\t%d\t%d\t%d\t%d\t%511s", enemy_name, &style, &frame,
                   &ticks, &ox, &oy, frame_path) == 7) {
            for (int id = 0; id < ENEMY_KINDS + VARIANT_ARTS + SKILL_ARTS; ++id) {
                __typeof__(objects->enemies[0]) *art;
                if (id >= ENEMY_KINDS + VARIANT_ARTS) {
                    int k = id - ENEMY_KINDS - VARIANT_ARTS;
                    if (strcmp(enemy_name, skill_art_names[k])) continue;
                    art = &objects->skills[k];
                } else if (id < ENEMY_KINDS) {
                    if (!enemy_names[id] || strcmp(enemy_name, enemy_names[id])) continue;
                    art = &objects->enemies[id];
                } else {
                    int v = id - ENEMY_KINDS, key = variant_keys[v].id;
                    char variant[32];
                    if (key < 0 || !enemy_names[key]) continue;
                    snprintf(variant, sizeof variant, "%s_bank%d", enemy_names[key],
                             variant_keys[v].bank);
                    if (strcmp(enemy_name, variant)) continue;
                    art = &objects->variants[v];
                }
                if (style < 0 || style >= AOS_ENEMY_MAX_ANIMS || frame != art->defs[style].count ||
                    frame >= MAX_FRAMES || ticks < 1 || ticks > 255 ||
                    !load_frame(&art->frames[style][frame], frame_path, ox, oy, renderer)) {
                    ok = false;
                    break;
                }
                if (id < ENEMY_KINDS) {
                    size_t length = strlen(frame_path) + 1;
                    art->paths[style][frame] = malloc(length);
                    if (art->paths[style][frame])
                        memcpy(art->paths[style][frame], frame_path, length);
                }
                art->ticks[style][frame] = (uint8_t)ticks;
                art->defs[style].durations = art->ticks[style];
                art->defs[style].count++;
                art->loaded = true;
                if (style + 1 > art->set.count) art->set.count = (uint16_t)(style + 1);
            }
            continue;
        }
        /* Object/special_NN/anim_M and Object/special_NN_vP/anim_M: room
         * SPECIAL object sprites. The object id is the SPECIAL entity id;
         * the optional _vP suffix is the param0-driven variant (ids 14 and
         * 18 resolve their sprite through entity->param0). Animation
         * selection per object is a later audit, so every exported
         * animation is kept. */
        char object_name[24];
        int object_id = -1, object_param = -1, object_style = -1;
        int burst_kind = 0; /* 1 = carrier (anim_), 2 = particle (frame_) */
        int burst_sub = 0;  /* still-row sub-index (always 0) */
        if (sscanf(line, "Object/%23[^/]/anim_%d\t%d\t%d\t%d\t%d\t%511s", object_name,
                   &object_style, &frame, &ticks, &ox, &oy, frame_path) == 7 &&
            strcmp(object_name, "whiteout_burst_carrier") == 0)
            burst_kind = 1;
        else if (sscanf(line, "Object/%23[^/]/frame_%d\t%d\t%d\t%d\t%d\t%511s",
                        object_name, &frame, &burst_sub, &ticks, &ox, &oy,
                        frame_path) == 7 &&
                 strcmp(object_name, "whiteout_burst_particle") == 0)
            burst_kind = 2;
        if (!burst_kind) {
            if (sscanf(line, "Object/%23[^/]/anim_%d\t%d\t%d\t%d\t%d\t%511s",
                       object_name, &object_style, &frame, &ticks, &ox, &oy,
                       frame_path) != 7)
                goto not_object;
            if (sscanf(object_name, "special_%d_v%d", &object_id,
                       &object_param) < 1)
                goto not_object;
        }
        if (burst_kind == 2) {
            /* Still frames land at their native +0x65 record index
             * (0xB/0xC), not a compacted slot — the draw indexes
             * frames[0][field_65] directly. */
            if (frame < 0 || frame >= MAX_FRAMES) { ok = false; break; }
            __typeof__(objects->special_objects[0]) *part =
                &objects->wo_burst_particle;
            if (!load_frame(&part->frames[0][frame], frame_path, ox, oy,
                            renderer)) {
                ok = false;
                break;
            }
            part->ticks[0][frame] = (uint8_t)ticks;
            part->loaded = true;
            continue;
        }
        if ((burst_kind != 2 &&
             (burst_kind == 1
                  ? (object_style < 0 ||
                     object_style >= OBJECT_MAX_ANIMS)
                  : (object_id < 0 || object_id >= 64 ||
                     object_style < 0 ||
                     object_style >= OBJECT_MAX_ANIMS))) ||
            frame < 0 || frame >= MAX_FRAMES || ticks < 1 ||
            ticks > 255) {
            ok = false;
            break;
        }
        {
            __typeof__(objects->special_objects[0]) *art = NULL;
            if (burst_kind == 1) {
                art = &objects->wo_burst_carrier;
            } else if (object_param >= 0) {
                for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                    if (objects->special_variants[v].id == object_id &&
                        objects->special_variants[v].param0 == object_param) {
                        art = &objects->special_variants[v];
                        break;
                    }
                if (!art)
                    for (int v = 0; v < SPECIAL_VARIANTS; ++v)
                        if (objects->special_variants[v].id < 0) {
                            objects->special_variants[v].id = object_id;
                            objects->special_variants[v].param0 = object_param;
                            art = &objects->special_variants[v];
                            break;
                        }
                if (!art) { ok = false; break; }
            } else {
                art = &objects->special_objects[object_id];
            }
            if (frame != art->defs[object_style].count ||
                !load_frame(&art->frames[object_style][frame], frame_path, ox, oy,
                            renderer)) {
                ok = false;
                break;
            }
            art->ticks[object_style][frame] = (uint8_t)ticks;
            art->defs[object_style].durations = art->ticks[object_style];
            art->defs[object_style].count++;
            art->loaded = true;
            if (object_style + 1 > art->set.count)
                art->set.count = (uint16_t)(object_style + 1);
            continue;
        }
    not_object:;
        int icon;
        if (sscanf(line, "Icon/i%d/anim_0\t%d\t%d\t%d\t%d\t%511s", &icon, &frame, &ticks, &ox,
                   &oy, frame_path) == 6) {
            if (icon > 0 && icon < ITEM_ICONS && frame == 0 &&
                !load_frame(&objects->icons[icon], frame_path, ox, oy, renderer))
                ok = false;
            continue;
        }
        if (sscanf(line, "WoodenDoor/style_%d\t%d\t%d\t%d\t%d\t%511s", &style, &frame, &ticks,
                   &ox, &oy, frame_path) != 6)
            continue;
        if (style < 0 || style >= DOOR_STYLES || frame != objects->counts[style] ||
            frame >= MAX_FRAMES || ticks < 1 || ticks > 255) { ok = false; break; }
        SDL_Surface *surface = SDL_LoadBMP(frame_path);
        if (!surface) { ok = false; break; }
        AriaFrame *out = &objects->frames[style][frame];
        out->w = (float)surface->w;
        out->h = (float)surface->h;
        out->offset_x = ox;
        out->offset_y = oy;
        out->texture = renderer ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
        SDL_DestroySurface(surface);
        if (renderer && !out->texture) { ok = false; break; }
        if (out->texture) SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
        objects->ticks[style][frame] = (uint8_t)ticks;
        objects->counts[style]++;
    }
    fclose(f);
    for (int id = 0; id < ENEMY_KINDS; ++id) objects->enemies[id].set.anims = objects->enemies[id].defs;
    for (int id = 0; id < 64; ++id)
        objects->special_objects[id].set.anims = objects->special_objects[id].defs;
    for (int v = 0; v < SPECIAL_VARIANTS; ++v)
        objects->special_variants[v].set.anims = objects->special_variants[v].defs;
    objects->wo_burst_carrier.set.anims = objects->wo_burst_carrier.defs;
    objects->wo_burst_particle.set.anims = objects->wo_burst_particle.defs;
    for (int v = 0; v < VARIANT_ARTS; ++v) objects->variants[v].set.anims = objects->variants[v].defs;
    for (int k = 0; k < SKILL_ARTS; ++k) objects->skills[k].set.anims = objects->skills[k].defs;
    return ok;
}

void aria_free_objects(AriaObjects *objects) {
    for (int i = 0; i < DOOR_STYLES; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (objects->frames[i][j].texture) SDL_DestroyTexture(objects->frames[i][j].texture);
    for (int id = 0; id < ENEMY_KINDS; ++id)
        for (int i = 0; i < AOS_ENEMY_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->enemies[id].frames[i][j].texture)
                    SDL_DestroyTexture(objects->enemies[id].frames[i][j].texture);
    for (int v = 0; v < VARIANT_ARTS; ++v)
        for (int i = 0; i < AOS_ENEMY_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->variants[v].frames[i][j].texture)
                    SDL_DestroyTexture(objects->variants[v].frames[i][j].texture);
    for (int actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor)
        for (int i = 0; i < OBJECT_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->opening_actors[actor].frames[i][j].texture)
                    SDL_DestroyTexture(objects->opening_actors[actor].frames[i][j].texture);
    for (int id = 0; id < 64; ++id)
        for (int i = 0; i < OBJECT_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->special_objects[id].frames[i][j].texture)
                    SDL_DestroyTexture(objects->special_objects[id].frames[i][j].texture);
    for (int v = 0; v < SPECIAL_VARIANTS; ++v)
        for (int i = 0; i < OBJECT_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->special_variants[v].frames[i][j].texture)
                    SDL_DestroyTexture(objects->special_variants[v].frames[i][j].texture);
    for (int b = 0; b < 2; ++b) {
        const __typeof__(objects->special_objects[0]) *burst_art[] = {
            &objects->wo_burst_carrier, &objects->wo_burst_particle,
        };
        for (int i = 0; i < OBJECT_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (burst_art[b]->frames[i][j].texture)
                    SDL_DestroyTexture(burst_art[b]->frames[i][j].texture);
    }
    for (int actor = 0; actor < 12; ++actor)
        for (int i = 0; i < OBJECT20_ACTOR_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->object20_actors[actor].frames[i][j].texture)
                    SDL_DestroyTexture(objects->object20_actors[actor].frames[i][j].texture);
    for (int k = 0; k < AOS_EFFECT_KINDS; ++k)
        for (int i = 0; i < MAX_FRAMES; ++i)
            if (objects->effects[k].frames[i].texture)
                SDL_DestroyTexture(objects->effects[k].frames[i].texture);
    for (int k = 0; k < AOS_DAMAGE_POPUP_GLYPHS; ++k)
        if (objects->damage[k].frames[0].texture)
            SDL_DestroyTexture(objects->damage[k].frames[0].texture);
    if (objects->damage_guard.frames[0].texture)
        SDL_DestroyTexture(objects->damage_guard.frames[0].texture);
    for (int id = 0; id < ENEMY_KINDS; ++id)
        for (int i = 0; i < objects->enemies[id].static_count; ++i)
            if (objects->enemies[id].statics[i].texture)
                SDL_DestroyTexture(objects->enemies[id].statics[i].texture);
    for (int k = 0; k < AOS_WALL_ENTRIES; ++k)
        if (objects->walls[k].texture) SDL_DestroyTexture(objects->walls[k].texture);
}

/* Burst list 0 as loaded by aria_load_bursts (sub_08045A98's source). */
const AosEffect *aria_burst_effects(size_t *count) {
    *count = burst_size;
    return burst_list;
}
