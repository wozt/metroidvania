/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Mother Brain (src/sprites_ai/mother_brain.c): the body
 * drives gSubSpriteData1 (beam shooter, eye, bottom); missiles break its
 * glass (the bottom part's health, drawn on BG0 in four stages), then hit
 * its eye, which closes after enough damage while it charges and fires its
 * beam; its death flashes it, removes it and starts the escape. Comments
 * name the pinned decompilation's routines. Not ported: MakeBackgroundFlash,
 * the escape explosion sprite (PSPRITE_EXPLOSION_ZEBES_ESCAPE, with the
 * escape timer) and the in-game timer at bosses. */
#include "mzm_sprite_ai.h"

enum {
    POSE_WAITING_GLASS = 0x1,
    POSE_MAIN_LOOP = 0x9,
    POSE_DYING = 0x67,
    POSE_START_ESCAPE = 0x68,
    PART_POSE_IDLE = 0x8,
    PART_POSE_SPAWN_BLOCK = 0xE,
    PART_POSE_GLASS_STAGE_1 = 0xF,
    PART_POSE_GLASS_STAGE_2 = 0x11,
    PART_POSE_GLASS_STAGE_3 = 0x13,
    PART_POSE_GLASS_BROKEN = 0x15,
    PART_POSE_UPDATE = 0x32,
    PART_POSE_GLASS_BREAKING = 0x62,
    BEAM_POSE_MOVING = 0x9,
    BLOCK_POSE_IDLE = 0x9,
    GLASS_POSE_BREAKING = 0x9,
    /* MotherBrainFightStage */
    STAGE_IN_GLASS = 0, STAGE_ACTIVE, STAGE_DYING, STAGE_DEAD,
    /* MotherBrainPartId */
    PART_BEAM_SHOOTER = 0, PART_BODY, PART_EYE, PART_BOTTOM,
    DAMAGE_THRESHOLD = 100,         /* SUPER_MISSILE_DAMAGE */
    MESSAGE_ZEBES_ESCAPE = 33,
    GFX_SLOT_SPECIAL = 6,           /* SPRITE_GFX_SLOT_SPECIAL */
    AREA_TOURIAN = 4,
};

/* sMotherBrainDynamicPaletteData: the palette row and its frames while
 * charging (128 ends it). */
static const uint8_t dynamic_palette[19][2] = {
    {2, 6}, {3, 6}, {4, 6}, {3, 6}, {2, 6}, {0, 2}, {2, 6}, {3, 6}, {4, 6}, {3, 6},
    {2, 6}, {0, 2}, {2, 3}, {3, 3}, {4, 3}, {3, 3}, {2, 3}, {0, 1}, {128, 128},
};

/* sMotherBrainGlassBreakingBaseTilemapValues */
static const uint16_t glass_tiles[5] = {0x170, 0x177, 0x1C0, 0x1C7, 0x210};

static MzmSprite *slot(const Ctx *c, uint8_t index) {
    return c->f->sprites && index < c->f->sprite_count ? &c->f->sprites[index] : NULL;
}

static const int16_t *part_data(const Ctx *c, uint8_t part) {
    const MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->mother_brain_multi;
    return m->parts[g->sub1.frame < MZM_MULTI_FRAMES ? g->sub1.frame : 0][part < MZM_MULTI_PARTS ? part : 0];
}

/* MotherBrainSyncSubSpritesPosition */
static void sync_sub_sprites(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = part_data(c, s->room_slot);
    if (p[0] >= 0 && p[0] < MZM_MOTHER_BRAIN_ANIMS && s->anim != p[0]) ai_set_anim(s, (uint8_t)p[0]);
    s->y = (uint16_t)(g->sub1.y + p[1]);
    s->x = (uint16_t)(g->sub1.x + p[2]);
}

/* SpriteUtilSyncCurrentSpritePositionWithSubSprite1Position */
static void sync_position(Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = part_data(c, c->s->room_slot);
    c->s->y = (uint16_t)(g->sub1.y + p[1]);
    c->s->x = (uint16_t)(g->sub1.x + p[2]);
}

/* SpriteUtilUpdateSubSprite1Anim */
static void update_sub1_anim(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->mother_brain_multi;
    g->sub1.adc++;
    if (m->timers[g->sub1.frame] < g->sub1.adc) {
        g->sub1.adc = 1;
        g->sub1.frame++;
        if (g->sub1.frame >= MZM_MULTI_FRAMES || m->timers[g->sub1.frame] == 0) g->sub1.frame = 0;
    }
}

/* SpriteUtilHasAnimationEnded for another sprite of the family. */
static bool animation_ended(const Ctx *c, const MzmSprite *s) {
    if (!c->kind || s->anim >= MZM_SPRITE_MAX_ANIMS) return false;
    unsigned next = s->frame + 1u;
    uint8_t current = s->frame < MZM_SPRITE_MAX_FRAMES ? c->kind->timers[s->anim][s->frame] : 0;
    uint8_t following = next < MZM_SPRITE_MAX_FRAMES ? c->kind->timers[s->anim][next] : 0;
    return current < (uint8_t)(s->adc + 1) && (next >= c->kind->counts[s->anim] || following == 0);
}

/* MotherBrainUpdatePalette: the charge's palette rows. */
static void update_palette(MzmSprite *s) {
    if (s->scaling != 0) {
        s->scaling--;
        s->palette_row = dynamic_palette[s->rotation < 19 ? s->rotation : 0][0];
        return;
    }
    uint8_t offset = (uint8_t)s->rotation++;
    uint8_t row = dynamic_palette[offset < 19 ? offset : 18][0];
    if (row == 128) {
        s->rotation = 1;
        offset = 0;
        row = dynamic_palette[0][0];
    }
    uint8_t timer = dynamic_palette[offset][1];
    if (offset == 0 || offset == 6) mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_CHARGING);
    s->palette_row = row;
    s->scaling = timer;
}

