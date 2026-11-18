/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_OPENING_EVENT_H
#define AOS_OPENING_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../core/aos_stats.h"

enum {
    AOS_OPENING_TEXT_ID = 0x000C,
    AOS_OPENING_EVENT_ID = 0x20,
    AOS_OPENING_OBJECT_ID = 0x20,
    AOS_OPENING_OBJECT_PARAMETER = 3,
    /* sub_0805D198 places Soma here before the first-room scene. */
    AOS_OPENING_SOMA_X = 0xA8,
    AOS_OPENING_SOMA_Y = 0x2C0,
    /* Object20's actor table places Mina, Arikado and the distant winged
     * skeleton at these world coordinates before the dialogue starts.
     * sub_0805D198 writes record-0 child's +0x42 as the player's own
     * +0x42 minus 0x18 — Mina stands one step left of Soma. */
    AOS_OPENING_ACTOR_COUNT = 3,
    AOS_OPENING_MINA_X = 0x90,
    AOS_OPENING_MINA_Y = 0x2C0,
    AOS_OPENING_ARIKADO_X = 0x119,
    AOS_OPENING_ARIKADO_Y = 0x2C0,
    AOS_OPENING_SKELETON_X = 0x16F,
    AOS_OPENING_SKELETON_Y = 0x268,
    /* The opening actors' exported art stores OBJECT_MAX_ANIMS
     * animation slots; animation indices stay far below this bound. */
    AOS_OPENING_ACTOR_ANIMS = 24,
    /* sub_08067934 writes WIN0H=0x08E8 (x=8..232). sub_08067804 opens
     * WIN0V around y=124 to y=92..156. sub_08067A04 maps the speaker row
     * at screen y=100 and the dialogue canvas at (40,116). */
    AOS_OPENING_TEXT_PANEL_X = 8,
    AOS_OPENING_TEXT_PANEL_Y = 92,
    AOS_OPENING_TEXT_PANEL_WIDTH = 224,
    AOS_OPENING_TEXT_PANEL_HEIGHT = 64,
    AOS_OPENING_SPEAKER_X = 40,
    AOS_OPENING_SPEAKER_Y = 100,
    AOS_OPENING_TEXT_X = 40,
    AOS_OPENING_TEXT_Y = 116,
    /* sub_08067A04 writes the 8x16 wait marker to map cell (27,18), then
     * the vertically adjacent cell, compensating the tile portion of BG0's
     * portrait scroll. Its remaining four-pixel VOFS fraction places it at
     * screen y=140. */
    AOS_OPENING_PROMPT_X = 216,
    AOS_OPENING_PROMPT_Y = 140,
    AOS_OPENING_LINES = 4,
    AOS_OPENING_GLYPHS_PER_LINE = 80,
    AOS_OPENING_EMISSION_MAX_SPAWNS = 6,
    AOS_OPENING_EMITTED_CAPACITY = 10,
    AOS_OPENING_DEBRIS_CAPACITY = AOS_OPENING_EMITTED_CAPACITY * 7,
    AOS_OPENING_PROJECTILE_CAPACITY = 7,
    /* sub_08045B44 type-8 sparkles live at most the type-8 sequence's
     * 12 frames ({2,4,4,2}); the soul orb emits one every fourth frame
     * and each projectile one every second, so a handful stays live. */
    AOS_OPENING_SPARK_CAPACITY = 16,
};

typedef bool (*AosOpeningTextResolver)(void *context, uint8_t id,
                                       const uint8_t **script,
                                       size_t *script_size);

/* Semantic Object20 beats exposed to hosts and editor previews. The event
 * parser does not play audio directly. */
typedef enum {
    AOS_OPENING_CUE_NONE,
    AOS_OPENING_CUE_ENTRY_PAN,
    /* sub_0805CED0's case-0 -> case-1 boundary: once the spawn/reveal
     * pan (sub_0805CCE4 through entity+0x38) reports done the native
     * controller runs sub_08013CF0(0x1C) — MUS_28 — before the
     * sub_0805CB74 MOSAIC transition opens the text window. */
    AOS_OPENING_CUE_REVEAL_PAN,
    AOS_OPENING_CUE_CONFRONTATION,
    AOS_OPENING_CUE_ENEMY_APPEARS,
    AOS_OPENING_CUE_ENEMY_ATTACKS,
    AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN,
    AOS_OPENING_CUE_ENEMY_DEFEATED,
    AOS_OPENING_CUE_SOUL_ABSORBED,
    AOS_OPENING_CUE_SOUL_NOTICE_DISMISS,
    AOS_OPENING_CUE_AFTER_BATTLE,
    /* Script beat 7 selects sub_0805D410 +0xB==8, the mode-4 return pan.
     * It carries no audio of its own. */
    AOS_OPENING_CUE_RETURN_PAN,
    AOS_OPENING_CUE_COMPLETE,
} AosOpeningCue;

