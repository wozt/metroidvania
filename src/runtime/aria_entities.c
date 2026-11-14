/* SPDX-License-Identifier: GPL-3.0-only */
/* Entity-update cluster of the Aria of Sorrow room runtime, moved
 * verbatim out of aria_runtime.c (AGENTS.md rule 8). See
 * aria_entities.h for the cluster boundary and the shared state the
 * runtime side still owns. */
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
#include "aria_tables.h"
#include "aria_assets.h"
#include "aria_draw.h"
#include "aria_entities.h"
#include "aos/core/aos_input.h"
#include "fusion/assets/rom.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runtime state owned by aria_runtime.c that the entity pass reads;
 * declared here the same way aria_assets.c does. */
extern uint32_t aria_events[AOS_WALL_EVENT_WORDS];
extern AosPickupCollectionState aria_pickup_state;
/* Object1C's SaveData_SaveSlotToSram needs the live save and its
 * store path; the room loops publish them (same pattern as
 * object10_save_ctx, which stays runtime-private). */
extern AosSaveData *object1c_save_ctx;
extern const char *object1c_save_path;
/* The shared room dialogue the Object20 glue drives; the main loop
 * renders, steps and locks the same instance. */
extern AosRoomDialogue room_dialogue;

/* gEwramData +4/+6 whiteout counters for the Object20 param0-30
 * sub_080628AC sequence (sub_08064518's renderer output, +0x15,
 * stays -1: the renderer is unported so the sequence parks at its
 * documented whiteout case — no room record uses param0 30). */
static uint16_t object20_ewram_unk4;
static uint16_t object20_ewram_unk6;
/* gEwramData +8 RNG state + +0x7864 byte for the ported
 * sub_08064518 whiteout renderer (param0 30). The RNG state
 * round-trips through the story Out; +0x7864 has no other
 * consumer and stays 0. */
static uint32_t object20_wo_rng;
static uint8_t object20_wo_7864;
/* sub_0805B5E8 child-spawn probe for the param0-26 companion
 * (in->npc.spawn_child is a bare function pointer): filled from
 * the same entity-pool approximation as story.spawn14_ok each
 * time object20_fill_in runs. */
static bool object20_spawn_child_pool;
static bool object20_spawn_child_probe(void) {
    return object20_spawn_child_pool;
}

/* gEwramData + 0x4C4: the shared spin clock Object1B state-1 objects
 * ramp by 0x1000 a frame up to 0x13FFF. */
static int32_t aria_spin_clock;
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
/* Object14's injected engine queries (aos_object14.h): the
 * sub_08001E58 floor walk maps to aos_floor_depth with use_mode,
 * the sub_080020D8 point test to the cell's floor bit and the
 * sub_080020A0/sub_08001F3C wall pair to the bit-1 wall cell and
 * aos_collision_special. */
