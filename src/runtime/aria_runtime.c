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
#include "aos_audio.h"
#include "aos_mp2k.h"
#include "aos_room_music.h"
#include "aos_door.h"
#include "aos_wall.h"
#include "aos_enemy.h"
#include "aos_game_lifecycle.h"
#include "aos_intro_font.h"
#include "aos_intro_resources.h"
#include "aos_opening_event.h"
#include "aos_pause_graphics.h"
#include "aos_portrait.h"
#include "aos_quick_map.h"
#include "aos_room.h"
#include "aos_save.h"
#include "aos_stats.h"
#include "aos_pickup.h"
#include "aos_soul.h"
#include "aos_soul_notice.h"
#include "aos_soul_menu.h"
#include "aos_skill_axe.h"
#include "aos_skill_crow.h"
#include "aos_skill_dagger.h"
#include "aos_skill_flea.h"
#include "aos_skill_fish_head.h"
#include "aos_skill_killer_fish.h"
#include "aos_skill_gladiator.h"
#include "aos_skill_devil.h"
#include "aos_skill_ghost.h"
#include "aos_skill_siren.h"
#include "aos_skill_merman.h"
#include "aos_skill_needles.h"
#include "aos_guardian_imp.h"
#include "aos_guardian_death.h"
#include "aos_guardian_ghost.h"
#include "aos_skill_lightning.h"
#include "aos_skill_killer_doll.h"
#include "aos_skill_nemesis.h"
#include "aos_skill_ripper.h"
#include "aos_skill_millione.h"
#include "aos_skill_shot.h"
#include "aos_skill_skull.h"
#include "aos_skill_valkyrie.h"
#include "aos_skill_spear.h"
#include "aos_skill_throw.h"
#include "aos_hud.h"
#include "aos_levelup.h"
#include "aos_game_over.h"
#include "aos_save_room.h"
#include "aos_soma.h"
#include "aos_weapon.h"
#include "aos_window_frame.h"
#include "debug_menu.h"
#include "gba_input.h"
#include "aos_input.h"
#include "fusion/assets/rom.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_W 240
#define VIEW_H 160
#define MAX_FILE_BYTES (8L * 1024 * 1024)
#define MAX_ANIMS 256
#define MAX_FRAMES 80
/* Opening actors and SPECIAL objects share one art struct; the shared
 * flame descriptor 0x0820ED60 exports 23 animations, past the enemy
 * AOS_ENEMY_MAX_ANIMS of 20. */
#define OBJECT_MAX_ANIMS 24
#define INDEX_SCHEMA "schema\tmetroidvania-sprite-index-v1"
#define DEFAULT_ROOMS "assets/extracted/aria/rooms/runtime"
#define DEFAULT_LIBRARY "assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv"
#define DEFAULT_OBJECTS "assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
#define DEFAULT_WEAPONS "assets/extracted/aria/metadata/weapons.tsv"
#define DEFAULT_WEAPON_FRAMES "assets/extracted/aria/metadata/weapon_frames.tsv"
#define DEFAULT_WEAPON_SPRITES "assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv"
#define DEFAULT_ENEMY_FRAMES "assets/extracted/aria/metadata/enemy_frames.tsv"
#define DEFAULT_ENEMY_STATS "assets/extracted/aria/metadata/enemies.tsv"
#define DEFAULT_MONEY "assets/extracted/aria/metadata/money.tsv"
#define DEFAULT_ITEM_USE "assets/extracted/aria/metadata/item_use.tsv"
#define DEFAULT_EQUIPMENT "assets/extracted/aria/metadata/equipment.tsv"
#define DEFAULT_RED_SOULS "assets/extracted/aria/metadata/red_souls.tsv"
#define DEFAULT_BLUE_SOULS "assets/extracted/aria/metadata/blue_souls.tsv"
#define DEFAULT_YELLOW_SOULS "assets/extracted/aria/metadata/yellow_souls.tsv"
#define DEFAULT_SKILL_THROWS "assets/extracted/aria/metadata/skill_throws.tsv"
#define DEFAULT_BURSTS "assets/extracted/aria/metadata/bursts.tsv"
#define DEFAULT_AUDIO_TABLE "assets/extracted/aria/sound/runtime/song_table.tsv"
#define DEFAULT_AUDIO_BANK "assets/extracted/aria/sound/runtime/sound_bank.bin"
#define DEFAULT_ROOM_MUSIC "assets/extracted/aria/sound/runtime/room_music.tsv"
#define DEFAULT_HUD "assets/extracted/aria/metadata/hud.bin"
#define DEFAULT_ITEM_ICONS "assets/extracted/aria/metadata/item_icons.tsv"
#define ITEM_ICONS 193     /* icon numbers 1-192 of sub_0801232C */
#define MAX_PICKUPS 16      /* dropped pickups alive at once (of entities 0x49-0xCF) */
/* The pickup sprites of the object export (coins 0-3, bags 4-6, hearts,
 * souls by type, then the four primary and four follower soul containers);
 * item icons are not exported. */
#define PICKUP_ARTS 21
#define PICKUP_ART_SOUL_CONTAINER 13
#define PICKUP_ART_SOUL_CONTAINER_FOLLOWER 17
static const char *const pickup_art_names[PICKUP_ARTS] = {
    "coin0", "coin1", "coin2", "coin3", "bag4", "bag5", "bag6", "heart_small", "heart_big",
    "soul0", "soul1", "soul2", "soul3",
    "soul_container0", "soul_container1", "soul_container2", "soul_container3",
    "soul_container_follower0", "soul_container_follower1", "soul_container_follower2",
    "soul_container_follower3"};
#define DOOR_STYLES 2
#define ENEMY_KINDS AOS_BESTIARY_NATIVE_RECORDS
static const char *const enemy_names[ENEMY_KINDS] = {[0] = "bat", [1] = "zombie",
                                                     [2] = "skeleton",
                                                     [3] = "merman",
                                                     [4] = "axe_armor", [5] = "skull_archer",
                                                     [6] = "peeping_eye", [7] = "killer_fish",
                                                     [8] = "bone_pillar",
                                                     [9] = "blue_crow",
                                                     [10] = "buer", [11] = "white_dragon",
                                                     [12] = "zombie_soldier",
                                                     [13] = "skeleton_knight",
                                                     [14] = "ghost",
                                                     [15] = "siren", [16] = "tiny_devil",
                                                     [17] = "durga",
                                                     [18] = "rock_armor",
                                                     [19] = "giant_ghost",
                                                     [20] = "winged_skeleton",
                                                     [21] = "minotaur",
                                                     [24] = "fleaman",
                                                     [25] = "evil_butcher",
                                                     [70] = "dryad",
                                                     [28] = "catoblepas",
                                                     [29] = "ghost_dancer",
                                                     [31] = "killer_doll",
                                                     [32] = "zombie_officer",
                                                     [38] = "nemesis",
                                                     [41] = "valkyrie",
                                                     [43] = "curly",
                                                     [45] = "red_crow",
                                                     [47] = "dead_warrior",
                                                     [49] = "imp", [51] = "gorgon",
                                                     [55] = "gremlin",
                                                     [56] = "harpy",
                                                     [57] = "medusa_head",
                                                     [58] = "bomber_armor",
                                                     [59] = "lightning_doll",
                                                     [63] = "needles",
                                                     [73] = "cagnazzo",
                                                     [74] = "ripper",
                                                     [62] = "giant_worm",
                                                     [71] = "poison_worm",
                                                     [78] = "biphron",
                                                     [81] = "sky_fish",
                                                     [85] = "killer_mantle",
                                                     [27] = "ectoplasm",
                                                     [82] = "dead_crusader",
                                                     [87] = "gargoyle",
                                                     [88] = "red_minotaur",
                                                     [89] = "beam_skeleton",
                                                     [91] = "skull_millione",
                                                     [96] = "mimic",
                                                     [98] = "erinys",
                                                     [99] = "lubicant"};
/* Other palette banks of a kind, exported as Enemy/<name>_bank<N>: the gold
 * Medusa head (parameter 1 bit 0) and the Ghost Dancer (parameter 0). */
#define VARIANT_ARTS 5
static const struct { int id, bank; } variant_keys[VARIANT_ARTS] = {
    {57, 1}, {29, 1}, {29, 2}, {25, 1}, {-1, 0},
};

/* A WAV recording of the mixer output (unsigned 8-bit mono). */
typedef struct {
    FILE *file;
    uint32_t bytes;
} AriaWav;

static void wav_header(FILE *file, uint32_t bytes) {
    uint8_t header[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                          16, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 8, 0,
                          'd', 'a', 't', 'a', 0, 0, 0, 0};
    uint32_t riff = 36 + bytes, rate = AOS_MP2K_MIX_RATE;
    for (int i = 0; i < 4; ++i) {
        header[4 + i] = (uint8_t)(riff >> (8 * i));
        header[24 + i] = (uint8_t)(rate >> (8 * i));
        header[28 + i] = (uint8_t)(rate >> (8 * i));
        header[40 + i] = (uint8_t)(bytes >> (8 * i));
    }
    fwrite(header, 1, sizeof header, file);
}

static bool wav_open(AriaWav *wav, const char *path) {
    wav->file = fopen(path, "wb");
    if (!wav->file) return false;
    wav_header(wav->file, 0);
    return true;
}

static void wav_write(AriaWav *wav, const int8_t *pcm, size_t count) {
    if (!wav->file) return;
    for (size_t i = 0; i < count; ++i) fputc((uint8_t)(pcm[i] + 128), wav->file);
    wav->bytes += (uint32_t)count;
}

static void wav_close(AriaWav *wav) {
    if (!wav->file) return;
    fseek(wav->file, 0, SEEK_SET);
    wav_header(wav->file, wav->bytes);
    fclose(wav->file);
    wav->file = NULL;
}

/* metadata room_music.tsv: sub_08013960's flags per (area, room). */
#define MUSIC_AREAS 16
#define MUSIC_ROOMS 256
static AosRoomMusicFlags room_music_flags[MUSIC_AREAS][MUSIC_ROOMS];

static bool load_room_music(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[128];
    unsigned rows = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned area, room, save, warp, transition;
        if (sscanf(line, "%u\t%u\t%u\t%u\t%u", &area, &room, &save, &warp, &transition) != 5 ||
            area >= MUSIC_AREAS || room >= MUSIC_ROOMS)
            continue;
        room_music_flags[area][room] = (AosRoomMusicFlags){save != 0, warp != 0, transition != 0};
        rows++;
    }
    fclose(f);
    return rows > 0;
}

static void start_mp2k_song(void *context, uint16_t song,
                            const AosAudioSongInfo *info) {
    (void)song;
    AosMp2kEngine *engine = context;
    (void)aos_mp2k_player_start(engine, info->player, info->header_pointer);
}

/* The palette bank an enemy is drawn with. */
static int enemy_bank(const AosEnemy *enemy) {
    if (enemy->id == AOS_ENEMY_MEDUSA_HEAD) return enemy->param1 & 1;
    if (enemy->id == AOS_ENEMY_GHOST_DANCER) return enemy->param0 & 0xFF;
    if (enemy->id == AOS_ENEMY_EVIL_BUTCHER) return enemy->param1 & 0xFF;
    return 0;
}
#define ENEMY_STATIC_FRAMES 20 /* single sprite frames of child entities per kind */
/* The red soul skills' sprites (Enemy/skill_<name>). */
enum { SKILL_ART_BAT, SKILL_ART_ARCHER, SKILL_ART_BONE, SKILL_ART_SHARED, SKILL_ART_BLADES,
       SKILL_ART_DEVIL, SKILL_ART_CROW, SKILL_ART_FLEA, SKILL_ART_SKULL, SKILL_ART_VALKYRIE,
       SKILL_ART_NEEDLES, SKILL_ART_RIPPER, SKILL_ART_KILLER_FISH, SKILL_ART_FISH_HEAD,
       SKILL_ART_CAGNAZZO, SKILL_ART_BUER, SKILL_ART_CATOBLEPAS, SKILL_ART_IMP,
       SKILL_ART_LIGHTNING, SKILL_ART_KILLER_DOLL, SKILL_ART_CURLY, SKILL_ART_GIANT_GHOST,
       SKILL_ART_WITCH, SKILL_ART_PILLAR, SKILL_ART_FLYING_ARMOR, SKILL_ART_DEATH,
       SKILL_ART_PERSEPHONE, SKILL_ART_GREAT_ARMOR, SKILL_ART_DEVIL_SPARK,
       SKILL_ART_SHADOW_KNIGHT, SKILL_ART_ALURA_UNE, SKILL_ART_BIG_GOLEM, SKILL_ARTS };
static const char *const skill_art_names[SKILL_ARTS] = {"skill_bat", "skill_archer", "skill_bone",
                                                         "skill_shared", "skill_blades",
                                                         "skill_devil", "skill_crow",
                                                         "skill_flea", "skill_skull",
                                                         "skill_valkyrie", "skill_needles",
                                                         "skill_ripper", "skill_killer_fish",
                                                         "skill_fish_head",
                                                         "skill_cagnazzo", "skill_buer",
                                                         "skill_catoblepas", "skill_imp",
                                                         "skill_lightning",
                                                         "skill_killer_doll", "skill_curly",
                                                         "skill_giant_ghost", "skill_witch",
                                                         "skill_bone_pillar",
                                                         "skill_flying_armor",
                                                         "skill_death", "skill_persephone",
                                                         "skill_great_armor",
                                                         "skill_devil_spark",
                                                         "skill_shadow_knight",
                                                         "skill_alura_une", "skill_big_golem"};
#define ARIA_KIND_ENEMY 1



#define MAX_TRANSITIONS 32
#define MAX_ENTITIES 128
#define MAX_EFFECTS 32      /* shared effects (flames, blood) alive at once */
#define MAX_DAMAGE_POPUPS 32
static const char *const effect_names[AOS_EFFECT_KINDS] = {"flame", "blood", "smoke",
                                                          "dark_smoke", "fire", "spark",
                                                          "dust", "impact", "casing",
                                                          "ember", "burst_1", "burst_12",
                                                          "burst_15", "sparkle",
                                                          "rock_fragment_0",
                                                          "rock_fragment_1",
                                                          "rock_fragment_2",
                                                          "wall_piece_1", "wall_piece_2",
                                                          "spark_7",
                                                          "opening_projectile",
                                                          "hit_element_3",
                                                          "hit_element_5",
                                                          "soul_container_spark0",
                                                          "soul_container_spark1",
                                                          "soul_container_spark2",
                                                          "soul_container_spark3",
                                                          "soul_held",
                                                          "soul_storm0",
                                                          "soul_storm1",
                                                          "soul_storm2",
                                                          "soul_storm3",
                                                          "death_sparkle",
                                                          "level_up"};
#define ARIA_KIND_SPECIAL 2
#define ARIA_KIND_PICKUP 4
#define ARIA_OBJECT_WOODEN_DOOR 0x00
#define ARIA_OBJECT_WALL 0x08           /* Object08: a breakable wall */
#define ARIA_OBJECT_EVENT_WALL 0x09     /* Object09: a wall its event bit breaks */

typedef struct {
    int kind, id, x, y, persistent_index, param0, param1, flags;
    bool spawned;           /* gEwramData + 0x3D0 bit of the record */
    int spawner;            /* entity index of a zombie's spawner, or -1 */
    AosDoor door;
    AosWall wall;           /* objects 0x08 and 0x09 */
    AosEnemy enemy;
    uint8_t reward_flags;   /* the entity's + 0x3E guard of sub_080683BC */
    bool secret_mark;       /* a broken wall's pending secret-room mark */
} AriaEntity;

typedef struct {
    int area, number;
    int width_screens, height_screens, width_cells, height_cells;
    uint8_t room_flags;
    uint8_t *cells;
    uint8_t *blocks;        /* gEwramData + 0xF0C0, cleared on every load */
    AosTransition transitions[MAX_TRANSITIONS];
    size_t transition_count;
    AriaEntity entities[MAX_ENTITIES];
    size_t entity_count;
    AosEffect effects[MAX_EFFECTS];
    size_t effect_count;
    AosDamagePopup damage_popups[MAX_DAMAGE_POPUPS];
    size_t damage_popup_count;
    AosPickup pickups[MAX_PICKUPS];
    size_t pickup_count;
    AosPickupIconSlots pickup_icons;
} AriaRoom;

static void free_room(AriaRoom *room) {
    free(room->cells);
    free(room->blocks);
    room->cells = room->blocks = NULL;
}

static AosCollision room_layer(const AriaRoom *room) {
    return (AosCollision){room->width_screens, room->height_screens, room->width_cells,
                          room->height_cells, room->cells, room->blocks, false};
}

typedef struct {
    SDL_Texture *texture;
    int offset_x, offset_y;
    float w, h;
} AriaFrame;

typedef struct {
    AosAnimDef defs[MAX_ANIMS];
    uint8_t durations[MAX_ANIMS][MAX_FRAMES];
    AriaFrame frames[MAX_ANIMS][MAX_FRAMES];
    AosAnimSet set;
} AriaLibrary;

static bool load_room(const char *path, AriaRoom *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[4096], extra;
    int version, area, number, room_flags = 0, row = 0;
    bool ended = false;
    if (!fgets(line, sizeof line, f)) goto failure;
    int fields = sscanf(line, "AOSROOM-NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &version,
                        &area, &number, &room->width_screens, &room->height_screens,
                        &room->width_cells, &room->height_cells, &room_flags, &extra);
    bool valid_header = (version == 5 || version == 4) && fields == 8 &&
                        room_flags >= 0 && room_flags <= 3;
    if (!valid_header) {
        room_flags = 0;
        fields = sscanf(line, "AOSROOM-NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &version,
                        &area, &number, &room->width_screens, &room->height_screens,
                        &room->width_cells, &room->height_cells, &extra);
        valid_header = version == 3 && fields == 7;
    }
    if (!valid_header ||
        room->width_cells < 1 || room->height_cells < 1 ||
        room->width_cells > 1024 || room->height_cells > 1024)
        goto failure;
    room->area = area;
    room->number = number;
    room->room_flags = (uint8_t)room_flags;
    room->transition_count = 0;
    room->entity_count = 0;
    room->blocks = NULL;
    room->cells = calloc((size_t)room->width_cells * room->height_cells, 1);
    if (!room->cells) goto failure;
    {
        AosCollision probe = room_layer(room);
        room->blocks = calloc(aos_collision_block_bytes(&probe), 1);
        if (!room->blocks) goto failure;
    }
    while (fgets(line, sizeof line, f)) {
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) { ended = true; break; }
        if (line[0] == 'E') {
            AriaEntity *e = &room->entities[room->entity_count];
            int entity_fields = version >= 5
                ? sscanf(line, "E\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c",
                         &e->kind, &e->id, &e->x, &e->y, &e->persistent_index,
                         &e->param0, &e->param1, &e->flags, &extra)
                : sscanf(line, "E\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c",
                         &e->kind, &e->id, &e->x, &e->y,
                         &e->param0, &e->param1, &e->flags, &extra);
            if (room->entity_count >= MAX_ENTITIES || entity_fields != (version >= 5 ? 8 : 7) ||
                e->kind < 0 || e->kind > 255 || e->id < 0 || e->id > 255)
                goto failure;
            e->spawned = false;
            e->secret_mark = false;
            if (e->kind == ARIA_KIND_SPECIAL && e->id == AOS_OBJECT_SAVE_POINT)
                room->room_flags |= AOS_ROOM_FLAG_SAVE;
            room->entity_count++;
            continue;
        }
        if (line[0] == 'T') {
            int sx, sy, adjust, load_x, load_y, target_area, target_room;
            if (room->transition_count >= MAX_TRANSITIONS ||
                sscanf(line, "T\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &sx, &sy, &adjust, &load_x,
                       &load_y, &target_area, &target_room, &extra) != 7 ||
                sx < -128 || sx > 127 || sy < -128 || sy > 127 || adjust < -32768 ||
                adjust > 32767 || load_x < 0 || load_x > 0xFFFF || load_y < 0 ||
                load_y > 0xFFFF || target_area < 0 || target_area > 255 || target_room < 0 ||
                target_room > 255)
                goto failure;
            room->transitions[room->transition_count++] = (AosTransition){
                (int8_t)sx, (int8_t)sy, (int16_t)adjust, (uint16_t)load_x, (uint16_t)load_y,
                (uint8_t)target_area, (uint8_t)target_room};
            continue;
        }
        if (strncmp(line, "R\t", 2) || row >= room->height_cells) goto failure;
        for (int x = 0; x < room->width_cells; ++x) {
            unsigned value;
            if (sscanf(line + 2 + 2 * x, "%2x", &value) != 1) goto failure;
            room->cells[row * room->width_cells + x] = (uint8_t)value;
        }
        ++row;
    }
    if (!ended || row != room->height_cells) goto failure;
    fclose(f);
    return true;
failure:
    fclose(f);
    free_room(room);
    fprintf(stderr, "Invalid Aria runtime room: %s\n", path);
    return false;
}

static bool room_has_opening_controller(const AriaRoom *room) {
    if (!room) return false;
    for (size_t index = 0; index < room->entity_count; ++index) {
        const AriaEntity *entity = &room->entities[index];
        if (entity->kind == ARIA_KIND_SPECIAL &&
            entity->id == AOS_OPENING_OBJECT_ID &&
            entity->param0 == AOS_OPENING_OBJECT_PARAMETER)
            return true;
    }
    return false;
}

static bool room_has_save_point(const AriaRoom *room) {
    return room && (room->room_flags & AOS_ROOM_FLAG_SAVE) != 0;
}

/* Temporary host presentation around Object1C's native fixed room position.
 * The original sprite, healing animation and save question are separate work. */
