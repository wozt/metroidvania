#include "core/room_sim.h"

#include "core/session.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool overlaps(SDL_FRect a, SDL_FRect b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

static SDL_FRect actor_rect(const RoomRuntime *runtime, const WorldState *world,
                            CharacterKind character)
{
    float width = runtime->definition.actor_width[character];
    float height = runtime->definition.actor_height[character];
    return (SDL_FRect) {world->actor_x - width * 0.5f, world->actor_y - height,
                        width, height};
}

static bool actor_collides(const RoomRuntime *runtime, SDL_FRect actor)
{
    int i;
    for (i = 0; i < runtime->definition.solid_count; ++i) {
        if (overlaps(actor, runtime->definition.solids[i]))
            return true;
    }
    return false;
}

static bool find_safe_position(RoomRuntime *runtime, WorldState *world,
                               CharacterKind character)
{
    static const float offsets[] = {0, -12, 12, -24, 24, -48, 48, -80, 80};
    float original = world->actor_x;
    size_t i;
    for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        world->actor_x = original + offsets[i];
        if (!actor_collides(runtime, actor_rect(runtime, world, character)))
            return true;
    }
    world->actor_x = original;
    return false;
}

static void set_notice(RoomRuntime *runtime, const char *message)
{
    snprintf(runtime->notice, sizeof(runtime->notice), "%s", message);
    runtime->notice_time = 2.0f;
}

void room_runtime_init(RoomRuntime *runtime, const RoomDefinition *definition)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->definition = *definition;
}

void room_enter(RoomRuntime *runtime, WorldState *world)
{
    runtime->active = true;
    runtime->on_ground = false;
    world->visited = true;
    set_notice(runtime, runtime->definition.title);
}

void room_leave(RoomRuntime *runtime, WorldState *world)
{
    (void)world;
    runtime->active = false;
}

static void move_horizontal(RoomRuntime *runtime, WorldState *world,
                            CharacterKind character, float dt)
{
    SDL_FRect actor;
    int i;
    world->actor_x += world->actor_vx * dt;
    actor = actor_rect(runtime, world, character);
    for (i = 0; i < runtime->definition.solid_count; ++i) {
        SDL_FRect solid = runtime->definition.solids[i];
        if (!overlaps(actor, solid)) continue;
        if (world->actor_vx > 0)
            world->actor_x = solid.x - actor.w * 0.5f;
        else if (world->actor_vx < 0)
            world->actor_x = solid.x + solid.w + actor.w * 0.5f;
        world->actor_vx = 0;
        actor = actor_rect(runtime, world, character);
    }
}

static void move_vertical(RoomRuntime *runtime, WorldState *world,
                          CharacterKind character, float dt)
{
    SDL_FRect actor;
    int i;
    runtime->on_ground = false;
    world->actor_y += world->actor_vy * dt;
    actor = actor_rect(runtime, world, character);
    for (i = 0; i < runtime->definition.solid_count; ++i) {
        SDL_FRect solid = runtime->definition.solids[i];
        if (!overlaps(actor, solid)) continue;
        if (world->actor_vy > 0) {
            world->actor_y = solid.y;
            runtime->on_ground = true;
        } else if (world->actor_vy < 0) {
            world->actor_y = solid.y + solid.h + actor.h;
        }
        world->actor_vy = 0;
        actor = actor_rect(runtime, world, character);
    }
}

void room_tick(RoomRuntime *runtime, SessionState *session,
               const FusionInput *input, float dt)
{
    WorldState *world = &session->worlds[session->active_world];
    CharacterKind character = session->active_character;
    float direction = (input->right ? 1.0f : 0.0f) - (input->left ? 1.0f : 0.0f);
    SDL_FRect actor;

    if (!runtime->active) return;
    if (runtime->notice_time > 0) runtime->notice_time -= dt;
    if (runtime->attack_flash > 0) runtime->attack_flash -= dt;
    if (runtime->damage_cooldown > 0) runtime->damage_cooldown -= dt;

    if (input->switch_character_pressed) {
        CharacterKind old = character;
        if (session_switch_character(session)) {
            character = session->active_character;
            if (!find_safe_position(runtime, world, character)) {
                session->active_character = old;
                set_notice(runtime, "Changement refuse: aucune position sure");
                character = old;
            } else {
                set_notice(runtime, "Personnage change sans recharger la salle");
            }
        } else {
            set_notice(runtime, "Autre personnage indisponible (KO)");
        }
    }

    world->actor_vx = direction * runtime->definition.move_speed[character];
    if (input->jump_pressed && runtime->on_ground)
        world->actor_vy = -runtime->definition.jump_speed[character];
    world->actor_vy += runtime->definition.gravity * dt;
    move_horizontal(runtime, world, character, dt);
    move_vertical(runtime, world, character, dt);

    actor = actor_rect(runtime, world, character);
    if (world->actor_y > 700) {
        world->actor_x = 80;
        world->actor_y = 420;
        world->actor_vy = 0;
        session_damage_active(session, 15);
        set_notice(runtime, "Chute: degats sur le personnage actif");
    }
    if (overlaps(actor, runtime->definition.hazard) && runtime->damage_cooldown <= 0) {
        session_damage_active(session, 10);
        runtime->damage_cooldown = 0.8f;
        set_notice(runtime, "Obstacle dangereux: -10 PV");
    }
    if (input->damage_pressed) {
        session_damage_active(session, 25);
        set_notice(runtime, "Degats de diagnostic: -25 PV");
    }
    if (input->attack_pressed && world->target_hp > 0) {
        float actor_center = world->actor_x;
        float target_center = runtime->definition.target.x + runtime->definition.target.w * 0.5f;
        if (fabsf(actor_center - target_center) <= runtime->definition.attack_range[character]) {
            int damage = character == CHARACTER_SAMUS ? 14 : 18;
            world->target_hp -= damage;
            if (world->target_hp < 0) world->target_hp = 0;
            runtime->attack_flash = 0.12f;
            set_notice(runtime, world->target_hp == 0 ? "Cible detruite" : "Cible touchee");
        } else {
            set_notice(runtime, "Cible hors de portee");
        }
    }
    if (world->target_hp == 0) world->door_open = true;
}