/* The two native sub_0805D2C8/sub_0805D35C emission groups. Entity types
 * refer to Object20's private 0x081186DC resource table, not room objects. */
typedef enum {
    AOS_OPENING_EMISSION_NONE = 0,
    AOS_OPENING_EMISSION_DISTANT = 1u << 0,
    AOS_OPENING_EMISSION_NEAR_SOMA = 1u << 1,
} AosOpeningEmission;

/* Portable requests raised by the two private assault callbacks. Hosts may
 * consume these after aos_opening_actors_update without coupling the story
 * controller to audio, particles or the combat entity allocator. */
typedef enum {
    AOS_OPENING_ENTITY_EVENT_NONE = 0,
    AOS_OPENING_ENTITY_EVENT_SOUND_0X6B = 1u << 0,
    AOS_OPENING_ENTITY_EVENT_DEBRIS = 1u << 1,
    AOS_OPENING_ENTITY_EVENT_PROJECTILE = 1u << 2,
    AOS_OPENING_ENTITY_EVENT_SOUND_0XBC = 1u << 3,
} AosOpeningEntityEvent;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t entity_type;
    uint8_t parameter;
    uint8_t variant;
} AosOpeningSpawn;

/* One private child allocated by sub_0805E260 or sub_0805E528. The resource
 * table type stays explicit so hosts can select an exported sprite without
 * turning these story actors into ordinary room enemies. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    uint16_t animation_age;
    uint16_t phase;
    uint8_t entity_type;
    uint8_t parameter;
    uint8_t variant;
    uint8_t animation;
    /* +0x6E/+0x6F analogues for the entity type's bound flattened
     * {entry,ticks} sequence: type 8 children reuse anim_seqs[2]
     * (the shared Enemy/opening_actor_8 binding) and type 10 children
     * use emitted_seqs (the Skeleton Knight enemy art), both filled
     * by the host's opening_actor_durations each frame. Unbound
     * animations keep the animation_age fallback. */
    uint8_t anim_subframe;
    uint8_t anim_tick;
    uint8_t state;
    uint8_t step;
    uint8_t timer;
    uint8_t events;
    bool active;
    bool mirrored;
    bool hidden;
    bool disappearing;
} AosOpeningEntity;

/* Seven pieces created by sub_0805E8C8/sub_0805E9F0 and updated by
 * sub_0805F044. source_type selects the Skeleton or Winged Skeleton sheet. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    uint8_t frame;
    uint8_t source_type;
    uint8_t timer;
    bool active;
    bool mirrored;
} AosOpeningDebris;

/* sub_0805ED80/sub_0805EE88's attack aimed at Object20's principal enemy. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    int32_t speed;
    uint16_t heading;
    uint16_t turn;
    uint16_t animation_age;
    uint8_t state;
    uint8_t timer;
    uint8_t events;
    bool active;
} AosOpeningProjectile;

/* A sub_08045B44 type-8 sparkle updated by sub_080450C8, emitted by
 * sub_0805EC20's soul orb (every fourth +0xD) and sub_0805EE88's
 * projectile (every second +0xD). Table 0x085251A8 entry 8 selects
 * the direct-sequence update with animation 0x08525194 — frames
 * {0x49,0x36,0x35,0x34} over ticks {2,4,4,2} — so entry/entry_tick
 * walk that sequence and the entity deletes when it ends. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    int32_t ax;
    int32_t ay;
    uint8_t entry;
    uint8_t entry_tick;
    uint8_t kind;
    bool started;
    bool active;
} AosOpeningSpark;

/* sub_0806D578's death flame and generic_death's four-frame respawns around
 * the scripted Winged Skeleton. */
typedef struct {
    int32_t x;
    int32_t y;
    uint8_t age;
    uint8_t timer;
    /* sub_0806D578's third argument is the dying entity's +0x5C band
     * minus one — the OAM priority band the flame draws at. */
    uint8_t band;
    bool active;
    bool mirrored;
} AosOpeningFlame;

#define AOS_OPENING_FLAME_CAPACITY 12