static void draw_save_point(SDL_Renderer *renderer, const AriaRoom *room, int cam_x, int cam_y,
                            bool player_inside, long step) {
    if (!room_has_save_point(room)) return;
    float x = 120.0f - (float)cam_x;
    float y = 104.0f - (float)cam_y;
    float pulse = (float)((step / 4) % 8);
    if (pulse > 4.0f) pulse = 8.0f - pulse;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, player_inside ? 130 : 80, 255, 255, 210);
    SDL_FRect core = {x - 3.0f, y - 3.0f, 6.0f, 6.0f};
    SDL_RenderFillRect(renderer, &core);
    SDL_SetRenderDrawColor(renderer, 100, 210, 255, 130);
    SDL_FRect glow = {x - 8.0f - pulse, y - 8.0f - pulse,
                      16.0f + pulse * 2.0f, 16.0f + pulse * 2.0f};
    SDL_RenderRect(renderer, &glow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* Reads the sprite index; textures are created only with a renderer. */
static bool load_library(const char *path, AriaLibrary *lib, SDL_Renderer *renderer) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[1024];
    if (!fgets(line, sizeof line, f) || strncmp(line, INDEX_SCHEMA, strlen(INDEX_SCHEMA))) {
        fclose(f);
        fprintf(stderr, "Not a sprite index: %s\n", path);
        return false;
    }
    int max_id = -1;
    while (fgets(line, sizeof line, f)) {
        char frame_path[512];
        int id, frame, ticks, ox, oy;
        if (sscanf(line, "Soma/animation_%d\t%d\t%d\t%d\t%d\t%511s", &id, &frame, &ticks,
                   &ox, &oy, frame_path) != 6)
            continue;   /* knife and other non-body keys */
        if (id < 0 || id >= MAX_ANIMS || frame < 0 || frame >= MAX_FRAMES || ticks < 1 ||
            ticks > 255 || frame != lib->defs[id].count) {
            fclose(f);
            fprintf(stderr, "Invalid Soma library row: %s", line);
            return false;
        }
        lib->durations[id][frame] = (uint8_t)ticks;
        lib->defs[id].count = (uint16_t)(frame + 1);
        lib->defs[id].durations = lib->durations[id];
        AriaFrame *out = &lib->frames[id][frame];
        out->offset_x = ox;
        out->offset_y = oy;
        if (renderer) {
            SDL_Surface *surface = SDL_LoadBMP(frame_path);
            if (!surface) {
                fclose(f);
                fprintf(stderr, "%s: %s\n", frame_path, SDL_GetError());
                return false;
            }
            out->texture = SDL_CreateTextureFromSurface(renderer, surface);
            out->w = (float)surface->w;
            out->h = (float)surface->h;
            SDL_DestroySurface(surface);
            if (!out->texture) { fclose(f); return false; }
            SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
        }
        if (id > max_id) max_id = id;
    }
    fclose(f);
    if (max_id < 0) { fprintf(stderr, "No Soma animation in %s\n", path); return false; }
    lib->set.anims = lib->defs;
    lib->set.count = (uint16_t)(max_id + 1);
    return true;
}

static void room_folder(char *out, size_t size, int area, int number) {
    snprintf(out, size, "%s/area_%02d_room_%03d", DEFAULT_ROOMS, area, number);
}

static bool load_room_folder(const char *folder, AriaRoom *room) {
    char path[600];
    snprintf(path, sizeof path, "%s/room.tsv", folder);
    return load_room(path, room);
}

/* Leaves every room through the middle of each transition's screen edge,
 * checks that the arrival lies inside the target room and that leaving the
 * target again through the opposite edge returns to the source room. */
static int audit_transitions(void) {
    int total = 0, outside = 0, missing = 0, unmatched = 0, rooms = 0, cell_exits = 0,
        cell_outside = 0;
    for (int area = 0; area < 16; ++area) {
        for (int number = 0; number < 256; ++number) {
            char folder[512];
            room_folder(folder, sizeof folder, area, number);
            AriaRoom source = {0};
            FILE *probe = NULL;
            char path[600];
            snprintf(path, sizeof path, "%s/room.tsv", folder);
            if (!(probe = fopen(path, "rb"))) continue;
            fclose(probe);
            if (!load_room_folder(folder, &source)) return 1;
            ++rooms;
            int width = source.width_screens > 1 ? source.width_screens << 8 : 0xF0;
            int height = source.height_screens > 1 ? source.height_screens << 8 : 0x100;
            for (size_t i = 0; i < source.transition_count; ++i) {
                const AosTransition *t = &source.transitions[i];
                ++total;
                /* A point just outside the room on that screen's edge. */
                int32_t x, y;
                if (t->screen_x < 0) x = -2;
                else if (t->screen_x >= source.width_screens) x = width + 2;
                else x = (t->screen_x << 8) + 0x78;
                if (t->screen_y < 0) y = 0x2E;
                else if (t->screen_y >= source.height_screens)
                    y = source.height_screens > 1 ? height - 0x2E : 0xD2;
                else y = (t->screen_y << 8) + 0x80;
                AosCollision layer = room_layer(&source);
                bool inside_x = t->screen_x >= 0 && t->screen_x < source.width_screens;
                bool inside_y = t->screen_y >= 0 && t->screen_y < source.height_screens;
                if (inside_x && inside_y) {
                    /* An interior entry: leave through an exit cell of that screen. */
                    bool found = false;
                    for (int cy = 0; cy < 32 && !found; ++cy)
                        for (int cx = 0; cx < 32 && !found; ++cx) {
                            int px = (t->screen_x << 8) + cx * 8, py = (t->screen_y << 8) + cy * 8;
                            if (aos_collision_cell(&layer, px, py) == AOS_EXIT_CELL) {
                                x = px;
                                y = py;
                                found = true;
                            }
                        }
                }
                if (!aos_room_outside(&layer, source.width_screens, source.height_screens, x, y) ||
                    aos_room_find_exit(source.transitions, source.transition_count,
                                       source.width_screens, source.height_screens, x, y) != t) {
                    ++unmatched;
                    printf("unmatched exit %d/%d entry %zu (%d,%d)\n", area, number, i,
                           t->screen_x, t->screen_y);
                    continue;
                }
                char target_folder[512];
                room_folder(target_folder, sizeof target_folder, t->area, t->room);
                AriaRoom target = {0};
                if (!load_room_folder(target_folder, &target)) { ++missing; continue; }
                int32_t ax, ay;
                aos_room_arrival(t, target.width_screens, target.height_screens, x, y, &ax, &ay);
                bool cell_exit = inside_x && inside_y;
                cell_exits += cell_exit;
                if (aos_room_outside(NULL, target.width_screens, target.height_screens, ax, ay)) {
                    /* Exit cells sit behind doors whose entity is not ported. */
                    if (cell_exit) ++cell_outside;
                    else ++outside;
                    printf("arrival outside %d/%d -> %d/%d at %d,%d\n", area, number, t->area,
                           t->room, ax, ay);
                }
                free_room(&target);
            }
            free_room(&source);
        }
    }
    printf("Aria transitions: %d rooms, %d entries, %d unmatched exits, %d missing "
           "targets, %d arrivals outside; %d exit-cell entries (%d arrive outside from "
           "the first exit cell, door entities not ported)\n", rooms, total, unmatched, missing,
           outside, cell_exits, cell_outside);
    return unmatched || outside ? 1 : 0;
}

/* Camera: BG1 follows Soma at screen (0x78, 0x6F), the settle target of
 * the room transition (gEwramData + 0x13214 / 0x13216); BG1 stays within the
 * room, with y >= 0x30 and one-screen axes pinned like sub_0803FBBC. Scroll
 * speed limits are not modelled. */
static void follow_camera(const AriaRoom *room, int32_t x, int32_t y, int *cam_x, int *cam_y) {
    int cx = x - 0x78, cy = y - 0x6F;
    int max_x = (room->width_screens << 8) - VIEW_W;
    int max_y = (room->height_screens << 8) - 0x30 - VIEW_H;
    if (room->width_screens <= 1 || cx < 0) cx = 0;
    else if (cx > max_x) cx = max_x;
    if (room->height_screens <= 1 || cy < 0x30) cy = 0x30;
    else if (cy > max_y) cy = max_y;
    *cam_x = cx;
    *cam_y = cy;
}

static SDL_Texture *load_background(SDL_Renderer *renderer, const char *folder) {
    char path[600];
    snprintf(path, sizeof path, "%s/background.bmp", folder);
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) { fprintf(stderr, "%s: %s\n", path, SDL_GetError()); return NULL; }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

/* sub_08011A44 .. sub_08010350: leaves the room when Soma is outside it or
 * on an exit cell; returns false when the exit or its target is missing. */
static bool take_exit(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
                      AosSoma *soma, int cam_x, int cam_y) {
    int32_t x = soma->x >> 16, y = soma->y >> 16;
    const AosTransition *exit = aos_room_find_exit(room->transitions, room->transition_count,
                                                   room->width_screens, room->height_screens,
                                                   x, y);
    if (!exit) {
        fprintf(stderr, "No transition for %d/%d at %d,%d\n", room->area, room->number, x, y);
        return false;
    }
    char folder[512];
    room_folder(folder, sizeof folder, exit->area, exit->room);
    AriaRoom target = {0};
    if (!load_room_folder(folder, &target)) return false;
    SDL_Texture *texture = load_background(renderer, folder);
    if (!texture) { free_room(&target); return false; }
    aos_room_exit_velocity(soma, x - cam_x, y - cam_y);
    int32_t ax, ay;
    aos_room_arrival(exit, target.width_screens, target.height_screens, x, y, &ax, &ay);
    soma->x = (int32_t)((uint32_t)ax << 16) | (soma->x & 0xFFFF);
    soma->y = (int32_t)((uint32_t)ay << 16) | (soma->y & 0xFFFF);
    printf("Room %d/%d -> %d/%d, arrival %d,%d\n", room->area, room->number, target.area,
           target.number, ax, ay);
    free_room(room);
    *room = target;
    SDL_DestroyTexture(*background);
    *background = texture;
    return true;
}

/* The player's combat stats: ATK and DEF come from aos_stats_refresh
 * (sub_0804AD9C) unless --atk / --def override them; HP from the file. */
typedef struct {
    int atk, def, hp, max_hp;
} AriaPlayerStats;

/* Everything the enemies need from the frame. */
typedef struct {
    const AosEnemyKind *kinds[ENEMY_KINDS];     /* NULL when not loaded */
    const AosEnemyStats *stats[ENEMY_KINDS];
    const AosEnemyDrops *drops[ENEMY_KINDS];
    const AosWeaponEntity *weapon;
    const AosWeaponFrames *weapon_frames;
    AriaPlayerStats *player;
    const AosAnimSet *effects[AOS_EFFECT_KINDS];   /* Effect/<name>, NULL when not loaded */
    AosStats *progress;                          /* level, experience, bestiary */
    const AosMoneyKind *money;                   /* NULL without metadata/money.tsv */
    const AosEquipmentTables *equipment;         /* NULL without metadata/equipment.tsv */
    const uint8_t *item_icons;                    /* item, weapon, armor -> icon id */
    AosPickupRecent *recent_items;                /* sub_0804AE3C's three caches */
    AosRecentSouls *recent_souls;                 /* sub_08049E64's soul caches */
    AosHud *hud;                                 /* sub_0800EA98 enemy-name notice */
    const AosHudFont *hud_font;
    bool derive_atk, derive_def;                 /* false under --atk / --def */
    /* sub_0800E708 on every soul collection (sub_080441F8 tail): the
     * obtained-soul banner plus sub_0800E540's description when the
     * soul is new (sub_080441F8's GetSoulTotal == 0 branch). */
    void (*soul_notice)(void *ctx, unsigned category, unsigned soul_index,
                        bool player_low, bool fresh);
    void *soul_notice_ctx;
    uint8_t soul_storm;         /* sub_08045350's remaining spawn frames */
    uint8_t soul_storm_kind;    /* the new soul's kind, for the palette */
    AosLevelUp levelup;         /* sub_080455BC's display entity */
} AriaCombat;

/* The homing list gEwramData + 0x13194 (sub_08021924, aged by
 * sub_08021DC4): four room entity indices with their seen and aged counts;
 * an entry not seen again since the last ageing is dropped. */
static struct {
    int index[4];
    uint8_t seen[4], aged[4];
} homing = {{-1, -1, -1, -1}, {0}, {0}};
static const AriaRoom *homing_room;

static void homing_register(int index) {
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

static void homing_clear(void) {
    for (int slot = 0; slot < 4; ++slot) {
        homing.index[slot] = -1;
        homing.seen[slot] = homing.aged[slot] = 0;
    }
}

/* A red soul skill entity (EntityCreateInRange(2, 0x10)): SkillBatUse,
 * SkillSkullArcherUse's bow and its arrow sub_08028BC4 are ported. */
enum { SKILL_BAT, SKILL_BOW, SKILL_ARROW, SKILL_THROW, SKILL_SPOUT, SKILL_DROP, SKILL_AXE,
       SKILL_AXE_TRAIL, SKILL_SPEAR, SKILL_SPEAR_TIP, SKILL_SHOT, SKILL_HARPY, SKILL_DAGGER,
       SKILL_DEVIL, SKILL_DEVIL_FIRE, SKILL_CROW, SKILL_FLEA, SKILL_GHOST,
       SKILL_GHOST_SOUL, SKILL_SIREN, SKILL_SKULL, SKILL_VALKYRIE, SKILL_NEEDLES,
       SKILL_RIPPER, SKILL_MILLIONE_EMITTER, SKILL_MILLIONE_CHILD, SKILL_FISH_HEAD,
       SKILL_KILLER_FISH, SKILL_GLADIATOR, SKILL_LIGHTNING, SKILL_DECOY, SKILL_NEMESIS };

typedef struct {
    bool alive, started;
    int kind;
    int slot;               /* + 0x1A */
    uint8_t soul;           /* + 0x1C: the soul it was cast with */
    int32_t x, y;           /* room pixels, 16.16 */
    int32_t vx;             /* + 0x48 */
    bool facing_left;
    uint8_t state, timer;   /* + 0x0A, + 0x0D */
    bool struck;            /* + 0x20 set by the arrow's hit callback */
    AosAnimState anim;
    AosThrow thrown;        /* SKILL_THROW: the whole entity */
    AosMermanDrop drop;     /* SKILL_DROP: the whole entity */
    AosAxe axe;             /* SKILL_AXE */
    AosAxeTrail trail;      /* SKILL_AXE_TRAIL */
    AosSpear spear;         /* SKILL_SPEAR */
    AosShot shot;           /* SKILL_SHOT */
    AosDagger dagger;       /* SKILL_DAGGER, with its trail entity */
    AosDevilFire fire;      /* SKILL_DEVIL_FIRE */
    AosCrow crow;           /* SKILL_CROW */
    AosFlea flea;           /* SKILL_FLEA */
    AosGhostSoul ghost;     /* SKILL_GHOST_SOUL */
    AosSirenSoul siren;     /* SKILL_SIREN */
    AosSkull skull;         /* SKILL_SKULL */
    AosValkyrie valkyrie;   /* SKILL_VALKYRIE */
    AosNeedles needles;     /* SKILL_NEEDLES */
    AosRipperSoul ripper;   /* SKILL_RIPPER */
    AosMillioneSoul millione; /* SKILL_MILLIONE_EMITTER */
    AosFishHeadSoul fish_head; /* SKILL_FISH_HEAD */
    AosKillerFishSoul killer_fish; /* SKILL_KILLER_FISH */
    AosGladiatorSoul gladiator; /* SKILL_GLADIATOR */
    AosLightning lightning; /* SKILL_LIGHTNING */
    AosDecoy decoy;         /* SKILL_DECOY */
    AosNemesisSoul nemesis; /* SKILL_NEMESIS */
    AosEnemy *ripper_target; /* sub_080239E4's +0x20 */
    int fire_target;        /* its + 0x20: the room entity chased, -1 for none */
    AosDaggerTrail dagger_trail;
    int parent;             /* + 0x14: a child's parent slot */
    uint8_t parameter;      /* + 0x1E: the record's parameter */
} AriaSkill;

#define MAX_SKILLS AOS_RED_SOUL_SLOTS
#define SKILL_BAT_ROUTINE 0x08026C55u
#define SKILL_ARCHER_ROUTINE 0x08028D69u
#define SKILL_THROW_ROUTINE 0x08025785u
#define SKILL_MERMAN_ROUTINE 0x08026F75u
#define SKILL_AXE_ROUTINE 0x08024275u
#define SKILL_SPEAR_ROUTINE 0x08026009u
#define SKILL_DURGA_ROUTINE 0x0802B809u
#define SKILL_HARPY_ROUTINE 0x0802CC85u
#define SKILL_DEVIL_ROUTINE 0x08029FB1u
#define SKILL_CROW_ROUTINE 0x0802C489u
#define SKILL_FLEA_ROUTINE 0x080299A1u
#define SKILL_GHOST_ROUTINE 0x0802A8D5u
#define SKILL_SIREN_ROUTINE 0x08026369u
#define SKILL_SKULL_ROUTINE 0x080289B5u
#define SKILL_VALKYRIE_ROUTINE 0x08024CBDu
#define SKILL_NEEDLES_ROUTINE 0x080280B1u
#define SKILL_RIPPER_ROUTINE 0x08023A71u
#define SKILL_MILLIONE_ROUTINE 0x0802B1B9u
#define SKILL_FISH_HEAD_ROUTINE 0x0802B351u
#define SKILL_KILLER_FISH_ROUTINE 0x08027C59u
#define SKILL_GLADIATOR_ROUTINE 0x0802B4C9u
#define SKILL_LIGHTNING_ROUTINE 0x080277B1u
#define SKILL_KILLER_DOLL_ROUTINE 0x08024F39u
#define SKILL_NEMESIS_ROUTINE 0x080251F1u

/* What a cast needs: the soul state and where its entity goes. */
typedef struct {
    AosStats *stats;
    AosRedSoulCaster caster;
    const AosRedSoulRecord *records;
    AriaSkill skills[MAX_SKILLS];
    const AosSoma *soma;
    const AosAnimSet *bat, *bow, *merman; /* NULL when not exported */
    const AosThrowRecord *throws;   /* metadata/skill_throws.tsv, NULL when missing */
    bool bone;                      /* resource 7 (Enemy/skill_bone) is loaded */
    const AosEnemyKind *bone_kind;  /* its walker probes and still frames */
    const AosEnemyKind *ripper_kind; /* resource 1 and probes 0x084F139C */
    const AosAnimSet *millione;     /* resource 7, sequence 0 */
    const AosAnimSet *fish_head;    /* resource 17, sequence 0 */
    const AosAnimSet *killer_fish;  /* resource 11, sequences 0-1 */
    const AosAnimSet *gladiator;    /* resource 24, sequence/frame 0 */
    const AosAnimSet *lightning;    /* resource 9, sequence 0 */
    const AosAnimSet *killer_doll;  /* resource 6, sequences 0-1 */
    const AosEnemyKind *gladiator_kind; /* its probes at 0x084F13C8 */
    const int *bone_ids;            /* its exported still frames */
    int bone_count;
    const int *ripper_ids;
    int ripper_count;
    AosEffect *effects;             /* the room's shared effects */
    size_t *effect_count;
    const AosEffect *burst;         /* burst list 0 of sub_08045A98 */
    size_t burst_count;
    const AosCollision *layer;      /* the room's BG1 collision */
    bool durga;                     /* resource 24's still frame 2 is loaded */
    bool harpy;                     /* and its frames 1 and 3-5 */
    const AosAnimSet *devil;        /* resource 21's sequence 0, NULL when missing */
    const AosAnimSet *crow;         /* resource 31, NULL when missing */
    const AosEnemyKind *crow_kind;  /* its frame boxes */
    const AosAnimSet *skull;        /* resource 19, NULL when missing */
    const AosEnemyKind *skull_kind; /* its frame boxes */
    const AosAnimSet *valkyrie;     /* resource 2, NULL when missing */
    const AosEnemyKind *valkyrie_kind;
    const AosAnimSet *needles;      /* resource 12 with its four piece frames */
    bool ripper;                    /* both still frames of 0x084F128C */
    const AosAnimSet *flea;         /* resource 20, NULL when missing */
    const AosEnemyKind *flea_kind;  /* its probes and frame boxes */
    const AosBox *flea_still_attack;   /* the attack boxes of its still frames */
    AosBox durga_attack;            /* that frame's attack box */
    const int *shared_ids;          /* resource 0's exported still frames */
    int shared_count;
    const int16_t (*margins)[2];    /* sub_0806D128's table */
    int cam_x, cam_y;
} AriaSouls;

/* The index of still frame `id` among an art's exported ones, or -1. */
static int static_index(const int *ids, int count, int id) {
    for (int i = 0; i < count; ++i)
        if (ids[i] == id) return i;
    return -1;
}

/* sub_08031EA4(entity, source, dx, dy): the source's facing and whole
 * position (only + 0x42 and + 0x46 are written; a fresh entity's fractions
 * are 0), dx mirrored when it faces left. */
static void place(AriaSkill *skill, int32_t x, int32_t y, bool facing_left, int dx, int dy) {
    skill->facing_left = facing_left;
    skill->x = (int32_t)((uint32_t)(int16_t)((x >> 16) + (facing_left ? -dx : dx)) << 16);
    skill->y = (int32_t)((uint32_t)(int16_t)((y >> 16) + dy) << 16);
}

/* Soma's cast hook: sub_08019478's guards and bookkeeping, then the skill
 * entity; souls whose routine is not ported are refused rather than
 * spending MP on nothing. */
static int cast_red_soul(void *context) {
    AriaSouls *souls = context;
    if (!souls->records || souls->stats->red_soul >= AOS_RED_SOUL_RECORDS) return -1;
    const AosRedSoulRecord *record = &souls->records[souls->stats->red_soul];
    uint32_t routine = record->skill;
    /* The shared throw routine is followed when all of the selected record's
     * flags and its resource-specific still frame are available. */
    const AosThrowRecord *throw_record = souls->throws && record->parameter < AOS_THROW_RECORDS
                                             ? &souls->throws[record->parameter]
                                             : NULL;
    bool throw_art = throw_record &&
                     ((throw_record->resource == 0 &&
                       static_index(souls->shared_ids, souls->shared_count,
                                    throw_record->frame) >= 0) ||
                      (throw_record->resource == 1 &&
                       static_index(souls->ripper_ids, souls->ripper_count,
                                    throw_record->frame) >= 0) ||
                      (throw_record->resource == 7 && souls->bone &&
                       static_index(souls->bone_ids, souls->bone_count,
                                    throw_record->frame) >= 0));
    bool throwable = routine == SKILL_THROW_ROUTINE && throw_record &&
                     aos_throw_supported(throw_record) && throw_art;
    int kind = routine == SKILL_BAT_ROUTINE && souls->bat      ? SKILL_BAT
               : routine == SKILL_ARCHER_ROUTINE && souls->bow ? SKILL_BOW
               : throwable                                     ? SKILL_THROW
               : routine == SKILL_MERMAN_ROUTINE && souls->merman ? SKILL_SPOUT
               : routine == SKILL_AXE_ROUTINE &&
                       static_index(souls->shared_ids, souls->shared_count, record->parameter) >= 0
                   ? SKILL_AXE
               : routine == SKILL_SPEAR_ROUTINE &&
                       static_index(souls->shared_ids, souls->shared_count, 0) >= 0
                   ? SKILL_SPEAR
               : routine == SKILL_DURGA_ROUTINE && souls->durga ? SKILL_SHOT
               : routine == SKILL_HARPY_ROUTINE && souls->harpy ? SKILL_HARPY
               : routine == SKILL_DEVIL_ROUTINE && souls->devil ? SKILL_DEVIL
               : routine == SKILL_CROW_ROUTINE && souls->crow   ? SKILL_CROW
               : routine == SKILL_FLEA_ROUTINE && souls->flea   ? SKILL_FLEA
               : routine == SKILL_GHOST_ROUTINE && souls->devil &&
                         souls->devil->count > AOS_GHOST_SEQUENCE
                   ? SKILL_GHOST
               : routine == SKILL_SIREN_ROUTINE &&
                         static_index(souls->shared_ids, souls->shared_count, 2) >= 0 &&
                         static_index(souls->shared_ids, souls->shared_count, 3) >= 0
                   ? SKILL_SIREN
               : routine == SKILL_SKULL_ROUTINE && souls->skull ? SKILL_SKULL
               : routine == SKILL_LIGHTNING_ROUTINE && souls->lightning ? SKILL_LIGHTNING
               : routine == SKILL_KILLER_DOLL_ROUTINE && souls->killer_doll ? SKILL_DECOY
               : routine == SKILL_NEMESIS_ROUTINE && souls->devil &&
                         souls->devil->count > AOS_NEMESIS_SOUL_SEQUENCE
                   ? SKILL_NEMESIS
               : routine == SKILL_VALKYRIE_ROUTINE && souls->valkyrie ? SKILL_VALKYRIE
               : routine == SKILL_NEEDLES_ROUTINE && souls->needles ? SKILL_NEEDLES
               : routine == SKILL_RIPPER_ROUTINE && souls->ripper && record->parameter < 2
                   ? SKILL_RIPPER
               : routine == SKILL_MILLIONE_ROUTINE && souls->millione
                   ? SKILL_MILLIONE_EMITTER
               : routine == SKILL_FISH_HEAD_ROUTINE && souls->fish_head
                   ? SKILL_FISH_HEAD
               : routine == SKILL_KILLER_FISH_ROUTINE && souls->killer_fish
                   ? SKILL_KILLER_FISH
               : routine == SKILL_GLADIATOR_ROUTINE && souls->gladiator &&
                         souls->gladiator_kind && souls->gladiator_kind->probes.count
                   ? SKILL_GLADIATOR
                                                                : -1;
    if (kind < 0) return -1;
    int slot = aos_red_soul_try(&souls->caster, souls->stats, souls->records,
                                (souls->stats->challenge_flags & 0x200000u) != 0);
    if (slot < 0) return -1;
    AriaSkill *skill = &souls->skills[slot];
    *skill = (AriaSkill){.alive = true, .kind = kind, .slot = slot,
                         .soul = souls->stats->red_soul};
    /* SkillBatUse: (0, -0x1A); SkillSkullArcherUse: (-0xC, -0x18). */
    if (kind == SKILL_BAT)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x1A);
    else if (kind == SKILL_NEEDLES) {
        skill->needles = aos_needles_create(souls->soma->x, souls->soma->y,
                                            souls->soma->facing_left);
        skill->x = skill->needles.x;
        skill->y = skill->needles.y;
        skill->facing_left = skill->needles.facing_left;
    } else if (kind == SKILL_RIPPER) {
        skill->parameter = (uint8_t)record->parameter;
        /* Both configuration entries have byte +15 clear and therefore
         * enter the rebound branch after a wall correction. */
        skill->ripper = aos_ripper_soul_create(souls->soma->x, souls->soma->y,
                                               souls->soma->facing_left,
                                               skill->parameter, true);
        skill->x = skill->ripper.x;
        skill->y = skill->ripper.y;
        skill->facing_left = skill->ripper.facing_left;
    } else if (kind == SKILL_MILLIONE_EMITTER) {
        skill->millione = aos_millione_soul_create();
    } else if (kind == SKILL_FISH_HEAD) {
        skill->fish_head = aos_fish_head_soul_create(souls->soma->x, souls->soma->y,
                                                     souls->soma->facing_left);
        skill->x = skill->fish_head.x;
        skill->y = skill->fish_head.y;
        skill->facing_left = skill->fish_head.facing_left;
    } else if (kind == SKILL_KILLER_FISH) {
        skill->killer_fish = aos_killer_fish_soul_create(
            souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->killer_fish.x;
        skill->y = skill->killer_fish.y;
        skill->facing_left = skill->killer_fish.facing_left;
    } else if (kind == SKILL_GLADIATOR) {
        skill->gladiator = aos_gladiator_soul_create(souls->soma->x, souls->soma->y,
                                                     souls->soma->facing_left);
        skill->x = skill->gladiator.body.x;
        skill->y = skill->gladiator.body.y;
        skill->facing_left = skill->gladiator.facing_left;
    } else if (kind == SKILL_VALKYRIE) {
        skill->valkyrie = aos_valkyrie_create(souls->soma->x, souls->soma->y,
                                              souls->soma->facing_left);
        skill->x = skill->valkyrie.x;
        skill->y = skill->valkyrie.y;
        skill->facing_left = skill->valkyrie.facing_left;
    } else if (kind == SKILL_SKULL) {
        skill->skull = aos_skull_create(souls->soma->facing_left);
        skill->facing_left = skill->skull.facing_left;
    } else if (kind == SKILL_DECOY) {
        skill->decoy = aos_decoy_create(souls->soma->x, souls->soma->y,
                                        souls->soma->facing_left);
        skill->x = skill->decoy.x;
        skill->y = skill->decoy.y;
        skill->facing_left = souls->soma->facing_left;
    } else if (kind == SKILL_NEMESIS) {
        skill->nemesis = aos_nemesis_soul_create();
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -8);
    } else if (kind == SKILL_LIGHTNING) {
        skill->lightning = aos_lightning_create(souls->soma->x, souls->soma->y,
                                                souls->soma->facing_left, aos_random() & 1);
        skill->x = skill->lightning.x;
        skill->y = skill->lightning.y;
        skill->facing_left = skill->lightning.facing_left;
    }
    else if (kind == SKILL_GHOST)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x18);
    else if (kind == SKILL_SIREN) {
        /* SkillSirenUse consumes its two random draws on the first update. */
    }
    else if (kind == SKILL_FLEA) {
        skill->flea = aos_flea_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->flea.body.x;
        skill->y = skill->flea.body.y;
        skill->facing_left = skill->flea.body.mirrored;
    } else if (kind == SKILL_CROW) {
        skill->crow = aos_crow_create(souls->soma->facing_left);
        skill->facing_left = skill->crow.facing_left;
    } else if (kind == SKILL_DEVIL)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 8, -0x1A);
    else if (kind == SKILL_HARPY)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0x14, -0x1A);
    else if (kind == SKILL_SHOT) {
        skill->shot = aos_shot_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->shot.x;
        skill->y = skill->shot.y;
        skill->facing_left = skill->shot.facing_left;
    } else if (kind == SKILL_SPEAR) {
        skill->spear = aos_spear_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->spear.x;
        skill->y = skill->spear.y;
        skill->facing_left = skill->spear.facing_left;
    } else if (kind == SKILL_AXE) {
        skill->parameter = (uint8_t)record->parameter;
        skill->axe = aos_axe_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
        skill->x = skill->axe.x;
        skill->y = skill->axe.y;
        skill->facing_left = false;
    } else if (kind == SKILL_SPOUT)
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 6, -0x1A);
    else if (kind == SKILL_THROW) {
        skill->parameter = (uint8_t)record->parameter;
        skill->thrown = aos_throw_create(souls->soma->x, souls->soma->y, souls->soma->facing_left);
    } else
        place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, -0xC, -0x18);
    printf("Red soul %u cast (MP %d)\n", souls->stats->red_soul, souls->stats->mp);
    return souls->records[souls->stats->red_soul].pose;
}