/* MotherBrainInit */
static void mother_brain_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (ai_check_event(c, MZM_EVENT_ESCAPED_ZEBES) || ai_check_event(c, MZM_EVENT_MOTHER_BRAIN_KILLED)) {
        s->status = 0;
        return;
    }
    s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
    g->sub1.y = s->y;
    g->sub1.x = s->x;
    g->boss_work1 = s->y;
    g->boss_work2 = s->x;
    g->boss_work3 = g->boss_work4 = g->boss_work5 = 0;
    s->draw_top = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE * 3 / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE);
    s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->hitbox_left = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE * 2;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    s->immune_to_projectiles = true;
    s->work0 = s->work1 = s->work2 = 0;
    s->scaling = 0;
    s->rotation = 0;
    g->sub1.multi = 0;
    g->sub1.adc = 0;
    g->sub1.frame = 0;
    g->sub1.work3 = STAGE_IN_GLASS;
    s->pose = POSE_WAITING_GLASS;
    s->room_slot = PART_BODY;
    int n = ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_PART, PART_BEAM_SHOOTER, s->gfx_slot, g->sub1.y, g->sub1.x, 0);
    g->sub1.work4 = (uint8_t)(n < 0 ? 0xFF : n);
    n = ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_PART, PART_EYE, s->gfx_slot, g->sub1.y, g->sub1.x, 0);
    g->sub1.work5 = (uint8_t)(n < 0 ? 0xFF : n);
    n = ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_PART, PART_BOTTOM, s->gfx_slot, g->sub1.y, g->sub1.x, 0);
    g->sub1.work6 = (uint8_t)(n < 0 ? 0xFF : n);
}

/* The native stun palette row 0xE in the runtime's numbering (one below,
 * from the sprite's graphics slot). */
static uint8_t stun_row(const MzmSprite *s) { return (uint8_t)(14 - s->gfx_slot); }

/* The eye closes and turns immune. */
static void close_eye(MzmSprite *eye) {
    ai_set_anim(eye, MZM_MOTHER_BRAIN_EYE_CLOSING);
    eye->immune_to_projectiles = true;
    mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_EYE_CLOSING);
}