typedef uint32_t (*AosOpeningRandom)(void *context);
typedef int32_t (*AosOpeningSine)(uint32_t angle, void *context);
typedef uint16_t (*AosOpeningAngle)(int16_t x, int16_t y, void *context);
typedef int (*AosOpeningFloorDepth)(int32_t x, int32_t y, void *context);
typedef int (*AosOpeningWallPush)(int32_t x, int32_t y, bool moving_left,
                                  void *context);

typedef struct {
    const uint8_t *script;
    size_t script_size;
    size_t position;
    uint8_t glyphs[AOS_OPENING_LINES][AOS_OPENING_GLYPHS_PER_LINE];
    uint8_t glyph_counts[AOS_OPENING_LINES];
    uint8_t line;
    uint8_t speaker;
    uint8_t portrait;
    uint8_t beat;
    uint8_t beat_count;
    /* sub_08067A04's WIN0V animation counter. Command 4 selects case 3,
     * retracting 0x24 -> 0 at three units per frame. A later portrait
     * command selects case 2 independently, extending 0 -> 0x24 before
     * the portrait resources and text map are installed. */
    int8_t slide_dir;
    uint8_t slide_pos;
    AosOpeningCue cue;
    bool active;
    bool waiting;
    bool complete;
    bool malformed;
    bool dirty;
    bool advanced;
    AosOpeningTextResolver resolve;
    void *resolve_context;
} AosOpeningEvent;

/* One flattened animation sequence in the Object1C tick model
 * (AosObject1CAnimTick): ticks[i] is entry i's duration in update
 * frames and the entry index is the +0x65 frame selector, because the
 * exported opening-actor art keeps one rendered frame per sequence
 * entry. A NULL ticks pointer or zero count means the host has no
 * exported sequence for that animation and the duration fallback in
 * actor_animation_done applies. */
typedef struct {
    const uint8_t *ticks;
    uint16_t count;
} AosOpeningAnimSeq;

/* The three child entities created by Object20/sub_0805D198. This compact
 * controller covers the source-visible entry movement while the complete
 * sub_0805D410 callback graph remains a later parity slice. */