/* sub_08023530: a child skill entity in a free slot, counted (0x131BE). */
static AriaSkill *spawn_child(AriaSouls *souls, int kind) {
    for (int i = 0; i < MAX_SKILLS; ++i) {
        if (souls->caster.slots[i]) continue;
        souls->caster.slots[i] = true;
        souls->caster.active++;
        AriaSkill *child = &souls->skills[i];
        *child = (AriaSkill){.alive = true, .started = false, .kind = kind, .slot = i};
        return child;
    }
    return NULL;
}

static void end_skill(AriaSouls *souls, AriaSkill *skill) {
    aos_red_soul_release(&souls->caster, skill->slot);
    skill->alive = false;
}

/* sub_0806D128(e, n) on the camera. */
static bool skill_outside(const AriaSouls *souls, const AriaSkill *skill, int n) {
    int mx = souls->margins ? souls->margins[n][0] : 16, my = souls->margins ? souls->margins[n][1] : 16;
    int sx = (skill->x >> 16) - souls->cam_x, sy = (skill->y >> 16) - souls->cam_y;
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}

/* The blue soul guardian (gEwramData + 0x13144) and its routine. */
#define GUARDIAN_MEDUSA_ROUTINE 0x08030CC9u
#define GUARDIAN_CAGNAZZO_ROUTINE 0x0802E4EDu
#define GUARDIAN_BUER_ROUTINE 0x0802FF41u
#define GUARDIAN_CATOBLEPAS_ROUTINE 0x08030E65u
#define GUARDIAN_PILLAR_ROUTINE 0x0802E731u
#define GUARDIAN_IMP_ROUTINE 0x080310ADu
#define GUARDIAN_TRANSFORM_ROUTINE 0x0802EFB1u
#define GUARDIAN_GHOST_ROUTINE 0x0802F96Du
#define GUARDIAN_PANTHER_ROUTINE 0x0802E3DDu
#define GUARDIAN_FLYING_ARMOR_ROUTINE 0x0802E23Du
#define GUARDIAN_DEATH_ROUTINE 0x08030519u
#define GUARDIAN_PERSEPHONE_ROUTINE 0x080300A5u
#define GUARDIAN_GREAT_ARMOR_ROUTINE 0x08031949u
#define GUARDIAN_SHADOW_KNIGHT_ROUTINE 0x0802F549u
#define GUARDIAN_ALURA_UNE_ROUTINE 0x0802E961u
#define GUARDIAN_BIG_GOLEM_ROUTINE 0x08031ACDu
enum { GUARDIAN_ATTACKS = 8 };
typedef struct {
    AosGuardian core;
    uint32_t routine;
    AosMedusaGuardian medusa;
    AosCagnazzoGuardian cagnazzo;
    AosBuerGuardian buer;
    AosCatoblepasGuardian catoblepas;
    AosImpGuardian imp;
    AosCurlyGuardian curly;
    AosGhostGuardian ghost;
    AosPantherGuardian panther;
    AosFlyingArmorGuardian flying_armor;
    AosDeathGuardian death;
    AosPersephoneGuardian persephone;
    AosGreatArmorGuardian great_armor;
    AosShadowKnightGuardian shadow_knight;
    AosAluraUneGuardian alura_une;
    bool anim_ended;             /* the guardian's sequence ended (+ 0x6C bit 2) */
    struct {
        bool alive;
        int32_t x, y;            /* + 0x40, + 0x44 */
        bool facing_left;
        uint8_t sequence;        /* r & 3 */
        AosAnimState anim;
    } sparks[7];                 /* sub_0802ECF4 in the guardian slots 1-7 */
    const AosAnimSet *spark_art; /* Enemy/skill_devil_spark, NULL when missing */
    int effect_count;            /* effects of this update for the room */
    AosEffect effects[2];
    int imp_target;              /* the room entity of the chased homing slot */
    AosAnimState anim;           /* Cagnazzo, or both Buer flames (same steps) */
    AosAnimState gas_anims[AOS_CATOBLEPAS_GASES];
    AosStats *stats;
    const AosBlueSoulRecord *records;
    AosSoma *soma;
    const AosAnimSet *cagnazzo_art;  /* Enemy/skill_cagnazzo, NULL when missing */
    const AosAnimSet *buer_art;      /* Enemy/skill_buer, NULL when missing */
    const AosAnimSet *catoblepas_art; /* Enemy/skill_catoblepas, NULL when missing */
    const AosAnimSet *pillar_art;    /* Enemy/skill_bone_pillar, NULL when missing */
    const AosAnimSet *imp_art;       /* Enemy/skill_imp, NULL when missing */
    const AosAnimSet *curly_art;     /* Enemy/skill_curly, NULL when missing */
    const AosAnimSet *ghost_art;     /* Enemy/skill_giant_ghost, NULL when missing */
    const AosAnimSet *witch_art;     /* Enemy/skill_witch, NULL when missing */
    const AosAnimSet *flying_armor_art; /* Enemy/skill_flying_armor, NULL when missing */
    const AosAnimSet *death_art;     /* Enemy/skill_death, NULL when missing */
    const AosAnimSet *persephone_art; /* Enemy/skill_persephone, NULL when missing */
    const AosEnemyKind *persephone_kind; /* its frame attack boxes */
    const AosAnimSet *great_armor_art; /* Enemy/skill_great_armor, NULL when missing */
    const AosAnimSet *shadow_knight_art; /* Enemy/skill_shadow_knight, NULL when missing */
    const AosAnimSet *alura_une_art; /* Enemy/skill_alura_une, NULL when missing */
    const AosAnimSet *big_golem_art; /* Enemy/skill_big_golem, NULL when missing */
    const AosEnemyKind *big_golem_kind; /* its frame attack boxes */
    const AosEnemyKind *shadow_knight_kind; /* its frame attack boxes */
    const int16_t (*margins)[2];     /* the screen margins of sub_0806D128 */
    const AosEnemyKind *imp_kind;    /* its frame attack boxes */
} AriaGuardian;

/* Soma's guardian hook: sub_0802E0C4 for the souls whose routine is
 * ported; the others are refused rather than draining MP for nothing. */
static void start_guardian(void *context) {
    AriaGuardian *guardian = context;
    const AosStats *stats = guardian->stats;
    if (stats->blue_soul >= AOS_BLUE_SOUL_RECORDS) return;
    uint32_t routine = guardian->records[stats->blue_soul].routine;
    if (routine != GUARDIAN_MEDUSA_ROUTINE && routine != GUARDIAN_PANTHER_ROUTINE &&
        !(routine == GUARDIAN_CAGNAZZO_ROUTINE && guardian->cagnazzo_art) &&
        !(routine == GUARDIAN_FLYING_ARMOR_ROUTINE && guardian->flying_armor_art) &&
        !(routine == GUARDIAN_DEATH_ROUTINE && guardian->death_art) &&
        !(routine == GUARDIAN_PERSEPHONE_ROUTINE && guardian->persephone_art) &&
        !(routine == GUARDIAN_GREAT_ARMOR_ROUTINE && guardian->great_armor_art) &&
        !(routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE && guardian->shadow_knight_art) &&
        !(routine == GUARDIAN_ALURA_UNE_ROUTINE && guardian->alura_une_art) &&
        !(routine == GUARDIAN_BIG_GOLEM_ROUTINE && guardian->big_golem_art) &&
        !(routine == GUARDIAN_BUER_ROUTINE && guardian->buer_art) &&
        !(routine == GUARDIAN_CATOBLEPAS_ROUTINE && guardian->catoblepas_art) &&
        !(routine == GUARDIAN_PILLAR_ROUTINE && guardian->pillar_art) &&
        !(routine == GUARDIAN_IMP_ROUTINE && guardian->imp_art) &&
        !(routine == GUARDIAN_GHOST_ROUTINE &&
          (guardian->records[stats->blue_soul].argument ? guardian->ghost_art
                                                        : guardian->witch_art)) &&
        !(routine == GUARDIAN_TRANSFORM_ROUTINE &&
          (guardian->records[stats->blue_soul].argument == AOS_CURLY_VARIANT
               ? guardian->curly_art != NULL
               : guardian->records[stats->blue_soul].argument < AOS_TRANSFORM_VARIANTS &&
                     guardian->spark_art != NULL)))
        return;
    /* sub_0802E0C4: NOSOUL (unk_33C[2] & 0x200000) limits the guardian to
     * blue souls 1-3 (+0x1326A <= 3). */
    if (aos_guardian_try(&guardian->core, stats, guardian->records, guardian->soma->held,
                         guardian->soma->pressed,
                         (stats->challenge_flags & 0x200000u) && stats->blue_soul > 3,
                         &guardian->soma->flags)) {
        guardian->routine = routine;
        guardian->medusa = (AosMedusaGuardian){0};
        guardian->cagnazzo = (AosCagnazzoGuardian){0};
        guardian->buer = (AosBuerGuardian){0};
        guardian->catoblepas = (AosCatoblepasGuardian){0};
        guardian->imp = (AosImpGuardian){.target = -1};
        guardian->curly = (AosCurlyGuardian){0};
        for (int k = 0; k < 7; ++k) guardian->sparks[k].alive = false;
        guardian->ghost = (AosGhostGuardian){0};
        guardian->panther = (AosPantherGuardian){0};
        guardian->flying_armor = (AosFlyingArmorGuardian){0};
        guardian->death = (AosDeathGuardian){0};
        guardian->persephone = (AosPersephoneGuardian){0};
        guardian->great_armor = (AosGreatArmorGuardian){0};
        guardian->shadow_knight = (AosShadowKnightGuardian){0};
        guardian->alura_une = (AosAluraUneGuardian){0};
        guardian->anim_ended = false;
        guardian->imp_target = -1;
    }
}

/* sub_0802DFF0: the transformed body met a ceiling; the guardian, its
 * child and 0x13260 bits 8-13 go, and the held sound 0x9C is released. */
static void cancel_transform(void *context) {
    AriaGuardian *guardian = context;
    if (!guardian || !guardian->core.active || guardian->routine != GUARDIAN_TRANSFORM_ROUTINE)
        return;
    aos_audio_release(0x9C);
    aos_curly_guardian_end(&guardian->curly, &guardian->core, &guardian->soma->flags,
                           &guardian->stats->flags);
}

/* The camera, for the ghost's spirits' screen margin. */
static int guardian_cam_x, guardian_cam_y;

