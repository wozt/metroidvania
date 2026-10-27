/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_entities.h"

#include <math.h>

#define FIXED(value) ((int32_t)(value) * 0x10000)

static uint16_t sine_table[0x400];

/* sub_080009E4 and its 1024-entry sSineTable. */
static void build_sine(void) {
    if (sine_table[1]) return;
    for (int index = 0; index < 0x400; ++index)
        sine_table[index] = (uint16_t)floor(
            sin(index * 3.14159265358979323846 / 2048.0) * 65536.0);
}

static int32_t intro_sine(uint32_t angle) {
    build_sine();
    uint32_t value = (angle + (0x80 >> 4)) & 0xFFFF;
    int32_t sign = 1;
    if (value & 0x8000) {
        value -= 0x8000;
        sign = -1;
    }
    if (value & 0x4000) {
        if (value & 0xFFFFBFF0u)
            return sign * (int32_t)sine_table[(0x800F - value) >> 4];
        return sign * 0x10000;
    }
    return sign * (int32_t)sine_table[value >> 4];
}

static uint16_t alpha_pack(unsigned first, unsigned second) {
    return (uint16_t)((first & 0x1Fu) | ((second & 0x1Fu) << 8));
}

static AosIntroEntity entity(uint8_t kind, uint8_t animation, uint8_t variant) {
    return (AosIntroEntity){
        .kind = kind,
        .animation = animation,
        .variant = variant,
        .alive = true,
    };
}

void aos_intro_entities_init(AosIntroEntities *group) {
    *group = (AosIntroEntities){.bg0_vertical = -0xA4};

    AosIntroEntity *main = &group->entities[AOS_INTRO_ENTITY_MAIN];
    *main = entity(AOS_INTRO_ENTITY_MAIN, 0, 0);
    main->state = 1;
    main->visible = true;
    main->x = FIXED(0x78);
    main->y = FIXED(0x50);
    main->vy = 0x1000;

    group->entities[AOS_INTRO_ENTITY_SCROLL_FOLLOWER] =
        entity(AOS_INTRO_ENTITY_SCROLL_FOLLOWER, 2, 0);
    group->entities[AOS_INTRO_ENTITY_AFFINE_RISE] =
        entity(AOS_INTRO_ENTITY_AFFINE_RISE, 3, 0);
    group->entities[AOS_INTRO_ENTITY_ORBIT] =
        entity(AOS_INTRO_ENTITY_ORBIT, 3, 0);
    group->entities[AOS_INTRO_ENTITY_PANEL_LEFT] =
        entity(AOS_INTRO_ENTITY_PANEL_LEFT, 1, 0);
    group->entities[AOS_INTRO_ENTITY_PANEL_RIGHT] =
        entity(AOS_INTRO_ENTITY_PANEL_RIGHT, 1, 1);
}

void aos_intro_entities_clear(AosIntroEntities *group) {
    for (unsigned index = 0; index < AOS_INTRO_ENTITY_COUNT; ++index)
        group->entities[index].alive = false;
    group->graphics_release_requested = true;
}

void aos_intro_entities_set_text_script(AosIntroEntities *group,
                                        const uint8_t *script,
                                        size_t script_size,
                                        AosIntroTextResolver resolver,
                                        void *resolver_user) {
    group->text_script = script;
    group->text_script_size = script_size;
    group->text_script_position = 0;
    group->text_resolver = resolver;
    group->text_resolver_user = resolver_user;
    group->text_script_missing = script == NULL || script_size < 2;
    group->text_script_finished = false;
    group->text_script_malformed = false;
}

static void append_glyph(AosIntroEntities *group, uint8_t glyph) {
    if (group->text_glyph_count < AOS_INTRO_TEXT_GLYPH_CAPACITY)
        group->text_glyphs[group->text_glyph_count++] = glyph;
    else
        group->text_output_truncated = true;
}

/* sub_08041318's printable-byte loop. Each script includes the two-byte
 * native header, while position indexes its body. */