typedef struct {
    int32_t x[AOS_OPENING_ACTOR_COUNT];
    int32_t y[AOS_OPENING_ACTOR_COUNT];
    uint8_t animation[AOS_OPENING_ACTOR_COUNT];
    uint16_t animation_age[AOS_OPENING_ACTOR_COUNT];
    /* sub_0803F2C8's third argument lands in +0x6C bit 3, the sequence
     * loop bit: a looping sequence never latches +0x59 bit 0, so
     * actor_animation_done can never report done for it. The spawn-time
     * values are sub_0805D198's record 0 anim 4 arg3=0, record 1 anim 1
     * arg3=1 and record 8 anim 4 arg3=1. */
    uint8_t anim_loop[AOS_OPENING_ACTOR_COUNT];
    /* The sub_0803F17C -> sub_0803EE24 tick chain state per actor,
     * mirroring the native child records: anim_seqs[actor][animation]
     * is the bound flattened {entry,ticks} sequence (filled by the
     * host's opening_actor_durations from the loaded opening_actor_*
     * art each frame), anim_subframe is +0x6E, anim_tick is +0x6F,
     * anim_finished is +0x59 bit 0 and anim_ended is +0x6C bit 2. */
    AosOpeningAnimSeq anim_seqs[AOS_OPENING_ACTOR_COUNT]
                               [AOS_OPENING_ACTOR_ANIMS];
    /* Flattened {entry,ticks} bindings for the emitted Skeleton
     * Knight children (entity_type 10 -> Enemy/skeleton_knight,
     * animation slots 0-7 are exported). Filled by the host's
     * opening_actor_durations next to anim_seqs; type-8 emitted
     * children need no table of their own because they draw the
     * same Enemy/opening_actor_8 art as actor slot 2. */
    AosOpeningAnimSeq emitted_seqs[AOS_OPENING_ACTOR_ANIMS];
    uint8_t anim_subframe[AOS_OPENING_ACTOR_COUNT];
    uint8_t anim_tick[AOS_OPENING_ACTOR_COUNT];
    bool anim_finished[AOS_OPENING_ACTOR_COUNT];
    bool anim_ended[AOS_OPENING_ACTOR_COUNT];
    /* The three story children's +0x58 attribute byte. Only bit 6
     * (0x40, the OAM horizontal flip consumed by sub_0803AC40's
     * `+0x58 << 0x19 >> 0x1f` -> attribute bit 12) and bit 0 (cleared
     * alongside it by the same writes) are tracked; every other bit
     * stays at its spawn value. sub_0805D410's +0xB==2 writes
     * child0 &= ~0x41 and child1 |= 0x40, and +0xB==3 sub-state 3
     * rewrites child1 as (old & ~0x41) | (!bit6 << 6). */
    uint8_t actor_flags58[AOS_OPENING_ACTOR_COUNT];
    bool visible[AOS_OPENING_ACTOR_COUNT];
    /* State 0 is the +0xB==0 0x40-frame suspend timer; state 2 is the
     * post-suspend idle. There is deliberately no walk state: native
     * WRAM captures keep Arikado's entity +0x40 world x at 0x119 for
     * the whole scene even though sub_0805D198 stores 0xFFFFA000 in
     * his +0x48, and every update path that could consume it
     * (sub_0805B748 -> sub_0803F17C, sub_0805D410's child writes,
     * sub_0805DAA0 which belongs to the skeleton child) is animation
     * or flag work only. The perceived approach is the camera pans;
     * any residual draw-side offset is aria_draw.c's concern. */
    uint8_t state;
    uint8_t step;
    uint8_t timer;
    /* sub_0805D410's beat serialization: scene_state mirrors entity +0xB,
     * scene_step its +0xC sub-state and scene_timer its +0xD countdown.
     * Each script control-2 beat increments +0xB and clears +0xC, so the
     * seven beats of the opening script select +0xB values 1, 2, 3,
     * 5, 6, 7 and 8; +0xB==3's own tail auto-increments to 4. */
    uint8_t scene_state;
    uint8_t scene_step;
    uint16_t scene_timer;
    int32_t camera_x;
    int32_t camera_y;
    int32_t soma_x;
    uint8_t camera_mode;
    uint8_t emitted_batches;
    uint8_t emitted_count;
    bool camera_active;
    bool camera_initialized;
    /* sub_0805CED0 case 0 waits on the spawn camera phase before the text
     * window opens; mode 5 is that initial reveal pan. */
    bool reveal_done;
    /* sub_0805CED0 case 1 runs sub_0805CB74's six-stage MOSAIC
     * transition after the reveal pan and before sub_08067934 opens the
     * WIN0 window. transition_step mirrors entity +0xB (0..5, 6 done),
     * transition_timer entity +0xD, mosaic the MOSAIC register value the
     * native routine writes each frame (0 once it clears). window_open
     * latches one frame after the transition reports done, matching the
     * native +0xC handoff into +0xA==2. */
    uint8_t transition_step;
    uint8_t transition_timer;
    uint16_t mosaic;
    bool transition_started;
    bool transition_wait_open;
    bool window_open;
    /* sub_0805D410 case +0xB==0: on the first +0xA==2 frame it raises the
     * text engine's suspend flag and holds it for a 0x40-frame timer.
     * sub_08067A04 returns early while suspended, so WIN0V keeps the
     * empty 0xC0C1 written by sub_08067934 and the window chrome only
     * becomes visible when sub_08067804 runs after the hold. text_hold
     * mirrors that timer; window_shown latches when it expires. */
    uint8_t text_hold;
    bool window_shown;
    /* sub_0805D410 case +0xB==1 (first script beat): entity+0x38=1's
     * camera pan runs under the suspend flag, then a 0x10-frame tail;
     * scene_step 2 marks the released idle sub-state. */
    uint8_t beat1_step;
    AosOpeningEntity emitted[AOS_OPENING_EMITTED_CAPACITY];
    AosOpeningDebris debris[AOS_OPENING_DEBRIS_CAPACITY];
    AosOpeningProjectile projectiles[AOS_OPENING_PROJECTILE_CAPACITY];
    AosOpeningFlame flames[AOS_OPENING_FLAME_CAPACITY];
    AosOpeningSpark sparks[AOS_OPENING_SPARK_CAPACITY];
    uint8_t debris_count;
    uint8_t projectile_count;
    uint8_t flame_count;
    uint8_t spark_count;
    /* sub_0805D410 +0xB==5 at +0xD==0xD0 on entity[0x1c]: entity +0x58
     * bit 0x20 enters the Winged Skeleton's generic death, a 0x28-frame
     * blink (table
     * 0x08118CE0) with sub_0806D578 flames while sub_0805E9F0's six pieces
     * and sub_0805EAFC's soul orb are already in flight. */
    uint8_t skeleton_death;
    AosOpeningRandom random;
    AosOpeningSine sine;
    AosOpeningAngle angle;
    AosOpeningFloorDepth floor_depth;
    AosOpeningWallPush wall_push;
    void *motion_context;
    AosOpeningCue pending_cue;
    uint32_t frame;
    uint8_t soma_animation;
    bool active;
    bool soma_hidden;
    /* The opening's guaranteed Winged Skeleton soul uses the same curved
     * homing motion as an ordinary red-soul pickup, but remains owned by the
     * story controller so editor previews do not mutate an inventory. */
    int32_t soul_x;
    int32_t soul_y;
    int32_t soul_vx;
    int32_t soul_vy;
    int32_t soul_speed;
    int32_t soul_turn;
    uint32_t soul_heading;
    uint16_t soul_age;
    /* sub_0805EC20's +0xA/+0xD lifetime: state 0 counts the creation
     * 0xC8 down and reloads 0x40 for state 1, whose expiry deletes the
     * orb (+0x59 bit 8) — the same 0x108-frame total soul_age models,
     * now split the native way so the +0xD&3 sparkle gate sees the
     * real countdown. */
    uint8_t soul_state;
    uint8_t soul_timer;
    bool soul_active;
    bool soul_absorbed;
    /* sub_0800E708's obtained-soul window: 0 closed, 1 opening, 2 waiting
     * for the player, 3 closing after sub_0800F0AC. The script stays
     * blocked while it is 1 or 2; native +0xB==5 keeps the suspend until
     * sub-state 5's tail, which lands about one frame after the close
     * starts, so 3 does not block. */
    uint8_t soul_notice;
    uint8_t soul_notice_timer;
    uint16_t notice_input;
    /* sub_0805DAA0, the scripted Winged Skeleton's own callback:
     * skeleton_state mirrors entity +0xA (0 waits parent +0x20 bit 8,
     * 1 bobs until bit 0x10, 2 runs the +0xB attack sub-states, 3 is
     * the post-walk idle), skeleton_step its +0xB, skeleton_timer its
     * +0xD and skeleton_phase its +0x26 bob phase. */
    uint8_t skeleton_state;
    uint8_t skeleton_step;
    uint8_t skeleton_timer;
    uint16_t skeleton_phase;
    /* sub_0805CE28 latches the live camera word (+0xA094+6) once at
     * spawn for entity+0x38's pan-mode pick. sub_0805DAA0's approach
     * stop compares entity +0x42 — the high word of the same +0x40
     * the integrator updates — against 0xCA minus that same camera
     * word, so the camera terms cancel and the guard is world x <=
     * 0xCA; the latch is retained for diagnostics only. */
    int32_t scene_camera_x;
    bool scene_camera_set;
    /* Parent +0x20 bit 1 (value 2): +0xB==4 sub-state 3 sets it when
     * child18's +0x59 animation-finished flag arrives. Emitted
     * children poll it every frame and only then enter their
     * disappearance tails, so children still rising or waiting on
     * their selector are not cut short. */
    bool attack_flag;
    /* sub_0805F0B0's player-touch callback on sub_0805EAFC's soul orb:
     * SE 0xBC plus +0x59 bit 8 removal. The port approximates the
     * touch margin with a 0x10-pixel box on the absorption target;
     * soul_events carries the sound request (AOS_OPENING_ENTITY_EVENT_
     * SOUND_0XBC) until a host consumes it, and soul_arrived latches
     * the arrival itself. */
    bool soul_arrived;
    uint8_t soul_events;
} AosOpeningActors;