/* The guardian entity's update, after Soma's in the entity order. */
static void update_guardian(AriaGuardian *guardian) {
    if (!guardian->core.active) return;
    AosSoma *soma = guardian->soma;
    if (guardian->routine == GUARDIAN_MEDUSA_ROUTINE) {
        aos_medusa_guardian_update(&guardian->medusa, &guardian->core, guardian->stats,
                                   guardian->records, soma->held, soma->pressed, &soma->flags,
                                   &soma->vy, &soma->gravity_mod, &soma->hold_still);
    } else if (guardian->routine == GUARDIAN_FLYING_ARMOR_ROUTINE) {
        /* Its looping sequence 0 starts on the first update and steps. */
        bool first = guardian->core.state == 0;
        if (!aos_flying_armor_guardian_update(&guardian->flying_armor, &guardian->core,
                                              guardian->stats, guardian->records, soma->held,
                                              soma->pressed, &soma->flags, soma->x >> 16,
                                              soma->y >> 16, soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->flying_armor_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->flying_armor_art);
    } else if (guardian->routine == GUARDIAN_ALURA_UNE_ROUTINE) {
        bool first = guardian->core.state == 0;
        if (!aos_alura_une_guardian_update(&guardian->alura_une, &guardian->core,
                                           guardian->stats, guardian->records, soma->held,
                                           soma->pressed, &soma->flags, soma->x >> 16,
                                           soma->y >> 16, soma->facing_left, &soma->hp,
                                           soma->max_hp, aos_random))
            return;
        /* Its looping sequence 0, started on creation (no step: it is drawn
         * through its affine slot only). */
        if (first) aos_anim_start(&guardian->anim, guardian->alura_une_art, 0, true);
    } else if (guardian->routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE ||
               guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE) {
        bool golem = guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE;
        const AosAnimSet *art = golem ? guardian->big_golem_art : guardian->shadow_knight_art;
        if (golem ? !aos_big_golem_guardian_update(&guardian->shadow_knight, &guardian->core,
                                                   guardian->stats, guardian->records,
                                                   soma->held, soma->pressed, &soma->flags,
                                                   soma->x >> 16, soma->y >> 16,
                                                   soma->facing_left, soma->weapon_active,
                                                   guardian->anim_ended, guardian->anim.frame,
                                                   guardian->anim.tick)
                  : !aos_shadow_knight_guardian_update(&guardian->shadow_knight, &guardian->core,
                                                       guardian->stats, guardian->records,
                                                       soma->held, soma->pressed, &soma->flags,
                                                       soma->x >> 16, soma->y >> 16,
                                                       soma->facing_left, soma->weapon_active,
                                                       guardian->anim_ended))
            return;
        int seq = guardian->shadow_knight.sequence;
        /* The idle sequence loops (1 for the knight, 0 for the golem). */
        if (seq >= 0) aos_anim_start(&guardian->anim, art, (unsigned)seq, seq == (golem ? 0 : 1));
        /* sub_0802E1D0: the step (and the frame's attack box). */
        guardian->anim_ended = aos_anim_step(&guardian->anim, art) == 3;
    } else if (guardian->routine == GUARDIAN_GREAT_ARMOR_ROUTINE) {
        /* Its looping sequence 1 starts on the first update and steps. */
        bool first = guardian->core.state == 0;
        if (!aos_great_armor_guardian_update(&guardian->great_armor, &guardian->core,
                                             guardian->stats, guardian->records, soma->held,
                                             soma->pressed, &soma->flags, soma->x >> 16,
                                             soma->y >> 16, soma->facing_left, aos_random))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->great_armor_art, 1, true);
        aos_anim_step(&guardian->anim, guardian->great_armor_art);
        if (guardian->great_armor.spark && guardian->effect_count < 2)
            guardian->effects[guardian->effect_count++] = (AosEffect){
                .kind = AOS_EFFECT_SPARK_ROW7, .drift = true, .x = guardian->great_armor.spark_x,
                .y = guardian->great_armor.spark_y};
    } else if (guardian->routine == GUARDIAN_PERSEPHONE_ROUTINE) {
        bool first = guardian->core.state == 0;
        if (!aos_persephone_guardian_update(&guardian->persephone, &guardian->core,
                                            guardian->stats, guardian->records, soma->held,
                                            soma->pressed, &soma->flags, soma->x >> 16,
                                            soma->y >> 16, soma->facing_left))
            return;
        /* sub_0802E1D0 (not while leaving): the looping sequence 0. */
        if (first) aos_anim_start(&guardian->anim, guardian->persephone_art, 0, true);
        if (guardian->core.state != AOS_PERSEPHONE_LEAVING)
            aos_anim_step(&guardian->anim, guardian->persephone_art);
    } else if (guardian->routine == GUARDIAN_DEATH_ROUTINE) {
        AosSkillTarget targets[AOS_DEATH_TARGETS] = {{0}};
        for (int slot = 0; slot < AOS_DEATH_TARGETS; ++slot) {
            int index = homing.index[slot];
            if (index < 0 || !homing_room || (size_t)index >= homing_room->entity_count) continue;
            const AriaEntity *entity = &homing_room->entities[index];
            targets[slot] = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                             entity->enemy.x, entity->enemy.y, entity->enemy.hp};
        }
        int mx = guardian->margins ? guardian->margins[1][0] : 16;
        int my = guardian->margins ? guardian->margins[1][1] : 16;
        aos_death_guardian_update(&guardian->death, &guardian->core, guardian->stats,
                                  guardian->records, soma->held, soma->pressed, &soma->flags,
                                  soma->x >> 16, soma->y >> 16, soma->facing_left, targets,
                                  guardian->death_art, guardian_cam_x, guardian_cam_y, mx, my,
                                  aos_random);
    } else if (guardian->routine == GUARDIAN_PANTHER_ROUTINE) {
        aos_panther_guardian_update(&guardian->panther, &guardian->core, guardian->stats,
                                    guardian->records, soma->held, soma->pressed, &soma->flags,
                                    soma->x >> 16, soma->y >> 16, soma->facing_left,
                                    soma->anim.id);
    } else if (guardian->routine == GUARDIAN_CAGNAZZO_ROUTINE) {
        /* Its looping sequence 0 starts on the first update and steps at
         * its end (sub_0803F17C); sub_08042A54(2, 0x14) is not run. */
        bool first = guardian->core.state == 0;
        if (!aos_cagnazzo_guardian_update(&guardian->cagnazzo, &guardian->core, guardian->stats,
                                          guardian->records, soma->held, soma->pressed,
                                          &soma->flags, soma->x >> 16, soma->y >> 16,
                                          soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->cagnazzo_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->cagnazzo_art);
    } else if (guardian->routine == GUARDIAN_BUER_ROUTINE) {
        /* The flames start looping sequence 0 together and step at the end
         * of each update (sub_08042A54(2, 0x18) is not run). */
        bool first = guardian->core.state == 0;
        if (!aos_buer_guardian_update(&guardian->buer, &guardian->core, guardian->stats,
                                      guardian->records, soma->held, soma->pressed,
                                      &soma->flags, soma->x >> 16, soma->y >> 16,
                                      soma->facing_left))
            return;
        if (first) aos_anim_start(&guardian->anim, guardian->buer_art, 0, true);
        aos_anim_step(&guardian->anim, guardian->buer_art);
    } else if (guardian->routine == GUARDIAN_CATOBLEPAS_ROUTINE ||
               guardian->routine == GUARDIAN_PILLAR_ROUTINE) {
        bool pose = false;
        bool pillar = guardian->routine == GUARDIAN_PILLAR_ROUTINE;
        if (pillar ? !aos_pillar_guardian_update(&guardian->catoblepas, &guardian->core,
                                                 guardian->stats, guardian->records,
                                                 soma->held, soma->pressed, &soma->flags,
                                                 soma->x >> 16, soma->y >> 16,
                                                 soma->facing_left, soma->vx,
                                                 &soma->hold_still, &pose, aos_random)
                   : !aos_catoblepas_guardian_update(&guardian->catoblepas, &guardian->core,
                                                     guardian->stats, guardian->records,
                                                     soma->held, soma->pressed, &soma->flags,
                                                     soma->x >> 16, soma->y >> 16,
                                                     soma->facing_left, soma->vx,
                                                     &soma->hold_still, &pose))
            return;
        if (pose) aos_soma_guardian_pose(soma, AOS_CATOBLEPAS_POSE);
        const AosAnimSet *gas_art = pillar ? guardian->pillar_art : guardian->catoblepas_art;
        /* Each cloud plays sequence 0 once from its first update. */
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
            AosCatoblepasGas *gas = &guardian->catoblepas.gases[k];
            if (!gas->alive) continue;
            if (!gas->started) {
                gas->started = true;
                aos_anim_start(&guardian->gas_anims[k], gas_art, 0, false);
            }
            aos_anim_step(&guardian->gas_anims[k], gas_art);
        }
    } else if (guardian->routine == GUARDIAN_TRANSFORM_ROUTINE) {
        bool had_child = guardian->curly.child;
        AosTransformTarget target = {&soma->flags, &guardian->stats->flags, &soma->state,
                                     &soma->transform_step, &soma->transform_variant,
                                     &soma->friction, &soma->gravity_mod};
        bool alive = aos_curly_guardian_update(&guardian->curly, &guardian->core, guardian->stats,
                                               guardian->records, soma->held, soma->pressed,
                                               soma->x, soma->y, soma->facing_left, target);
        /* sub_0802ECF4: a translucent spark at Soma with his facing, its
         * sequence r & 3 played once, deleted at its end; the guardian's
         * end deletes them (sub_0802DD84). */
        for (int k = 0; k < 7; ++k)
            if (guardian->sparks[k].alive &&
                (!alive || aos_anim_step(&guardian->sparks[k].anim, guardian->spark_art) == 3))
                guardian->sparks[k].alive = false;
        if (alive && guardian->curly.spawn_spark && guardian->spark_art)
            for (int k = 0; k < 7; ++k) {
                if (guardian->sparks[k].alive) continue;
                uint8_t seq = (uint8_t)(aos_random() & 3);
                guardian->sparks[k].alive = true;
                guardian->sparks[k].x = soma->x;
                guardian->sparks[k].y = soma->y;
                guardian->sparks[k].facing_left = soma->facing_left;
                guardian->sparks[k].sequence = seq;
                aos_anim_start(&guardian->sparks[k].anim, guardian->spark_art, seq, false);
                break;
            }
        if (!alive) return;
        if (aos_transform_variants[guardian->curly.variant].sparks) return;
        /* sub_0802EE40: the shell's looping sequence 0. */
        if (guardian->curly.child && !had_child)
            aos_anim_start(&guardian->anim, guardian->curly_art, 0, true);
        if (guardian->curly.child) aos_anim_step(&guardian->anim, guardian->curly_art);
    } else if (guardian->routine == GUARDIAN_GHOST_ROUTINE) {
        /* Its looping sequence (0 the Witch, 1 the Ghost), stepped at the
         * end of each update (sub_0803F17C); sub_08042A54(2, 0xA) is not
         * run. */
        bool first = guardian->core.state == 0;
        if (!aos_ghost_guardian_update(&guardian->ghost, &guardian->core, guardian->stats,
                                       guardian->records, soma->held, soma->pressed,
                                       &soma->flags, soma->x, soma->y, soma->facing_left,
                                       guardian_cam_x, guardian_cam_y))
            return;
        const AosAnimSet *art = guardian->ghost.giant ? guardian->ghost_art : guardian->witch_art;
        if (first) aos_anim_start(&guardian->anim, art, guardian->ghost.giant ? 1 : 0, true);
        aos_anim_step(&guardian->anim, art);
    } else if (guardian->routine == GUARDIAN_IMP_ROUTINE) {
        /* The homing list's slots; a chased slot keeps its entity. */
        AosSkillTarget targets[AOS_IMP_TARGETS] = {{0}};
        for (int slot = 0; slot < AOS_IMP_TARGETS; ++slot) {
            int index = homing.index[slot];
            if (guardian->core.state == AOS_IMP_DIVING && guardian->imp.target == slot &&
                guardian->imp_target >= 0)
                index = guardian->imp_target;
            if (index < 0 || !homing_room || (size_t)index >= homing_room->entity_count)
                continue;
            const AriaEntity *entity = &homing_room->entities[index];
            targets[slot] = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                             entity->enemy.x, entity->enemy.y, entity->enemy.hp};
        }
        bool first = guardian->core.state == 0;
        int chased = guardian->imp.target;
        if (!aos_imp_guardian_update(&guardian->imp, &guardian->core, guardian->stats,
                                     guardian->records, soma->held, soma->pressed, &soma->flags,
                                     soma->x, soma->y, soma->facing_left, targets, aos_random))
            return;
        if (guardian->imp.target != chased || guardian->imp_target < 0)
            guardian->imp_target = guardian->imp.target >= 0
                                       ? homing.index[guardian->imp.target]
                                       : -1;
        /* sub_0802E1D0 (not in state 6): its looping sequence 0 and the
         * frame's attack box (sub_08042A54(2, 0x3C) is not run). */
        if (first) aos_anim_start(&guardian->anim, guardian->imp_art, 0, true);
        if (guardian->core.state != AOS_IMP_LEAVING)
            aos_anim_step(&guardian->anim, guardian->imp_art);
    }
}

/* The guardian's attacks this frame (type 6, sub_08021530(6) with the
 * 0x131C2 power and the 0x131C4 elements). Returns their count. */
static int guardian_attacks(const AriaGuardian *guardian,
                            AosSoulAttack attacks[GUARDIAN_ATTACKS]) {
    if (!guardian->core.active || guardian->core.state == 0) return 0;
    if (guardian->routine == GUARDIAN_GHOST_ROUTINE) {
        /* Its body (type 7: power 0, its touches throwing the spirits),
         * then each spirit (type 6, box 0x0808FCFC: -4, -4, 8 x 8). */
        const AosGhostGuardian *ghost = &guardian->ghost;
        int count = 0;
        if (ghost->attacking) {
            int8_t b[4];
            aos_ghost_guardian_box(ghost, b);
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){b[0], b[1], (uint8_t)b[2], (uint8_t)b[3]},
                                ghost->x >> 16, ghost->y >> 16, ghost->facing_left, false),
                0, 0, false, NULL, AOS_GHOST_ATTACK_TYPE};
        }
        for (int k = 0; k < AOS_GHOST_SHOTS; ++k)
            if (ghost->shots[k].alive && ghost->shots[k].state == 1)
                attacks[count++] = (AosSoulAttack){
                    aos_entity_rect((AosBox){-4, -4, 8, 8}, ghost->shots[k].x >> 16,
                                    ghost->shots[k].y >> 16, false, false),
                    aos_guardian_power(guardian->stats, AOS_GHOST_POWER, false),
                    AOS_GHOST_ELEMENTS, false, NULL, AOS_TYPE_GUARDIAN};
        return count;
    }
    if (guardian->routine == GUARDIAN_CAGNAZZO_ROUTINE) {
        const AosCagnazzoGuardian *cagnazzo = &guardian->cagnazzo;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){10, 5, 20, 20}, cagnazzo->x, cagnazzo->y, /* 0x1414050A */
                            cagnazzo->facing_left, false),
            aos_guardian_power(guardian->stats, AOS_CAGNAZZO_POWER, false), 0, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_SHADOW_KNIGHT_ROUTINE ||
        guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box in the current sequence. */
        bool golem = guardian->routine == GUARDIAN_BIG_GOLEM_ROUTINE;
        const AosEnemyKind *kind = golem ? guardian->big_golem_kind : guardian->shadow_knight_kind;
        if (!kind || guardian->anim.id >= AOS_ENEMY_MAX_ANIMS ||
            guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES)
            return 0;
        AosBox box = kind->attack[guardian->anim.id][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->shadow_knight.x, guardian->shadow_knight.y,
                            guardian->shadow_knight.mirrored, false),
            aos_guardian_power(guardian->stats,
                               golem ? AOS_BIG_GOLEM_POWER : AOS_SHADOW_KNIGHT_POWER, false),
            golem ? AOS_BIG_GOLEM_ELEMENTS : AOS_SHADOW_KNIGHT_ELEMENTS, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_PERSEPHONE_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box, none without one or while
         * leaving. */
        if (guardian->core.state == AOS_PERSEPHONE_LEAVING || !guardian->persephone_kind ||
            guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES)
            return 0;
        AosBox box = guardian->persephone_kind->attack[0][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->persephone.x, guardian->persephone.y,
                            guardian->persephone.facing_left, false),
            aos_guardian_power(guardian->stats, AOS_PERSEPHONE_POWER, false),
            AOS_PERSEPHONE_ELEMENTS, false, NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_DEATH_ROUTINE) {
        int count = 0;
        for (int k = 0; k < AOS_DEATH_SICKLES; ++k) {
            const AosDeathSickle *s = &guardian->death.sickles[k];
            if (!s->alive || !s->attacking || s->state > 3) continue;
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){-8, -8, 16, 16}, s->x >> 16, s->y >> 16,  /* 0x1010F8F8 */
                                s->mirrored, false),
                aos_guardian_power(guardian->stats, AOS_DEATH_POWER, false), AOS_DEATH_ELEMENTS,
                false, NULL, AOS_TYPE_GUARDIAN};
        }
        return count;
    }
    if (guardian->routine == GUARDIAN_PANTHER_ROUTINE) {
        if (!guardian->panther.attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-12, -40, 48, 48},                     /* 0x3030D8F4 */
                            guardian->panther.x, guardian->panther.y,
                            guardian->panther.facing_left, false),
            aos_guardian_power(guardian->stats, AOS_PANTHER_POWER, false), AOS_PANTHER_ELEMENTS,
            false, NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_BUER_ROUTINE) {
        for (int k = 0; k < AOS_BUER_FLAMES; ++k)
            attacks[k] = (AosSoulAttack){
                aos_entity_rect((AosBox){-10, -10, 20, 20},                  /* 0x1414F6F6 */
                                guardian->buer.flames[k].x >> 16,
                                guardian->buer.flames[k].y >> 16, false, false),
                aos_guardian_power(guardian->stats, AOS_BUER_POWER, false), AOS_BUER_ELEMENTS,
                false, NULL, AOS_TYPE_GUARDIAN};
        return AOS_BUER_FLAMES;
    }
    if (guardian->routine == GUARDIAN_TRANSFORM_ROUTINE) {
        if (!guardian->curly.attacking) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect((AosBox){-24, -64, 48, 68}, guardian->curly.x >> 16,
                            guardian->curly.y >> 16, guardian->curly.facing_left,
                            false),                                          /* 0x4430C0E8 */
            aos_guardian_power(guardian->stats,
                               aos_transform_variants[guardian->curly.variant].power, false),
            aos_transform_variants[guardian->curly.variant].elements, false, NULL,
            AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_IMP_ROUTINE) {
        /* sub_0802E1D0: the frame's attack box, none without one. */
        if (!guardian->imp_kind || guardian->anim.frame >= AOS_ENEMY_MAX_FRAMES) return 0;
        AosBox box = guardian->imp_kind->attack[0][guardian->anim.frame];
        if (box.w <= 1 || box.h <= 1) return 0;
        attacks[0] = (AosSoulAttack){
            aos_entity_rect(box, guardian->imp.x >> 16, guardian->imp.y >> 16,
                            guardian->imp.mirrored, false),
            aos_guardian_power(guardian->stats, AOS_IMP_POWER, false), AOS_IMP_ELEMENTS, false,
            NULL, AOS_TYPE_GUARDIAN};
        return 1;
    }
    if (guardian->routine == GUARDIAN_CATOBLEPAS_ROUTINE ||
        guardian->routine == GUARDIAN_PILLAR_ROUTINE) {
        bool pillar = guardian->routine == GUARDIAN_PILLAR_ROUTINE;
        int count = 0;
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
            const AosCatoblepasGas *gas = &guardian->catoblepas.gases[k];
            if (!gas->alive) continue;
            attacks[count++] = (AosSoulAttack){
                aos_entity_rect((AosBox){-10, -10, 20, 20}, gas->x >> 16, gas->y >> 16,
                                gas->facing_left, false),                    /* 0x1414F6F6 */
                aos_guardian_power(guardian->stats,
                                   pillar ? AOS_PILLAR_SOUL_POWER : AOS_CATOBLEPAS_POWER, false),
                pillar ? AOS_PILLAR_SOUL_ELEMENTS : AOS_CATOBLEPAS_ELEMENTS, false, NULL,
                AOS_TYPE_GUARDIAN};
        }
        return count;
    }
    return 0;
}