static int32_t draw_script(AosIntroEntities *group, const uint8_t *script,
                           size_t script_size, int32_t position) {
    if (!script || script_size < 2 || position < 0) return -1;
    const uint8_t *body = script + 2;
    size_t body_size = script_size - 2;
    size_t cursor = (size_t)position;
    while (cursor < body_size && (body[cursor] & 0xE0u))
        append_glyph(group, body[cursor++]);
    return cursor < body_size ? (int32_t)cursor : -1;
}

/* sub_080093E0. Glyph rasterization remains a host operation, but the native
 * row clear, command consumption, insertion and script cursor are retained. */
static void update_text_script(AosIntroEntities *group) {
    group->text_step_requested = true;
    group->text_clear_requested = true;
    group->text_draw_column =
        (uint8_t)(group->text_column % AOS_INTRO_TEXT_ROWS);

    if (!group->text_script || group->text_script_size < 2) {
        group->text_script_missing = true;
        ++group->text_column;
        return;
    }
    if (group->text_script_position < 0) {
        group->text_script_finished = true;
        ++group->text_column;
        return;
    }

    const uint8_t *body = group->text_script + 2;
    size_t body_size = group->text_script_size - 2;
    int32_t position = group->text_script_position;
    bool parsing = true;
    while (parsing) {
        if (position < 0 || (size_t)position >= body_size) {
            group->text_script_malformed = true;
            position = -1;
            break;
        }
        uint8_t command = body[position];
        switch (command) {
            case 1: {
                if ((size_t)position + 1 >= body_size) {
                    group->text_script_malformed = true;
                    position = -1;
                    parsing = false;
                    break;
                }
                uint8_t id = body[position + 1];
                if (group->text_insert_count < AOS_INTRO_TEXT_INSERT_CAPACITY)
                    group->text_insert_ids[group->text_insert_count++] = id;
                else
                    group->text_output_truncated = true;
                position += 2;
                const uint8_t *insert = NULL;
                size_t insert_size = 0;
                if (!group->text_resolver ||
                    !group->text_resolver(group->text_resolver_user, id,
                                          &insert, &insert_size) ||
                    draw_script(group, insert, insert_size, 0) < 0) {
                    group->text_script_missing = true;
                }
                break;
            }
            case 5:
                ++position;
                break;
            case 6:
            case 9:
                ++position;
                parsing = false;
                break;
            case 10:
                position = -1;
                group->text_script_finished = true;
                parsing = false;
                break;
            default: {
                int32_t next = draw_script(group, group->text_script,
                                           group->text_script_size, position);
                if (next < 0 || next == position) {
                    group->text_script_malformed = true;
                    position = -1;
                    parsing = false;
                } else {
                    position = next;
                }
                break;
            }
        }
    }
    group->text_script_position = position;
    ++group->text_column;
}

/* sub_08009648. */
static void update_main(AosIntroEntities *group, AosIntroEntity *entity,
                        int32_t mode_stage) {
    entity->frame_modulo = (uint8_t)((entity->frame_modulo + 1) % 5);
    if (entity->frame_modulo == 0) {
        ++group->bg0_vertical;
        if ((group->bg0_vertical + 0xF0) % 0x0C == 0)
            update_text_script(group);
    }
    if (mode_stage > 6) {
        entity->alive = false;
        group->graphics_release_requested = true;
    }
}

/* sub_08008F40. animation_action is +0x72 (unk_552), supplied by the future
 * native animation resource; action 0 is the initialized frame. */
static void update_scroll_follower(AosIntroEntity *entity, int32_t bg1_y,
                                   int32_t bg1_velocity) {
    if (entity->state == 0) {
        entity->visible = true;
        entity->x = FIXED(0x78);
        entity->y = FIXED(0x9E);
        entity->state = 1;
    }
    if (bg1_y < 0x200000) entity->alive = false;
    entity->vy = entity->animation_action == 1 || entity->animation_action == 3
                     ? -0x2E00
                     : 0;
    entity->y += entity->vy - bg1_velocity;
}