bool aos_opening_event_should_start(uint8_t area, uint16_t room,
                                    uint8_t character,
                                    const uint32_t events[8],
                                    bool room_has_controller);
bool aos_opening_event_init(AosOpeningEvent *event, const uint8_t *script,
                            size_t script_size);
/* sub_08067804's WIN0V bytes for the 0..0x24 animation counter. */
void aos_opening_window_vertical(uint8_t slide_pos, uint8_t *top,
                                 uint8_t *bottom);
void aos_opening_event_set_resolver(AosOpeningEvent *event,
                                    AosOpeningTextResolver resolve,
                                    void *context);
void aos_opening_event_update(AosOpeningEvent *event, uint16_t new_input);
/* Localized speaker ids used by the first-room Object20 script. */
const char *aos_opening_event_speaker_name(uint8_t speaker);
void aos_opening_actors_init(AosOpeningActors *actors);
/* Injects the native RNG, sine, ArcTan2 and collision probes without coupling the
 * portable scene model to the gameplay runtime. NULL callbacks leave their
 * corresponding query unavailable; a missing floor probe means open air. */
void aos_opening_actors_set_motion(AosOpeningActors *actors,
                                   AosOpeningRandom random,
                                   AosOpeningSine sine,
                                   AosOpeningAngle angle,
                                   AosOpeningFloorDepth floor_depth,
                                   AosOpeningWallPush wall_push,
                                   void *context);