static void color(SDL_Renderer *renderer, SDL_Color value)
{
    SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
}

static void bar(SDL_Renderer *renderer, float x, float y, float width, float height,
                float ratio, SDL_Color fill)
{
    SDL_FRect back = {x, y, width, height};
    SDL_FRect front = {x + 2, y + 2, (width - 4) * ratio, height - 4};
    color(renderer, (SDL_Color){32, 32, 40, 255}); SDL_RenderFillRect(renderer, &back);
    color(renderer, fill); SDL_RenderFillRect(renderer, &front);
    color(renderer, (SDL_Color){220, 220, 220, 255}); SDL_RenderRect(renderer, &back);
}

void room_render(const RoomRuntime *runtime, const SessionState *session,
                 SDL_Renderer *renderer, bool debug_overlay, float fps)
{
    const WorldState *world = &session->worlds[session->active_world];
    CharacterKind character = session->active_character;
    SDL_FRect actor = actor_rect(runtime, world, character);
    SDL_Color actor_color = character == CHARACTER_SAMUS
        ? (SDL_Color){255, 190, 30, 255} : (SDL_Color){80, 180, 255, 255};
    int i;

    color(renderer, runtime->definition.background); SDL_RenderClear(renderer);
    color(renderer, (SDL_Color){82, 88, 100, 255});
    for (i = 0; i < runtime->definition.solid_count; ++i)
        SDL_RenderFillRect(renderer, &runtime->definition.solids[i]);
    color(renderer, (SDL_Color){190, 55, 55, 255}); SDL_RenderFillRect(renderer, &runtime->definition.hazard);
    color(renderer, world->door_open ? (SDL_Color){55, 210, 110, 255} : (SDL_Color){130, 60, 155, 255});
    SDL_RenderFillRect(renderer, &runtime->definition.portal);
    if (world->target_hp > 0) {
        color(renderer, runtime->attack_flash > 0 ? (SDL_Color){255,255,255,255} : runtime->definition.accent);
        SDL_RenderFillRect(renderer, &runtime->definition.target);
    }
    color(renderer, actor_color); SDL_RenderFillRect(renderer, &actor);

    bar(renderer, 20, 18, 220, 18,
        (float)session->characters[CHARACTER_SAMUS].hp / session->characters[CHARACTER_SAMUS].max_hp,
        (SDL_Color){240, 160, 30, 255});
    bar(renderer, 20, 46, 220, 18,
        (float)session->characters[CHARACTER_SOMA].hp / session->characters[CHARACTER_SOMA].max_hp,
        (SDL_Color){45, 140, 230, 255});
    bar(renderer, 20, 74, 220, 12, session->synergy_placeholder / 100.0f,
        (SDL_Color){180, 80, 210, 255});
    color(renderer, (SDL_Color){245,245,245,255});
    SDL_RenderDebugTextFormat(renderer, 250, 20, "SAMUS %d/%d", session->characters[0].hp, session->characters[0].max_hp);
    SDL_RenderDebugTextFormat(renderer, 250, 48, "SOMA  %d/%d", session->characters[1].hp, session->characters[1].max_hp);
    SDL_RenderDebugText(renderer, 250, 74, "SYNERGIE (FACTICE / NON IMPLEMENTEE)");
    SDL_RenderDebugTextFormat(renderer, 20, 100, "%s | %s | CIBLE %d/%d | PORTE %s",
        runtime->definition.title, character_name(character), world->target_hp,
        world->target_max_hp, world->door_open ? "OUVERTE" : "FERMEE");
    SDL_RenderDebugText(renderer, 20, 116,
        "FLECHES/QD: BOUGER  ESPACE: SAUT  J: ATTAQUE  TAB: PERSO  M: MONDE");
    SDL_RenderDebugText(renderer, 20, 130,
        "F3: DEBUG  K: DEGATS  F5: SAUVER  F9: CHARGER  ECHAP: QUITTER");
    if (runtime->notice_time > 0)
        SDL_RenderDebugText(renderer, 20, 150, runtime->notice);
    if (debug_overlay) {
        color(renderer, (SDL_Color){255,255,255,255}); SDL_RenderRect(renderer, &actor);
        for (i = 0; i < runtime->definition.solid_count; ++i)
            SDL_RenderRect(renderer, &runtime->definition.solids[i]);
        SDL_RenderDebugTextFormat(renderer, 20, 170,
            "DEBUG backend=%s pos=(%.1f,%.1f) vel=(%.1f,%.1f) sol=%s fps=%.1f",
            world_name(session->active_world), world->actor_x, world->actor_y,
            world->actor_vx, world->actor_vy, runtime->on_ground ? "oui" : "non", fps);
    }
}