static int obj14_probe(void *ctx, int kind, int32_t a, int32_t b,
                       int32_t c) {
    const AosCollision *layer = ctx;
    switch (kind) {
    case AOS_OBJ14_PROBE_TILE:
        return aos_floor_depth(layer, a, b, (int)c, true);
    case AOS_OBJ14_PROBE_POINT:
        return (aos_collision_cell(layer, a, b) & 1) != 0;
    case AOS_OBJ14_PROBE_WALL_A:
        return (aos_collision_cell(layer, a, b) & 2) != 0;
    case AOS_OBJ14_PROBE_WALL_B:
        return aos_collision_special(layer, a, b);
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------
 * Object20 (SPECIAL entity id 32, native object 0x20) runtime wiring.
 *
 * aos_object20_create/aos_object20_update dispatch the entity +0x30
 * param0 to the ported dirs/story/npc_talk/shop variants. The shared
 * Object20Create gates and Object20Update prologue/tail read
 * gEwramData globals; the mapping below names each In field's runtime
 * source. Fields with no runtime equivalent are fed their neutral
 * value and documented here — never an arbitrary gameplay constant:
 *
 *   flag1327a    gEwramData + 0x1327A is Soma's current HP word
 *                (AosSoma.hp carries the same address): the create
 *                gate passes while HP > 0.
 *   flag42c      gEwramData + 0x42C is aria_pickup_state.global_flags
 *                (aos_pickup.h documents the same address).
 *   byte8e, byte13266, word60, byte41b, byte414
 *                gEwramData mode/pause bytes with no runtime channel;
 *                0 is the fresh-state value.
 *   word16        gEwramData + 0x16 is inputData.newInput — the GBA
 *                key mask newly pressed this frame (include/structs/
 *                ewram.h InputData). update_entities' new_input is
 *                the same word (aria_input.new_input /
 *                sim_new_input, AOS_KEY_* bits equal the GBA bits);
 *                forwarded verbatim. The npc_talk flag bit 1
 *                (word16 & 0x42 = KEY_B|KEY_UP edge) is what lets
 *                probe_inside reach the dialogue state.
 *   text_done    the shared room dialogue's last sub_08067A04 result.
 *   keep_5b754   sub_0805B754(e, arg) is a sprite-variant existence
 *                probe (sub_0806E0D0 over the 0x081186DC records);
 *                approximated true — the runtime draws the exported
 *                special_32 art generically and cannot evaluate the
 *                native graphics probe.
 *   a094_4       the + 0xA094 + 4 scroll word has no runtime channel
 *                (the camera is recomputed each frame); 0.
 *   a094_6/a/a0  gEwramData + 0xA094 + 6/+ 0xA is the camera position
 *                in room pixels (sub_0805CCE4 polls it against pixel
 *                thresholds); a094_0 is the shop's room-width record,
 *                approximated by room->width_screens.
 *   player_*     the player entity (+0x13110) fields: +0x42 screen X
 *                pixel, +0x40/+0x44 fine 16.16 position, +0x5C
 *                animation byte (approximated by soma.anim.id, the
 *                +0x6D selector — the runtime keeps no +0x5C), +0x58
 *                bit 0x40 facing (soma.facing_left).
 *
 * Variant probes:
 *   dirs flag12078_21..25 / story flag12078_26/27/2b and npc
 *   cond_5b864: sub_08012078(i) reads the unk_33C bitfield, which the
 *   runtime stores as aria_events (aos_wall_event). cond_5b864 is
 *   sub_0805B864 = sub_08012078(0x3A).
 *   story flags37e: gEwramData + 0x37E is aria_pickup_state.special.
 *   story unk2cb0: gUnk_03002CB0 has no runtime channel; 0.
 *   story spawn14_ok/spawn18_ok: the sub_0805B5E8 child-spawn probes,
 *   approximated by remaining entity slots.
 *   story rng_3e654/rng_32df0: RandomNumberGenerator draws (aos_random).
 *   story fade_done: sub_0803D408() — the runtime has no blocking
 *   room-fade channel for these sequences; true (no fade pending).
 *   story c328_ok: the param0-29 sub_0805C328 record machine has no
 *   runtime source; true keeps it running.
 *   dirs probe_6e140: the sub_0806E140 collision probe is unmodelled;
 *   0. dirs table_852682c: the ROM byte table is not committed; NULL
 *   reads as the table-end sentinel, matching the module contract.
 *   npc flag12078: bits 0..0x32 of the same aria_events bitfield.
 *   npc probe_inside: sub_08068AD4's box test approximated by the
 *   player screen position inside a 0x40x0x40 box centred on the
 *   entity's screen position.
 *   npc gate_4be: the gEwramData + 0x4BE pause word has no runtime
 *   channel; 0 (clear, the in-game value).
 *   npc picked_text: DEPRECATED upstream — the param0-27 +0x14
 *   pick runs the ported sub_08066308/sub_0806666C/sub_0806668C
 *   pickers on the committed 0x08118C85 table through guard_pick;
 *   the stored guard text id is still fed for legacy callers.
 *   npc word16: gEwramData + 0x16 is inputData.newInput; the
 *   (flags & 7)==7 gate in npc_flags_66094 needs bit1 =
 *   word16 & 0x42 (KEY_B | KEY_UP edge, new_input bits 0x02/0x40).
 *   AOS_KEY_B and AOS_KEY_UP match the native GBA bits, so the
 *   runtime forwards update_entities' new_input verbatim.
 *   npc rng/rng_bit: RandomNumberGenerator draws; rng_bit is RNG&1.
 *   npc spawn_child: NULL — the param0-26 companion's +0x18 child
 *   (update sub_08065C2C) is not spawned (documented gap).
 *   npc cam_x/cam_y: the +0x42/+0x46 park words for the param0-25
 *   follower spawn; cam_x/cam_y.
 *   npc ent_pos40: the entity +0x40 fine X at create; e->x << 16.
 *   shop input_busy: sub_080212C8() & 0x1E has no runtime channel; 0.
 *   shop probe: player +0x42 + A094[6] collapses to the player's
 *   room-pixel X.
 *   shop dir: sub_0805B814's direction pick approximated by the
 *   player/entity screen-X order.
 *   shop ent_left_of_player: the +0x42 screen-X compare.
 *   shop room_q3: the entity room-X record approximated by e->x >> 3.
 *   shop record_left: the shop record +0x40 has no runtime source;
 *   false.
 * ------------------------------------------------------------------ */

static bool object20_flag12078(int bit) {
    return aos_wall_event(aria_events, bit);
}

/* Build the shared + variant In block for one entity update. `pre`
 * carries the npc family's pre-update state byte so the window poll
 * approximation can read it (see the branch comment). */
static void object20_fill_in(AriaRoom *room, const AriaEntity *e,
                             const AosSoma *soma, int cam_x, int cam_y,
                             uint16_t new_input, AosObject20In *in) {
    memset(in, 0, sizeof *in);
    in->word16 = new_input;    /* gEwramData + 0x16 inputData.newInput */
    in->flag1327a = soma ? soma->hp : 0;
    in->flag42c = aria_pickup_state.global_flags;
    in->text_done = aos_room_dialogue_text_done(&room_dialogue);
    in->keep_5b754 = true;      /* sprite-variant probe approximation */
    in->a094_6 = (uint16_t)cam_x;
    in->a094_a = (uint16_t)cam_y;
    in->a094_0 = (uint16_t)room->width_screens;
    if (soma) {
        in->player_scr42 = (int32_t)(soma->x >> 16) - cam_x;
        in->player_pos40 = soma->x;
        in->player_pos44 = soma->y;
        in->player_anim5c = soma->anim.id;
        in->player_flags58 = soma->facing_left ? 0x40 : 0;
    }
    /* dirs probes (param0 4-8): the flag bytes and the unmodelled
     * collision probe / uncommitted ROM table stay neutral. */
    in->dirs.flag12078_21 = object20_flag12078(0x21) ? 1 : 0;
    in->dirs.flag12078_22 = object20_flag12078(0x22) ? 1 : 0;
    in->dirs.flag12078_23 = object20_flag12078(0x23) ? 1 : 0;
    in->dirs.flag12078_24 = object20_flag12078(0x24) ? 1 : 0;
    in->dirs.flag12078_25 = object20_flag12078(0x25) ? 1 : 0;
    /* story probes (param0 9/10/19/21-24/29-31). ret_6e0d0 is the
     * param0-19-style sprite-variant gate; a nonzero low byte keeps
     * the entity, matching keep_5b754's approximation (the
     * object20_actor_* records are exported). The param0-30/31
     * cinematic probes (ret_628ac whiteout, ret_3d06c, ret_9e4,
     * rng_p1/rng_p2, ret_6e140) have no runtime channel and stay
     * neutral: update30 waits in state 1 and update31's floor
     * probe reports clear — no room record uses param0 30/31. */
    in->story.flag12078_26 = object20_flag12078(0x26) ? 1 : 0;
    in->story.flag12078_27 = object20_flag12078(0x27) ? 1 : 0;
    in->story.flag12078_29 = object20_flag12078(0x29) ? 1 : 0;
    in->story.flag12078_2b = object20_flag12078(0x2B) ? 1 : 0;
    in->story.flags37e = aria_pickup_state.special;
    in->story.spawn14_ok = room->entity_count < MAX_ENTITIES;
    in->story.spawn18_ok = room->entity_count < MAX_ENTITIES;
    in->story.rng_3e654 = (int)aos_random();
    in->story.rng_32df0 = (int32_t)aos_random();
    in->story.fade_done = true;
    in->story.c328_ok = true;
    in->story.ret_6e0d0 = 1;
    in->story.ewram_unk4 = object20_ewram_unk4;
    in->story.ewram_unk6 = object20_ewram_unk6;
    in->story.wo_field15 = -1;
    if (!object20_wo_rng) object20_wo_rng = aos_random();
    in->story.wo_rng = object20_wo_rng;
    in->story.wo_7864 = object20_wo_7864;
    in->story.field10 = 0;   /* entity +0x10 unmodelled */
    /* shop probes (param0 19/20). */
    in->shop.input_busy = 0;
    in->shop.probe = soma ? (int16_t)(soma->x >> 16) : 0;
    in->shop.a094_6 = (int16_t)cam_x;
    in->shop.a094_0 = (int16_t)room->width_screens;
    in->shop.dir = 0;
    if (soma) {
        int psx = (int)(soma->x >> 16) - cam_x;
        int esx = e->x - cam_x;
        in->shop.dir = psx < esx ? -1 : psx > esx ? 1 : 0;
        in->shop.ent_left_of_player = esx < psx;
    }
    in->shop.room_q3 = e->x >> 3;
    /* npc probes (param0 25-28). */
    for (int bit = 0; bit <= 0x32; ++bit)
        if (object20_flag12078(bit))
            in->npc.flag12078 |= 1ull << bit;
    if (soma) {
        int psx = (int)(soma->x >> 16) - cam_x;
        int psy = (int)(soma->y >> 16) - cam_y;
        int esx = e->x - cam_x;
        int esy = e->y - cam_y;
        in->npc.probe_inside =
            (uint16_t)(psx - esx + 0x20) <= 0x40 &&
            (uint16_t)(psy - esy + 0x20) <= 0x40;
    }
    in->npc.cond_5b864 = object20_flag12078(0x3A);
    in->npc.flags37e = aria_pickup_state.special;
    in->npc.picked_text = (uint8_t)e->object20.v.guard.text_id;
    in->npc.rng = aos_random();
    in->npc.rng_bit = (uint8_t)(aos_random() & 1);
    in->npc.rng_pick = aos_random();
    in->npc.rng_queue = aos_random();
    in->npc.cam_x = (uint16_t)cam_x;
    in->npc.cam_y = (uint16_t)cam_y;
    in->npc.ent_pos40 = (int32_t)e->x << 16;
    /* param0-26 companion child spawn probe (sub_0805B5E8(3)). */
    object20_spawn_child_pool = room->entity_count < MAX_ENTITIES;
    in->npc.spawn_child = object20_spawn_child_probe;
    /* slots probes (param0 0-2, 11-18). player_w4c is the player
     * +0x4C fine word — approximated by the fine Y velocity;
     * ret_roomx by the room-pixel X; probe_68ad4 reuses the same
     * 0x40-box approximation as npc.probe_inside; s16_4be has no
     * channel (0). */
    in->slots.player_w4c = soma ? soma->vy : 0;
    in->slots.ret_roomx = soma ? (int)((soma->x >> 16) - cam_x) : 0;
    in->slots.probe_68ad4 = in->npc.probe_inside;
    in->slots.s16_4be = 0;
}

/* Forward one update's Out effects. The window triple goes through
 * the shared room dialogue first (open -> poll -> close, the native
 * order inside a single update); the rest follow the sibling
 * branches' conventions. */
static void object20_apply_out(AriaEntity *e, const AosObject20Out *out,
                               AriaCombat *combat, AosForcedInput *input,
                               uint16_t new_input, int npc_pre_state) {
    /* sub_08067934/sub_08067A04/sub_08067898 triple. The npc_talk
     * families keep their window requests inside the variant state
     * structs instead of a normalized Out: the poll frames are the
     * state-2 updates the ported routines run sub_08067A04 in
     * (npc_loop/guard/talk_loop), and the guide's state-0x80 exit is
     * the window close (its native tail runs sub_08013960/
     * sub_08012048/sub_0805B66C — the close itself is approximated). */
    AosRoomDialogueRequest req = {-1, false, false};
    switch (e->object20.family) {
    case AOS_OBJECT20_FAMILY_DIRS:
        req.window_text = out->dirs.window_text;
        req.window_poll = out->dirs.window_poll;
        break;
    case AOS_OBJECT20_FAMILY_STORY:
        req.window_text = out->story.window_text;
        req.window_poll = out->story.window_poll;
        req.window_close = out->story.window_close;
        break;
    case AOS_OBJECT20_FAMILY_SHOP:
        req.window_text = out->shop.window_text;
        req.window_poll = out->shop.window_poll;
        break;
    case AOS_OBJECT20_FAMILY_SLOTS:
        req.window_text = out->slots.window_text;
        req.window_poll = out->slots.window_poll;
        req.window_close = out->slots.window_close;
        break;
    case AOS_OBJECT20_FAMILY_GUIDE: {
        const AosNpcGuide *g = &e->object20.v.guide;
        if (g->window_open) { req.window_text = 0x28; }
        req.window_poll = g->text_poll;
        req.window_close = g->state_80;
        break;
    }
    case AOS_OBJECT20_FAMILY_LOOP: {
        const AosNpcLoop *g = &e->object20.v.npc_loop;
        if (g->window_text) req.window_text = g->text_id;
        /* sub_08065DB8 state 2 polls sub_08067A04; the poll runs
         * the frame after the open, when the pre-update state
         * was still 2. */
        req.window_poll = npc_pre_state == 2;
        req.window_close = g->window_close;
        break;
    }
    case AOS_OBJECT20_FAMILY_GUARD: {
        const AosNpcGuard *g = &e->object20.v.guard;
        if (g->window_text) req.window_text = g->text_id;
        req.window_poll = npc_pre_state == 2;
        /* Gap: sub_08066094 state 2 loops back to state 1 which
         * re-opens the window — no sub_08067898 close is
         * reported, so the bridge re-open stands in. */
        break;
    }
    case AOS_OBJECT20_FAMILY_TALK_LOOP: {
        const AosNpcTalkLoop *g = &e->object20.v.talk_loop;
        if (g->window_text) req.window_text = (int)g->text_id;
        req.window_poll = npc_pre_state == 2;
        /* Gap: sub_080666D4's state-2 close has no Out flag. */
        break;
    }
    default:
        break;
    }
    aos_room_dialogue_apply(&room_dialogue, new_input, &req);

    /* gEwramData writes with runtime channels: +0x42C (shared and
     * per-family reports) and +0x37E bit 0. The +0x417/+0x427/+0x414/
     * +0x64/+0x60/+0x254CC/+0xA094+4/gUnk_03002CB0 writes and the
     * sub_0800EB04/sub_080235C8/sub_0802DFF0/sub_0806666C/
     * sub_0800EF6C/sub_08013960/sub_08013CF0/sub_08013D60/
     * sub_08013E18/sub_08013EEC/sub_080121E0/sub_0800ED24/
     * sub_0800C5A8/sub_0800C778/
     * sub_0803F2C8/sub_0803F17C/sub_0803E058/sub_0803E654/
     * sub_0805B5E8/sub_0805B66C/sub_08067804/sub_0803D18C/
     * sub_0803CDF0/sub_0803D270/sub_0803CED4/sub_0801D028/
     * sub_080127F0 engine calls, the DMA chains and the animation/
     * spawn requests have no runtime consumer and are reported in
     * the module headers; the shop transaction (sub_0800ED24) is
     * printed because it is gameplay-visible. */
    uint32_t set42c = out->set_42c | out->dirs.set_42c |
                      out->story.set_42c | out->shop.set_42c |
                      out->slots.set_42c;
    uint32_t clear42c = out->clear_42c | out->dirs.clear_42c |
                        out->story.clear_42c | out->shop.clear_42c |
                        out->slots.clear_42c;
    if (out->story.flags37e_clear1) aria_pickup_state.special &= ~1;
    if (out->story.wo_unk4 >= 0) object20_ewram_unk4 = (uint16_t)out->story.wo_unk4;
    if (out->story.wo_unk6 >= 0) object20_ewram_unk6 = (uint16_t)out->story.wo_unk6;
    if (out->story.wo_rng >= 0) object20_wo_rng = (uint32_t)out->story.wo_rng;
    if (out->story.wo_bars_drawn || out->story.wo_spawn_count ||
        out->story.wo_particle_count || out->story.wo_purge_547 ||
        out->story.wo_burst_63aec)
        printf("Object20 %d: whiteout render bars=%u spawns=%u "
               "particles=%u purge=%d/%d burst=%d (%d,%d) "
               "(bars->draw pass; spawn children run, "
               "carrier/burst callees unforwarded)\n",
               e->id, out->story.wo_bars_drawn,
               out->story.wo_spawn_count, out->story.wo_particle_count,
               out->story.wo_purge_547, out->story.wo_purge_flagged,
               out->story.wo_burst_63aec, out->story.wo_burst_x,
               out->story.wo_burst_y);
    /* The whiteout's per-scanline BgAffineSet records persist for
     * the draw pass: natively the hblank streams them into BG2's
     * affine register window; the host has no affine pipeline, so
     * draw_object20_wo_bars approximates them as white strips.
     * wo_bars_drawn is 0 or AOS_WO_BAR_COUNT and is rewritten every
     * update, so a frame without bars clears the persisted set and
     * the whiteout drops as soon as the cinematic stops emitting. */
    e->wo_bars_count = out->story.wo_bars_drawn > AOS_WO_BAR_COUNT
                           ? AOS_WO_BAR_COUNT : out->story.wo_bars_drawn;
    if (e->wo_bars_count)
        memcpy(e->wo_bars, out->story.wo_bars,
               e->wo_bars_count * sizeof e->wo_bars[0]);
    /* sub_08064518 spawn children (0x08118B34 update-function
     * table): instantiate one AosWoSpawnChild per
     * TABLE_118B34 event, capped at AOS_WO_MAX_EVENTS, then
     * advance the pool one frame. The children's RNG draws
     * share the object20_wo_rng gEwramData+8 channel with the
     * renderer. Carrier/burst Out events are consumed the same
     * frame by the burst pool below (sub_08063168/sub_080632B0/
     * sub_08065834/sub_080630E8 state machines); the carriers'
     * and particles' draw records (frames 0x08267EE4, graphics
     * 0x08260390) stay unexported — state and RNG run natively,
     * no sprites. */
    for (uint8_t s = 0; s < out->story.wo_spawn_count; ++s) {
        if (e->wo_child_count >= AOS_WO_MAX_EVENTS) break;
        if (aos_wo_spawn_child_init(
                &e->wo_children[e->wo_child_count],
                &out->story.wo_spawns[s]))
            e->wo_child_count++;
    }
    for (uint8_t ci = 0; ci < e->wo_child_count;) {
        AosWoSpawnChildIn cin = { object20_wo_rng };
        AosWoSpawnChildOut cout = {0};
        aos_wo_spawn_child_update(&e->wo_children[ci], &cin, &cout);
        object20_wo_rng = cout.rng;
        /* Same-frame event consumption, matching the native call
         * order: the child's fire event invokes the callee before
         * the next child updates. The burst In fields the host
         * cannot observe natively (entity-pool allocation, the
         * sub_0806E0D0 graphics/frame gate) are passed as the
         * success path — the pool bound is the host's analogue of
         * EntityCreateInRange failure. */
        if (cout.carrier_spawn || cout.burst_63168 ||
            cout.burst_632b0) {
            if (e->wo_burst_count < AOS_WO_MAX_EVENTS) {
                AosWoBurst *b =
                    &e->wo_bursts[e->wo_burst_count];
                memset(b, 0, sizeof *b);
                AosWoBurstIn bin = {
                    .rng = object20_wo_rng,
                    .carrier_alloc_ok = true,
                    .gate_6e0d0_ok = true,
                    .particle_alloc_fail = 0,
                };
                AosWoBurstOut bout = {0};
                if (cout.carrier_spawn)
                    b->carrier_valid = aos_wo_carrier_spawn(
                        &b->carrier, cout.carrier_x,
                        cout.carrier_y, cout.carrier_variant,
                        &bin, &bout);
                else if (cout.burst_63168)
                    aos_wo_burst_63168(b, cout.burst_x,
                                       cout.burst_y, &bin, &bout);
                else
                    aos_wo_burst_632b0(b, cout.burst_632b0_x,
                                       cout.burst_632b0_y,
                                       &bin, &bout);
                object20_wo_rng = bout.rng;
                if (b->carrier_valid || b->particle_count)
                    e->wo_burst_count++;
            }
        }
        if (cout.done) {
            e->wo_children[ci] =
                e->wo_children[--e->wo_child_count];
            continue;
        }
        ci++;
    }
    /* Advance the burst pool one frame: carrier sub_08065834 and
     * particle sub_080630E8. The particle's sub_08068AD4 probe is
     * a pure screen-rect containment test (+0x42/+0x46 inside
     * [-0x80,0x1F0] x [-0x40,0x120]), computed here from the
     * 16.16 accumulators instead of injected. */
    for (uint8_t bi = 0; bi < e->wo_burst_count;) {
        AosWoBurst *b = &e->wo_bursts[bi];
        if (b->carrier_valid) {
            AosWoBurstCarrierOut cout2;
            aos_wo_carrier_update(&b->carrier, &cout2);
            if (cout2.done) b->carrier_valid = false;
        }
        for (uint8_t pi = 0; pi < b->particle_count;) {
            AosWoBurstParticle *p = &b->particles[pi];
            int32_t px = p->field_40 >> 16;
            int32_t py = p->field_44 >> 16;
            AosWoBurstParticleIn pin = {
                .probe_inside = px >= -0x80 && px <= 0x1F0 &&
                                py >= -0x40 && py <= 0x120,
            };
            AosWoBurstParticleOut pout;
            aos_wo_particle_update(p, &pin, &pout);
            if (pout.done) {
                b->particles[pi] =
                    b->particles[--b->particle_count];
                continue;
            }
            pi++;
        }
        if (!b->carrier_valid && !b->particle_count) {
            *b = e->wo_bursts[--e->wo_burst_count];
            continue;
        }
        bi++;
    }
    if (set42c || clear42c) {
        aria_pickup_state.global_flags |= set42c;
        aria_pickup_state.global_flags &= ~clear42c;
    }

    /* Songs: PlaySong arguments from every family's report — the
     * shared shell's song, the dirs/story song fields and the
     * sub_0805B878 helpers (which are PlaySong itself, confirmed
     * in code_08050A3C.s). The shop song/jingle fields are
     * sub_08013CF0/sub_08012048 calls, reported below instead.
     * The update_entities `sound` return is ignored by both
     * callers, so the requests play directly like the wall
     * branch's report.sound. */
    const int *song_fields[] = {
        &out->song, &out->dirs.song, &out->story.song, &out->shop.b878,
        &out->dirs.b878, &out->story.b878,
    };
    for (size_t s = 0; s < sizeof song_fields / sizeof song_fields[0]; ++s) {
        int song = *song_fields[s];
        if (song >= 0) {
            aos_audio_play_song((uint16_t)song);
            printf("Object20 %d: song 0x%X\n", e->id, song);
        }
    }
    /* npc_talk family's own song/sfx pulses (the variant structs
     * carry them as bools naming the native call). */
    switch (e->object20.family) {
    case AOS_OBJECT20_FAMILY_GUIDE: {
        const AosNpcGuide *g = &e->object20.v.guide;
        if (g->song_1040) { aos_audio_play_song(0x1040);
                            printf("Object20 %d: song 0x1040\n", e->id); }
        if (g->player_x_clamped || g->player_facing_set ||
            g->flag_3b7_set || g->flag_3b7_clear || g->wrote_417)
            printf("Object20 %d: guide player/global write "
                   "(clamp=%d facing=%d 3b7=%d/%d 417=%d)\n",
                   e->id, g->player_x_clamped ? 1 : 0,
                   g->player_facing_set ? 1 : 0,
                   g->flag_3b7_set ? 1 : 0, g->flag_3b7_clear ? 1 : 0,
                   g->wrote_417 ? 1 : 0);
        break;
    }
    case AOS_OBJECT20_FAMILY_LOOP: {
        const AosNpcLoop *g = &e->object20.v.npc_loop;
        if (g->song_26) { aos_audio_play_song(0x26);
                          printf("Object20 %d: song 0x26\n", e->id); }
        if (g->sound_1041) { aos_audio_play_song(0x1041);
                             printf("Object20 %d: song 0x1041\n", e->id); }
        if (g->sfx_26 || g->ev_1e || g->wrote_254cc || g->froze_player ||
            g->armed_42c_0 || g->cleared_42c_200 || g->facing_mirror)
            printf("Object20 %d: loop npc effect pulse\n", e->id);
        break;
    }
    case AOS_OBJECT20_FAMILY_GUARD: {
        const AosNpcGuard *g = &e->object20.v.guard;
        if (g->song_26) { aos_audio_play_song(0x26);
                          printf("Object20 %d: song 0x26\n", e->id); }
        if (g->song_1b5) { aos_audio_play_song(0x1B5);
                           printf("Object20 %d: song 0x1B5\n", e->id); }
        if (g->sound_1040) { aos_audio_play_song(0x1040);
                             printf("Object20 %d: song 0x1040\n", e->id); }
        if (g->sound_1041) { aos_audio_play_song(0x1041);
                             printf("Object20 %d: song 0x1041\n", e->id); }
        if (g->sfx_26_entry || g->ev_1d || g->wrote_254cc ||
            g->froze_player || g->armed_42c_200 ||
            g->cleared_42c_200 || g->facing_mirror || g->cleared_18)
            printf("Object20 %d: guard npc effect pulse\n", e->id);
        break;
    }
    case AOS_OBJECT20_FAMILY_TALK_LOOP: {
        const AosNpcTalkLoop *g = &e->object20.v.talk_loop;
        if (g->cleared_42c_200 || g->froze_player || g->armed_42c_1 ||
            g->mark_38)
            printf("Object20 %d: talk-loop npc effect pulse\n", e->id);
        break;
    }
    default:
        break;
    }
    /* Held/directional sound requests with no direct channel. */
    if (out->shop.sound_2b) printf("Object20 %d: held sound 0x2B\n", e->id);
    if (out->shop.release_sound_2b)
        printf("Object20 %d: release sound 0x2B\n", e->id);
    if (out->shop.transaction) {
        printf("Object20 %d: shop transaction arg2 0x%X (unforwarded)\n",
               e->id, (unsigned)out->shop.transaction_arg2);
    }
    if (out->story.ed24_a >= 0) {
        printf("Object20 %d: story transaction ed24(0x%X, 0x%X) "
               "(unforwarded)\n", e->id, out->story.ed24_a,
               (unsigned)out->story.ed24_b);
    }
    if (out->fade_out || out->story.fade_out || out->story.fade_3d18c ||
        out->story.fade_3cdf0 || out->slots.fade_count)
        printf("Object20 %d: fade request (unforwarded)\n", e->id);

    /* Slots family (param0 0-2, 11-18): PlaySong args play through
     * the shared channel; the fade pairs, sfx/call reports, player
     * writes and misc engine effects have no consumer yet. */
    for (unsigned si = 0; si < out->slots.playsong_count; ++si) {
        aos_audio_play_song(out->slots.playsong[si]);
        printf("Object20 %d: slots song 0x%X\n", e->id,
               out->slots.playsong[si]);
    }
    if (out->slots.sfx_13eec >= 0 || out->slots.call_13cf0 >= 0 ||
        out->slots.call_00ea8 >= 0 || out->slots.call_6666c ||
        out->slots.call_0eb04 || out->slots.call_235c8 ||
        out->slots.call_2dff0 || out->slots.call_67804 ||
        out->slots.unk2cb0_feff || out->slots.unk2cb0_or100 ||
        out->slots.unk7864_or2 || out->slots.write_90 ||
        out->slots.write_417 || out->slots.write_414 ||
        out->slots.player5a_count || out->slots.rec_53c_write ||
        out->slots.player_scr42_set || out->slots.player58_or_40 ||
        out->slots.player58_andnot_41 || out->slots.player_w48_zero ||
        out->slots.player_w50_zero || out->slots.player_w4c_min0)
        printf("Object20 %d: slots engine effects (unforwarded)\n",
               e->id);

    /* SetPlayerInput(enable, arg): the runtime's forced-input slot.
     * The second argument is approximated as the held mask (0 locks
     * the player, matching the freeze the talk sequences ask for). */
    int setplayerinput = out->setplayerinput >= 0 ? out->setplayerinput
                            : out->dirs.setplayerinput >= 0
                                ? out->dirs.setplayerinput
                            : out->story.setplayerinput >= 0
                                ? out->story.setplayerinput
                                : out->slots.setplayerinput;
    if (setplayerinput >= 0 && input) {
        input->active = true;
        input->held = (uint16_t)(out->setplayerinput >= 0
                                     ? out->setplayerinput_arg
                                     : out->dirs.setplayerinput >= 0
                                         ? out->dirs.setplayerinput_arg
                                         : out->story.setplayerinput >= 0
                                             ? out->story.setplayerinput_arg
                                             : out->slots.setplayerinput_arg);
    }

    /* Player-entity writes (story family): +0x40/+0x44 are 16.16
     * room pixels, +0x42 is a screen pixel. The runtime keeps no
     * player +0x58 byte (facing_left carries only bit 0x40 and the
     * and-not masks cannot be expressed), and the +0x40/+0x44
     * writes arrive as absolute 16.16 values whose screen-vs-room
     * space is ambiguous in the port — reported, not applied,
     * until a native comparison pins the semantics down. */
    const AosObject20StoryOut *so = &out->story;
    if (so->player_scr42_set || so->player_pos40_set ||
        so->player_pos44_set || so->player58_or_40 ||
        so->player58_or_20 || so->player58_andnot_41 ||
        so->player58_andnot_21 || so->player_w48_zero ||
        so->player_w4c_zero || so->player_w4a_zero ||
        so->player_w4e_zero || so->player_anim5c_inc)
        printf("Object20 %d: player write scr42=%d pos40=0x%X "
               "pos44=0x%X flags58 or40=%d or20=%d andnot41=%d "
               "andnot21=%d animinc=%d\n",
               e->id, so->player_scr42_set ? (int)so->player_scr42_val : -1,
               so->player_pos40_set ? (unsigned)so->player_pos40_val : 0u,
               so->player_pos44_set ? (unsigned)so->player_pos44_val : 0u,
               so->player58_or_40 ? 1 : 0, so->player58_or_20 ? 1 : 0,
               so->player58_andnot_41 ? 1 : 0,
               so->player58_andnot_21 ? 1 : 0,
               so->player_anim5c_inc ? 1 : 0);

    /* Release/delete requests: sub_0805B66C releases the entity back
     * to the pool and EntityDelete removes it; both retire the record
     * for this room visit. */
    bool npc_released = false;
    switch (e->object20.family) {
    case AOS_OBJECT20_FAMILY_LOOP:
        npc_released = e->object20.v.npc_loop.release_0;
        break;
    case AOS_OBJECT20_FAMILY_GUARD:
        npc_released = e->object20.v.guard.release_0;
        break;
    case AOS_OBJECT20_FAMILY_TALK_LOOP:
        npc_released = e->object20.v.talk_loop.release_0;
        break;
    case AOS_OBJECT20_FAMILY_GUIDE:
        npc_released = e->object20.v.guide.release;
        break;
    default:
        break;
    }
    if (out->deleted || out->npc_deleted || npc_released ||
        out->shop.release || out->shop.frames_release ||
        out->story.release_arg >= 0 || out->dirs.release_arg >= 0 ||
        out->slots.release_arg >= 0) {
        e->object20.deleted = true;
    }

    /* The ported variants report each sub_0803F2C8 sequence pick in
     * their family's Out anim list; the last pick of the call is the
     * entity's +0x6D animation for the draw pass. The animation
     * engine itself (sub_0803F17C frame stepping) is not ported, so
     * e->object20_anim only selects the sequence — frames advance by
     * wall clock like the sibling SPECIAL objects. The npc_talk
     * families report no sequence picks and keep the create-time
     * default. */
    {
        int anim = -1;
        if (out->story.anim_count)
            anim = out->story.anim_a[out->story.anim_count - 1];
        if (out->dirs.anim_count)
            anim = out->dirs.anim_a[out->dirs.anim_count - 1];
        if (out->shop.anim_a >= 0)
            anim = out->shop.anim_a;
        if (anim >= 0)
            e->object20_anim = anim;
    }
    (void)combat;
}

/* sub_0800F4F8/sub_0800F5BC's record scan: a record whose flags byte
 * has any of the low nibble set (the exported unk_7 & 0xF) is handed
 * to sub_0800F1FC unconditionally at room load; the rest wait for the
 * camera window — [cam_x - 80, cam_x + 320] on x, or [cam_y - 80,
 * cam_y + 240] on y for rooms taller than wide (sub_0800F5BC; the
 * native bgInfo yPos +0x30 adjustment for unk_60.unk_8E_4 is not
 * modelled). Kind-1 (enemy) records never reach this helper: they
 * all spawn unconditionally in sub_0800F92C. */
static bool entity_spawn_due(const AriaEntity *e, const AriaRoom *room,
                             int cam_x, int cam_y) {
    if (e->flags & 0xF) return true;
    if (room->height_screens > room->width_screens)
        return e->y >= cam_y - 80 && e->y <= cam_y + 240;
    return e->x >= cam_x - 80 && e->x <= cam_x + 320;
}

/* Object1C children engine probes: sub_080009E4 is the shared
 * angle->sine lookup (aos_sine) and the spark spawn RNG is the
 * shared aos_random stream. */
static int32_t object1c_trig(uint32_t angle, void *ctx) {
    (void)ctx;
    return aos_sine(angle);
}
static uint32_t object1c_rng(void *ctx) {
    (void)ctx;
    return aos_random();
}

/* Object1C child animation sequences — the table sub_0803F17C's
 * advance walks (injected through AosObject1CChildrenIn.anims).
 * Every child's sub_0803B924 call resolves the same frame descriptor
 * 0x08257964: twelve type-1 sequences (sub_0803EC34, 4-byte
 * {frame, ticks} entries — verified against the ROM dump) whose
 * sub-frames are contiguous flat frame indices, so anim a's entries
 * are base..base+count-1 with base the cumulative sub-frame count.
 * All four special_28_v<bank> exports share that one descriptor and
 * differ only in OBJ palette bank, so the first loaded variant
 * supplies the timing; defs[a].count / ticks[a][j] mirror the
 * native seq count/durations 1:1. Rebuilt on each objects load;
 * NULL clears (children then freeze on their spawn frame). */
static AosObject1CAnimTick object1c_anim_ticks[OBJECT_MAX_ANIMS * MAX_FRAMES];
static AosObject1CAnimSeq object1c_anim_seqs[OBJECT_MAX_ANIMS];
static unsigned object1c_anim_count;

void object1c_anim_bind(const AriaObjects *objects) {
    object1c_anim_count = 0;
    if (objects == NULL)
        return;
    const __typeof__(objects->special_variants[0]) *art = NULL;
    for (int v = 0; v < SPECIAL_VARIANTS; ++v)
        if (objects->special_variants[v].id == 28 &&
            objects->special_variants[v].loaded) {
            art = &objects->special_variants[v];
            break;
        }
    if (art == NULL)
        return;
    unsigned base = 0, total = 0;
    for (unsigned a = 0; a < OBJECT_MAX_ANIMS; ++a) {
        unsigned n = art->defs[a].count;
        AosObject1CAnimSeq *s = &object1c_anim_seqs[a];
        s->type = 1;                    /* 0x08257964: all type 1 */
        s->count = (uint16_t)n;
        s->ticks = NULL;
        if (n && total + n <= OBJECT_MAX_ANIMS * MAX_FRAMES) {
            s->ticks = &object1c_anim_ticks[total];
            for (unsigned j = 0; j < n; ++j) {
                object1c_anim_ticks[total].frame = (uint8_t)(base + j);
                object1c_anim_ticks[total].ticks = art->ticks[a][j];
                ++total;
            }
        } else {
            s->count = 0;
        }
        base += n;
    }
    /* The native +0xC table holds twelve sequences; anims past the
     * populated prefix stay count-0 (inert) — expose only the
     * prefix so +0x6D bounds match the native table. */
    object1c_anim_count = 0;
    for (unsigned a = 0; a < OBJECT_MAX_ANIMS; ++a)
        if (object1c_anim_seqs[a].count)
            object1c_anim_count = a + 1;
}

int update_entities(AriaRoom *room, AosCollision *layer, AosSoma *soma,
                           int cam_x, int cam_y, AosForcedInput *input, AriaCombat *combat,
                           uint16_t new_input) {
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
            if (!e->spawned && entity_spawn_due(e, room, cam_x, cam_y)) {
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
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
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
                /* sub_0800F92C hands every kind-1 (unk_5 == 1) record
                 * to sub_0800F1FC unconditionally at room load — no
                 * camera window, no flags check. The persistent-index
                 * bit sub_0800F1FC consults stays unmodelled, as
                 * before; removed enemies early-out of
                 * aos_enemy_update, so the whole-room spawn cannot
                 * flood the entity table (records are fixed at load)
                 * or keep dead enemies updating. */
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
                 e->enemy.role == AOS_ROLE_MANTLE_GHOST ||
                 e->enemy.role == AOS_ROLE_WAITER_COMPANION ||
                 e->enemy.role == AOS_ROLE_WAITER_TRAY ||
                 e->enemy.role == AOS_ROLE_COCKATRICE_SHADOW ||
                 e->enemy.role == AOS_ROLE_COCKATRICE_ROCK ||
                 e->enemy.role == AOS_ROLE_MANDRAGORA_BODY ||
                 e->enemy.role == AOS_ROLE_MANDRAGORA_TAIL ||
                 e->enemy.role == AOS_ROLE_MANDRAGORA_SEED ||
                 e->enemy.role == AOS_ROLE_DISC_BLADE ||
                 e->enemy.role == AOS_ROLE_DISC_SHARD ||
                 e->enemy.role == AOS_ROLE_DISC_GLOW ||
                 e->enemy.role == AOS_ROLE_QUEZLCOATL_SEGMENT ||
                 e->enemy.role == AOS_ROLE_QUEZLCOATL_GORE ||
                 e->enemy.role == AOS_ROLE_ALASTOR_BODY ||
                 e->enemy.role == AOS_ROLE_ALASTOR_SHOT ||
                 e->enemy.role == AOS_ROLE_MAN_LEVEL ||
                 e->enemy.role == AOS_ROLE_MAN_VOLLEY ||
                 e->enemy.role == AOS_ROLE_MAN_SHOT ||
                 e->enemy.role == AOS_ROLE_MAN_DEATHP ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_PART0 ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_PART1 ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_PART2 ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_SHOT ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_TRAIL ||
                 e->enemy.role == AOS_ROLE_GLADIATOR_GORE) &&
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
                                       hit.children[c].role == AOS_ROLE_MANTLE_GHOST ||
                                       hit.children[c].role == AOS_ROLE_WAITER_COMPANION ||
                                       hit.children[c].role == AOS_ROLE_WAITER_TRAY ||
                                       hit.children[c].role == AOS_ROLE_COCKATRICE_SHADOW ||
                                       hit.children[c].role == AOS_ROLE_COCKATRICE_ROCK ||
                                       hit.children[c].role == AOS_ROLE_MANDRAGORA_BODY ||
                                       hit.children[c].role == AOS_ROLE_MANDRAGORA_TAIL ||
                                       hit.children[c].role == AOS_ROLE_MANDRAGORA_SEED ||
                                       hit.children[c].role == AOS_ROLE_DISC_BLADE ||
                                       hit.children[c].role == AOS_ROLE_DISC_SHARD ||
                                       hit.children[c].role == AOS_ROLE_DISC_GLOW ||
                                       hit.children[c].role == AOS_ROLE_ALURA_SCYTHE ||
                                       hit.children[c].role == AOS_ROLE_ALURA_BLADE ||
                                       hit.children[c].role == AOS_ROLE_WITCH_COMPANION ||
                                       hit.children[c].role == AOS_ROLE_WITCH_ORBIT ||
                                       hit.children[c].role == AOS_ROLE_WITCH_BOLT ||
                                       hit.children[c].role == AOS_ROLE_WITCH_SPARK ||
                                       hit.children[c].role == AOS_ROLE_WITCH_TRAIL ||
                                       hit.children[c].role == AOS_ROLE_MUDMAN_BUBBLE ||
                                       hit.children[c].role == AOS_ROLE_MUDMAN_ARM ||
                                       hit.children[c].role == AOS_ROLE_QUEZLCOATL_SEGMENT ||
                                       hit.children[c].role == AOS_ROLE_QUEZLCOATL_GORE ||
                                       hit.children[c].role == AOS_ROLE_ALASTOR_BODY ||
                                       hit.children[c].role == AOS_ROLE_ALASTOR_SHOT ||
                                       hit.children[c].role == AOS_ROLE_MAN_LEVEL ||
                                       hit.children[c].role == AOS_ROLE_MAN_VOLLEY ||
                                       hit.children[c].role == AOS_ROLE_MAN_SHOT ||
                                       hit.children[c].role == AOS_ROLE_MAN_DEATHP ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_PART0 ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_PART1 ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_PART2 ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_SHOT ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_TRAIL ||
                                       hit.children[c].role == AOS_ROLE_GLADIATOR_GORE
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
        if (e->kind == ARIA_KIND_SPECIAL && (e->id == 10 || e->id == 11)) {
            /* Object0ACreate / Object0BCreate (0x0805045C / 0x080504C0):
             * event-gated enemy spawners. param0 indexes the 36-byte
             * records of 0x080E9644 — record + 0 is the enemy's create
             * (param0 IS the enemy id) and record + 4 its update, which
             * the create installs on the entity; + 0x30/+ 0x32 are
             * cleared so the enemy runs with param0 = param1 = 0 and
             * + 0x34 takes record + 0xC (not modelled). Object 0x0A
             * gates on the + 0x37E bit param1 (special), object 0x0B on
             * the + 0x33C bitfield bit param1 (aria_events). A failed
             * gate deletes the entity; a passed one turns it into the
             * enemy — here a spawned ENEMY entity takes its place. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool gate = e->id == 10
                    ? ((aria_pickup_state.special >> e->param1) & 1) != 0
                    : aos_wall_event(aria_events, e->param1);
                if (gate && e->param0 >= 0 && e->param0 < ENEMY_KINDS &&
                    combat->kinds[e->param0] &&
                    room->entity_count < MAX_ENTITIES) {
                    AriaEntity *child = &room->entities[room->entity_count++];
                    *child = (AriaEntity){.kind = ARIA_KIND_ENEMY,
                                          .id = e->param0, .x = e->x,
                                          .y = e->y, .persistent_index = -1,
                                          .param0 = 0, .param1 = 0,
                                          .spawner = -1};
                    child->spawned = aos_enemy_create(
                        &child->enemy, (uint8_t)e->param0, e->x, e->y, 0, 0,
                        soma, layer, combat->kinds[e->param0],
                        combat->stats[e->param0]);
                    if (child->spawned)
                        printf("Object%02X spawned enemy %d at %d,%d\n", e->id,
                               e->param0, e->x, e->y);
                    else
                        room->entity_count--;
                }
                continue;   /* the object slot is consumed either way */
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 1) {
            /* Object01Create / Object01Update: claims the + 0x1316C
             * slot once per room and spawns its static children; an
             * unclaimed object deletes itself on the first update. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object01_create(&e->object01, (int16_t)e->param0);
                continue;
            }
            if (e->object01.deleted) continue;
            aos_object01_update(&e->object01);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 3) {
            /* Object03Create / Object03Update: the escort director —
             * the column is always sealed at create; state 0 waits on
             * the + 0x13220/+ 0x4CC globals and the partner entities
             * (none modelled: cinematic false takes the direct
             * state-0 -> 2 path). */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object03_create(&e->object03, layer, e->param0,
                                    e->param1, e->x - cam_x, e->x, e->y);
                continue;
            }
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            aos_object03_update(&e->object03, layer, cam_x, cam_y,
                                e->x, e->y, ssx, ssy,
                                false, false, 0x4000, 0x4000);
            if (e->object03.force_zero || e->object03.forced) {
                input->active = true;
                input->held = e->object03.forced;
            }
            if (e->object03.song >= 0) {
                sound = e->object03.song;
                printf("Object03 %zu: song 0x%X\n", i, e->object03.song);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 4) {
            /* Object04Create / Object04Update: the directional sibling
             * of Object02 — same block column and forced walk, but the
             * approach probe is one-sided and cases 2/10 push opposite
             * ways with no + 0x42C hold. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                aos_object04_create(&e->object04, e->param0, e->param1,
                                    aria_pickup_state.special, e->x - cam_x,
                                    ssx, soma->vx);
                continue;
            }
            if (e->object04.deleted) continue;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            aos_object04_update(&e->object04, layer, cam_x, cam_y, e->x, e->y,
                                ssx, ssy, aria_pickup_state.special, aos_random);
            if (e->object04.force_zero || e->object04.forced) {
                input->active = true;
                input->held = e->object04.forced;
            }
            if (e->object04.zero_soma_vx) {
                soma->vx = 0;
                soma->friction = 0;
            }
            if (e->object04.song >= 0) {
                sound = e->object04.song;
                printf("Object04 %zu: song 0x%X\n", i, e->object04.song);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 5) {
            /* Object05Create / Object05Update: passive proximity
             * trigger — no blocks, no forced input, params unused. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                int32_t sx = e->x - cam_x, sy = e->y - cam_y;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
                aos_object05_create(&e->object05, sx, sy, ssx, ssy);
                continue;
            }
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            aos_object05_update(&e->object05, e->x - cam_x, e->y - cam_y,
                                ssx, ssy);
            if (e->object05.song >= 0) {
                sound = e->object05.song;
                printf("Object05 %zu: song 0x%X\n", i, e->object05.song);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 6) {
            /* Object06Create / Object06Update: the cinematic trigger —
             * state-0 particle idle, the 0x111 song + snapshot on the
             * probe, then the forced-walk release gated on the + 0x4CC
             * bit 6 cinematic flag (not modelled: passed false). */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                int32_t sx = e->x - cam_x, sy = e->y - cam_y;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
                aos_object06_create(&e->object06, layer, e->param0,
                                    e->param1, aria_pickup_state.special,
                                    sx, sy, ssx, ssy, e->x, e->y);
                continue;
            }
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            aos_object06_update(&e->object06, layer, cam_x, cam_y,
                                e->x, e->y, ssx, ssy,
                                (uint16_t)soma->flags,
                                aria_pickup_state.special, false, aos_random);
            if (e->object06.force_zero || e->object06.forced) {
                input->active = true;
                input->held = e->object06.forced;
            }
            if (e->object06.song >= 0) {
                sound = e->object06.song;
                printf("Object06 %zu: song 0x%X\n", i, e->object06.song);
            }
            if ((e->object06.fx_drift || e->object06.fx_burst) &&
                room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = true,
                    .x = e->object06.fx_x << 16,
                    .y = e->object06.fx_y << 16,
                    .vy = -0x1000};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 7) {
            /* Object07Create / Object07Update: the ambient-mote object.
             * The parent has no sprite; param0 dispatches child spawns
             * (ambient motes + anchored children), the BG-anim cycler
             * (param0 1) or global writes (param0 3/4). Child sprites
             * draw through special_07_v100/v101 in the special-object
             * pass. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object07_create(&e->object07, e->param0, e->param1,
                                    cam_x, cam_y, aos_random);
                continue;
            }
            aos_object07_update(&e->object07, cam_x, cam_y,
                                cam_x << 16, cam_y << 16, aos_random);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 12) {
            /* Object0CCreate / Object0CUpdate (sUnk_084F0DF8[12] /
             * sUnk_084F0ED8[12]): the room-region trigger. The player
             * room position is Soma's fixed-point position minus the
             * camera; the + 0x10 bit-30 gate is unmodelled and passed
             * as set while Soma is active. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object0c_create(&e->object0c, e->param0);
                continue;
            }
            if (soma) {
                aos_object0c_update(&e->object0c,
                                    (uint16_t)(soma->x >> 16),
                                    (uint16_t)(soma->y >> 16),
                                    true, false,
                                    soma->x, soma->y, soma->state);
                if (e->object0c.heavy_set)
                    soma->flags |= AOS_FLAG_HEAVY;
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 15) {
            /* Object0FCreate / Object0FUpdate (sUnk_084F0DF8[15] /
             * sUnk_084F0ED8[15]): the destructible prop. The
             * sub_08068AD4 probe becomes the player-near window, the
             * sub_08045F9C attack-box match the weapon hitbox overlap
             * with the entity box (documented approximation). */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object0f_create(&e->object0f, e->param0);
                continue;
            }
            if (e->object0f.finished) continue;
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near = (uint16_t)(ssx - osx + 0x10) <= 0x130 &&
                        (uint16_t)(ssy - osy + 0x20) <= 0xC0;
            int wx, wy, ww, wh;
            bool hit = combat->weapon && combat->weapon_frames &&
                       aos_weapon_hitbox(combat->weapon, soma,
                                         combat->weapon_frames,
                                         &wx, &wy, &ww, &wh) &&
                       ww > 1 && wh > 1 &&
                       !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                         wy + wh - 1 < e->y - 16 || e->y < wy);
            aos_object0f_update(&e->object0f, near, hit,
                                ((int32_t)e->x) << 16, ((int32_t)e->y) << 16,
                                aos_random);
            if (e->object0f.song >= 0) {
                sound = e->object0f.song;
                printf("Object0F %zu: song 0x%X\n", i, e->object0f.song);
            }
            if (e->object0f.particle && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = true,
                    .x = e->object0f.particle_x,
                    .y = e->object0f.particle_y};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 13) {
            /* Object0DCreate / Object0DUpdate (sUnk_084F0DF8[13] /
             * sUnk_084F0ED8[13]): the swinging object. The + 0x13190
             * focus and the sub_080421AC attack match are approximated
             * by a proximity window and the weapon hitbox overlap. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object0d_create(&e->object0d, e->param0, e->param1,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool focused = (uint16_t)(ssx - osx + 0x80) <= 0x100 &&
                           (uint16_t)(ssy - osy + 0x40) <= 0x80;
            int wx, wy, ww, wh;
            bool attacked = combat->weapon && combat->weapon_frames &&
                            aos_weapon_hitbox(combat->weapon, soma,
                                              combat->weapon_frames,
                                              &wx, &wy, &ww, &wh) &&
                            ww > 1 && wh > 1 &&
                            !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                              wy + wh - 1 < e->y - 16 || e->y < wy);
            aos_object0d_update(&e->object0d, (int16_t)osy, focused,
                                attacked, soma->x < ((int32_t)e->x) << 16);
            if (e->object0d.song >= 0) {
                sound = e->object0d.song;
                printf("Object0D %zu: song 0x%X\n", i, e->object0d.song);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 14) {
            /* Object0ECreate / Object0EUpdate (sUnk_084F0DF8[14] /
             * sUnk_084F0ED8[14]): the multi-variant breakable prop.
             * The sub_08068AD4 probe and the sub_08045F9C hit test
             * become the same window/overlap approximations Object0F
             * uses; the sub_08050A3C content children and the
             * sub_0806D5C0 gore burst are counted, not spawned. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object0e_create(&e->object0e, e->param0, e->param1,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            if (e->object0e.finished) continue;
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near = (uint16_t)(ssx - osx + 0x20) <= 0x120 &&
                        (uint16_t)(ssy - osy) <= 0x130;
            int wx, wy, ww, wh;
            bool hit = combat->weapon && combat->weapon_frames &&
                       aos_weapon_hitbox(combat->weapon, soma,
                                         combat->weapon_frames,
                                         &wx, &wy, &ww, &wh) &&
                       ww > 1 && wh > 1 &&
                       !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                         wy + wh - 1 < e->y - 16 || e->y < wy);
            aos_object0e_update(&e->object0e, near, hit,
                                ((int32_t)e->x) << 16,
                                ((int32_t)e->y) << 16);
            if (e->object0e.song >= 0) {
                sound = e->object0e.song;
                printf("Object0E %zu: song 0x%X\n", i, e->object0e.song);
            }
            for (int fx = 0; fx < e->object0e.content_fx +
                 e->object0e.gore_fx && room->effect_count < MAX_EFFECTS;
                 fx++) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = true,
                    .x = e->x, .y = e->y - 8};
            }
            e->object0e.content_fx = 0;
            e->object0e.gore_fx = 0;
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 16) {
            /* Object10Create / Object10Update (sUnk_084F0DF8[16] /
             * sUnk_084F0ED8[16]): the save point statue. Create's
             * sprite pick keys off the + 0x4D0 word — approximated
             * by param0 for the _vP art — and its 0x08526390 grant
             * scan runs against the portable inventory. The
             * + 0x42C bit-0x400 trigger becomes Up held inside a
             * small box; the SRAM slot sequence is reported while
             * the live save stays on the Object1C path. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object10_create(&e->object10, 0,
                                    (uint8_t)e->param0, 0, 0,
                                    object10_item_owned,
                                    NULL);
                continue;
            }
            int psx = (int)(soma->x >> 16), psy = (int)(soma->y >> 16);
            bool near = psx >= e->x - 16 && psx <= e->x + 16 &&
                        psy >= e->y - 24 && psy <= e->y + 8;
            uint8_t before = e->object10.state;
            aos_object10_update(&e->object10,
                                near && (input->held & AOS_KEY_UP) != 0);
            if (before == 1 && e->object10.state == 2)
                printf("Object10 %zu: save sequence committed (item 0x%X)\n",
                       i, e->object10.item_id);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 17) {
            /* Object11Create / Object11Update (sUnk_084F0DF8[17] /
             * sUnk_084F0ED8[17]): the save-point remover. Create
             * deletes the + 0x124C slot entity — the save point —
             * and spawns the invisible type-0x1A helper whose
             * sub_080512FC callback counts + 0x14 frames while the
             * + 0x42C bit-1 (B) hold lasts, capped at 0x34BBD. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object11_create(&e->object11);
                if (e->object11.removed_save)
                    room->room_flags &= (uint16_t)~AOS_ROOM_FLAG_SAVE;
                continue;
            }
            aos_object11_update(&e->object11,
                                (input->held & AOS_KEY_B) != 0);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 18) {
            /* Object12Create / Object12Update (sUnk_084F0DF8[18] /
             * sUnk_084F0ED8[18]): the param-driven sprite. The
             * bit-3 probe becomes a player-near window, the + 0x6C
             * bit-2 re-arm the weapon hitbox overlap. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object12_create(&e->object12, e->param0, e->param1,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near = (uint16_t)(ssx - osx + 0x80) <= 0x100 &&
                        (uint16_t)(ssy - osy + 0x40) <= 0xF0;
            int wx, wy, ww, wh;
            bool hit = combat->weapon && combat->weapon_frames &&
                       aos_weapon_hitbox(combat->weapon, soma,
                                         combat->weapon_frames,
                                         &wx, &wy, &ww, &wh) &&
                       ww > 1 && wh > 1 &&
                       !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                         wy + wh - 1 < e->y - 16 || e->y < wy);
            aos_object12_update(&e->object12, near, hit,
                                e->param1 == 1, aos_random);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 19) {
            /* Object13Update (sUnk_084F0ED8[19]; Object13Create's
             * sprite attach and eleven chain-link children are the
             * special_sprites coverage): the swing phase/amplitude
             * machine and its + 0x344 event trigger. The head
             * child's sub_0805170C strike tail is ported as
             * aos_object13_strike; the link physics of
             * sub_080515F0/sub_0805170C stays approximated. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object13_create(&e->object13, e->param0, e->param1,
                                    0, 0, 0);
                continue;
            }
            if (e->object13.deleted) continue;   /* native EntityDelete */
            aos_object13_update(&e->object13,
                                aos_wall_event(aria_events, e->param1));
            /* sub_0805170C's strike tail: a weapon hit on the bob
             * while the player is not above it pumps the parent
             * swing amplitude. The + 0x68AD4 box and the
             * sub_080421AC attack-slot match collapse to the
             * shared weapon hitbox overlap; player_left uses the
             * entity x as the bob anchor. */
            int swx, swy, sww, swh;
            if (combat->weapon && combat->weapon_frames &&
                aos_weapon_hitbox(combat->weapon, soma,
                                  combat->weapon_frames,
                                  &swx, &swy, &sww, &swh) &&
                sww > 1 && swh > 1 &&
                !(swx + sww - 1 < e->x - 8 || e->x + 8 < swx ||
                  swy + swh - 1 < e->y - 16 || e->y < swy) &&
                (int32_t)(soma->y >> 16) >= e->y - 16)
                aos_object13_strike(&e->object13,
                                    (int32_t)(soma->x >> 16) < e->x);
            if (e->object13.event_fired) {
                int word = (int16_t)e->param1 >> 5;
                if (word >= 0 && word < AOS_WALL_EVENT_WORDS)
                    aria_events[word] |= 1u << (e->param1 & 31);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 20) {
            /* Object14Create/Object14Update (sUnk_084F0DF8[20] /
             * sUnk_084F0ED8[20]): the param0-selected moving probe
             * object; the sprite triple exports as special_20.
             * The + 0x4BE gate, the + 0x14 collision writers and
             * the + 0x13190 self-link are approximated clear/false
             * (the player snap stays off), the palette allocator
             * is a fixed slot, the tile/wall queries map onto
             * aos_collision through obj14_probe and the six
             * sub_08051CC4 children collapse to counted sparkles. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object14_create(&e->object14, (int16_t)e->param0,
                                    (int32_t)e->x << 16,
                                    (int32_t)e->y << 16, true, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            AosObject14Env o14env = {
                .gate_4be = 0,
                .collide14 = 0,
                .linked_self = false,
                .player_x = soma ? soma->x : 0,
                .player_y = soma ? soma->y : 0,
                .room_ox = (uint16_t)cam_x,
                .room_oy = (uint16_t)cam_y,
                .rng_a = aos_random(),
                .rng_b = aos_random(),
                .palette_slot = 0,
                .probe = obj14_probe,
                .probe_ctx = layer,
            };
            if (!aos_object14_update(&e->object14, &o14env))
                e->spawned = false;
            if (e->object14.song >= 0) {
                sound = e->object14.song;
                printf("Object14 %zu: song 0x%X\n", i,
                       e->object14.song);
            }
            if (e->object14.song_stop >= 0)
                printf("Object14 %zu: song stop 0x%X\n", i,
                       e->object14.song_stop);
            for (unsigned c = 0; c < e->object14.child_count &&
                 room->effect_count < MAX_EFFECTS; ++c)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            /* Keep the draw anchor on the simulated position. */
            e->x = (int)(e->object14.pos_x >> 16);
            e->y = (int)(e->object14.pos_y >> 16);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 25) {
            /* Object19Update (sUnk_084F0ED8[25]; Object19Create's
             * sprite attach is the special_sprites coverage): the
             * four-leg patrol machine. The sub_0803E058 affine
             * rotation draw and the sub_080421AC attack-slot read
             * are approximated by the static animation. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object19_create(&e->object19, e->param0, e->param1,
                                    ((int32_t)e->x) << 16,
                                    ((int32_t)e->y) << 16, 0);
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near = (uint16_t)(ssx - osx + 0x10) <= 0xB0 &&
                        (uint16_t)(ssy - osy) <= 0x110;
            aos_object19_update(&e->object19, near);
            e->x = (int)(e->object19.fx >> 16);
            e->y = (int)(e->object19.fy >> 16);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 27) {
            /* Object1BUpdate (sUnk_084F0ED8[27]; Object1BCreate's
             * sprite attach is the special_sprites coverage): the
             * spin-synced object. The + 0x13190 focus reuses the
             * Object0D proximity approximation; the sub_0803C918
             * tile push collapses to the frame output. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object1b_create(&e->object1b);
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool focused = (uint16_t)(ssx - osx + 0x80) <= 0x100 &&
                           (uint16_t)(ssy - osy + 0x40) <= 0x80;
            aos_object1b_update(&e->object1b, focused, &aria_spin_clock);
            if (e->object1b.song >= 0) {
                sound = e->object1b.song;
                printf("Object1B %zu: song 0x%X\n", i, e->object1b.song);
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 21) {
            /* Object15Update (sUnk_084F0ED8[21]; Object15Create's
             * sprite attach and type-0x1B child stay under
             * special_sprites): the story cutscene director. The
             * probes become player-near windows, the + 0x42C input
             * lock the forced-input outputs, the fades and the
             * dialogue record are reported, not played. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                /* + 0x348 bit 0x8000000 has no runtime equivalent;
                 * the create-time event test stays clear. */
                aos_object15_create(&e->object15, e->param0, false);
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near1 = (uint16_t)(ssx - osx + 0x10) <= 0x60 &&
                         (uint16_t)(ssy - osy + 0x7F) <= 0x20;
            bool near2 = (uint16_t)(ssx - osx + 0x10) <= 0x70 &&
                         (uint16_t)(ssy - osy) <= 0x10;
            /* The + 0x1C bit 0x40 player flag and the + 0x1339A
             * direction word have no direct runtime fields; the
             * approximations keep the machine reachable. */
            aos_object15_update(&e->object15, near1, near2, true,
                                true, 0, soma->x < ((int32_t)e->x) << 16);
            if (e->object15.forced == 0)
                input->held = 0;
            else if (e->object15.forced == 1)
                input->held = e->object15.dir_word;
            (void)e->object15.fade;   /* sub_0803D18C/sub_0803CDF0 */
            (void)e->object15.dialogue;
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 22) {
            /* Object16Update (sUnk_084F0ED8[22]; Object16Create's
             * sprite attach and sub_0803CC70 record pick stay
             * under special_sprites): the boss-intro zoom
             * director. The + 0x42C flags, the probe and the x
             * compare are approximated; the BgAffineSet zoom is
             * reported on the object, songs/fades/dialogue are
             * outputs only. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object16_create(&e->object16, e->param0);
                continue;
            }
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            bool near = (uint16_t)(ssx - osx + 0x10) <= 0x70 &&
                        (uint16_t)(ssy - osy) <= 0x10;
            /* + 0x42C bit 0 has no runtime field: the player-near
             * window stands in for the external event that arms
             * the cutscene. */
            bool flag0 = near;
            aos_object16_update(&e->object16, flag0, near,
                                soma->x < ((int32_t)e->x) << 16,
                                aos_sine);
            if (e->object16.forced == 0)
                input->held = 0;
            else if (e->object16.forced == 1)
                input->held = e->object16.walk_dir;
            if (e->object16.song >= 0) {
                sound = e->object16.song;
                printf("Object16 %zu: song 0x%X\n", i, e->object16.song);
            }
            if (e->object16.particles && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 23) {
            /* Object17Create / Object17Update (sUnk_084F0DF8[23] /
             * sUnk_084F0ED8[23]): the vertical pan director. Create's
             * sprite attach stays under special_sprites (special_23);
             * the variant pick, position and nineteen counted
             * children run in the module. The A094 room fields pass
             * as 0 (runtime rooms carry no world offset), the
             * sub_08021304 gate is approximated clear, the + 0x1C
             * flags the held Up/Down approximation and the
             * sub_08068AD4 probe a player-near window. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object17_create(&e->object17,
                                    (int)(soma->y >> 16), 0, 0);
                continue;
            }
            int fsx = (int)(soma->x >> 16) - 0x20;
            int fsy = (int)(soma->y >> 16) - 0x20;
            bool near = (uint16_t)(e->x - fsx) <= 0x40 &&
                        (uint16_t)(e->y - fsy) <= 0x10;
            uint16_t flags = 0;
            if (input->held & AOS_KEY_UP) flags |= 0x80;
            if (input->held & AOS_KEY_DOWN) flags |= 0x40;
            aos_object17_update(&e->object17, false, near, flags,
                                0, 0);
            if (e->object17.locked)
                input->held = 0;
            if (e->object17.song >= 0) {
                sound = e->object17.song;
                printf("Object17 %zu: song 0x%X\n", i,
                       e->object17.song);
            }
            if (e->object17.song_stop >= 0)
                printf("Object17 %zu: song stop 0x%X\n", i,
                       e->object17.song_stop);
            if (e->object17.fade)
                printf("Object17 %zu: fade (sub_08011FC0)\n", i);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 24) {
            /* Object18Create / Object18Update (sUnk_084F0DF8[24] /
             * sUnk_084F0ED8[24]): the breakable scatter director.
             * Create's sprite attach stays under special_sprites
             * (special_24). The sub_08068AD4 probe is the entity's
             * own screen position inside the (-0x20, -0x20, 0x130,
             * 0xE0) rect, the sub_080421AC low16 & 0x1E gate the
             * weapon hitbox overlap, the gEwramData + 0x13266 byte
             * passes 0 (no runtime room flag), the eight
             * sub_08043F70 scatter and the sub_08043C10 heart
             * become real room pickups, and the sub_0805379C
             * ring, the sub_0806D5C0 gore and the type-0x1B
             * sub_080C24AD/0x080C2559 children are counted. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object18_create(&e->object18, e->param0);
                continue;
            }
            if (e->object18.finished) continue;
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            bool near = (uint16_t)(osx + 0x20) <= 0x130 &&
                        (uint16_t)(osy + 0x20) <= 0xE0;
            int wx, wy, ww, wh;
            bool attack = combat->weapon && combat->weapon_frames &&
                          aos_weapon_hitbox(combat->weapon, soma,
                                            combat->weapon_frames,
                                            &wx, &wy, &ww, &wh) &&
                          ww > 1 && wh > 1 &&
                          !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                            wy + wh - 1 < e->y - 16 || e->y < wy);
            aos_object18_update(&e->object18, near, attack, false,
                                e->x, e->y, aos_random);
            if (e->object18.song >= 0) {
                sound = e->object18.song;
                printf("Object18 %zu: song 0x%X\n", i,
                       e->object18.song);
            }
            for (int d = 0; d < e->object18.drop_count &&
                 room->pickup_count < MAX_PICKUPS; ++d) {
                const AosObject18Drop *drop = &e->object18.drops[d];
                room->pickups[room->pickup_count++] =
                    aos_pickup_create_money(combat->money, drop->variant,
                                            drop->drop_x, drop->drop_y);
            }
            if (e->object18.heart && room->pickup_count < MAX_PICKUPS)
                room->pickups[room->pickup_count++] =
                    aos_pickup_create_heart(true, false, e->x, e->y - 8);
            int fx = e->object18.ring_fx + e->object18.gore_fx +
                     e->object18.child_fx;
            for (int f = 0; f < fx && room->effect_count < MAX_EFFECTS;
                 ++f) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = true,
                    .x = e->x, .y = e->y - 8};
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 48) {
            /* Object30Create / Object30Update (sUnk_084F0DF8[48] /
             * sUnk_084F0ED8[48]): the ambient sparkle emitter.
             * The sub_08058FE8 sprite attach is the shared
             * Object2F art (special_47); the + 0xB latch reads
             * the gEwramData + 0x42C word through the pickup
             * collection state and each sub_08058D7C child is
             * approximated by a sparkle effect at a random
             * screen x over the camera. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object30_create(&e->object30, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object30_update(&e->object30,
                                aria_pickup_state.global_flags,
                                aos_random);
            if (e->object30.spawn_fx &&
                room->effect_count < MAX_EFFECTS) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = cam_x + (int)(aos_random() % 0xF0),
                    .y = cam_y};
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 49) {
            /* Object31Create / Object31Update (sUnk_084F0DF8[49] /
             * sUnk_084F0ED8[49]): the delayed-launch spinning
             * projectile. The sprite triple is the shared
             * special_47 bank; the + 0x37E bit-2 gate reads
             * aria_pickup_state.special, the + 0x42C bit-0x1000
             * trigger reads global_flags, the + 0x30 param is
             * param0, the sub_0806D128(3) probe gets the entity
             * screen pixels and the palette allocator is
             * approximated as always available. Each launch
             * spawns the type-0x49 fx child as a sparkle. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object31_create(&e->object31, (int16_t)e->param0,
                                    aria_pickup_state.special,
                                    (int)(aos_random() % 6), true,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int16_t sx = (int16_t)(e->x - cam_x);
            int16_t sy = (int16_t)(e->y - cam_y);
            if (!aos_object31_update(&e->object31, sx, sy,
                                     aria_pickup_state.global_flags))
                continue;
            if (e->object31.child_spawn &&
                room->effect_count < MAX_EFFECTS) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 51) {
            /* Object33Create / Object33Update (sUnk_084F0DF8[51] /
             * sUnk_084F0ED8[51]): the dual-burst ambient emitter.
             * The sprite triple exports as special_51; the + 0x30
             * param is param0 and each sub_08059928/sub_08059A8C
             * child burst is approximated by sparkle effects at
             * the entity position. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object33_create(&e->object33, (int16_t)e->param0,
                                    true, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object33_update(&e->object33, aos_random);
            unsigned bursts = (e->object33.burst_a ? 1u : 0u) +
                              (e->object33.burst_b ? 1u : 0u);
            for (unsigned b = 0; b < bursts &&
                 room->effect_count < MAX_EFFECTS; ++b)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x + (int)(aos_random() % 0x81) - 0x40,
                    .y = e->y + (int)(aos_random() % 0x41) - 0x20};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 34) {
            /* Object22Create / Object22Update (sUnk_084F0DF8[34] /
             * sUnk_084F0ED8[34]): the scripted elevator director.
             * The sprite triple exports as special_34; the + 0x13190
             * linked-entity check approximates as false (no linked
             * carrier in the runtime), the volley hit mask reads 0,
             * room fields pass 0 and each sub_08056F50 shot /
             * sub_08056D6C carrier becomes a sparkle effect. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object22_create(&e->object22, e->param1, 0, 0,
                                    0, 0, true, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object22_update(&e->object22, false, 0);
            if (e->object22.song >= 0)
                aos_audio_play_song((uint16_t)e->object22.song);
            if (e->object22.spawn_projectile &&
                room->effect_count < MAX_EFFECTS) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = (int)(e->object22.shot_x >> 16) + e->x - cam_x,
                    .y = (int)(e->object22.shot_y >> 16) + e->y - cam_y};
            }
            if (e->object22.spawn_carrier &&
                room->effect_count < MAX_EFFECTS) {
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            }
            if (e->object22.set_global_348)
                aria_pickup_state.global_flags |= 0x8000;
            if (e->object22.deactivate) e->spawned = false;
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 41) {
            /* Object29Create / Object29Update (sUnk_084F0DF8[41] /
             * sUnk_084F0ED8[41]): the ping-pong moving platform.
             * The + 0x30 param is param0 (threshold << 16), the
             * + 0x32 record selector is param1, the sprite
             * resolves through the special_42_v00-v03 variants
             * and room fields pass 0. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object29_create(&e->object29, (int16_t)e->param0,
                                    (int16_t)e->param1, 0, 0,
                                    e->x << 16, e->y << 16, true,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object29_update(&e->object29, 0, 0);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 50) {
            /* Object32Create / Object32Update (sUnk_084F0DF8[50] /
             * sUnk_084F0ED8[50]): the stationary cinematic
             * director. The sprite triple exports as special_50;
             * the + 0x13190 linked check and the + 0x13110 player
             * proximity are approximated (linked false, player
             * proximity from the soma distance) and the
             * sub_080596E0 children become counted effects. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object32_create(&e->object32, (int16_t)e->param0,
                                    (int16_t)e->param1, 0, 0,
                                    e->x << 16, e->y << 16, true,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int32_t psx = soma ? (int32_t)(soma->x >> 16) : 0x7FFFFFFF;
            int32_t psy = soma ? (int32_t)(soma->y >> 16) : 0x7FFFFFFF;
            int32_t ddx = psx - e->x, ddy = psy - e->y;
            if (ddx < 0) ddx = -ddx;
            if (ddy < 0) ddy = -ddy;
            bool near = ddx <= 0x1F && ddy <= 0x1F;
            aos_object32_update(&e->object32, false, near);
            if (e->object32.song >= 0)
                aos_audio_play_song((uint16_t)e->object32.song);
            for (unsigned c = 0; c < e->object32.child_count &&
                 room->effect_count < MAX_EFFECTS; ++c)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            /* object32.deactivate is the sub_08021278(1)
             * proximity-table registration, not a delete — the
             * entity stays spawned (resetting it here churned
             * create/update every frame near the player). */
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 53) {
            /* Object35Create / Object35Update (sUnk_084F1004[53] /
             * sUnk_084F1184[53]): the sequence actor. The sprite
             * triple exports as special_53; the + 0x344 global
             * bitfield and + 0x13190 link are approximated and the
             * sub_0806D9CC particles become counted effects. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object35_create(&e->object35, (int16_t)e->param0,
                                    (int16_t)e->param1, e->x << 16,
                                    e->y << 16, false, false, true,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object35_update(&e->object35, false, false);
            if (e->object35.song >= 0)
                aos_audio_play_song((uint16_t)e->object35.song);
            for (unsigned c = 0; c < e->object35.particle_count &&
                 room->effect_count < MAX_EFFECTS; ++c)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 55) {
            /* Object37Create / Object37Update (sUnk_084F10F8[55] /
             * sUnk_084F1278[55]): the proximity fade director.
             * The sprite triple exports as special_55; the + 0x40
             * x-axis probe uses the entity screen position, the
             * player + 0x58 bit 0x40 and the RNG rolls are
             * approximated. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                int32_t psx = soma ? (int32_t)(soma->x >> 16) : 0x7FFFFFFF;
                int32_t ddx = psx - e->x;
                if (ddx < 0) ddx = -ddx;
                aos_object37_create(&e->object37, e->x << 16,
                                    e->y << 16, ddx <= 0x2F,
                                    true, 0, true, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int32_t psx = soma ? (int32_t)(soma->x >> 16) : 0x7FFFFFFF;
            int32_t ddx = psx - e->x;
            if (ddx < 0) ddx = -ddx;
            aos_object37_update(&e->object37, ddx > 0x2F, false);
            if (e->object37.song >= 0)
                aos_audio_play_song((uint16_t)e->object37.song);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 29) {
            /* Object1DCreate (sUnk_084F0EA4[29]): the sum-indexed
             * display. Native sums the signed byte at + 4 (the
             * source screen X, s8) of each 0x10-stride record in the
             * current room node's + 0x18 table — gEwramData + 0x60 ->
             * + 0x28 -> + 0x18, count at + 0x1C — which is the room's
             * transition table (src/assets/aos/world.c reads the same
             * ROM layout: 16-byte entries ending at the room
             * descriptor). AriaRoom.transitions is that table, so the
             * faithful sum is the total of screen_x over
             * transition_count entries. */
            if (!e->spawned) {
                e->spawned = true;
                int sum = 0;
                for (size_t t = 0; t < room->transition_count; ++t)
                    sum += room->transitions[t].screen_x;
                bool deleted;
                aos_object1d_create(&e->object1d, (int16_t)e->param0,
                                    e->x, e->y, sum, &deleted);
                if (deleted) e->spawned = false;
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 31) {
            /* Object1FCreate / Object1FUpdate / sub_08055748
             * (sUnk_084F0F0C[31] / sUnk_084F108C[31] / entity
             * + 0): the opening gate and its nine-state think.
             * The sprite attach is the packed 0x082052F0
             * descriptor (LZ77 tiles 0x0863803C); its eleven
             * children draw through the special_31_v<bank>
             * exports (see draw_special_objects). Inputs:
             * the sub_08068AD4 probe is the player + 0x42/+ 0x46
             * screen position inside x [0x55,0x9E] and the
             * y {0x7F..0x81} band; the update gate is
             * inputData.heldInput & 0x40 (soma->held, the + 0x1C
             * player mask) with the + 0x64 game-mode stage == 1
             * folded to always-true (entities only run in-game)
             * and the probe; the think's + 0x42C bit 0x40 is
             * aria_pickup_state.global_flags bit 6, its
             * substate-0 gate inputData.newInput & 0x40 is
             * new_input, sub_08021304 is approximated done as
             * for Object17, and + 0x3F4 / the + 0x88/+ 0x3F8
             * compare stay clear. Outputs: + 0x42C writes fold
             * into global_flags, sub_08012048(0x39) arms
             * aria_events bit 0x39, the stage-5/7 sequences,
             * fades, display window and player + 0x5A writes
             * stay reported outputs. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object1f_create(&e->object1f, true, &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            int32_t gssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t gssy = (int32_t)(soma->y >> 16) - cam_y;
            bool gate_probe = gssx >= 0x55 && gssx <= 0x9E &&
                              (uint16_t)(gssy - 0x7F) <= 2;
            bool gate_held = gate_probe &&
                             (soma->held & AOS_KEY_UP) != 0;
            aos_object1f_update(&e->object1f, gate_held);
            aos_object1f_think(&e->object1f,
                               (aria_pickup_state.global_flags & 0x40) != 0,
                               (new_input & AOS_KEY_UP) != 0,
                               gate_probe, true, false, false);
            /* Children: sub_08055DF8 ring x9 + sub_08056130 flank
             * x2. record_state is the parent's own +0xA byte —
             * gEwramData + 0x4E4 + slot*132 IS entityData[slot]
             * and +0x11 propagates the parent's slot, so the
             * "record" the children poll is this entity's state
             * (aos_object1f_record_state). Flanks get the same
             * probe and the always-done poll approximation.
             * Child sounds and affine/anim op counts stay
             * unreported. */
            uint8_t o1f_record = aos_object1f_record_state(
                &e->object1f);
            for (int ci = 0; ci < 9; ++ci)
                aos_object1f_child_ring_update(
                    &e->object1f.children[ci], o1f_record,
                    aos_random(),
                    (aria_pickup_state.global_flags & 0x40) != 0);
            for (int ci = 9; ci < 11; ++ci)
                aos_object1f_child_flank_update(
                    &e->object1f.children[ci], o1f_record,
                    gate_probe, true);
            /* sub_08055DF8 case-8 tail: the idx65==0 child's
             * record_inc bumps entityData[slot]+0xA — the
             * parent's own state byte. */
            aos_object1f_record_commit(&e->object1f);
            if (e->object1f.event_39)
                aria_events[0x39 >> 5] |= 1u << (0x39 & 31);
            if (e->object1f.think_song >= 0) {
                aos_audio_play_song((uint16_t)e->object1f.think_song);
                printf("Object1F %zu: song 0x%X\n", i,
                       e->object1f.think_song);
            }
            if (e->object1f.event_39 && !e->object1f.event_39_reported) {
                e->object1f.event_39_reported = true;
                printf("Object1F %zu: story event 0x39 armed\n", i);
            }
            if (e->object1f.flags_42c_40)
                aria_pickup_state.global_flags |= 0x40;
            if (e->object1f.flags_42c_20000)
                aria_pickup_state.global_flags |= 0x20000;
            if (e->object1f.flags_42c_clr41)
                aria_pickup_state.global_flags &= ~0x41u;
            if (e->object1f.global_42c_bit0)
                aria_pickup_state.global_flags |= 1;
            if (e->object1f.seq_end)
                printf("Object1F %zu: sequence end "
                       "(sub_08010978 -> stage 5)\n", i);
            if (e->object1f.input_lock) {
                /* SetPlayerInput(1, 0): lock player input. */
                input->active = true;
                input->held = 0;
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 28) {
            /* Object1CCreate / Object1CUpdate (sUnk_084F0EA4[28] /
             * sUnk_084F0F8C[28]): the save point. Twenty kind-2
             * records (persistent_index 1) share the fixed anchor
             * + 0x42 = 0x78 / + 0x46 = 0x50 floor-snapped by
             * sub_0806AF98 — approximated by probing the collision
             * layer at that spot. The + 0x30..+ 0x33 sync bytes are
             * the record's param0/param1. The sub_08068AD4 probe is
             * the same screen-space box family as Object1F's but
             * with rect (0x68,0x80,0x20,0x37): the player screen
             * position inside it. buttons_16 is the held mask
             * (+0x16: Up 0x40, A/B bits 0-1) and buttons_14 the
             * new-input edge mask (+0x14: A bit 0, Right/Left
             * 0x10/0x20) — the edge/level split is documented but
             * unverified in aos_object1c.h. sub_08021304 is
             * approximated not-busy (Object17 precedent); the
             * sub_0803E654 affine slot has no host table, so slot 0
             * is reported. save_result feeds back the previous
             * frame's actual aos_save_store result (0 until the
             * first commit — SRAM writes are synchronous and
             * effectively infallible). The child routines
             * (sub_080544E8/0x08055070/0x0805514C/...) run in
             * aos_object1c_children_update below; only the
             * marker's drawFunc commits draw (the other kinds'
             * frame-record exports are unverified). */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                AosObject1CCreateIn cin = {0};
                AosObject1CCreateOut cout;
                cin.affine_slot = 0;    /* no host affine table */
                cin.palette_index = 0;  /* sub_0803B800 bank-0
                                         * fallback index */
                cin.param0 = (int16_t)e->param0;
                cin.param1 = (int16_t)e->param1;
                /* sub_0806AF98 floor snap at the fixed anchor:
                 * first floor cell at/below y = 0x50 under x = 0x78
                 * on the collision layer (same probe shape as
                 * find_spawn). */
                int gy = 0x50;
                for (int yy = 0x50; yy < 0x1000; ++yy)
                    if (aos_collision_cell(layer, 0x78, yy) & 1) {
                        gy = yy;
                        break;
                    }
                cin.ground_y = (int16_t)gy;
                cin.entity_index = (int)i;
                /* Native create spawns the marker child and the
                 * two live sparkle records unconditionally
                 * (EntityCreateInRange + sub_08054E38). */
                cin.marker_spawned = true;
                cin.sparkle_spawned = 2;
                cin.frame_record = 0;
                if (!aos_object1c_create(&e->object1c, &cin, &cout)) {
                    e->spawned = false;
                    continue;
                }
                if (cout.deleted) e->spawned = false;
                continue;
            }
            AosObject1CIn in1c = {0};
            AosObject1COut out1c;
            int32_t px = (int32_t)(soma->x >> 16) - cam_x;
            int32_t py = (int32_t)(soma->y >> 16) - cam_y;
            in1c.player_x = (int16_t)px;
            in1c.player_y = (int16_t)py;
            in1c.buttons_16 = soma->held;
            in1c.buttons_14 = new_input;
            in1c.player_busy = false;   /* sub_08021304 ~ done */
            in1c.save_slot = 0;         /* +0x428: slot 0 until a
                                         * slot-select UI exists */
            in1c.save_result = e->object1c_save_result;
            aos_object1c_update(&e->object1c, &in1c, &out1c);
            if (out1c.song >= 0) {
                sound = out1c.song;
                printf("Object1C %zu: song 0x%X\n", i, out1c.song);
            }
            if (out1c.forced_keys >= 0) {
                /* SetPlayerInput(1, 0x40): lock player input while
                 * the save flow owns the pad. */
                input->active = true;
                input->held = (uint16_t)out1c.forced_keys;
            }
            aria_pickup_state.global_flags |= out1c.flags42c_set;
            aria_pickup_state.global_flags &= ~out1c.flags42c_clear;
            if (out1c.save_mode_set || out1c.save_mode_clear) {
                /* +0xA074 save-mode byte has no host consumer yet. */
                printf("Object1C %zu: +0xA074 %s0x%02X\n", i,
                       out1c.save_mode_set ? "set " : "clear ",
                       out1c.save_mode_set | out1c.save_mode_clear);
            }
            if (out1c.save_setup)
                printf("Object1C %zu: save screen setup "
                       "(sub_08054280) — UI not ported\n", i);
            if (out1c.save_block_refresh)
                printf("Object1C %zu: +0x60 save block refresh "
                       "(sub_08012688/0x080135C0)\n", i);
            if (out1c.save_write) {
                /* SaveData_SaveSlotToSram(+0x428): the portable
                 * store writes the live save; the result feeds the
                 * next frame's in->save_result. */
                char error[128];
                int result = -1;
                if (object1c_save_ctx && object1c_save_path)
                    result = aos_save_store(object1c_save_path,
                                            object1c_save_ctx, error,
                                            sizeof error) ? 0 : -1;
                else
                    result = 0;    /* no save context (e.g. --simulate):
                                    * report the commit, skip the file */
                e->object1c_save_result = result;
                printf("Object1C %zu: SaveData_SaveSlotToSram -> %d\n",
                       i, result);
            }
            if (out1c.beam_children)
                printf("Object1C %zu: %u beam children spawned "
                       "(sub_08054F74)\n", i, out1c.beam_children);
            if (out1c.save_committed)
                printf("Object1C %zu: save committed (state 0x14)\n", i);
            if (out1c.save_rejected)
                printf("Object1C %zu: save rejected (state 0)\n", i);
            /* Children updateFunc pass (sub_080544E8 drawFunc is
             * draw-side): runs after the parent like the native
             * slot order, writes the +0x30..+0x33 sync bytes back.
             * sub_0803F2C8 frame sets and the sub_0803F17C animation
             * advance run inside the module; the sequence table
             * comes from object1c_anim_bind (the loaded
             * special_28_v<bank> art — NULL freezes the children
             * on their spawn frame). */
            {
                AosObject1CChildrenIn cin2 = {0};
                AosObject1CChildrenOut cout2;
                cin2.trig = object1c_trig;
                cin2.focus_x = (int16_t)px;
                cin2.focus_y = (int16_t)py;
                cin2.rng = object1c_rng;
                cin2.anims = object1c_anim_seqs;
                cin2.anim_count = object1c_anim_count;
                aos_object1c_children_update(&e->object1c, &cin2,
                                              &cout2);
                /* +0x59 bit 3 marks children for post-draw
                 * deletion natively; the host deletes here. */
                for (unsigned ci = 0;
                     ci < e->object1c.child_count;) {
                    if (e->object1c.children[ci].flags59 & 8) {
                        aos_object1c_child_delete(&e->object1c, ci);
                        continue;
                    }
                    ci++;
                }
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 33) {
            /* Object21Create / Object21Update (sUnk_084F0DF8[33] /
             * sUnk_084F0ED8[33]): the cyclic lift. Create's sprite
             * attach exports as special_33 (same tiles/frames as
             * special_35, palette bank 1). The A094 room fields
             * pass as 0, the + 0x3C camera byte as param1, the
             * + 0x6D collision flag as 0 and the sub_0806D460
             * physics runs inside the module; the + 0x44 y
             * position writes back to e->y. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object21_create(&e->object21, 0, 0, e->param1,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            aos_object21_update(&e->object21, 0, 0);
            if (e->object21.song >= 0) {
                sound = e->object21.song;
                printf("Object21 %zu: song 0x%X\n", i,
                       e->object21.song);
            }
            e->y = (int)(e->object21.pos_y >> 16);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 36) {
            /* Object24Update (sUnk_084F0ED8[36]; Object24Create's
             * sprite attach stays under special_sprites and its art
             * is exported as special_36): the flickering ambient
             * sprite — RNG-gated animation step plus the + 0x58
             * bit-0x20 parity mark the draw reads as hidden. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object24_create(&e->object24, aos_random(),
                                    aos_random());
                continue;
            }
            aos_object24_update(&e->object24, aos_random());
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 37) {
            /* Object25Create / Object25Update (sUnk_084F0DF8[37] /
             * sUnk_084F0ED8[37]): the Arena soul carrier — a
             * param1 == 1 invisible spawner or a floating mover
             * that drifts right and falls until a param0 bound,
             * then dissolves. State-1 children spawn as real
             * SPECIAL entities like Object26's orbs, linked
             * through link_parent; the child's + 0x59 bit-3 done
             * mark decrements the parent's + 0x18 once. The
             * + 0x4CC gate passes 0 (unmodelled global), the
             * + 0x37E bit 0x20 is aria_pickup_state.special bit
             * 5, the A094 room origin passes 0, the + 0x59
             * bit-0 engine mark approximates an on-screen window
             * and sub_0806AF98's tile depenetration is skipped.
             * The hurt callback takes the weapon hitbox against
             * a nominal box with Soma's ATK as the damage. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                bool deleted;
                aos_object25_create(&e->object25, (int16_t)e->param0,
                                    (int16_t)e->param1,
                                    (int32_t)e->x << 16,
                                    (int32_t)e->y << 16,
                                    0, aria_pickup_state.special,
                                    &deleted);
                if (deleted) e->spawned = false;
                continue;
            }
            if (e->object25.deleted) continue;
            int32_t osx = e->x - cam_x, osy = e->y - cam_y;
            bool active = osx >= -0x20 && osx <= 0x120 &&
                          osy >= -0x20 && osy <= 0xE0;
            if (e->object25.state == 0 && e->object25.hp > 0 &&
                combat->weapon && combat->weapon_frames) {
                int wx, wy, ww, wh;
                if (aos_weapon_hitbox(combat->weapon, soma,
                                      combat->weapon_frames,
                                      &wx, &wy, &ww, &wh) &&
                    ww > 1 && wh > 1 &&
                    !(wx + ww - 1 < e->x - 8 || e->x + 8 < wx ||
                      wy + wh - 1 < e->y - 24 || e->y + 8 < wy))
                    aos_object25_hurt(&e->object25, combat->player->atk);
            }
            aos_object25_update(&e->object25, active, layer, 0, 0,
                                aos_random);
            e->x = (int)(e->object25.pos_x >> 16);
            e->y = (int)(e->object25.pos_y >> 16);
            if (e->object25.spawn_pending &&
                room->entity_count < MAX_ENTITIES) {
                AriaEntity *child = &room->entities[room->entity_count];
                *child = (AriaEntity){.kind = ARIA_KIND_SPECIAL,
                    .id = 37,
                    .x = (int)(e->object25.spawn_x >> 16),
                    .y = (int)(e->object25.spawn_y >> 16),
                    .spawned = true,
                    .link_parent = (int)(e - room->entities) + 1};
                bool cdeleted;
                aos_object25_create(&child->object25,
                                    e->object25.spawn_param0, 0,
                                    e->object25.spawn_x,
                                    e->object25.spawn_y, 0,
                                    aria_pickup_state.special,
                                    &cdeleted);
                if (!cdeleted) {
                    room->entity_count++;
                    child->object25.linked = true;
                    child->object25.field36 = e->object25.field36;
                }
            }
            if (e->object25.damage >= 0 &&
                room->damage_popup_count < MAX_DAMAGE_POPUPS)
                room->damage_popups[room->damage_popup_count++] =
                    aos_damage_popup_create(e->object25.pos_x,
                                            e->object25.pos_y,
                                            (int16_t)e->object25.damage,
                                            0);
            for (int f = 0; f < e->object25.particle_fx &&
                 room->effect_count < MAX_EFFECTS; ++f)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = false,
                    .x = e->x, .y = e->y};
            if (e->object25.finished && e->link_parent) {
                AriaEntity *parent = &room->entities[e->link_parent - 1];
                if (parent->kind == ARIA_KIND_SPECIAL &&
                    parent->id == 37 && parent->object25.children)
                    parent->object25.children--;
                e->link_parent = 0;
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 38) {
            /* Object26Update (sUnk_084F0ED8[38]; Object26Create's
             * sprite attach stays under special_sprites, the two
             * orb children are spawned as real SPECIAL entities):
             * the chandelier hub. Its sub_08057F4C probe tests the
             * 0x8C x 0xC8 box at (x - 0x46, y - 0x64) — a 140x200
             * box centred on the entity — against Soma and feeds
             * the + 0x18/+ 0x1A counters the linked orbs consume. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object26_create(&e->object26);
                /* Object26Create's two EntityCreateInRange(0x1B,
                 * 0x48) children: the Object28 orb upper-left and
                 * the Object27 orb lower-right, each linked to this
                 * entity through + 0x14. */
                if (room->entity_count + 2 <= MAX_ENTITIES) {
                    int parent_index = (int)(e - room->entities);
                    AriaEntity *orb_a = &room->entities[room->entity_count++];
                    *orb_a = (AriaEntity){.kind = ARIA_KIND_SPECIAL, .id = 40,
                        .x = e->x - 0x15, .y = e->y + 0x44, .spawned = true,
                        .link_parent = parent_index + 1};
                    aos_object28_create(&orb_a->object28, 0);
                    AriaEntity *orb_b = &room->entities[room->entity_count++];
                    *orb_b = (AriaEntity){.kind = ARIA_KIND_SPECIAL, .id = 39,
                        .x = e->x + 0x12, .y = e->y + 0x2C, .spawned = true,
                        .link_parent = parent_index + 1};
                    aos_object27_create(&orb_b->object27, 0);
                }
                continue;
            }
            int32_t px = (int32_t)(soma->x >> 16), py = (int32_t)(soma->y >> 16);
            bool inside = px >= e->x - 0x46 && px < e->x - 0x46 + 0x8C &&
                          py >= e->y - 0x64 && py < e->y - 0x64 + 0xC8;
            aos_object26_update(&e->object26, inside);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 39) {
            /* Object27Update (sUnk_084F0ED8[39]; Object27Create's
             * sprite attach, attack preset and camera-derived + 0x3C
             * stay under special_sprites): the spinning attack orb.
             * The + 0x14 target link has no runtime slot — the
             * player-near window stands in for its + 0x18 > 0x77
             * gate and the sub_0803E238 angular step is exposed as
             * the angle accumulator. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object27_create(&e->object27, (uint8_t)e->param1);
                continue;
            }
            uint16_t *gate = NULL;
            bool target_far;
            if (e->link_parent) {
                /* An Object26 child: the + 0x14 link reads the
                 * parent's + 0x18 as a signed s16 > 0x77 and the
                 * orb consumes it (strh 0) on arming. */
                AriaEntity *parent = &room->entities[e->link_parent - 1];
                gate = &parent->object26.counter_a;
                target_far = (int16_t)*gate > 0x77;
            } else {
                int32_t osx = e->x - cam_x, osy = e->y - cam_y;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
                target_far = (uint16_t)(ssx - osx + 0x10) <= 0x70 &&
                             (uint16_t)(ssy - osy) <= 0x10;
            }
            uint8_t phase_before = e->object27.phase;
            aos_object27_update(&e->object27, target_far);
            if (gate && phase_before == 0 && e->object27.phase != 0)
                *gate = 0;
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 42) {
            /* Object2AUpdate (sUnk_084F0ED8[42]; Object2ACreate's
             * sprite attach, attack records and sub_08057774 load
             * stay under special_sprites/special_42_v*, the
             * bgInfo[1] scroll frame is approximated by the logic
             * camera): the screen-space horizontal oscillator.
             * The + 0x13190 focus slot and the sub_080421AC armed
             * read are approximated by the player-near window and
             * the weapon hitbox overlap. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2a_create(&e->object2a, e->param0, e->param1,
                                    (int32_t)e->x << 16,
                                    (int32_t)e->y << 16,
                                    (int32_t)cam_x << 16,
                                    (int32_t)cam_y << 16);
                continue;
            }
            int32_t sx = (int32_t)cam_x << 16, sy = (int32_t)cam_y << 16;
            int wbx = (int)(e->object2a.out_x >> 16);
            int wby = (int)(e->object2a.out_y >> 16);
            int32_t osx = (int32_t)e->x << 16, osy = (int32_t)e->y << 16;
            int32_t ssx = soma->x, ssy = soma->y;
            bool focus = (uint32_t)(ssx - osx + 0x100000) <= 0x200000 &&
                         (uint32_t)(ssy - osy + 0xC0000) <= 0x180000;
            int wx, wy, ww, wh;
            bool attack = combat->weapon && combat->weapon_frames &&
                          aos_weapon_hitbox(combat->weapon, soma,
                                            combat->weapon_frames,
                                            &wx, &wy, &ww, &wh) &&
                          ww > 1 && wh > 1 &&
                          !(wx + ww - 1 < wbx - 8 || wbx + 8 < wx ||
                            wy + wh - 1 < wby - 8 || wby + 24 < wy);
            aos_object2a_update(&e->object2a, sx, sy, focus, attack);
            e->x = (int)(e->object2a.out_x >> 16);
            e->y = (int)(e->object2a.out_y >> 16);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 43) {
            /* Object2BUpdate (sUnk_084F0ED8[43]; Object2BCreate's
             * sprite attach stays under special_sprites, art
             * exported as special_43): a plain animated sprite —
             * one sub_0803F17C step per frame. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2b_create(&e->object2b);
                continue;
            }
            aos_object2b_update(&e->object2b);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 35) {
            /* Object23Update (sUnk_084F0ED8[35]; Object23Create's
             * sprite attach stays under special_sprites, art
             * exported as special_35): a plain animated sprite —
             * one sub_0803F17C step per frame. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object23_create(&e->object23);
                continue;
            }
            aos_object23_update(&e->object23);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 45) {
            /* Object2DUpdate (sUnk_084F0ED8[45]; Object2DCreate's
             * sprite attach and child spawner sub_080589A4 stay
             * under special_sprites, art exported as special_45):
             * the proximity-armed attack sprite. The + 0x13110
             * entity is Soma; the + 0x6D/+ 0x59 bytes and the
             * sub_0806D128 attack step are approximated. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2d_create(&e->object2d);
                continue;
            }
            int32_t dx = soma->x - ((int32_t)e->x << 16);
            int32_t dy = soma->y - ((int32_t)e->y << 16);
            aos_object2d_update(&e->object2d, dx, dy);
            if (e->object2d.attack && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARK, .direct = false,
                    .x = e->x, .y = e->y};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 46) {
            /* Object2EUpdate (sUnk_084F0ED8[46]; Object2ECreate's
             * sprite attach stays under special_sprites, art
             * exported as special_46): the one-shot breakable —
             * the sub_0806CFFC rectangle probe becomes the weapon
             * hitbox overlap, the three sub_080589A4 children are
             * counted and the song is reported. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2e_create(&e->object2e);
                continue;
            }
            int wx, wy, ww, wh;
            bool hit = combat->weapon && combat->weapon_frames &&
                       aos_weapon_hitbox(combat->weapon, soma,
                                         combat->weapon_frames,
                                         &wx, &wy, &ww, &wh) &&
                       ww > 1 && wh > 1 &&
                       !(wx + ww - 1 < (int32_t)e->x - 0x20 ||
                         (int32_t)e->x + 0x20 < wx ||
                         wy + wh - 1 < (int32_t)e->y - 0x20 ||
                         (int32_t)e->y + 0x50 < wy);
            aos_object2e_update(&e->object2e, hit);
            if (e->object2e.song >= 0) {
                sound = e->object2e.song;
                printf("Object2E %zu: song 0x%X\n", i, e->object2e.song);
                if (room->effect_count < MAX_EFFECTS)
                    room->effects[room->effect_count++] = (AosEffect){
                        .kind = AOS_EFFECT_SPARK, .direct = false,
                        .x = e->x, .y = e->y};
            }
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 47) {
            /* Object2FCreate / Object2FUpdate (sUnk_084F0DF8[47] /
             * sUnk_084F0ED8[47]): the skull-eye flame jets. Create
             * deletes on the + 0x37E bitfield bit 2
             * (aria_pickup_state.special), picks the + 0x42/+ 0x46
             * anchor from the 0x08118650 table by the + 0x30 param
             * and arms the + 0x58 bit-0x20 mark. Update latches
             * + 0xB and clears the mark on the + 0x42C bit-0x1000
             * trigger — the keys<<4 word maps it to R held. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2f_create(&e->object2f,
                                    aria_pickup_state.special,
                                    (int16_t)e->param0);
                continue;
            }
            if (e->object2f.deleted) continue;
            aos_object2f_update(&e->object2f,
                                (input->held & AOS_KEY_R) != 0);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 54) {
            /* Object36Create / Object36Update (sUnk_084F0DF8[54] /
             * sUnk_084F0ED8[54]): the metal gate and button — a
             * BG-affine scroller with no sprite. Create reads the
             * 0x081186A4 record by the + 0x30 param and registers
             * the + 0x1316C singleton slot (approximated as always
             * free); update queues the BgAffineSet block and
             * advances + 0x14 by + 0x1A. The block is consumed
             * nowhere yet, like Object16's zoom. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object36_create(&e->object36, (int16_t)e->param0,
                                    false);
                continue;
            }
            uint16_t block[9];
            aos_object36_update(&e->object36, block);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 52) {
            /* Object34Create / Object34Update (sUnk_084F0DF8[52] /
             * sUnk_084F0ED8[52]; sub_08059D98): the waterfall
             * splash — a screen-space sprite whose + 0x18 offset
             * ping-pongs between the 0x3FFF0000 and 0x40000000
             * bounds with + 0x1A velocity accelerated by the +
             * 0x32 param. The bgInfo[1] scroll frame is
             * approximated by the logic camera; the three
             * sub_08059E4C particle children are counted, not
             * spawned. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object34_create(&e->object34, (int16_t)e->param0,
                                    (uint16_t)e->param1,
                                    (int32_t)cam_x << 16,
                                    (int32_t)cam_y << 16,
                                    (int32_t)e->x << 16,
                                    (int32_t)e->y << 16);
                continue;
            }
            aos_object34_update(&e->object34, (int32_t)cam_x << 16,
                                (int32_t)cam_y << 16);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 44) {
            /* Object2CCreate / Object2CUpdate (sUnk_084F0DF8[44] /
             * sUnk_084F0ED8[44]): the fountain — the parent update
             * is a bare frame; create spawns four type-0x49 droplet
             * children (sub_0805866C) on the special_44 sprite
             * triple. The children are counted, not spawned, like
             * Object34's particles. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object2c_create(&e->object2c);
                continue;
            }
            aos_object2c_update(&e->object2c);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 26) {
            /* Object1ACreate / Object1AUpdate (sUnk_084F0DF8[26] /
             * sUnk_084F0ED8[26]): the patrolling-skull slot — both
             * native routines are a bare bx lr, an inert
             * placeholder. Called for dispatch parity only. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object1a_create(&e->object1a);
                continue;
            }
            aos_object1a_update(&e->object1a);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 30) {
            /* Object1ECreate / Object1EUpdate (sUnk_084F0DF8[30] /
             * sUnk_084F0ED8[30]): the save/warp room-walls slot —
             * both native routines are a bare bx lr, an inert
             * placeholder. Called for dispatch parity only. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object1e_create(&e->object1e);
                continue;
            }
            aos_object1e_update(&e->object1e);
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 40) {
            /* Object28Update (sUnk_084F0ED8[40]; Object28Create's
             * sprite attach stays under special_sprites): the
             * Object27 orb sibling — same machine over the same
             * chandelier art, approximated identically. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                aos_object28_create(&e->object28, (uint8_t)e->param1);
                continue;
            }
            uint16_t *gate = NULL;
            bool target_far;
            if (e->link_parent) {
                /* Object26's Object28 child gates on the parent's
                 * + 0x1A s16 > 0x77, consumed on arming. */
                AriaEntity *parent = &room->entities[e->link_parent - 1];
                gate = &parent->object26.counter_b;
                target_far = (int16_t)*gate > 0x77;
            } else {
                int32_t osx = e->x - cam_x, osy = e->y - cam_y;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
                target_far = (uint16_t)(ssx - osx + 0x10) <= 0x70 &&
                             (uint16_t)(ssy - osy) <= 0x10;
            }
            uint8_t phase_before = e->object28.phase;
            aos_object28_update(&e->object28, target_far);
            if (gate && phase_before == 0 && e->object28.phase != 0)
                *gate = 0;
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 2) {
            /* Object02Create / Object02Update (sUnk_084F0DF8[2] /
             * sUnk_084F0ED8[2]): the cutscene director. Screen pixels
             * are room pixels minus the camera, the same approximation
             * the door uses for + 0x42/+ 0x46. */
            if (!e->spawned) {
                if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
                e->spawned = true;
                int32_t sx = e->x - cam_x, sy = e->y - cam_y;
                int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
                int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
                bool near = (uint16_t)(sx - (ssx - 0x1C)) <= 0x38 &&
                            (uint16_t)(sy - ssy) <= 0x22;
                aos_object02_create(&e->object02, e->param0, e->param1,
                                    aria_pickup_state.special, near, sx, ssx,
                                    soma->vx);
                continue;
            }
            if (e->object02.deleted) continue;
            int32_t ssx = (int32_t)(soma->x >> 16) - cam_x;
            int32_t ssy = (int32_t)(soma->y >> 16) - cam_y;
            aos_object02_update(&e->object02, layer, cam_x, cam_y, e->x, e->y,
                                ssx, ssy, aria_pickup_state.special, aos_random);
            if (e->object02.force_zero || e->object02.forced) {
                input->active = true;
                input->held = e->object02.forced;
            }
            if (e->object02.zero_soma_vx) {
                soma->vx = 0;
                soma->friction = 0;    /* + 0x48 and + 0x50 */
            }
            if (e->object02.song >= 0) {
                sound = e->object02.song;
                printf("Object02 %zu: song 0x%X\n", i, e->object02.song);
            }
            if (e->object02.sparkle && room->effect_count < MAX_EFFECTS)
                room->effects[room->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_SPARKLE, .direct = true,
                    .x = e->object02.sparkle_x << 16,
                    .y = e->object02.sparkle_y << 16,
                    .vy = -0x1000};
            continue;
        }
        if (e->kind == ARIA_KIND_SPECIAL && e->id == 32) {
            /* Object20Create (0x0805B42C) / Object20Update
             * (0x0805B510): the param0-dispatched room-object
             * family — dirs/story/shop gates, NPC talk loops and
             * the shops. In-field mapping and Out forwarding live
             * in object20_fill_in/object20_apply_out above; the
             * window triple goes through the shared room
             * dialogue (aos_room_dialogue_apply). param0 3 stays
             * owned by the bespoke opening path: the dispatcher
             * reports handled_externally and the record is
             * consumed without updates so area_00_room_000's
             * opening-adjacent params cannot double-run. */
            room_dialogue_bind();
            if (!e->spawned) {
                /* entity_spawn_due carries sub_0800F4F8/sub_0800F5BC's
                 * second loop: every record with unk_7 & 0xF (the
                 * exported flags byte) goes to sub_0800F1FC
                 * unconditionally at room load; the rest wait for the
                 * camera window. */
                if (!entity_spawn_due(e, room, cam_x, cam_y))
                    continue;
                e->spawned = true;
                AosObject20In in;
                object20_fill_in(room, e, soma, cam_x, cam_y,
                                 new_input, &in);
                AosObject20Out out;
                if (!aos_object20_create(&e->object20, e->param0, &in,
                                         &out))
                    e->object20.deleted = true;
                if (out.unported)
                    printf("Object20 %zu: param0 %d unported variant "
                           "(inert)\n", i, e->param0);
                if (!e->object20.deleted)
                    object20_apply_out(e, &out, combat, input,
                                       new_input, -1);
                continue;
            }
            if (e->object20.deleted ||
                e->object20.family == AOS_OBJECT20_FAMILY_OPENING)
                continue;
            /* The npc window poll needs the pre-update +0xA state:
             * state 2 of the loop/guard/talk_loop machines is the
             * sub_08067A04 frame (see object20_apply_out). */
            int npc_pre_state = -1;
            switch (e->object20.family) {
            case AOS_OBJECT20_FAMILY_LOOP:
                npc_pre_state = e->object20.v.npc_loop.state;
                break;
            case AOS_OBJECT20_FAMILY_GUARD:
                npc_pre_state = e->object20.v.guard.state;
                break;
            case AOS_OBJECT20_FAMILY_TALK_LOOP:
                npc_pre_state = e->object20.v.talk_loop.state;
                break;
            default:
                break;
            }
            AosObject20In in;
            object20_fill_in(room, e, soma, cam_x, cam_y,
                             new_input, &in);
            AosObject20Out out;
            aos_object20_update(&e->object20, &in, &out);
            /* Synchronize the entity's room-pixel position from
             * the variant state the ported routines maintain
             * (+0x40/+0x44 fine 16.16 -> e->x/e->y pixels). The
             * In block carries no own-entity position input, so a
             * zero field means "not written yet" — the record's
             * spawn position is kept rather than zeroed. */
            int32_t o20_px = 0, o20_py = 0;
            switch (e->object20.family) {
            case AOS_OBJECT20_FAMILY_DIRS:
                o20_px = e->object20.v.dirs.pos40;
                o20_py = e->object20.v.dirs.pos44;
                break;
            case AOS_OBJECT20_FAMILY_STORY:
                o20_px = e->object20.v.story.pos40;
                o20_py = e->object20.v.story.pos44;
                break;
            case AOS_OBJECT20_FAMILY_SHOP:
                o20_px = e->object20.v.shop.pos40;
                break;
            case AOS_OBJECT20_FAMILY_GUIDE:
                o20_px = e->object20.v.guide.x;
                break;
            case AOS_OBJECT20_FAMILY_LOOP:
                o20_px = e->object20.v.npc_loop.x;
                break;
            case AOS_OBJECT20_FAMILY_GUARD:
                o20_px = e->object20.v.guard.x;
                break;
            default:
                break;
            }
            if (o20_px) e->x = o20_px >> 16;
            if (o20_py) e->y = o20_py >> 16;
            object20_apply_out(e, &out, combat, input, new_input,
                               npc_pre_state);
            continue;
        }
        if (!door) continue;
        if (!e->spawned) {
            if (!entity_spawn_due(e, room, cam_x, cam_y)) continue;
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