static void update_skills(AriaSouls *souls) {
    for (int i = 0; i < MAX_SKILLS; ++i) {
        AriaSkill *skill = &souls->skills[i];
        if (!skill->alive) continue;
        switch (skill->kind) {
        case SKILL_BAT:
            /* SkillBatUse: on its first update it counts itself and starts
             * sequence 0 (sound 0xE2 and sub_08042A54(1, 0xF) are not
             * played); it stays where it was created and ends with it. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->bat, 0, false);
            }
            if (aos_anim_step(&skill->anim, souls->bat) == 3) end_skill(souls, skill);
            break;
        case SKILL_BOW:
            /* SkillSkullArcherUse: counted on its first update, sequence 0;
             * on frame 14 it shoots the arrow from its own position; state
             * 2 (animation over or off screen) deletes it on the next
             * update. */
            if (skill->state == 2) {
                end_skill(souls, skill);
                break;
            }
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->bow, 0, false);
                skill->state = 1;
            }
            if (skill->anim.frame == 14) {
                AriaSkill *arrow = spawn_child(souls, SKILL_ARROW);
                if (arrow) {
                    arrow->soul = skill->soul;
                    place(arrow, skill->x, skill->y, skill->facing_left, 0, 0);
                }
            }
            if (aos_anim_step(&skill->anim, souls->bow) == 3) skill->state = 2;
            if (skill->state != 2 && skill_outside(souls, skill, 4)) skill->state = 2;
            break;
        case SKILL_THROW: {
            /* The shared throw routine: counted on its first update
             * (sub_08042A54(1, 0x1E) is not run). */
            const AosThrowRecord *record = &souls->throws[skill->parameter];
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            AosThrow *thrown = &skill->thrown;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            const AosEnemyKind *throw_kind = record->resource == 1 ? souls->ripper_kind
                                                                   : souls->bone_kind;
            if (!aos_throw_update_world(thrown, record, souls->layer,
                                        throw_kind ? &throw_kind->probes : NULL,
                                        souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045A98(x, y, 0): burst list 0 at its whole position. */
            for (size_t k = 0; thrown->exploded && souls->effects && k < souls->burst_count &&
                               *souls->effect_count < MAX_EFFECTS;
                 ++k) {
                AosEffect piece = souls->burst[k];
                piece.x = (int32_t)((uint32_t)(int16_t)((thrown->x >> 16) + (piece.x >> 16)) << 16);
                piece.y = (int32_t)((uint32_t)(int16_t)((thrown->y >> 16) + (piece.y >> 16)) << 16);
                souls->effects[(*souls->effect_count)++] = piece;
            }
            /* sub_08025540: four randomly selected static fragments. */
            for (int k = 0; thrown->landing_burst && souls->effects && k < 4 &&
                            *souls->effect_count < MAX_EFFECTS;
                 ++k) {
                AosEffect piece = {
                    .kind = (uint8_t)(AOS_EFFECT_ROCK_FRAGMENT_0 + (aos_random() & 1)),
                    .ballistic = true,
                    .x = thrown->x + (int32_t)((aos_random() & 7) - 4) * 0x10000,
                    .y = thrown->y + (int32_t)((aos_random() & 7) - 4) * 0x10000,
                    .vx = (int32_t)(aos_random() & 0xFF) << 8,
                    .ay = 0x2000};
                if (aos_random() & 1) piece.vx = -piece.vx;
                piece.vy = -0x10000 - ((int32_t)(aos_random() & 0xFF) << 8);
                souls->effects[(*souls->effect_count)++] = piece;
            }
            /* Flag 0x400's last eight even frames call the standard
             * animation-4, row-6 drifting dust helper. */
            if (thrown->timeout_dust && souls->effects &&
                *souls->effect_count < MAX_EFFECTS) {
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_DUST,
                    .drift = true,
                    .x = thrown->x + (int32_t)(aos_random() & 0x7FFFF) - 0x40000,
                    .y = thrown->y + (int32_t)(aos_random() & 0x7FFFF) - 0x40000,
                    .ay = (int32_t)(aos_random() & 0x3FF) - 0x1A00};
            }
            skill->x = thrown->x;
            skill->y = thrown->y;
            skill->facing_left = thrown->facing_left;
            break;
        }
        case SKILL_SPOUT:
            /* SkillMermanUse: on its first update it starts sequence 0
             * (sound 0xED and sub_08042A54(1, 0x1E) are not played), counts
             * itself and drops five sub_08026DB8 children from (6, -0x1A);
             * every update it then stays at Soma's (0, -0x1A) and it ends
             * with its animation. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->merman, AOS_MERMAN_SPOUT_SEQUENCE, false);
                souls->caster.active++;
                for (int k = 0; k < AOS_MERMAN_DROPS; ++k) {
                    AriaSkill *drop = spawn_child(souls, SKILL_DROP);
                    if (!drop) continue;
                    drop->soul = skill->soul;
                    drop->drop = aos_merman_drop_create(k, skill->x, skill->y, skill->facing_left);
                    drop->x = skill->x;
                    drop->y = skill->y;
                    drop->facing_left = skill->facing_left;
                }
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0, -0x1A);
            if (aos_anim_step(&skill->anim, souls->merman) == 3) end_skill(souls, skill);
            break;
        case SKILL_DROP: {
            /* sub_08026DB8: its splash (sub_0802DAD4) is not created. */
            if (!skill->started) {
                skill->started = true;
                aos_anim_start(&skill->anim, souls->merman, AOS_MERMAN_DROP_SEQUENCE, true);
            }
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool splash;
            if (!aos_merman_drop_update(&skill->drop, souls->cam_x, souls->cam_y, mx, my, &splash)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->drop.x;
            skill->y = skill->drop.y;
            aos_anim_step(&skill->anim, souls->merman);
            break;
        }
        case SKILL_AXE: {
            /* SkillAxeArmorUse: counted and its two sub_080240A0
             * afterimages made on its first update (sound 0xE3 and
             * sub_08042A54 are not played). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                for (int k = 0; k < AOS_AXE_TRAILS; ++k) {
                    AriaSkill *trail = spawn_child(souls, SKILL_AXE_TRAIL);
                    if (!trail) continue;
                    trail->soul = skill->soul;
                    trail->parameter = skill->parameter;
                    trail->parent = skill->slot;
                    trail->trail = aos_axe_trail_create(k, souls->soma->facing_left);
                    trail->x = skill->x;
                    trail->y = skill->y;
                }
            }
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            if (!aos_axe_update(&skill->axe, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->axe.x;
            skill->y = skill->axe.y;
            break;
        }
        case SKILL_SPEAR:
            /* SkillWingedSkeletonUse: counted, with its tip child
             * sub_08025F70, on its first update (sound 0xE4 is not
             * played). */
            if (!skill->started) {
                skill->started = true;
                AriaSkill *tip = spawn_child(souls, SKILL_SPEAR_TIP);
                if (tip) {
                    tip->soul = skill->soul;
                    tip->parent = skill->slot;
                    tip->x = skill->x;
                    tip->y = skill->y;
                }
                souls->caster.active++;
            }
            {
                int mx = souls->margins ? souls->margins[4][0] : 16;
                int my = souls->margins ? souls->margins[4][1] : 16;
                if (!aos_spear_update(&skill->spear, souls->layer, souls->cam_x, souls->cam_y, mx,
                                      my)) {
                    end_skill(souls, skill);
                    break;
                }
            }
            skill->x = skill->spear.x;
            skill->y = skill->spear.y;
            break;
        case SKILL_FLEA: {
            /* SkillFleamanUse: counted with sound 0x19C on its first update
             * (sub_08042A54(1, 0x14) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x19C);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            bool ending;
            if (!aos_flea_update(&skill->flea, souls->layer, &souls->flea_kind->probes, aos_random,
                                 souls->cam_x, souls->cam_y, mx, my, &ending)) {
                end_skill(souls, skill);
                break;
            }
            if (ending) aos_anim_start(&skill->anim, souls->flea, AOS_FLEA_END_SEQUENCE, false);
            else if (skill->flea.state == 2) aos_anim_step(&skill->anim, souls->flea);
            skill->x = skill->flea.body.x;
            skill->y = skill->flea.body.y;
            skill->facing_left = skill->flea.body.mirrored;
            break;
        }
        case SKILL_GHOST:
            /* SkillGhostUse: one counted, invisible owner plays 0xD6 and
             * emits two sub_0802A49C spirits, then releases itself. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            aos_audio_play_song(0xD6);
            for (int k = 0; k < AOS_GHOST_SOULS; ++k) {
                AriaSkill *ghost = spawn_child(souls, SKILL_GHOST_SOUL);
                if (!ghost) break;
                ghost->soul = skill->soul;
                ghost->ghost = aos_ghost_soul_create(k, skill->x, skill->y, aos_random());
                ghost->x = ghost->ghost.x;
                ghost->y = ghost->ghost.y;
            }
            break;
        case SKILL_GHOST_SOUL: {
            AosGhostSoul *ghost = &skill->ghost;
            ghost->struck = skill->struck;
            if (ghost->state == 0) {
                aos_anim_start(&skill->anim, souls->devil, AOS_GHOST_SEQUENCE, false);
                ghost->state = 1;
            }
            if (ghost->state == 1) {
                if (aos_anim_step(&skill->anim, souls->devil) == 3) {
                    AosSkillTarget targets[4] = {{0}};
                    for (int slot = 0; slot < 4; ++slot) {
                        int index = homing.index[slot];
                        if (index < 0 || !homing_room ||
                            (size_t)index >= homing_room->entity_count)
                            continue;
                        const AriaEntity *entity = &homing_room->entities[index];
                        targets[slot] = (AosSkillTarget){
                            entity->spawned && !entity->enemy.removed,
                            entity->enemy.x, entity->enemy.y, entity->enemy.hp};
                    }
                    aos_ghost_soul_launch(ghost, targets);
                    skill->fire_target = ghost->target >= 0 ? homing.index[ghost->target] : -1;
                    skill->started = true;
                }
            } else {
                AosSkillTarget target = {0};
                if (skill->fire_target >= 0 && homing_room &&
                    (size_t)skill->fire_target < homing_room->entity_count) {
                    const AriaEntity *entity = &homing_room->entities[skill->fire_target];
                    target = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                              entity->enemy.x, entity->enemy.y,
                                              entity->enemy.hp};
                }
                aos_ghost_soul_update(ghost, &target, aos_random);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (aos_ghost_soul_outside(ghost, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = ghost->x;
            skill->y = ghost->y;
            break;
        }
        case SKILL_SIREN: {
            /* SkillSirenUse parameter 0: one counted note, sound 0xDD or
             * 0xDE, then the native horizontal and sine-wave motion.
             * sub_08042A54(1, 0xA) is not run. */
            AosSirenSoul *note = &skill->siren;
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                uint32_t variant_random = aos_random();
                uint32_t phase_random = aos_random();
                *note = aos_siren_soul_create(souls->soma->x, souls->soma->y,
                                              souls->soma->facing_left, variant_random,
                                              phase_random);
                aos_audio_play_song(note->song);
            }
            note->struck = skill->struck;
            int mx = souls->margins ? souls->margins[1][0] : 16;
            int my = souls->margins ? souls->margins[1][1] : 16;
            if (!aos_siren_soul_update(note, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = note->x;
            skill->y = note->y;
            skill->facing_left = false;
            break;
        }
        case SKILL_NEEDLES: {
            /* SkillNeedlesUse: counted with sound 0xE0 and looping sequence
             * 0 on its first update (sub_08042A54(1, 0xF) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xE0);
                aos_anim_start(&skill->anim, souls->needles, 0, true);
            }
            if (skill->needles.state == 1) aos_anim_step(&skill->anim, souls->needles);
            skill->needles.struck |= skill->struck;
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool burst;
            if (!aos_needles_update(&skill->needles, aos_random, souls->cam_x, souls->cam_y, mx,
                                    my, &burst)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045CEC(x, y, 15, 7): its dust. */
            if (burst && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_BURST_15, .drift = true, .x = skill->needles.x,
                    .y = skill->needles.y};
            skill->x = skill->needles.x;
            skill->y = skill->needles.y;
            break;
        }
        case SKILL_RIPPER: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            int16_t wall = 0;
            bool outside = false;
            if (skill->ripper.state == AOS_RIPPER_FLYING) {
                int32_t tested_x = skill->ripper.x;
                if (!skill->ripper.attached) {
                    tested_x = (int32_t)((uint32_t)tested_x + (uint32_t)skill->ripper.vx);
                    int x = tested_x >> 16, y = skill->ripper.y >> 16;
                    if (souls->layer)
                        wall = skill->ripper.facing_left
                                   ? (int16_t)aos_wall_push_right(souls->layer, x - 6, y)
                                   : (int16_t)aos_wall_push_left(souls->layer, x + 6, y);
                }
                int mx = souls->margins ? souls->margins[4][0] : 16;
                int my = souls->margins ? souls->margins[4][1] : 16;
                int sx = (tested_x >> 16) - souls->cam_x;
                int sy = (skill->ripper.y >> 16) - souls->cam_y;
                outside = sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
            }
            AosRipperTarget target = {0};
            const AosRipperTarget *target_ptr = NULL;
            if (skill->ripper_target) {
                target = (AosRipperTarget){.x = skill->ripper_target->x,
                                           .y = skill->ripper_target->y,
                                           .active = !skill->ripper_target->removed,
                                           .defeated = skill->ripper_target->defeated ||
                                                       skill->ripper_target->hp <= 0};
                target_ptr = &target;
            }
            if (!aos_ripper_soul_update(&skill->ripper, wall, outside, target_ptr)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->ripper.x;
            skill->y = skill->ripper.y;
            break;
        }
        case SKILL_MILLIONE_EMITTER: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            AosMillioneSpawn spawn;
            bool alive = aos_millione_soul_update(&skill->millione, souls->soma->x,
                                                   souls->soma->y,
                                                   souls->soma->facing_left, &spawn);
            skill->x = skill->millione.x;
            skill->y = skill->millione.y;
            skill->facing_left = skill->millione.facing_left;
            if (spawn.created) {
                AriaSkill *child = spawn_child(souls, SKILL_MILLIONE_CHILD);
                if (child) {
                    child->soul = skill->soul;
                    child->x = spawn.x;
                    child->y = spawn.y;
                    child->facing_left = spawn.facing_left;
                }
            }
            if (!alive || skill_outside(souls, skill, 3)) end_skill(souls, skill);
            break;
        }
        case SKILL_MILLIONE_CHILD:
            /* sub_0802B028: sequence 0 supplies both the visible frames
             * and its changing type-5 attack box. sub_08023530 already
             * counted this child when it was allocated. */
            if (skill->state == 2) {
                end_skill(souls, skill);
                break;
            }
            if (!skill->started) {
                skill->started = true;
                skill->state = 1;
                aos_anim_start(&skill->anim, souls->millione, 0, false);
            }
            if (aos_anim_step(&skill->anim, souls->millione) == 3) skill->state = 2;
            if (skill_outside(souls, skill, 2)) end_skill(souls, skill);
            break;
        case SKILL_FISH_HEAD: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->fish_head, 0, true);
            }
            skill->fish_head.struck |= skill->struck;
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (!aos_fish_head_soul_update(&skill->fish_head, souls->cam_x, souls->cam_y,
                                           mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->fish_head.x;
            skill->y = skill->fish_head.y;
            aos_anim_step(&skill->anim, souls->fish_head);
            break;
        }
        case SKILL_KILLER_FISH: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->killer_fish, 1, false);
            }
            AosKillerFishTarget targets[4] = {{0}};
            for (int slot = 0; slot < 4; ++slot) {
                int index = homing.index[slot];
                if (index < 0 || !homing_room ||
                    (size_t)index >= homing_room->entity_count)
                    continue;
                const AriaEntity *entity = &homing_room->entities[index];
                targets[slot] = (AosKillerFishTarget){
                    entity->spawned && !entity->enemy.removed,
                    entity->enemy.x, entity->enemy.y};
            }
            uint8_t old_state = skill->killer_fish.state;
            AosKillerFishEffect effect;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            bool alive = aos_killer_fish_soul_update(
                &skill->killer_fish, souls->layer, targets, 4,
                (skill->anim.flags & AOS_ANIM_ENDED) != 0, aos_random,
                souls->cam_x, souls->cam_y, mx, my, &effect);
            if (old_state == AOS_KILLER_FISH_APPEARING &&
                skill->killer_fish.state != AOS_KILLER_FISH_APPEARING)
                aos_anim_start(&skill->anim, souls->killer_fish, 0, true);
            if (effect.spawn && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_IMPACT, .drift = true,
                    .x = effect.x, .y = effect.y};
            if (!alive) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->killer_fish.x;
            skill->y = skill->killer_fish.y;
            skill->facing_left = skill->killer_fish.facing_left;
            aos_anim_step(&skill->anim, souls->killer_fish);
            break;
        }
        case SKILL_GLADIATOR: {
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_anim_start(&skill->anim, souls->gladiator, 0, true);
            }
            AosGladiatorDust dust;
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            bool alive = aos_gladiator_soul_update(
                &skill->gladiator, souls->layer, &souls->gladiator_kind->probes,
                aos_random, souls->cam_x, souls->cam_y, mx, my, &dust);
            if (dust.spawn && souls->effects && *souls->effect_count < MAX_EFFECTS)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_FLAME, .drift = true, .x = dust.x, .y = dust.y};
            if (!alive) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->gladiator.body.x;
            skill->y = skill->gladiator.body.y;
            aos_anim_step(&skill->anim, souls->gladiator);
            break;
        }
        case SKILL_VALKYRIE: {
            /* SkillValkyrieUse: counted with sound 0x1AF and sequence 0 on
             * its first update (sub_08042A54(1, 0x3C) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1AF);
                aos_anim_start(&skill->anim, souls->valkyrie, 0, false);
            }
            AosValkyrieSpark spark;
            if (!aos_valkyrie_update(&skill->valkyrie, souls->cam_x, souls->cam_y, aos_random,
                                     &spark)) {
                end_skill(souls, skill);
                break;
            }
            /* sub_08045B44(x, y, 0, ay, 0, 6), shown twice by its draw. */
            for (int copy = 0; spark.spawn && souls->effects && copy < 2 &&
                               *souls->effect_count < MAX_EFFECTS;
                 ++copy)
                souls->effects[(*souls->effect_count)++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .timed = true,
                    .x = spark.x + (copy ? spark.offset_x : 0),
                    .y = spark.y + (copy ? spark.offset_y : 0), .vy = -0x10000, .ay = spark.ay,
                    .timer = 12};
            if (aos_anim_step(&skill->anim, souls->valkyrie) == 3)
                aos_valkyrie_animation_ended(&skill->valkyrie);
            break;
        }
        case SKILL_SKULL:
            /* SkillBeamSkeletonUse: counted with sound 0x1A9 and sequence 0
             * on its first update (sub_08042A54(1, 0x10) is not run). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A9);
                aos_anim_start(&skill->anim, souls->skull, 0, false);
            }
            if (!aos_skull_update(&skill->skull, souls->soma->x, souls->soma->y,
                                  skill->anim.frame, skill->anim.tick)) {
                end_skill(souls, skill);
                break;
            }
            if (aos_anim_step(&skill->anim, souls->skull) == 3) aos_skull_end(&skill->skull);
            skill->x = skill->skull.x;
            skill->y = skill->skull.y;
            break;
        case SKILL_DECOY:
            /* SkillKillerDollUse: counted with sound 0xE5 and looping
             * sequence 0 or 1 at random on its first update, the enemies'
             * aim (0x13168) while it exists; deleted when its time is out
             * or beyond the margins of entry 2. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xE5);
                aos_anim_start(&skill->anim, souls->killer_doll, aos_random() & 1, true);
            }
            if (!aos_decoy_update(&skill->decoy, souls->layer)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->decoy.x;
            skill->y = skill->decoy.y;
            if (skill_outside(souls, skill, 2)) {
                end_skill(souls, skill);
                break;
            }
            aos_anim_step(&skill->anim, souls->killer_doll);
            break;
        case SKILL_NEMESIS:
            /* SkillNemesisUse: counted with sound 0x97 and looping
             * sequence 2 of resource 21 on its first update, the enemies
             * without target (0x13168 = 0) while it exists; it rises over
             * Soma (sub_08031EA4) and is deleted after its blink. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x97);
                aos_anim_start(&skill->anim, souls->devil, AOS_NEMESIS_SOUL_SEQUENCE, true);
            }
            if (!aos_nemesis_soul_update(&skill->nemesis)) {
                end_skill(souls, skill);
                break;
            }
            place(skill, souls->soma->x, souls->soma->y, souls->soma->facing_left, 0,
                  aos_nemesis_soul_dy(&skill->nemesis));
            aos_anim_step(&skill->anim, souls->devil);
            break;
        case SKILL_LIGHTNING:
            /* SkillLightningDollUse: counted with sounds 0x1A9 (sub_080213F4
             * is false: 0x13220 is never set) and 0xE9 and sequence 0 once
             * on its first update; deleted when it ends or beyond the
             * screen margins of entry 2 (sub_0806D128). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0x1A9);
                aos_audio_play_song(0xE9);
                aos_anim_start(&skill->anim, souls->lightning, 0, false);
            }
            aos_lightning_update(&skill->lightning, (uint8_t)skill->anim.frame,
                                 souls->soma->flags, souls->soma->x, souls->soma->y,
                                 souls->soma->facing_left);
            skill->x = skill->lightning.x;
            skill->y = skill->lightning.y;
            skill->facing_left = skill->lightning.facing_left;
            if (aos_anim_step(&skill->anim, souls->lightning) == 3 ||
                skill_outside(souls, skill, 2))
                end_skill(souls, skill);
            break;
        case SKILL_CROW:
            /* sub_SkillBlueCrowUse: counted with sound 0xEE on its first
             * update, looping sequence 2 (sub_08042A54(1, 0x14) is not
             * run); it circles Soma for 50 frames. */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
                aos_audio_play_song(0xEE);
                aos_anim_start(&skill->anim, souls->crow, AOS_CROW_SEQUENCE, true);
            }
            if (!aos_crow_update(&skill->crow, souls->soma->x, souls->soma->y)) {
                end_skill(souls, skill);
                break;
            }
            aos_anim_step(&skill->anim, souls->crow);
            skill->x = skill->crow.x;
            skill->y = skill->crow.y;
            break;
        case SKILL_DEVIL:
            /* SkillTinyDevilUse: counted, sound 0x1A3 and three
             * SkillTinyDevilUpdate fires from its position (sub_08042A54(1,
             * 0x14) is not run); deleted on its next update. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            aos_audio_play_song(0x1A3);
            for (int k = 0; k < AOS_DEVIL_FIRES; ++k) {
                AriaSkill *fire = spawn_child(souls, SKILL_DEVIL_FIRE);
                if (!fire) break;
                fire->soul = skill->soul;
                fire->fire = aos_devil_fire_create(k, skill->x, skill->y, skill->facing_left);
                fire->fire_target = -1;
                fire->x = skill->x;
                fire->y = skill->y;
            }
            break;
        case SKILL_DEVIL_FIRE: {
            AosDevilFire *fire = &skill->fire;
            fire->struck = skill->struck;
            if (fire->state == 0) {
                aos_anim_start(&skill->anim, souls->devil, 0, false);
                fire->state = 1;
            }
            if (fire->state == 1) {
                if (aos_anim_step(&skill->anim, souls->devil) == 3) {
                    /* The homing list's filled slots (0x13220 is never set
                     * by the runtime), the 0x134 sound; the two afterimage
                     * trails of sub_0802383C are not ported. */
                    bool slots[4];
                    for (int slot = 0; slot < 4; ++slot) slots[slot] = homing.index[slot] >= 0;
                    aos_devil_fire_launch(fire, aos_random(), slots);
                    skill->fire_target = fire->target >= 0 ? homing.index[fire->target] : -1;
                    aos_audio_play_song(0x134);
                    skill->started = true;
                }
            } else {
                AosSkillTarget target = {false, 0, 0, 0};
                if (skill->fire_target >= 0 && homing_room &&
                    (size_t)skill->fire_target < homing_room->entity_count) {
                    const AriaEntity *entity = &homing_room->entities[skill->fire_target];
                    target = (AosSkillTarget){entity->spawned && !entity->enemy.removed,
                                              entity->enemy.x, entity->enemy.y, entity->enemy.hp};
                }
                aos_devil_fire_update(fire, &target);
            }
            int mx = souls->margins ? souls->margins[2][0] : 16;
            int my = souls->margins ? souls->margins[2][1] : 16;
            if (aos_devil_fire_outside(fire, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = fire->x;
            skill->y = fire->y;
            break;
        }
        case SKILL_HARPY:
            /* SkillHarpyUse: counted, then (same update) two sub_0802CAC0
             * daggers from its position (sound 0xE6 and sub_08042A54(1,
             * 0x14) are not played); deleted on its next update. */
            if (skill->started) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            souls->caster.active++;
            for (int k = 0; k < AOS_HARPY_DAGGERS; ++k) {
                AriaSkill *dagger = spawn_child(souls, SKILL_DAGGER);
                if (!dagger) break;
                dagger->soul = skill->soul;
                dagger->dagger = aos_dagger_create(k, skill->x, skill->y);
                dagger->dagger_trail = aos_dagger_trail_create(&dagger->dagger);
                dagger->x = skill->x;
                dagger->y = skill->y;
                dagger->facing_left = dagger->dagger.facing_left;
            }
            break;
        case SKILL_DAGGER: {
            /* sub_0802CAC0, then its trail entity's sub_0802C944. */
            skill->started = true;
            skill->dagger.struck = skill->struck;
            int mx = souls->margins ? souls->margins[4][0] : 16;
            int my = souls->margins ? souls->margins[4][1] : 16;
            if (!aos_dagger_update(&skill->dagger, souls->layer, souls->cam_x, souls->cam_y, mx,
                                   my)) {
                end_skill(souls, skill);
                break;
            }
            aos_dagger_trail_update(&skill->dagger_trail, &skill->dagger);
            skill->x = skill->dagger.x;
            break;
        }
        case SKILL_SHOT: {
            /* SkillDurgaUse: counted on its first update (sound 0xC7 and
             * sub_08042A54(1, 0x1E) are not played). */
            if (!skill->started) {
                skill->started = true;
                souls->caster.active++;
            }
            int mx = souls->margins ? souls->margins[3][0] : 16;
            int my = souls->margins ? souls->margins[3][1] : 16;
            if (!aos_shot_update(&skill->shot, souls->cam_x, souls->cam_y, mx, my)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->shot.x;
            break;
        }
        case SKILL_SPEAR_TIP: {
            /* sub_08025F70: the spear's type-5 box at its tip, until the
             * spear reaches state 3 (sub_08042A54(1, 0x14) is not run). */
            const AriaSkill *parent = &souls->skills[skill->parent];
            if (!parent->alive || parent->kind != SKILL_SPEAR || parent->spear.state == 3) {
                end_skill(souls, skill);
                break;
            }
            skill->started = true;
            skill->x = parent->spear.x + parent->spear.tip_x;
            skill->y = parent->spear.y + parent->spear.tip_y;
            break;
        }
        case SKILL_AXE_TRAIL: {
            const AriaSkill *parent = &souls->skills[skill->parent];
            if (!skill->started) skill->started = true;
            /* The parent's slot may be taken again only after it was freed,
             * which happens after its state 2: a trail sees it first. */
            if (!parent->alive || parent->kind != SKILL_AXE ||
                !aos_axe_trail_update(&skill->trail, &parent->axe)) {
                end_skill(souls, skill);
                break;
            }
            skill->x = skill->trail.x;
            skill->y = skill->trail.y;
            break;
        }
        case SKILL_ARROW:
            /* sub_08028BC4: frame 1, 6 pixels a frame (sound 0xC8 is not
             * played); once its hit callback has run it stops and blinks 16
             * frames (the stuck-arrow helper sub_080239E4 is not ported). */
            if (!skill->started) {
                skill->started = true;
                skill->vx = skill->facing_left ? -0x60000 : 0x60000;
                skill->state = 1;
            }
            if (skill->state == 1) {
                skill->x += skill->vx;
                if (skill->struck) {
                    skill->state = 2;
                    skill->timer = 0x10;
                }
            } else if (skill->state == 2) {
                if (skill->timer == 0) {
                    end_skill(souls, skill);
                    break;
                }
                skill->timer--;
            }
            if (skill_outside(souls, skill, 2)) end_skill(souls, skill);
            break;
        default: break;
        }
    }
}

/* The skills' attack boxes this frame for the enemies' type-5 tests:
 * SkillBatUse enables its frame's box (frame record + 8) when it has one;
 * the arrow keeps 0x0202FF08 (8, -1, 2 x 2) until it has struck. The power
 * and elements come from the last used soul's record (sub_0802D244). */