/* MotherBrainHandler */
static void handler(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSprite *shooter = slot(c, g->sub1.work4), *eye = slot(c, g->sub1.work5), *bottom = slot(c, g->sub1.work6);
    if (!shooter || !eye || !bottom) return;
    uint8_t palette = eye->palette_row;
    s->palette_row = palette;
    bottom->palette_row = palette;
    if (eye->health == 0) {
        /* Dying */
        s->pose = POSE_DYING;
        ai_set_anim(s, MZM_MOTHER_BRAIN_CHARGING_BEAM);
        s->isft = 120;              /* CONVERT_SECONDS(2.f) */
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->health = 1;
        s->palette_row = eye->palette_row = bottom->palette_row = 0;
        ai_set_anim(eye, MZM_MOTHER_BRAIN_EYE_DYING);
        eye->samus_collision = SSC_NONE;
        shooter->status = 0;
        g->sub1.work3 = STAGE_DYING;
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->set_event) h->set_event(h->context, MZM_EVENT_MOTHER_BRAIN_KILLED);
        mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_DYING);
        return;
    }
    if (s->anim == MZM_MOTHER_BRAIN_CHARGING_BEAM) {
        if (--s->work0 == 0) ai_set_anim(s, MZM_MOTHER_BRAIN_IDLE);
    } else {
        if (s->frame == 1 && s->adc == 1) mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_IDLE);
        if ((eye->isft & 0x7F) == 16) {
            mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_DAMAGED);
            if (eye->health < g->boss_work4) {
                /* Enough damage closes the eye. */
                g->boss_work5 = (uint16_t)(g->boss_work5 + (g->boss_work4 - eye->health));
                g->boss_work4 = eye->health;
                uint16_t threshold = c->f->difficulty == 0 ? DAMAGE_THRESHOLD * 2
                                     : c->f->difficulty == 2 ? DAMAGE_THRESHOLD * 6 / 10 : DAMAGE_THRESHOLD;
                if (g->boss_work5 >= threshold) {
                    g->boss_work5 = 0;
                    close_eye(eye);
                    s->work2 = 1;
                }
            }
        }
    }
    if (eye->anim == MZM_MOTHER_BRAIN_EYE_CLOSING) {
        if (animation_ended(c, eye)) ai_set_anim(eye, MZM_MOTHER_BRAIN_EYE_CLOSED);
    } else if (eye->anim == MZM_MOTHER_BRAIN_EYE_CLOSED) {
        /* Samus above the blocks: it charges its beam. */
        if ((uint16_t)(g->sub1.y + BLOCK_SIZE * 2) >= c->f->samus_y && --s->work2 == 0) {
            ai_set_anim(s, MZM_MOTHER_BRAIN_CHARGING_BEAM);
            s->work1 = s->work0 = 72;   /* CONVERT_SECONDS(1.2f) */
            s->scaling = 0;
            s->rotation = 0;
        }
    } else if (eye->anim == MZM_MOTHER_BRAIN_EYE_OPENING && s->work1 == 0) {
        if ((uint16_t)(g->sub1.y + BLOCK_SIZE * 2) < c->f->samus_y) {
            /* Samus fell below the blocks. */
            close_eye(eye);
            s->work2 = 60;
        }
        return;
    }
    if (s->work1 == 0) return;
    /* The beam */
    if (--s->work1 == 0) {
        if (palette != stun_row(s)) s->palette_row = eye->palette_row = bottom->palette_row = 0;
        return;
    }
    if (s->work1 == 12) {           /* CONVERT_SECONDS(.2f) */
        ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_BEAM, 0, s->gfx_slot, shooter->y,
                           (uint16_t)(shooter->x + BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE - PIXEL_SIZE), 0);
        shooter->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
    } else if (s->work1 == 20) {    /* ONE_THIRD_SECOND */
        ai_set_anim(eye, MZM_MOTHER_BRAIN_EYE_OPENING);
        eye->immune_to_projectiles = false;
        mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_EYE_OPENING);
    } else if (s->work1 == 24) {    /* CONVERT_SECONDS(.4f) */
        ai_set_anim(shooter, MZM_MOTHER_BRAIN_BEAM_SPAWNING);
        shooter->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_SHOOTING);
    }
    if (palette != stun_row(s)) {
        update_palette(s);
        eye->palette_row = bottom->palette_row = s->palette_row;
    }
}