/* Connects a script control-2 beat to sub_0805D410's actor state. */
void aos_opening_actors_trigger(AosOpeningActors *actors, AosOpeningCue cue);
/* Feeds edge input to the modal obtained-soul window; consumed by the next
 * aos_opening_actors_update. Other scene input stays with the text parser. */
void aos_opening_actors_input(AosOpeningActors *actors, uint16_t new_input);
/* True while an Object20 beat owns the scene. The text parser waits instead
 * of accepting the next page early, so rapid A presses cannot skip or strand
 * the enemy reveal, attack or soul absorption. */
bool aos_opening_actors_blocks_script(const AosOpeningActors *actors);
/* Advances one frame and returns one audio cue due on that frame. Each
 * actor's bound animation sequence is ticked once per frame through the
 * sub_0803F17C-style {entry,ticks} chain; animation_durations remains the
 * fallback for animations without an exported sequence. camera_x is a
 * whole-
 * pixel host camera position; Object20 retains the horizontal fractional
 * part itself. camera_y is read by sub_08068AD4's portable margin test. */
AosOpeningCue aos_opening_actors_update(
    AosOpeningActors *actors,
    const uint16_t animation_durations[AOS_OPENING_ACTOR_COUNT],
    int32_t *camera_x, int32_t camera_y);
/* Returns the source-derived spawn descriptors for one emission bit. Soma's
 * world X is required because sub_0805D35C places two entities relative to
 * him. The return value is the full batch size, even when capacity is lower. */
size_t aos_opening_emission_spawns(AosOpeningEmission emission,
                                   int16_t soma_x, AosOpeningSpawn *spawns,
                                   size_t capacity);
bool aos_opening_event_commit(AosOpeningEvent *event, uint32_t events[8],
                              AosStats *stats);

/* The player +0x6D animation id the Object20 story beats request for Soma,
 * read back out of the serialized +0xB/+0xC/+0xD beat state. The story
 * machine drives Soma through the normal player pose path — sub_0801D028
 * binds +0x6D and sub_0803F17C steps it — so the host feeds this id into the
 * same guarded bind+step the gameplay update uses. The mapping follows the
 * native sequence 0x51 -> 0x0D -> 0x00 -> 0x04 -> 0x05 -> 0x52 -> 0x09 ->
 * 0x02 -> 0x00 observed on a real ROM:
 *   +0xB==0 (reveal/MOSAIC):              the unconscious floor pose 0x51.
 *   +0xB==1 pan (beat1_step 0):           0x0D LAND — the entry step.
 *   +0xB==1 dialogue / +0xB 2,3,4:        0x00 idle.
 *   +0xB==5 pre-notice sub-states:        0x04 then 0x05 absorb wind-up.
 *   +0xB==5 obtained-soul window:         0x52 — sub_0805D410 calls
 *       sub_0801D028(0x52,0,0,0) every frame while +0xC==2's +0xD<=0x8F
 *       tail and sub-states 3-5 run, which the port serializes as
 *       scene_step==2 plus the soul_notice modal window.
 *   +0xB 6,7,8 (post-absorb):             the host sequences the
 *       CROUCH_DOWN -> CROUCH -> IDLE flourish itself; AOS_OPENING_SOMA_
 *       ANIM_POST marks that window. */
#define AOS_OPENING_SOMA_ANIM_POST 0xFFu
static inline uint8_t aos_opening_soma_anim(const AosOpeningActors *actors) {
    if (!actors) return 0x00;
    if (actors->scene_state == 5 &&
        (actors->scene_step == 2 || actors->soul_notice == 1 ||
         actors->soul_notice == 2))
        return 0x52;
    switch (actors->scene_state) {
    case 0: return 0x51;
    case 1: return actors->beat1_step == 0 ? 0x0D : 0x00;
    case 5:
        if (actors->scene_step == 0) return 0x04;
        if (actors->scene_step == 1) return 0x05;
        return 0x52;
    case 2:
    case 3:
    case 4: return 0x00;
    case 6:
    case 7:
    case 8: return (uint8_t)AOS_OPENING_SOMA_ANIM_POST;
    default: return 0x00;
    }
}

#endif