static int soul_attacks(AriaSouls *souls, const AosEnemyKind *bat, AosSoulAttack attacks[MAX_SKILLS],
                        AriaSkill *owners[MAX_SKILLS]) {
    int count = 0;
    if (!souls->records || souls->caster.last_used >= AOS_RED_SOUL_RECORDS) return 0;
    const AosRedSoulRecord *record = &souls->records[souls->caster.last_used];
    for (int i = 0; i < MAX_SKILLS; ++i) {
        AriaSkill *skill = &souls->skills[i];
        if (!skill->alive || !skill->started) continue;
        AosBox box = {0, 0, 0, 0};
        if (skill->kind == SKILL_BAT && bat && skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = bat->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_ARROW && skill->state == 1 && !skill->struck)
            box = (AosBox){8, -1, 2, 2};
        else if (skill->kind == SKILL_THROW && skill->thrown.attacking &&
                 skill->thrown.state != 3)
            /* 0x1010F8F8, or the fused throws' 0x2838E2E4 once exploded. */
            box = souls->throws[skill->parameter].flags & 0x60 ? (AosBox){-28, -30, 56, 40}
                                                                : (AosBox){-8, -8, 16, 16};
        else if (skill->kind == SKILL_DROP && skill->drop.attacking)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SHOT)
            box = souls->durga_attack;          /* the still frame's, sub_08042848 */
        else if (skill->kind == SKILL_DAGGER)
            box = (AosBox){-1, -1, 2, 2};      /* 0x0202FFFF */
        else if (skill->kind == SKILL_FLEA && souls->flea_kind)
            box = skill->flea.state == 1
                      ? souls->flea_still_attack[skill->flea.frame]
                      : (skill->anim.frame < AOS_ENEMY_MAX_FRAMES
                             ? souls->flea_kind->attack[AOS_FLEA_END_SEQUENCE][skill->anim.frame]
                             : (AosBox){0, 0, 0, 0});
        else if (skill->kind == SKILL_NEEDLES && skill->needles.state != 3)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_RIPPER && skill->ripper.state == AOS_RIPPER_FLYING)
            box = skill->ripper.attached ? (AosBox){-10, -8, 20, 16} /* 0x1014F8F6 */
                                         : (AosBox){-1, -1, 2, 2};   /* 0x0202FFFF */
        else if (skill->kind == SKILL_MILLIONE_CHILD && souls->bone_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->bone_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_FISH_HEAD)
            box = (AosBox){-6, -6, 12, 12};    /* 0x0C0CFAFA */
        else if (skill->kind == SKILL_KILLER_FISH &&
                 skill->killer_fish.state != AOS_KILLER_FISH_ENDING)
            box = (AosBox){-24, -16, 48, 32};  /* 0x2030F0E8 */
        else if (skill->kind == SKILL_GLADIATOR)
            box = (AosBox){-16, -16, 32, 32};  /* 0x2020F0F0 */
        else if (skill->kind == SKILL_VALKYRIE && souls->valkyrie_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->valkyrie_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_LIGHTNING)
            box = aos_lightning_box(&skill->lightning);
        else if (skill->kind == SKILL_SKULL && souls->skull_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->skull_kind->attack[0][skill->anim.frame];
        else if (skill->kind == SKILL_CROW && souls->crow_kind &&
                 skill->anim.frame < AOS_ENEMY_MAX_FRAMES)
            box = souls->crow_kind->attack[AOS_CROW_SEQUENCE][skill->anim.frame];
        else if (skill->kind == SKILL_GHOST_SOUL && skill->ghost.state >= 2)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SIREN && skill->siren.state == 1)
            box = (AosBox){-4, -4, 8, 8};      /* 0x0808FCFC */
        else if (skill->kind == SKILL_DEVIL_FIRE && skill->fire.state >= 2)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_SPEAR_TIP)
            box = (AosBox){-8, -8, 16, 16};    /* 0x1010F8F8 */
        else if (skill->kind == SKILL_AXE && skill->axe.state == 1)
            box = (AosBox){-10, -10, 20, 20};  /* 0x1414F6F6 */
        if (!box.w || !box.h) continue;
        owners[count] = skill;
        attacks[count++] = (AosSoulAttack){
            aos_entity_rect(box, skill->x >> 16, skill->y >> 16, skill->facing_left, false),
            aos_red_soul_power(souls->stats, record, false), record->elements, false, NULL,
            AOS_TYPE_SOUL};
    }
    return count;
}

/* A room change deletes the skill entities with the room's. */
static void clear_skills(AriaSouls *souls) {
    for (int i = 0; i < MAX_SKILLS; ++i) souls->skills[i].alive = false;
    homing_clear();
    souls->caster = (AosRedSoulCaster){.last_used = souls->caster.last_used};
}

/* gEwramData + 0x344: the event bits (a breakable wall's break), stored
 * in the save (schema v10). */
static uint32_t aria_events[AOS_WALL_EVENT_WORDS];
/* The durable words touched by the common pickup collection tail. */
static AosPickupCollectionState aria_pickup_state;

/* A sound the next frame's entity pass must request: the native sparkle
 * entity plays 0xBF on its first update, one frame after the collection
 * that played 0xBC — same player, so emitting both in one frame would
 * drop the collection sound. */
static uint16_t deferred_sound;
static bool deferred_sound_pending;

static const char *pickup_notice_name(uint8_t type, uint16_t variant) {
    if (type == AOS_PICKUP_ITEM)
        return variant < AOS_ITEM_RECORDS ? aos_item_name((uint8_t)variant) : NULL;
    if (type == AOS_PICKUP_WEAPON)
        return variant < AOS_WEAPON_RECORDS
                   ? aos_equipment_name(AOS_EQUIP_WEAPON, (uint8_t)variant)
                   : NULL;
    if (type == AOS_PICKUP_ARMOR)
        return variant < AOS_ARMOR_RECORDS
                   ? aos_equipment_name(AOS_EQUIP_ARMOR, (uint8_t)variant)
                   : NULL;
    return NULL;
}

static bool spawn_special_boss_soul(AriaRoom *room, const AriaEntity *entity,
                                    const AosSoma *soma, const AriaCombat *combat) {
    uint16_t special_index;
    if (!room || !entity || !soma || !combat || !combat->drops[entity->id] ||
        !aos_pickup_special_boss_index((uint8_t)entity->id, &special_index) ||
        (aria_pickup_state.special & (uint16_t)(1u << special_index)) ||
        room->pickup_count >= MAX_PICKUPS)
        return false;
    const AosEnemyDrops *drops = combat->drops[entity->id];
    if (!drops->soul_index) return false;
    AosPickup special;
    if (!aos_pickup_create_special_soul(
            &special, 0, false, drops->soul_type, (uint8_t)(drops->soul_index - 1u),
            special_index, entity->enemy.x >> 16, entity->enemy.y >> 16,
            soma->x, soma->y, aos_random))
        return false;
    room->pickups[room->pickup_count++] = special;
    return true;
}

/* sub_0800F4F8 / sub_0800F1FC: records whose X is within the camera window
 * spawn once per room visit; native enemies include hidden producers such
 * as the Ghost's, whose children retain their producer slot. */
static int update_entities(AriaRoom *room, AosCollision *layer, AosSoma *soma,
                           int cam_x, int cam_y, AosForcedInput *input, AriaCombat *combat) {
    if (deferred_sound_pending) {
        deferred_sound_pending = false;
        aos_audio_play_song(deferred_sound);
    }
    int sound = 0;
    /* unk_A074_0: while the level-up display's counter sits in
     * [2, 0x3C], the native entity update loop is suspended; the
     * display and its sparkles are drawFunc-driven and keep running. */
    const bool entities_frozen = aos_levelup_frozen(&combat->levelup);
    for (size_t i = 0; !entities_frozen && i < room->entity_count; ++i) {
        AriaEntity *e = &room->entities[i];
        bool placed_pickup = e->kind == ARIA_KIND_PICKUP && e->id >= 1 && e->id <= 8;
        if (placed_pickup) {
            if (!e->spawned && e->x >= cam_x - 80 && e->x <= cam_x + 320) {
                e->spawned = true;
                AosPickup pickup;
                bool created = e->id <= 4
                    ? aos_pickup_create_placed(&pickup, combat->money, (uint8_t)e->id,
                                                (uint16_t)e->param1, (uint16_t)e->param0,
                                                e->x, e->y, aria_pickup_state.placed)
                    : aos_pickup_create_placed_soul(&pickup, (uint8_t)e->id,
                                                     (uint16_t)e->param1,
                                                     (uint16_t)e->param0, e->x, e->y,
                                                     aria_pickup_state.placed);
                if (room->pickup_count < MAX_PICKUPS && created)
                    room->pickups[room->pickup_count++] = pickup;
            }
            continue;
        }
        bool door = e->kind == ARIA_KIND_SPECIAL && e->id == ARIA_OBJECT_WOODEN_DOOR;
        bool wall = e->kind == ARIA_KIND_SPECIAL &&
                    (e->id == ARIA_OBJECT_WALL || e->id == ARIA_OBJECT_EVENT_WALL);
        if (wall) {
            if (!e->spawned) {
                if (e->x < cam_x - 80 || e->x > cam_x + 320) continue;
                e->spawned = true;
                if (!aos_wall_create(&e->wall, layer, e->x, e->y, e->param0, e->param1,
                                     e->id == ARIA_OBJECT_EVENT_WALL, aria_events))
                    e->wall.broken = true;   /* deleted: never drawn, never updated */
                continue;
            }
            if (!aos_wall_alive(&e->wall)) continue;
            bool struck = aos_wall_weapon_hit(&e->wall, combat->weapon, soma,
                                              combat->weapon_frames);
            AosWallReport report = {0};
            aos_wall_update(&e->wall, layer, aria_events,
                            combat->progress ? combat->progress->flags : 0, struck, aos_random,
                            &report);
            for (int k = 0; k < report.effect_count && room->effect_count < MAX_EFFECTS; ++k)
                if (combat->effects[report.effects[k].kind])
                    room->effects[room->effect_count++] = report.effects[k];
            if (report.sound) {
                aos_audio_play_song(report.sound);
                printf("Wall %zu: %s (sound 0x%X)\n", i, report.broke ? "broken" : "struck",
                       report.sound);
            }
            /* Object08Update / Object09Update mark the broken wall's
             * secret-room cell; the quick map lives in the caller. */
            if (report.broke) e->secret_mark = true;
            continue;
        }
        bool enemy = e->kind == ARIA_KIND_ENEMY && e->id < ENEMY_KINDS && combat->kinds[e->id];
        if (enemy) {
            const AosEnemyKind *kind = combat->kinds[e->id];
            if (!e->spawned) {
                if (e->x < cam_x - 80 || e->x > cam_x + 320) continue;
                e->spawned = aos_enemy_create(&e->enemy, (uint8_t)e->id, e->x, e->y,
                                              (int16_t)e->param0, (int16_t)e->param1, soma, layer,
                                              kind, combat->stats[e->id]);
                e->spawner = -1;
                continue;
            }
            bool was_removed = e->enemy.removed;
            /* Linked child entities follow their owner's room slot. */
            if ((e->enemy.role == AOS_ROLE_FIN || e->enemy.role == AOS_ROLE_BEAM ||
                 e->enemy.role == AOS_ROLE_BEAM_FLASH ||
                 e->enemy.role == AOS_ROLE_NEMESIS_HELPER ||
                 e->enemy.role == AOS_ROLE_NEMESIS_EFFECT ||
                 e->enemy.role == AOS_ROLE_FRY ||
                 e->enemy.role == AOS_ROLE_PILLAR_BREATH ||
                 e->enemy.role == AOS_ROLE_IMP_HELPER ||
                 e->enemy.role == AOS_ROLE_RIPPER_BLADE ||
                 e->enemy.role == AOS_ROLE_HEADHUNTER_HEAD ||
                 e->enemy.role == AOS_ROLE_PEEPING_SEGMENT ||
                 e->enemy.role == AOS_ROLE_BONE_PILLAR_SEGMENT ||
                 e->enemy.role == AOS_ROLE_BUER_FIRE ||
                 e->enemy.role == AOS_ROLE_SKELETON_HEAD ||
                 e->enemy.role == AOS_ROLE_SKELETON_HELD ||
                 e->enemy.role == AOS_ROLE_SKELETON_BONE ||
                 e->enemy.role == AOS_ROLE_WORM_SEGMENT ||
                 e->enemy.role == AOS_ROLE_WHITE_DRAGON_PART ||
                 e->enemy.role == AOS_ROLE_MANTLE_ORBIT ||
                 e->enemy.role == AOS_ROLE_MANTLE_GHOST) &&
                e->spawner >= 0)
                e->enemy.parent = &room->entities[e->spawner].enemy;
            /* sub_080AD7F0's + 0x18 link: segment 1 follows the parent,
             * segments 2-4 the previous segment (the children are
             * appended consecutively, so i - 1 is it). */
            if (e->enemy.role == AOS_ROLE_WORM_SEGMENT && e->spawner >= 0) {
                if (e->enemy.worm_index <= 1 || i == 0)
                    e->enemy.follow = e->enemy.parent;
                else if (room->entities[i - 1].enemy.role == AOS_ROLE_WORM_SEGMENT &&
                         room->entities[i - 1].id == e->id)
                    e->enemy.follow = &room->entities[i - 1].enemy;
                else
                    e->enemy.follow = e->enemy.parent;
            }
            AosHitReport hit = aos_enemy_update(&e->enemy, soma, layer, kind, combat->weapon,
                                                combat->weapon_frames, combat->player->atk,
                                                combat->player->def, cam_x, cam_y, aos_random);
            if (hit.targetable) homing_register((int)i);
            /* EnemyImpUpdate calls SetPlayerInput(1, B/0) on alternating
             * frames while its linked helper controls Soma. As with doors,
             * the forced value is consumed on the following frame. */
            /* sub_080683BC: a defeated enemy grants its experience once.
             * A level gained calls sub_080455BC(0): the level-up display
             * entity (still frame 0x48, OBJ row 6) with PlaySong(0xBC);
             * the equipment refresh is sub_08033CAC's tail. */
            /* Bone Pillar's skulls are enemies in their own right. */
            if (hit.killed &&
                (e->enemy.role == AOS_ROLE_ENEMY ||
                 e->enemy.role == AOS_ROLE_BONE_PILLAR_SEGMENT) &&
                combat->progress) {
                int32_t gained = aos_stats_reward_enemy(
                    combat->progress, &e->reward_flags, 0, (uint8_t)e->id,
                    combat->stats[e->id]->experience);
                if (gained > 0) {
                    soma->max_hp = (int16_t)combat->progress->max_hp;
                    /* sub_080455BC(0): the display entity; its creation
                     * plays 0xBC on character index 0. */
                    aos_levelup_start(&combat->levelup);
                    aos_audio_play_song(0xBC);
                    /* sub_08033CAC ends with sub_0804AD9C. */
                    if (combat->equipment) {
                        aos_stats_refresh(combat->progress, combat->equipment);
                        if (combat->derive_atk) combat->player->atk = combat->progress->atk;
                        if (combat->derive_def) combat->player->def = combat->progress->def;
                    }
                    printf("Level up: level %u, max HP %u\n", combat->progress->level,
                           combat->progress->max_hp);
                }
                if (gained >= 0) {
                    if (combat->hud && combat->hud_font)
                        aos_hud_show_enemy(combat->hud, combat->hud_font,
                                           aos_bestiary_name((uint8_t)e->id));
                    printf("Experience %u (next level in %d)\n", combat->progress->experience,
                           aos_stats_experience_to_next(combat->progress));
                    /* The drop draws, in the native order; the pickups they
                     * would create (souls, items, sub_08043F70/43C10) are not
                     * ported. No equipment or souls are owned yet, so the
                     * accessory is none and the soul counts as unowned. */
                    AosDropContext context = {
                        .luck = (int16_t)combat->progress->current[AOS_STAT_LCK],
                        .accessory = combat->progress->accessory,
                        .soul_owned = combat->drops[e->id]->soul_index &&
                                      aos_souls_total(combat->progress,
                                                      combat->drops[e->id]->soul_type,
                                                      combat->drops[e->id]->soul_index - 1),
                        .ground = aos_collision_cell(layer, e->enemy.x >> 16,
                                                     (e->enemy.y >> 16) - 8)};
                    const AosEnemyDrops *enemy_drops = combat->drops[e->id];
                    if (e->id != AOS_BOSS_HEADHUNTER)
                        (void)spawn_special_boss_soul(room, e, soma, combat);
                    AosDrop drop = aos_stats_roll_drop(combat->progress, (uint8_t)e->id,
                                                       enemy_drops, &context,
                                                       aos_random);
                    if (drop.kind == AOS_DROP_SOUL) {
                        /* sub_0804459C(x, y - 8, type, index, 0). */
                        if (room->pickup_count < MAX_PICKUPS)
                            room->pickups[room->pickup_count++] = aos_pickup_create_soul(
                                drop.soul_type, drop.soul_index, e->enemy.x >> 16,
                                (e->enemy.y >> 16) - 8, soma->x, soma->y, aos_random);
                        printf("Drop: soul type %u index %u\n", drop.soul_type,
                               drop.soul_index);
                    }
                    else if (drop.kind == AOS_DROP_ITEM) {
                        /* sub_08044054(x, y - 8, 0xFF, item). */
                        uint8_t category, index;
                        if (aos_item_decode(drop.item, &category, &index) &&
                            room->pickup_count < MAX_PICKUPS)
                            room->pickups[room->pickup_count++] = aos_pickup_create_item(
                                category, index, e->enemy.x >> 16, (e->enemy.y >> 16) - 8);
                        printf("Drop: %s item 0x%02X\n", drop.rare ? "rare" : "common",
                               drop.item);
                    }
                    else if (drop.kind == AOS_DROP_PICKUP_43F70 && combat->money &&
                             room->pickup_count < MAX_PICKUPS)
                        /* sub_08043F70 at the enemy's (x, y - 8). */
                        room->pickups[room->pickup_count++] = aos_pickup_create_money(
                            combat->money, drop.variant, e->enemy.x >> 16,
                            (e->enemy.y >> 16) - 8);
                    else if (drop.kind == AOS_DROP_PICKUP_43C10 &&
                             room->pickup_count < MAX_PICKUPS)
                        /* sub_08043C10(x, y - 8, big, 1): an MP heart. */
                        room->pickups[room->pickup_count++] = aos_pickup_create_heart(
                            drop.variant != 0, true, e->enemy.x >> 16, (e->enemy.y >> 16) - 8);
                    else if (drop.kind != AOS_DROP_NONE)
                        printf("Drop: pickup sub_%s variant %u\n",
                               drop.kind == AOS_DROP_PICKUP_43F70 ? "08043F70" : "08043C10",
                               drop.variant);
                }
            }
            if (hit.special_soul)
                (void)spawn_special_boss_soul(room, e, soma, combat);
            if (hit.force_input && !input->active) {
                input->active = true;
                input->held = hit.forced_held;
            }
            if (!was_removed && e->enemy.removed && e->spawner >= 0) {
                /* A spawned enemy or linked piece leaves: its producer's
                 * count drops. */
                AosEnemy *spawner = &room->entities[e->spawner].enemy;
                if (spawner->spawned_count) spawner->spawned_count--;
            }
            /* Shared effects, updated after the entities of this frame
             * (their slot order against the enemies' is not traced). */
            for (int c = 0; c < hit.effect_count && room->effect_count < MAX_EFFECTS; ++c)
                if (combat->effects[hit.effects[c].kind])
                    room->effects[room->effect_count++] = hit.effects[c];
            if (hit.has_damage_popup && room->damage_popup_count < MAX_DAMAGE_POPUPS)
                room->damage_popups[room->damage_popup_count++] = hit.damage_popup;
            /* Child entities of this kind (grenades, axes, arrows, debris). */
            for (int c = 0; c < hit.child_count && room->entity_count < MAX_ENTITIES; ++c)
                room->entities[room->entity_count++] = (AriaEntity){
                    .kind = ARIA_KIND_ENEMY, .id = e->id, .x = hit.children[c].x >> 16,
                    .y = hit.children[c].y >> 16, .spawned = true,
                    /* Needles pieces count down their parent's + 0x13; a
                     * Mimic's trigger wakes its parent. */
                    .spawner = hit.children[c].role == AOS_ROLE_NEEDLE_PIECE ||
                                       hit.children[c].role == AOS_ROLE_MIMIC_TRIGGER ||
                                       hit.children[c].role == AOS_ROLE_FIN ||
                                       hit.children[c].role == AOS_ROLE_BEAM ||
                                       hit.children[c].role == AOS_ROLE_BEAM_FLASH ||
                                       hit.children[c].role == AOS_ROLE_DOLL_PIECE ||
                                       hit.children[c].role == AOS_ROLE_NEMESIS_HELPER ||
                                       hit.children[c].role == AOS_ROLE_NEMESIS_EFFECT ||
                                       hit.children[c].role == AOS_ROLE_FRY ||
                                       hit.children[c].role == AOS_ROLE_PILLAR_BREATH ||
                                       hit.children[c].role == AOS_ROLE_IMP_HELPER ||
                                       hit.children[c].role == AOS_ROLE_RIPPER_BLADE ||
                                       hit.children[c].role == AOS_ROLE_HEADHUNTER_HEAD ||
                                       hit.children[c].role == AOS_ROLE_PEEPING_SEGMENT ||
                                       hit.children[c].role == AOS_ROLE_BONE_PILLAR_SEGMENT ||
                                       hit.children[c].role == AOS_ROLE_BUER_FIRE ||
                                       hit.children[c].role == AOS_ROLE_WORM_SEGMENT ||
                                       hit.children[c].role == AOS_ROLE_WHITE_DRAGON_PART ||
                                       hit.children[c].role == AOS_ROLE_MANTLE_ORBIT ||
                                       hit.children[c].role == AOS_ROLE_MANTLE_GHOST
                                   ? (int)i
                                   : -1,
                    .enemy = hit.children[c]};
            if (hit.wake_spawner && e->spawner >= 0) {
                /* sub_080993EC: a waiting Mimic (state 0, step 0) wakes. */
                AosEnemy *mimic = &room->entities[e->spawner].enemy;
                if (mimic->state == 0 && mimic->step == 0) mimic->step++;
            }
            if (hit.spawn && room->entity_count < MAX_ENTITIES) {
                AriaEntity *child = &room->entities[room->entity_count];
                *child = (AriaEntity){.kind = ARIA_KIND_ENEMY, .id = e->id, .x = hit.spawn_x,
                                      .y = hit.spawn_y, .param1 = hit.spawn_param1,
                                      .spawned = true, .spawner = (int)i};
                /* Merman's hidden producer writes parameter 0 = 1 on its
                 * children, selecting their 60-frame submerged wait. Ghost
                 * producer children keep both parameters at zero. */
                int16_t child_param0 = e->id == AOS_ENEMY_MERMAN ? 1 : 0;
                if (aos_enemy_create(&child->enemy, (uint8_t)e->id, hit.spawn_x, hit.spawn_y,
                                     child_param0,
                                     hit.spawn_param1,
                                     soma, layer, kind, combat->stats[e->id]))
                    room->entity_count++;
            }
            if (hit.enemy_hit)
                printf("%s hit: %d damage%s\n", enemy_names[e->id], hit.enemy_damage,
                       hit.killed ? ", killed" : "");
            if (hit.soma_hit) {
                /* sub_08021654: Mimic's soul (0x13260 bit 0x40000). */
                if (combat->progress) aos_yellow_soul_hit(combat->progress, hit.soma_damage);
                printf("Soma hit: %d damage, HP %d/%d\n", hit.soma_damage, soma->hp, soma->max_hp);
            }
            continue;
        }
        if (!door) continue;
        if (!e->spawned) {
            if (e->x < cam_x - 80 || e->x > cam_x + 320) continue;
            e->spawned = true;
            aos_door_create(&e->door, layer, e->x, e->y, soma, cam_x, input);
            continue;
        }
        int played = aos_door_update(&e->door, layer, soma, input, cam_x, cam_y);
        if (played) sound = played;
    }
    /* The blink pattern 0x08118CE0, shared by every enemy kind. */
    const uint8_t *blink = NULL;
    for (int id = 0; id < ENEMY_KINDS && !blink; ++id)
        if (combat->kinds[id]) blink = combat->kinds[id]->blink;
    /* sub_0801CFB0 (its gEwramData + 0x13260 mode, y - 0x10, is not
     * modelled). */
    int16_t top = (int16_t)((soma->y >> 16) - ((soma->flags & AOS_FLAG_CROUCH) ? 0x16 : 0x22));
    const AosEffectWorld world = {layer, cam_x, cam_y, aos_random, soma->x, top,
                                  soma->y};
    /* sub_08045350: while the new-soul entity's +0xD stays <= 0x2F, one
     * type-0x49 sparkle spawns per frame at the player plus a random
     * byte-angle's sine/cosine * 48, drifting with velocity offset >> 4
     * and acceleration -(offset * 5) >> 4 in the soul kind's palette. */
    if (combat->soul_storm) {
        combat->soul_storm--;
        if (room->effect_count < MAX_EFFECTS) {
            uint32_t angle = (uint32_t)(aos_random() & 0xFFu) << 8;
            int32_t sx = aos_sine(angle + 0x4000), sy = aos_sine(angle);
            room->effects[room->effect_count++] = (AosEffect){
                .kind = (uint8_t)(AOS_EFFECT_SOUL_STORM0 + combat->soul_storm_kind),
                .direct = true,
                .x = soma->x + sx * 48, .y = soma->y + sy * 48,
                .vx = sx * 3, .vy = sy * 3,
                .ax = -(sx * 20) / 16, .ay = -(sy * 20) / 16};
        }
    }
    /* sub_080455BC/sub_08045454: the level-up display ticks in the draw
     * pass; each odd counter <= 0x27 bursts a sub_08045270 sparkle —
     * storm art on OBJ row 6 (AOS_EFFECT_SOUL_STORM1), launched from
     * the player's position 6 pixels up with the radial velocity and
     * -v/64 acceleration of its creation. */
    if (aos_levelup_tick(&combat->levelup) && room->effect_count < MAX_EFFECTS) {
        int32_t v[4];
        aos_levelup_sparkle_velocity((uint8_t)(aos_random() & 0xFFu), aos_sine, v);
        room->effects[room->effect_count++] = (AosEffect){
            .kind = AOS_EFFECT_SOUL_STORM1, .direct = true,
            .x = soma->x, .y = soma->y - 0x60000,
            .vx = v[0], .vy = v[1], .ax = v[2], .ay = v[3]};
    }
    size_t kept = 0;
    for (size_t i = 0; i < room->effect_count; ++i)
        if (aos_effect_update(&room->effects[i], combat->effects[room->effects[i].kind], blink,
                              &world))
            room->effects[kept++] = room->effects[i];
    room->effect_count = kept;
    kept = 0;
    for (size_t i = 0; i < room->damage_popup_count; ++i)
        if (aos_damage_popup_update(&room->damage_popups[i]))
            room->damage_popups[kept++] = room->damage_popups[i];
    room->damage_popup_count = kept;
    /* Pickups (sub_08043DD0), then the player's collisions with them
     * (sub_080441F8 through their type-0xB box). */
    kept = 0;
    const size_t pickup_source_count = room->pickup_count;
    AosPickup released_souls[MAX_PICKUPS];
    size_t released_count = 0;
    for (size_t i = 0; i < pickup_source_count; ++i) {
        AosPickup *pickup = &room->pickups[i];
        if (pickup->soul_container) {
            AosSoulContainerSpark spark;
            /* The native update emits its child before testing the weapon
             * collision, so the eighth frame can contain both a sparkle and
             * the released soul. */
            aos_pickup_soul_container_update(pickup, &spark, aos_random);
            if (spark.spawn && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = (uint8_t)(AOS_EFFECT_CONTAINER_SPARK0 + spark.soul_type),
                    .direct = true, .x = spark.x, .y = spark.y, .vx = spark.vx,
                    .vy = spark.vy, .ay = spark.ay};
            int wx, wy, ww, wh;
            bool weapon_hit = combat->weapon && combat->weapon_frames &&
                              aos_weapon_hitbox(combat->weapon, soma, combat->weapon_frames,
                                                &wx, &wy, &ww, &wh) &&
                              ww > 1 && wh > 1;
            AosRect target = aos_entity_rect(aos_soul_container_box, pickup->x >> 16,
                                              pickup->y >> 16, false, false);
            if (weapon_hit && !pickup->container_released &&
                !(wx + ww - 1 < target.x1 || target.x2 < wx || wy + wh - 1 < target.y1 ||
                  target.y2 < wy) && pickup_source_count + released_count < MAX_PICKUPS &&
                aos_pickup_release_placed_soul(pickup, &released_souls[released_count], soma->x,
                                               soma->y, aos_random)) {
                released_count++;
                aos_audio_play_song(0xA2);
            }
            if (!pickup->removed) room->pickups[kept++] = *pickup;
            continue;
        }
        if (pickup->type >= AOS_PICKUP_ITEM && pickup->type <= AOS_PICKUP_ARMOR &&
            combat->item_icons && pickup->variant < 59)
            aos_pickup_icon_bind(pickup, &room->pickup_icons,
                                 combat->item_icons[(pickup->type - AOS_PICKUP_ITEM) * 59u +
                                                    pickup->variant]);
        bool soul = pickup->type >= AOS_PICKUP_SOUL;
        /* Souls move before their collision pass (sub_080446D4); money and
         * items test it first (sub_08043DD0). */
        if (soul) aos_pickup_soul_move(pickup, soma->x, soma->y);
        AosRect body = aos_player_rect(soma->hurtbox, soma->x >> 16, soma->y >> 16,
                                       soma->facing_left, false);
        bool touching = !pickup->removed && combat->progress &&
                        aos_rect_hits_box(body, aos_pickup_box_of(pickup), pickup->x >> 16,
                                          pickup->y >> 16, false, false);
        if (touching && pickup->type == AOS_PICKUP_HEART) {
            aos_pickup_collect_heart(pickup, combat->progress);
            aos_audio_play_song(aos_pickup_collection_sound(pickup));
            printf("Collected an MP heart (MP %d)\n", combat->progress->mp);
        } else if (touching && pickup->type == AOS_PICKUP_MONEY && combat->money) {
            uint32_t text = aos_pickup_collect_money(pickup, combat->money,
                                                     &combat->progress->gold);
            aos_pickup_commit_collection(pickup, &aria_pickup_state);
            if (combat->hud && combat->hud_font && pickup->variant < AOS_MONEY_KINDS) {
                char name[24];
                snprintf(name, sizeof name, "%u GOLD", combat->money[pickup->variant].value);
                aos_hud_show_item(combat->hud, combat->hud_font, name);
            }
            aos_audio_play_song(aos_pickup_collection_sound(pickup));
            printf("Collected %u gold (total %u, message %u)\n",
                   combat->money[pickup->variant].value, combat->progress->gold, text);
        } else if (touching && pickup->type >= AOS_PICKUP_ITEM &&
                   pickup->type <= AOS_PICKUP_ARMOR) {
            static const char *const kinds[] = {"item", "weapon", "armor"};
            bool refusal_sound;
            if (aos_pickup_collect_item(pickup, combat->progress, &refusal_sound) ==
                AOS_COLLECT_TAKEN) {
                aos_pickup_commit_collection(pickup, &aria_pickup_state);
                if (combat->recent_items)
                    aos_pickup_recent_record(combat->recent_items, pickup->type,
                                             (uint8_t)pickup->variant);
                const char *name = pickup_notice_name(pickup->type, pickup->variant);
                if (combat->hud && combat->hud_font && name)
                    aos_hud_show_item(combat->hud, combat->hud_font, name);
                printf("Collected %s %u\n", kinds[pickup->type - AOS_PICKUP_ITEM],
                       pickup->variant);
                aos_audio_play_song(aos_pickup_collection_sound(pickup));
            } else if (refusal_sound) {
                aos_audio_play_song(AOS_PICKUP_SOUND_REFUSED);
                printf("Cannot carry more of %s %u\n", kinds[pickup->type - AOS_PICKUP_ITEM],
                       pickup->variant);
            }
        } else if (touching && soul) {
            static const char *const colours[] = {"red", "blue", "yellow", "ability"};
            bool fresh = aos_pickup_collect_soul(pickup, combat->progress);
            aos_pickup_commit_collection(pickup, &aria_pickup_state);
            /* sub_08049E64 caches the soul for the Soul Set ordering on
             * every collection, new or repeated. */
            if (combat->recent_souls)
                aos_pickup_recent_soul_record(
                    combat->recent_souls,
                    (uint8_t)(pickup->type - AOS_PICKUP_SOUL), pickup->variant);
            printf("Collected %s %s soul %u (held %u, %u souls)\n", fresh ? "new" : "another",
                   colours[pickup->type - AOS_PICKUP_SOUL], pickup->variant,
                   aos_souls_total(combat->progress, pickup->type - AOS_PICKUP_SOUL,
                                   pickup->variant),
                   combat->progress->souls_collected);
            /* sub_0800E708(r6=soul index, r5=kind, r8): the obtained-soul
             * banner; sub_0800E540's description only joins it when the
             * soul is new (r8 = 1 from the GetSoulTotal == 0 branch,
             * sub_08045C34's path). The banner row picks the upper
             * layout when Soma's pixel y passes 0x70. */
            if (combat->soul_notice)
                combat->soul_notice(combat->soul_notice_ctx,
                                    (unsigned)(pickup->type - AOS_PICKUP_SOUL),
                                    pickup->variant, (soma->y >> 16) > 0x70,
                                    fresh);
            /* sub_08045C34/sub_08045654: a new soul starts the 48-frame
             * sparkle storm (sub_08045350) around Soma; the entity itself
             * is invisible (no drawFunc). Song 0xBE shares player 2 with
             * 0xBC in the song table, so the same-frame collection sound
             * replaces it natively too. */
            if (fresh) {
                combat->soul_storm = 48;
                combat->soul_storm_kind =
                    (uint8_t)(pickup->type - AOS_PICKUP_SOUL);
            }
            /* sub_08045BC8/sub_08045164: the soul-absorbed sparkle pinned
             * at the player's position plus (0, -0x100000), animation 12,
             * palette nibble 6. A repeated soul spawns it at once and its
             * first update — next frame's entity pass — requests song
             * 0xBF (deferred so it does not replace the collection's
             * 0xBC on the same player); a new soul natively waits for the
             * sub_08045C34 icon's 64th frame (not yet ported). */
            if (room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SOUL_HELD, .pinned = true,
                    .ax = 0, .ay = -0x100000};
            aos_audio_play_song(aos_pickup_collection_sound(pickup));
            if (!fresh) {
                deferred_sound = 0xBF;
                deferred_sound_pending = true;
            }
        }
        if (soul) {
            AosSoulTrailSpark trail;
            aos_pickup_soul_finish(pickup, aos_random, &trail);
            if (trail.spawn && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = (uint8_t)(AOS_EFFECT_CONTAINER_SPARK0 + trail.soul_type),
                    .direct = true, .x = trail.x, .y = trail.y, .vx = trail.vx,
                    .vy = trail.vy, .ax = trail.ax, .ay = trail.ay};
        }
        else if (pickup->small_heart) aos_pickup_small_heart_update(pickup, layer);
        else aos_pickup_update(pickup, layer);
        if (!pickup->removed)
            room->pickups[kept++] = *pickup;
        else
            aos_pickup_icon_unbind(pickup, &room->pickup_icons);
    }
    room->pickup_count = kept;
    for (size_t i = 0; i < released_count && room->pickup_count < MAX_PICKUPS; ++i)
        room->pickups[room->pickup_count++] = released_souls[i];
    return sound;
}