/* MotherBrainDeath */
static void death(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSprite *eye = slot(c, g->sub1.work5), *bottom = slot(c, g->sub1.work6);
    if (s->isft != 0) {
        /* (MakeBackgroundFlash is not ported) */
        if (eye) eye->palette_row = s->palette_row;
        if (bottom) bottom->palette_row = s->palette_row;
        return;
    }
    if (eye) eye->status = 0;
    if (bottom) bottom->status = 0;
    s->pose = POSE_START_ESCAPE;
    s->work0 = 60;
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
    ai_particle_set(c, (uint16_t)(g->sub1.y + BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2),
                    (uint16_t)(g->sub1.x - (BLOCK_SIZE - PIXEL_SIZE)), MZM_PE_MAIN_BOSS_DEATH);
    ai_particle_set(c, (uint16_t)(g->sub1.y + BLOCK_SIZE - PIXEL_SIZE),
                    (uint16_t)(g->sub1.x + BLOCK_SIZE + QUARTER_BLOCK_SIZE), MZM_PE_MAIN_BOSS_DEATH);
    mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_DEATH_EXPLOSION);
}

/* MotherBrainStartEscape */
static void start_escape(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 != 0) return;
    s->status = 0;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->spawn_message) h->spawn_message(h->context, MESSAGE_ZEBES_ESCAPE);
    /* (PSPRITE_EXPLOSION_ZEBES_ESCAPE is not ported) */
    mzm_sfx_music_play(MZM_MUSIC_ESCAPE, 0x40);
    mzm_sfx_play(MZM_SOUND_ESCAPE_BEEP);
    c->f->globals->sub1.work3 = STAGE_DEAD;
}

/* MotherBrain */
static void mother_brain(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: mother_brain_init(c); break;
    case POSE_WAITING_GLASS:
        /* MotherBrainCheckGlassBroke */
        if (g->sub1.work3 == STAGE_ACTIVE) {
            MzmSprite *eye = slot(c, g->sub1.work5);
            s->pose = POSE_MAIN_LOOP;
            g->boss_work4 = s->health;
            if (eye) {
                eye->health = s->health;
                ai_set_anim(eye, MZM_MOTHER_BRAIN_EYE_OPENING);
            }
            mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_EYE_OPENING);
        }
        break;
    case POSE_MAIN_LOOP: handler(c); break;
    case POSE_DYING: death(c); break;
    case POSE_START_ESCAPE: start_escape(c); break;
    default: break;
    }
    update_sub1_anim(c);
    if (s->pose < PART_POSE_IDLE) sync_sub_sprites(c);
    else sync_position(c);
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN && g->sub1.work3 < STAGE_DEAD &&
        c->f->samus_x < (uint16_t)(g->sub1.x + BLOCK_SIZE * 12)) {
        g->lock_screen = true;
        g->lock_y = g->boss_work1;
        g->lock_x = (uint16_t)(g->sub1.x + BLOCK_SIZE * 5);
        /* Samus kept out of it (bigger while in its glass). */
        uint16_t size = g->sub1.work3 == STAGE_IN_GLASS ? BLOCK_SIZE * 3 + HALF_BLOCK_SIZE - PIXEL_SIZE
                                                        : BLOCK_SIZE * 2 + HALF_BLOCK_SIZE;
        uint16_t limit = (uint16_t)(g->boss_work2 + size);
        if (link && link->x < limit) {
            link->x = limit;
            link->moved = true;
        }
    } else {
        g->lock_screen = false;
    }
}

/* BgClipCallMotherBrainUpdateGlass: the glass's BG0 blocks of a stage. */
static void update_glass(const Ctx *c, uint8_t stage) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (c->f->area != AREA_TOURIAN || c->f->room != 4 || !h || !h->set_bg_block) return;
    for (uint16_t i = 0; i < 5; ++i)
        for (uint16_t j = 0; j < 7; ++j)
            h->set_bg_block(h->context, 0, (uint16_t)(glass_tiles[stage] + i * 16 + j), (uint16_t)(i + 13),
                            (uint16_t)(j + 4));
}