/* sub_08009178, including sub_0806D4A8(vy, 0x50). */
static void update_affine_rise(AosIntroEntities *group, AosIntroEntity *entity,
                               int32_t bg1_y) {
    if (entity->state == 0) {
        entity->affine = true;
        entity->x = FIXED(0x78);
        entity->y = FIXED(0xB4);
        entity->scale = 0x20000;
        entity->vy = -0x16000;
        entity->state = 1;
    }
    if (entity->state == 1) {
        if ((uint32_t)bg1_y > 0x1E0000) return;
        group->affine_sound_requested = true;
        entity->visible = true;
        entity->state = 2;
    }
    entity->vy = entity->vy * (0x50 - 1) / 0x50;
    entity->y += entity->vy;
    if (entity->scale != 0)
        entity->scale -= 0x200;
    else
        entity->alive = false;
}

/* sub_080092A0. */
static void update_orbit(AosIntroEntity *entity, int32_t bg1_velocity) {
    if (entity->state == 0) {
        entity->affine = true;
        entity->visible = true;
        entity->x = FIXED(0xD2);
        entity->y = FIXED(0x87);
        entity->scale = 0x10000;
        entity->state = 1;
    }
    entity->phase_x += 0x600;
    entity->phase_y += 0x800;
    int32_t horizontal = intro_sine(entity->phase_x + 0x4000) * 0xA0;
    int32_t vertical = intro_sine(entity->phase_y) * 0xA0;
    if (horizontal < 0) horizontal += 0xFF;
    if (vertical < 0) vertical += 0xFF;
    entity->vx = horizontal >> 8;
    entity->vy = vertical >> 8;
    entity->y += entity->vy - bg1_velocity - 0x1400;
    entity->x += entity->vx;
    if ((entity->y >> 16) > 0x12C) entity->alive = false;
}

/* sub_08009054; sub_08008FEC's two half-pixel-separated draws are exposed
 * through double_draw until a native sprite renderer consumes them. */
static void update_panel(AosIntroEntities *group, AosIntroEntity *entity,
                         int32_t bg1_y, int32_t bg3_y) {
    if (entity->state == 0) {
        entity->double_draw = true;
        entity->visible = true;
        entity->x = FIXED(entity->variant == 0 ? 0xF8 : -8);
        entity->vx = entity->variant == 0 ? -0x1000 : 0x1000;
        entity->state = 1;
        if (entity->variant == 0) {
            group->alpha_override_valid = true;
            group->alpha_override = alpha_pack(10, 6);
        }
    }
    if (entity->state == 1) {
        entity->x += entity->vx;
        if ((uint32_t)bg1_y < 0x200000) {
            entity->timer = 0x120;
            entity->state = 2;
        }
    } else if (entity->state == 2) {
        entity->x += entity->vx;
        if (entity->timer != 0) {
            --entity->timer;
            if (entity->variant == 0) {
                unsigned first = 16u - entity->timer / 48u;
                group->alpha_override_valid = true;
                group->alpha_override = alpha_pack(first, 16u - first);
            }
        } else {
            entity->alive = false;
        }
    }
    entity->y = FIXED(0x5A) - bg3_y;
}

void aos_intro_entities_update(AosIntroEntities *group, int32_t mode_stage,
                               int32_t bg1_y, int32_t bg1_velocity,
                               int32_t bg3_y) {
    group->text_step_requested = false;
    group->text_clear_requested = false;
    group->text_glyph_count = 0;
    group->text_insert_count = 0;
    group->text_output_truncated = false;
    group->affine_sound_requested = false;
    group->alpha_override_valid = false;
    group->graphics_release_requested = false;
    for (unsigned index = 0; index < AOS_INTRO_ENTITY_COUNT; ++index) {
        AosIntroEntity *current = &group->entities[index];
        if (!current->alive) continue;
        switch (current->kind) {
            case AOS_INTRO_ENTITY_MAIN:
                update_main(group, current, mode_stage);
                break;
            case AOS_INTRO_ENTITY_SCROLL_FOLLOWER:
                update_scroll_follower(current, bg1_y, bg1_velocity);
                break;
            case AOS_INTRO_ENTITY_AFFINE_RISE:
                update_affine_rise(group, current, bg1_y);
                break;
            case AOS_INTRO_ENTITY_ORBIT:
                update_orbit(current, bg1_velocity);
                break;
            case AOS_INTRO_ENTITY_PANEL_LEFT:
            case AOS_INTRO_ENTITY_PANEL_RIGHT:
                update_panel(group, current, bg1_y, bg3_y);
                break;
            default:
                break;
        }
    }
}