/* Doors use their native sprite (frame 0 or 5 by parameter 0) and palette
 * cycle; without the object library an outline marks the blocked area. */
static void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
                       long frame_count, int cam_x, int cam_y);

/* The sprite art of a pickup, or -1 (items). */
static int pickup_art(const AosPickup *pickup) {
    if (pickup->soul_container) return -1;
    if (pickup->type == AOS_PICKUP_HEART) return pickup->small_heart ? 7 : 8;
    if (pickup->type == AOS_PICKUP_MONEY) return pickup->variant < 7 ? pickup->variant : -1;
    if (pickup->type >= AOS_PICKUP_SOUL && pickup->type < AOS_PICKUP_SOUL + 4)
        return 9 + pickup->type - AOS_PICKUP_SOUL;
    return -1;
}

/* Object sequences of src/assets/aos/objects.c: WoodenDoor/style_N
 * (the door frame under each step of its palette cycle). */
typedef struct {
    AriaFrame frames[DOOR_STYLES][MAX_FRAMES];
    uint8_t ticks[DOOR_STYLES][MAX_FRAMES];
    int counts[DOOR_STYLES];
    struct {
        AriaFrame frames[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        uint8_t ticks[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        AosAnimDef defs[AOS_ENEMY_MAX_ANIMS];
        AosAnimSet set;
        AosEnemyKind kind;
        AosEnemyStats stats;
        AosEnemyDrops drops;
        bool loaded;
        int static_ids[ENEMY_STATIC_FRAMES];
        AriaFrame statics[ENEMY_STATIC_FRAMES];
        AosBox static_attack[64];       /* by sprite frame: static_attack rows */
        int static_count;
        /* Bestiary detail portrait (sub_0804CA18): the exported BMP path of
         * each frame, kept so the single menu frame can be decoded to RGBA
         * without holding every frame twice. */
        char *paths[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        char *static_paths[ENEMY_STATIC_FRAMES];
        int16_t portrait_x, portrait_y; /* enemy record + 0x1E / + 0x20 */
        const AriaFrame *portrait_frame;
        uint8_t *portrait;              /* RGBA pixels of the menu frame */
        int portrait_w, portrait_h;
        int8_t portrait_state;          /* 0 untried, 1 ready, -1 unavailable */
        bool portrait_flip;
    } enemies[ENEMY_KINDS], variants[VARIANT_ARTS], skills[SKILL_ARTS]; /* Enemy/skill_* */
    struct {
        AriaFrame frames[OBJECT_MAX_ANIMS][MAX_FRAMES];
        uint8_t ticks[OBJECT_MAX_ANIMS][MAX_FRAMES];
        AosAnimDef defs[OBJECT_MAX_ANIMS];
        AosAnimSet set;
        bool loaded;
        int id, param0; /* special_variants only: -1 id = free slot */
    } opening_actors[AOS_OPENING_ACTOR_COUNT], /* Enemy/opening_actor_N */
      special_objects[64],                     /* Object/special_NN */
      /* Object/special_NN_vP: param0-driven variants of SPECIAL objects
       * (ids 14 and 18 resolve their sprite through entity->param0). */
      special_variants[24];
    struct {
        AriaFrame frames[MAX_FRAMES];
        uint8_t ticks[MAX_FRAMES];
        AosAnimDef def;
        AosAnimSet set;
    } effects[AOS_EFFECT_KINDS],    /* Effect/<effect_names[k]>/anim_N */
      pickup_arts[PICKUP_ARTS];     /* Effect/<pickup_art_names[k]>/anim_N */
    struct {
        AriaFrame frames[MAX_FRAMES];
        uint8_t ticks[MAX_FRAMES];
        AosAnimDef def;
        AosAnimSet set;
    } damage[AOS_DAMAGE_POPUP_GLYPHS], damage_guard;
    AriaFrame icons[ITEM_ICONS];    /* Icon/i<n>/anim_0 */
    AriaFrame walls[AOS_WALL_ENTRIES]; /* Enemy/wall_<n>/frame_<f>: an entry's frame */
    uint8_t item_icon[3][59];       /* item, weapon, armor record -> icon */
    AosMoneyKind money[AOS_MONEY_KINDS];
    bool money_loaded;
} AriaObjects;

static void draw_pickup_art(SDL_Renderer *renderer, const AriaObjects *objects, int index,
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
static const AriaFrame *blade_frame(const __typeof__(((AriaObjects *)0)->skills[0]) *art, int id) {
    int index = static_index(art->static_ids, art->static_count, id);
    return index < 0 ? NULL : &art->statics[index];
}

/* enemy_frames.tsv (boxes, death blink, margins) and enemies.tsv (stats). */
static bool load_enemy_data(AriaObjects *objects) {
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
static bool load_yellow_souls(const char *path,
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
static bool load_blue_souls(const char *path, AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS]) {
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
static bool load_red_souls(const char *path, AosRedSoulRecord records[AOS_RED_SOUL_RECORDS]) {
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

static bool load_bursts(const char *path) {
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
static bool load_skill_throws(const char *path, AosThrowRecord records[AOS_THROW_RECORDS]) {
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
static void souls_from_save(AosStats *stats, const AosSaveData *save) {
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

static void runtime_to_save(AosSaveData *save, const AriaRoom *room,
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

static bool load_equipment(const char *path, AosEquipmentTables *tables) {
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

static bool load_item_use(const char *path, AosItemUseRecord items[AOS_ITEM_RECORDS]) {
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
    out->texture = SDL_CreateTextureFromSurface(renderer, surface);
    out->w = (float)surface->w;
    out->h = (float)surface->h;
    out->offset_x = ox;
    out->offset_y = oy;
    SDL_DestroySurface(surface);
    if (!out->texture) return false;
    SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
    return true;
}

static bool load_objects(const char *path, AriaObjects *objects, SDL_Renderer *renderer) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    for (int v = 0; v < 24; ++v) objects->special_variants[v].id = -1;
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
        if (sscanf(line, "Object/%23[^/]/anim_%d\t%d\t%d\t%d\t%d\t%511s", object_name,
                   &object_style, &frame, &ticks, &ox, &oy, frame_path) != 7)
            goto not_object;
        if (sscanf(object_name, "special_%d_v%d", &object_id, &object_param) < 1)
            goto not_object;
        if (object_param < 0 &&
            sscanf(object_name, "special_%d", &object_id) != 1)
            goto not_object;
        if (object_id < 0 || object_id >= 64 || object_style < 0 ||
            object_style >= OBJECT_MAX_ANIMS || frame < 0 ||
            frame >= MAX_FRAMES || ticks < 1 || ticks > 255) {
            ok = false;
            break;
        }
        {
            __typeof__(objects->special_objects[0]) *art = NULL;
            if (object_param >= 0) {
                for (int v = 0; v < 24; ++v)
                    if (objects->special_variants[v].id == object_id &&
                        objects->special_variants[v].param0 == object_param) {
                        art = &objects->special_variants[v];
                        break;
                    }
                if (!art)
                    for (int v = 0; v < 24; ++v)
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
        out->texture = SDL_CreateTextureFromSurface(renderer, surface);
        out->w = (float)surface->w;
        out->h = (float)surface->h;
        out->offset_x = ox;
        out->offset_y = oy;
        SDL_DestroySurface(surface);
        if (!out->texture) { ok = false; break; }
        SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
        objects->ticks[style][frame] = (uint8_t)ticks;
        objects->counts[style]++;
    }
    fclose(f);
    for (int id = 0; id < ENEMY_KINDS; ++id) objects->enemies[id].set.anims = objects->enemies[id].defs;
    for (int id = 0; id < 64; ++id)
        objects->special_objects[id].set.anims = objects->special_objects[id].defs;
    for (int v = 0; v < 24; ++v)
        objects->special_variants[v].set.anims = objects->special_variants[v].defs;
    for (int v = 0; v < VARIANT_ARTS; ++v) objects->variants[v].set.anims = objects->variants[v].defs;
    for (int k = 0; k < SKILL_ARTS; ++k) objects->skills[k].set.anims = objects->skills[k].defs;
    return ok;
}

static void free_objects(AriaObjects *objects) {
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
    for (int v = 0; v < 24; ++v)
        for (int i = 0; i < OBJECT_MAX_ANIMS; ++i)
            for (int j = 0; j < MAX_FRAMES; ++j)
                if (objects->special_variants[v].frames[i][j].texture)
                    SDL_DestroyTexture(objects->special_variants[v].frames[i][j].texture);
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

static void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
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
static void draw_walls(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
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

/* Enemies: sprites face left; mirrored ones are flipped around x. */
static void draw_enemies(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
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
        int ex = e->enemy.x >> 16, ey = e->enemy.y >> 16;
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
            (e->enemy.id == AOS_ENEMY_DRYAD && e->enemy.role == AOS_ROLE_ENEMY &&
             e->enemy.state == 3) ||
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
static void draw_special_objects(SDL_Renderer *renderer, const AriaRoom *room,
                                 const AriaObjects *objects, int cam_x, int cam_y,
                                 unsigned step) {
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL || e->id < 0 || e->id >= 64)
            continue;
        const __typeof__(objects->special_objects[0]) *art = NULL;
        /* Object0FCreate (id 15) selects the OBJ palette row param0 >> 8,
         * defaulting to 7; its variant suffix carries that row. Other
         * variant ids match the raw param0. */
        int variant_key = e->param0;
        if (e->id == 15) {
            variant_key = e->param0 >> 8;
            if (!variant_key) variant_key = 7;
        }
        for (int v = 0; v < 24; ++v)
            if (objects->special_variants[v].id == e->id &&
                objects->special_variants[v].param0 == variant_key &&
                objects->special_variants[v].loaded) {
                art = &objects->special_variants[v];
                break;
            }
        if (!art) art = &objects->special_objects[e->id];
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
static void draw_opening_actors(SDL_Renderer *renderer, const AriaObjects *objects,
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

static void opening_actor_durations(
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

static uint32_t opening_random(void *context) {
    (void)context;
    return aos_random();
}

static int32_t opening_sine(uint32_t angle, void *context) {
    (void)context;
    return aos_sine(angle);
}

static uint16_t opening_angle(int16_t x, int16_t y, void *context) {
    (void)context;
    return aos_arctan2(x, y);
}

static int opening_floor_depth(int32_t x, int32_t y, void *context) {
    const AosCollision *layer = context;
    return layer ? aos_floor_depth(layer, x, y, 0, false) : 0;
}

static int opening_wall_push(int32_t x, int32_t y, bool moving_left,
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

static void draw_effects(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
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
static void draw_levelup(SDL_Renderer *renderer, const AriaObjects *objects,
                         const AriaCombat *combat, const AosSoma *soma,
                         int cam_x, int cam_y) {
    if (!combat->levelup.active) return;
    const __typeof__(objects->effects[0]) *art = &objects->effects[AOS_EFFECT_LEVEL_UP];
    if (!art->def.count || !art->frames[0].texture) return;
    const AriaFrame *frame = &art->frames[0];
    /* The entity anchor: player x, player y - 0x20 (drawFunc
     * sub_08045454 writes the player record's + 0x42/+ 0x46 - 0x20). */
    float ax = (float)((soma->x >> 16) - cam_x);
    float ay = (float)(((soma->y >> 16) - 0x20) - cam_y);
    float scale = (float)aos_levelup_scale(combat->levelup.env_frame, aos_sine) / 65536.0f;
    if (scale <= 0.0f) return;
    /* The unscaled top-left is anchor + frame offset; the affine scale
     * grows it about the anchor, so the scaled offset is offset*scale. */
    SDL_FRect rect = {ax + (float)frame->offset_x * scale,
                      ay + (float)frame->offset_y * scale,
                      (float)frame->w * scale, (float)frame->h * scale};
    double degrees =
        (double)aos_levelup_angle(combat->levelup.env_frame, aos_sine) * 360.0 / 65536.0;
    /* The pivot is the entity anchor: (-offset * scale) from the
     * scaled top-left, in rect-relative units like the enemy spins. */
    SDL_FPoint pivot = {-(float)frame->offset_x * scale,
                        -(float)frame->offset_y * scale};
    SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, degrees,
                             &pivot, SDL_FLIP_NONE);
}

/* sub_0804301C: skip blank entries, drawing every visible glyph five
 * pixels after the previous one. The frame's native anchor still applies. */
static void draw_damage_popup(SDL_Renderer *renderer, const AriaObjects *objects,
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

static void free_library(AriaLibrary *lib) {
    for (int i = 0; i < MAX_ANIMS; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (lib->frames[i][j].texture) SDL_DestroyTexture(lib->frames[i][j].texture);
}

/* A floor pixel with 40 free pixels above it, searched from the room
 * centre outward; this is a test placement, not native spawn data. */
static bool find_spawn(const AosCollision *layer, int *out_x, int *out_y) {
    int width = layer->width_cells * 8, height = layer->height_cells * 8;
    for (int d = 0; d < width / 2; d += 4) {
        for (int side = -1; side <= 1; side += 2) {
            int x = width / 2 + side * d;
            int free_run = 0;
            for (int y = 0; y < height - 1; ++y) {
                if (aos_collision_cell(layer, x, y)) { free_run = 0; continue; }
                if (++free_run >= 40 && (aos_collision_cell(layer, x, y + 1) & 1)) {
                    *out_x = x;
                    *out_y = y;
                    return true;
                }
            }
        }
    }
    return false;
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

/* Debug menu (F1, or the gamepad chord): every item edits the running
 * engine's state; nothing here exists in the original game. */
enum {
    DEBUG_PAUSE = 1, DEBUG_STEP, DEBUG_HITBOXES, DEBUG_HP, DEBUG_MAX_HP, DEBUG_REFILL,
    DEBUG_ATK, DEBUG_DEF, DEBUG_MOVES, DEBUG_ENEMY, DEBUG_SPAWN, DEBUG_CLEAR,
    DEBUG_AREA, DEBUG_ROOM, DEBUG_TELEPORT,
};
static const char *const debug_move_labels[5] = {
    "Move: backdash", "Move: slide", "Move: mid-air jump", "Move: dive kick", "Move: high jump",
};
static const uint32_t debug_move_bits[5] = {
    AOS_MOVE_BACKDASH, AOS_MOVE_SLIDE, AOS_MOVE_AIR_JUMP, AOS_MOVE_DIVE_KICK, AOS_MOVE_HIGH_JUMP,
};

/* An exported room at a floor found by find_spawn; Soma keeps HP, moves,
 * weapon and facing, and restarts in his normal state. */
static bool debug_teleport(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
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
    free_room(room);
    *room = target;
    SDL_DestroyTexture(*background);
    *background = texture;
    printf("Debug teleport: room %d/%d at %d,%d\n", area, number, x, y);
    return true;
}

/* A ported enemy 48 pixels in front of Soma, created as a room record. */
static bool debug_spawn(AriaRoom *room, const AriaCombat *combat, const AosSoma *soma,
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

static bool draw_native_pause_hub(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_soul_set(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_equip(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_config(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_sleep(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_ability(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_item_use(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_enemy(SDL_Renderer *renderer, SDL_Texture *texture,
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

static bool draw_native_quick_map(SDL_Renderer *renderer, SDL_Texture *texture,
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
static void draw_soul_menu(SDL_Renderer *renderer, const AosSoulMenu *menu,
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

static void draw_debug(SDL_Renderer *renderer, const DebugMenu *menu, bool paused,
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

/* weapons.tsv of src/assets/aos/weapons.c: "none" is the unarmed record. */
static bool load_weapon(const char *path, const char *name, AosWeapon *weapon) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256];
    bool found = false;
    /* v3 adds the sound (record + 0x18); a v2 table has none. */
    static const char schema[] = "schema\tmetroidvania-aos-weapons-v";
    if (!fgets(line, sizeof line, f) || strncmp(line, schema, strlen(schema)) ||
        (line[strlen(schema)] != '2' && line[strlen(schema)] != '3')) {
        fclose(f);
        fprintf(stderr, "Not a weapon table: %s\n", path);
        return false;
    }
    while (!found && fgets(line, sizeof line, f)) {
        char key[16];
        unsigned item, flags, sound = 0;
        int cls, variant, a[5], interval;
        if (line[0] == '#' ||
            sscanf(line, "%15s\t%x\t%d\t%d\t%x\t%d\t%d\t%d\t%d\t%d\t%d\t%x", key, &item,
                   &cls, &variant, &flags, &a[0], &a[1], &a[2], &a[3], &a[4], &interval,
                   &sound) < 11 ||
            strcmp(key, name))
            continue;
        *weapon = (AosWeapon){(uint8_t)cls, (uint16_t)flags,
                              {(uint8_t)a[0], (uint8_t)a[1], (uint8_t)a[2], (uint8_t)a[3],
                               (uint8_t)a[4]}, (uint8_t)interval, (uint8_t)variant,
                              (uint16_t)sound};
        found = true;
    }
    fclose(f);
    if (!found) fprintf(stderr, "No weapon %s in %s\n", name, path);
    return found;
}

/* The weapon entity's sprite sequence and hitboxes (classes 0, 2, 3). */
typedef struct {
    AriaFrame frames[MAX_FRAMES];
    uint8_t ticks[MAX_FRAMES];
    AosHitbox hitboxes[MAX_FRAMES];
    AosAnimDef def;
    AosAnimSet set;
    AosWeaponFrames data;
    int count;
} AriaWeaponSprite;

static bool load_weapon_sprite(const char *name, AriaWeaponSprite *out, SDL_Renderer *renderer) {
    char key[32], line[1024];
    snprintf(key, sizeof key, "Weapon/%s\t", name);
    FILE *f = fopen(DEFAULT_WEAPON_SPRITES, "rb");
    if (!f) return false;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, strlen(key))) continue;
        char frame_path[512];
        int frame, ticks, ox, oy;
        if (sscanf(line + strlen(key), "%d\t%d\t%d\t%d\t%511s", &frame, &ticks, &ox, &oy,
                   frame_path) != 5 || frame != out->count || frame >= MAX_FRAMES)
            break;
        SDL_Surface *surface = SDL_LoadBMP(frame_path);
        if (!surface) break;
        AriaFrame *dst = &out->frames[frame];
        dst->texture = SDL_CreateTextureFromSurface(renderer, surface);
        dst->w = (float)surface->w;
        dst->h = (float)surface->h;
        dst->offset_x = ox;
        dst->offset_y = oy;
        SDL_DestroySurface(surface);
        if (!dst->texture) break;
        SDL_SetTextureScaleMode(dst->texture, SDL_SCALEMODE_NEAREST);
        out->ticks[frame] = (uint8_t)ticks;
        out->count++;
    }
    fclose(f);
    if (!out->count) return false;
    f = fopen(DEFAULT_WEAPON_FRAMES, "rb");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char weapon[16];
            int index, frame, ticks, hit, x, y, w, h;
            if (sscanf(line, "%15s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", weapon, &index, &frame,
                       &ticks, &hit, &x, &y, &w, &h) == 9 &&
                !strcmp(weapon, name) && index >= 0 && index < out->count)
                out->hitboxes[index] = (AosHitbox){(int8_t)x, (int8_t)y, (uint8_t)w, (uint8_t)h,
                                                   hit != 0};
        }
        fclose(f);
    }
    out->def = (AosAnimDef){(uint16_t)out->count, out->ticks};
    out->set = (AosAnimSet){&out->def, 1};
    out->data = (AosWeaponFrames){&out->set, out->hitboxes};
    return true;
}

static void free_weapon_sprite(AriaWeaponSprite *sprite) {
    for (int i = 0; i < sprite->count; ++i)
        if (sprite->frames[i].texture) SDL_DestroyTexture(sprite->frames[i].texture);
}

static bool resolve_opening_text(void *context, uint8_t id,
                                 const uint8_t **script,
                                 size_t *script_size) {
    return aos_intro_resources_entry(context, 1, id, script, script_size);
}

static bool rebuild_opening_text(const AosOpeningEvent *event,
                                 const AosIntroFont *font,
                                 AosIntroTextTiles *tiles) {
    aos_intro_text_tiles_clear(tiles);
    const char *speaker = aos_opening_event_speaker_name(event->speaker);
    /* Context B maps its row 6 (tiles 0xC0..0xCA) at screen (40,100).
     * Context A maps tiles 0x00..0x95 at screen (40,116). */
    if (!aos_intro_font_render_small_span(font, tiles, 0, 48, 88, 8,
                                          (const uint8_t *)speaker,
                                          strlen(speaker)))
        return false;
    for (unsigned line = 0; line < AOS_OPENING_LINES; ++line)
        if (!aos_intro_font_render_span(font, tiles, 0, line * 12,
                                        176, 8, event->glyphs[line],
                                        event->glyph_counts[line]))
            return false;
    return true;
}

/* sub_0800E708(0, 0, 1) selects text 0xE3 for Winged Skeleton and
 * sub_0800E540 selects 0x1E4 for its description. Decode those verified-ROM
 * scripts through the same bounded parser as dialogue; no localized string
 * is embedded in the runtime. */
static bool decode_opening_soul_entry(AosIntroResources *resources,
                                      uint16_t id, AosOpeningEvent *text) {
    const uint8_t *script = NULL;
    size_t script_size = 0;
    if (!aos_intro_resources_entry(resources, 1, id, &script, &script_size))
        return false;
    if (!aos_opening_event_init(text, script, script_size)) return false;
    aos_opening_event_set_resolver(text, resolve_opening_text, resources);
    for (unsigned frame = 0; frame < 1024 && text->active; ++frame)
        aos_opening_event_update(text, text->waiting ? AOS_KEY_A : 0);
    return text->complete && !text->malformed;
}

static size_t flatten_opening_text(const AosOpeningEvent *text, uint8_t *out,
                                   size_t capacity) {
    size_t count = 0;
    for (unsigned line = 0; line < AOS_OPENING_LINES; ++line)
        for (unsigned i = 0; i < text->glyph_counts[line] && count < capacity; ++i)
            out[count++] = text->glyphs[line][i];
    return count;
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

static void draw_opening_soul_notice(SDL_Renderer *renderer,
                                     const AosSoulNotice *notice,
                                     uint32_t frame) {
    for (unsigned y = 0; y < VIEW_H; ++y)
        for (unsigned x = 0; x < 240; ++x)
        {
            uint16_t color;
            if (!aos_soul_notice_pixel(notice, frame, x, y, &color)) continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u), 255);
            SDL_RenderPoint(renderer, (float)x, (float)y);
        }
}

static bool load_opening_portrait(SDL_Renderer *renderer, const FusionRom *rom,
                                  uint8_t portrait, SDL_Texture **texture,
                                  uint8_t *loaded_portrait) {
    if (!renderer || !rom || !texture || !loaded_portrait ||
        portrait >= AOS_PORTRAIT_COUNT)
        return false;
    if (*texture && *loaded_portrait == portrait) return true;
    if (*texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }
    AosPortraitImage image;
    char error[128];
    if (!aos_portrait_decode(rom, portrait, &image, error, sizeof error))
        return false;
    SDL_Texture *decoded = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC,
                                             AOS_PORTRAIT_WIDTH, AOS_PORTRAIT_HEIGHT);
    if (!decoded) return false;
    if (!SDL_UpdateTexture(decoded, NULL, image.rgba, AOS_PORTRAIT_WIDTH * 4)) {
        SDL_DestroyTexture(decoded);
        return false;
    }
    SDL_SetTextureBlendMode(decoded, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(decoded, SDL_SCALEMODE_NEAREST);
    *texture = decoded;
    *loaded_portrait = portrait;
    return true;
}

static void draw_opening_backdrop(SDL_Renderer *renderer,
                                  const AosOpeningEvent *event,
                                  const FusionRom *rom,
                                  SDL_Texture **portrait_texture,
                                  uint8_t *portrait_id) {
    if (!event->active) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    /* sub_08067804 selects brightness decrease and BLDY=8 inside WIN0.
     * BG1-3, OBJ and the backdrop are targets; BG0 (portrait and text) is
     * deliberately excluded. A half-black veil reproduces that operation. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_FRect panel = {AOS_OPENING_TEXT_PANEL_X,
                       AOS_OPENING_TEXT_PANEL_Y,
                       AOS_OPENING_TEXT_PANEL_WIDTH,
                       AOS_OPENING_TEXT_PANEL_HEIGHT};
    SDL_RenderFillRect(renderer, &panel);
    /* sub_080676BC draws the portrait as BG0, using the 12-byte descriptor
     * at 0x08526C78: tiles, palette row 13 and native metatile map. The
     * decoded texture is a full transparent 240x160 screen layer. Only the
     * native portrait resource is drawn; a decode failure is reported once
     * per portrait id instead of substituting gameplay sprites. */
    static uint8_t reported_portrait = 0xFF;
    bool native_portrait = load_opening_portrait(renderer, rom, event->portrait,
                                                 portrait_texture, portrait_id);
    if (!native_portrait && reported_portrait != event->portrait) {
        reported_portrait = event->portrait;
        fprintf(stderr, "Aria opening portrait 0x%02X unavailable: "
                "native portrait resource could not be decoded\n",
                event->portrait);
    }
    if (native_portrait && portrait_texture && *portrait_texture) {
        /* The decoded map already includes BG0VOFS. Crop its first four
         * output rows at composition time to match screenshots/dialogue.jpeg's
         * visible portrait anchor without altering the reusable resource. */
        SDL_FRect source = {0, 4, VIEW_W, VIEW_H - 4};
        SDL_FRect destination = {0, 0, VIEW_W, VIEW_H - 4};
        SDL_RenderTexture(renderer, *portrait_texture, &source, &destination);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_opening_text(SDL_Renderer *renderer,
                              const AosOpeningEvent *event,
                              const AosIntroTextTiles *tiles,
                              const AosWindowFrame *window,
                              uint32_t frame) {
    if (!event->active) return;
    if (!window) return;
    for (unsigned y = 88; y < 160; ++y)
        for (unsigned x = 8; x < 232; ++x) {
            uint16_t color;
            if (!aos_window_frame_dialogue_pixel(window, tiles, x, y, &color))
                continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u),
                                   255);
            SDL_RenderPoint(renderer, (float)x, (float)y);
        }
    if (!event->waiting) return;
    /* sub_08067A04 writes one of two 8x16 pairs at BG0 cells (27,18) and
     * (27,19). gEwramData bit 3 selects the pair every eight frames. */
    unsigned variant = (frame & 8u) ? 0u : 1u;
    for (unsigned y = 0; y < 16; ++y) {
        uint16_t entry = (uint16_t)(0xF0CE + variant + (y >= 8 ? 0x10 : 0));
        for (unsigned x = 0; x < 8; ++x) {
            uint16_t color;
            if (!aos_window_frame_entry_pixel(window, entry, x, y & 7u, &color))
                continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u),
                                   255);
            SDL_RenderPoint(renderer, AOS_OPENING_PROMPT_X + (float)x,
                            AOS_OPENING_PROMPT_Y + (float)y);
        }
    }
}

static void play_opening_cue(AosOpeningCue cue, AosRoomMusic *music) {
    switch (cue) {
    case AOS_OPENING_CUE_CONFRONTATION:
        /* sub_0805D410 state 2: MUS_16, Confrontation. */
        aos_room_music_change(music, 0x10);
        break;
    case AOS_OPENING_CUE_ENEMY_APPEARS:
        /* State 3's monster reveal. */
        aos_audio_play_song(0x1B4);
        break;
    case AOS_OPENING_CUE_ENEMY_ATTACKS:
        /* State 4 starts Arikado's first attack animation. */
        aos_audio_play_song(0x1DF);
        break;
    case AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN:
        /* State 4, 0x10 frames after Arikado's first attack ends. */
        aos_audio_play_song(0x1E0);
        break;
    case AOS_OPENING_CUE_ENEMY_DEFEATED:
        /* State 5 removes the Winged Skeleton at timer 0xD0. */
        aos_audio_play_song(0x6B);
        break;
    case AOS_OPENING_CUE_SOUL_ABSORBED:
        /* State 5: SE_0x197 followed by sub_0805B878(0x1040), which mutes
         * music player 0 while sub_0800E708 displays the obtained soul. */
        aos_audio_play_song(0x197);
        aos_audio_play_song(AOS_AUDIO_PLAYER_MUTE_BASE);
        break;
    case AOS_OPENING_CUE_SOUL_NOTICE_DISMISS:
        /* State 5's modal window advance sound. */
        aos_audio_play_song(0x132);
        break;
    case AOS_OPENING_CUE_AFTER_BATTLE:
        /* sub_0805D410 state 6: MUS_23, Premonition. */
        aos_room_music_change(music, 0x17);
        break;
    case AOS_OPENING_CUE_COMPLETE:
        /* sub_0805D410 state 7's one-shot sub_0805B878(0x1041). */
        aos_audio_play_song(0x1041);
        break;
    default:
        break;
    }
}

/* Apply the equipment menu's weapon choice to the same runtime objects used
 * at launch. A missing sprite keeps the record equipped but disables its
 * still-unported weapon entity instead of retaining the previous art. */
static bool reload_equipped_weapon(uint8_t item, AosSoma *soma,
                                   AriaWeaponSprite *sprite, bool *loaded,
                                   AosWeaponEntity *entity, AriaCombat *combat,
                                   SDL_Renderer *renderer) {
    char name[8];
    if (item == AOS_EQUIPMENT_NONE) snprintf(name, sizeof name, "none");
    else snprintf(name, sizeof name, "%u", item);
    AosWeapon weapon;
    if (!load_weapon(DEFAULT_WEAPONS, name, &weapon)) return false;
    free_weapon_sprite(sprite);
    *sprite = (AriaWeaponSprite){0};
    *entity = (AosWeaponEntity){0};
    soma->weapon = weapon;
    *loaded = weapon.weapon_class <= 4 && load_weapon_sprite(name, sprite, renderer);
    if (!*loaded) {
        free_weapon_sprite(sprite);
        *sprite = (AriaWeaponSprite){0};
    }
    combat->weapon = entity;
    combat->weapon_frames = *loaded ? &sprite->data : NULL;
    return true;
}

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s [--check] [--save slot.sav] [--save-target slot.sav]"
            " [--consume-save] [--rom aria.gba] [--library index.tsv]\n"
            "       [--spawn X Y] [--moves MASK]\n"
            "       [--weapon none|INDEX] [--red-soul N] [--blue-soul N] [--yellow-soul N] [--hitboxes] [--atk N] [--def N] [--hp N]\n"
            "       [--repeat N: release the capture buttons one frame in N]\n"
            "       [--capture out.bmp FRAMES BUTTONS] [--input-map map.txt] [--debug-menu]\n"
            "       [--debug-input MASK,MASK,...: menu key edges, one per captured frame]\n"
            "       [--play-song N: PlaySong at the start] [--audio-wav out.wav] [--mute]\n"
            "       (--area A --room R | room-folder)\n",
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
    unsigned long capture_buttons = 0;
    /* A new file owns no ability soul. In particular, Malphas' mid-air jump
     * must be collected before AOS_MOVE_AIR_JUMP becomes available. Saves and
     * the explicit --moves diagnostic override this zero mask below. */
    unsigned long moves = 0;
    int spawn_x = -1, spawn_y = -1;
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
        else if (!strcmp(argv[i], "--audit-transitions")) return audit_transitions();
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
    if (audio_wav_path && !wav_open(&wav, audio_wav_path))
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
    bool objects_ok = load_objects(DEFAULT_OBJECTS, &objects, renderer);
    bool enemies_ok = objects_ok && load_enemy_data(&objects);
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
        souls_from_save(&progress, &active_save);
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
    bool have_equipment = load_equipment(DEFAULT_EQUIPMENT, &equipment);
    static AosItemUseRecord item_use[AOS_ITEM_RECORDS];
    bool have_item_use = load_item_use(DEFAULT_ITEM_USE, item_use);
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
    bool have_red_souls = load_red_souls(DEFAULT_RED_SOULS, red_souls);
    static AosThrowRecord skill_throws[AOS_THROW_RECORDS];
    bool have_skill_throws = load_skill_throws(DEFAULT_SKILL_THROWS, skill_throws);
    if (!load_bursts(DEFAULT_BURSTS)) fprintf(stderr, "No Aria burst list: %s\n", DEFAULT_BURSTS);
    aos_enemy_set_burst_source(burst_list, burst_size);
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
                        .burst = burst_list, .burst_count = burst_size};
    soma.cast = cast_red_soul;
    soma.cast_context = &souls;
    soma.stats = &progress;
    soma.random = aos_random;      /* the random voices */
    static AosYellowSoulRecord yellow_souls[AOS_YELLOW_SOUL_RECORDS];
    bool have_yellow_souls = load_yellow_souls(DEFAULT_YELLOW_SOULS, yellow_souls);
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
    if (load_blue_souls(DEFAULT_BLUE_SOULS, blue_souls)) {
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
                wav_write(&wav, pcm, sizeof pcm);
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
                    runtime_to_save(&active_save, &room, &soma, &progress);
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
                            room.entities[k].id == ARIA_OBJECT_WOODEN_DOOR &&
                            room.entities[k].spawned && room.entities[k].door.holding;
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
            if (aos_room_outside(&layer, room.width_screens, room.height_screens, soma.x >> 16,
                                 soma.y >> 16)) {
                if (!take_exit(renderer, &room, &background, &soma, cam_x, cam_y)) goto cleanup;
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
                    runtime_to_save(&active_save, &room, &soma, &progress);
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
            update_entities(&room, &layer, &soma, cam_x, cam_y, &forced, &combat);
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
        draw_save_point(renderer, &room, cam_x, cam_y,
                        aos_save_room_player_inside(sx, sy), step);
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
        if (objects_ok) draw_levelup(renderer, &objects, &combat, &soma, cam_x, cam_y);
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
    wav_close(&wav);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&mp2k);
    gba_input_close(&gamepad);
    free_library(&library);
    free_weapon_sprite(&weapon_sprite);
    free_objects(&objects);
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