static void set_box(MzmSprite *s, int16_t top, int16_t bottom, int16_t left, int16_t right) {
    s->hitbox_top = top;
    s->hitbox_bottom = bottom;
    s->hitbox_left = left;
    s->hitbox_right = right;
}

/* MotherBrainPartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->room_slot) {
    case PART_BEAM_SHOOTER:
        s->draw_top = s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 4 / PIXEL_SIZE;
        set_box(s, -PIXEL_SIZE, PIXEL_SIZE, -PIXEL_SIZE, PIXEL_SIZE);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_order = 3;
        s->pose = PART_POSE_UPDATE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->immune_to_projectiles = true;
        s->health = 1;
        break;
    case PART_EYE:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        set_box(s, -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE), HALF_BLOCK_SIZE, -HALF_BLOCK_SIZE, PIXEL_SIZE);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_order = 5;
        s->pose = PART_POSE_IDLE;
        break;
    case PART_BOTTOM:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        set_box(s, -(BLOCK_SIZE * 4 - EIGHTH_BLOCK_SIZE), HALF_BLOCK_SIZE, -(BLOCK_SIZE * 2 + THREE_QUARTER_BLOCK_SIZE),
                BLOCK_SIZE * 2 + THREE_QUARTER_BLOCK_SIZE);
        s->samus_collision = SSC_NONE;
        s->draw_order = 6;
        s->pose = PART_POSE_SPAWN_BLOCK;
        s->health = c->tables->secondary_health[s->sprite_id];
        c->f->globals->boss_work3 = s->health;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    default:
        s->status = 0;
        break;
    }
}

/* MotherBrainPartHitboxInit: the beam shooter's hitbox by frame. */
static void shooter_hitbox(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
        s->ignore_samus_timer = 1;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        return;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    switch (s->frame) {
    case 1: set_box(s, -QUARTER_BLOCK_SIZE, QUARTER_BLOCK_SIZE, HALF_BLOCK_SIZE, BLOCK_SIZE + HALF_BLOCK_SIZE); break;
    case 2: set_box(s, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE * 2 + HALF_BLOCK_SIZE); break;
    case 3:
        set_box(s, -THREE_QUARTER_BLOCK_SIZE, THREE_QUARTER_BLOCK_SIZE, BLOCK_SIZE * 2 + HALF_BLOCK_SIZE,
                BLOCK_SIZE * 3 + HALF_BLOCK_SIZE);
        break;
    default: set_box(s, -EIGHTH_BLOCK_SIZE, EIGHTH_BLOCK_SIZE, -PIXEL_SIZE, HALF_BLOCK_SIZE); break;
    }
}

/* The glass stages: a quarter of the bottom's health each. */
static void glass_stage(Ctx *c, uint16_t threshold, uint8_t next_pose, uint8_t stage, uint16_t sound) {
    MzmSprite *s = c->s;
    s->isft = 0;
    if (s->health <= threshold) {
        s->pose = next_pose;
        update_glass(c, stage);
        mzm_sfx_play(sound);
    }
}

/* MotherBrainPart */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        part_init(c);
        if (s->status) sync_sub_sprites(c);
        return;
    case PART_POSE_UPDATE: shooter_hitbox(s); break;
    case PART_POSE_IDLE: break;
    case PART_POSE_SPAWN_BLOCK: {
        /* MotherBrainSpawnBlock: once Samus is left of the block's place. */
        uint16_t y = (uint16_t)(g->sub1.y - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE));
        uint16_t x = (uint16_t)(g->sub1.x + (BLOCK_SIZE * 11 + HALF_BLOCK_SIZE));
        if (c->f->samus_x < (uint16_t)(x - (BLOCK_SIZE - PIXEL_SIZE))) {
            ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_BLOCK, 0, GFX_SLOT_SPECIAL, y, x, 0);
            s->pose = PART_POSE_GLASS_STAGE_1;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
        break;
    }
    case PART_POSE_GLASS_STAGE_1:
        glass_stage(c, (uint16_t)(g->boss_work3 * 3 / 4), PART_POSE_GLASS_STAGE_2, 1, MZM_SOUND_MOTHER_BRAIN_JAR_DAMAGE_1);
        break;
    case PART_POSE_GLASS_STAGE_2:
        glass_stage(c, (uint16_t)(g->boss_work3 / 2), PART_POSE_GLASS_STAGE_3, 2, MZM_SOUND_MOTHER_BRAIN_JAR_DAMAGE_2);
        break;
    case PART_POSE_GLASS_STAGE_3:
        glass_stage(c, (uint16_t)(g->boss_work3 / 4), PART_POSE_GLASS_BROKEN, 3, MZM_SOUND_MOTHER_BRAIN_JAR_DAMAGE_3);
        break;
    case PART_POSE_GLASS_BROKEN: s->isft = 0; break;
    case PART_POSE_GLASS_BREAKING:
        if (s->room_slot == PART_BOTTOM) {
            /* MotherBrainPartSpawnGlassBreaking */
            s->isft = 0;
            s->pose = PART_POSE_GLASS_BROKEN;
            g->sub1.work3 = STAGE_ACTIVE;
            ai_spawn_secondary(c, MZM_SSPRITE_MOTHER_BRAIN_GLASS_BREAKING, 0, s->gfx_slot, g->sub1.y, g->sub1.x, 0);
            update_glass(c, 4);
            mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_JAR_SHATTER);
        }
        break;
    default: break;
    }
    if (s->room_slot == PART_BOTTOM) sync_sub_sprites(c);
    else sync_position(c);
}

/* MotherBrainBeam */
static void beam(Ctx *c) {
    MzmSprite *s = c->s;
    if (c->f->globals->sub1.work3 >= STAGE_DYING) {
        s->ignore_samus_timer = 1;
        s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->immune_to_projectiles = true;
        s->draw_top = s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 3 / PIXEL_SIZE;
        set_box(s, -THREE_QUARTER_BLOCK_SIZE, THREE_QUARTER_BLOCK_SIZE, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);
        ai_set_anim(s, MZM_MOTHER_BRAIN_BEAM_MOVING);
        s->pose = BEAM_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY;
        s->draw_order = 3;
        s->health = 1;
        s->work0 = 16;
    }
    if (s->pose != BEAM_POSE_MOVING) return;
    s->x = (uint16_t)(s->x + QUARTER_BLOCK_SIZE - PIXEL_SIZE);
    if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) {
        ai_particle_set(c, (uint16_t)(s->y + (HALF_BLOCK_SIZE - PIXEL_SIZE)), s->x, MZM_PE_SPRITE_EXPLOSION_BIG);
        s->status = 0;
        mzm_sfx_play(MZM_SOUND_MOTHER_BRAIN_LASER_EXPLODING);
    }
}

/* MotherBrainBlock: it appears once Samus is out of it, a block she can't
 * grab. */
static void block(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        set_box(s, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);
        ai_set_anim(s, MZM_MOTHER_BRAIN_BLOCK);
        s->pose = BLOCK_POSE_IDLE;
        s->samus_collision = SSC_CHECK_COLLIDING;
        return;
    }
    if (!(s->status & MZM_SPRITE_STATUS_NOT_DRAWN)) return;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        return;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->samus_collision = SSC_NONE;
    s->adc = 0;
    s->frame = 0;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, s->y, s->x, 4);    /* CAA_MAKE_NON_POWER_GRIP */
}

/* MotherBrainGlassBreaking */
static void glass_breaking(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 5 / PIXEL_SIZE;
        set_box(s, 0, 0, 0, 0);
        ai_set_anim(s, MZM_MOTHER_BRAIN_GLASS_BREAKING);
        s->pose = GLASS_POSE_BREAKING;
        s->samus_collision = SSC_NONE;
        s->draw_order = 3;
        return;
    }
    if (s->pose == GLASS_POSE_BREAKING && ai_animation_ending(c, 1)) s->status = 0;
}

void ai_mother_brain(Ctx *c) {
    if (!c->f->globals) return;
    if (!c->s->secondary) mother_brain(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MOTHER_BRAIN_PART) part(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MOTHER_BRAIN_BEAM) beam(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MOTHER_BRAIN_BLOCK) block(c);
    else glass_breaking(c);
}
