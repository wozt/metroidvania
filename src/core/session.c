#include "core/session.h"

#include <string.h>

const char *world_name(WorldKind world)
{
    return world == WORLD_METROID ? "Metroid" : "Castlevania";
}

const char *character_name(CharacterKind character)
{
    return character == CHARACTER_SAMUS ? "Samus" : "Soma";
}

void session_init(SessionState *session)
{
    memset(session, 0, sizeof(*session));
    session->version = FUSION_SAVE_VERSION;
    session->active_world = WORLD_METROID;
    session->active_character = CHARACTER_SAMUS;

    session->characters[CHARACTER_SAMUS] = (CharacterState) {
        .hp = 99, .max_hp = 99, .available = true
    };
    session->characters[CHARACTER_SOMA] = (CharacterState) {
        .hp = 120, .max_hp = 120, .available = true
    };

    session->worlds[WORLD_METROID] = (WorldState) {
        .actor_x = 80.0f, .actor_y = 420.0f,
        .target_hp = 100, .target_max_hp = 100
    };
    session->worlds[WORLD_CASTLEVANIA] = (WorldState) {
        .actor_x = 90.0f, .actor_y = 420.0f,
        .target_hp = 140, .target_max_hp = 140
    };
    session->synergy_placeholder = 25;
}

bool session_damage_active(SessionState *session, int32_t damage)
{
    CharacterState *character = &session->characters[session->active_character];
    CharacterKind other;

    if (damage <= 0 || !character->available)
        return false;

    character->hp -= damage;
    if (character->hp > 0)
        return true;

    character->hp = 0;
    character->available = false;
    other = session->active_character == CHARACTER_SAMUS ? CHARACTER_SOMA : CHARACTER_SAMUS;
    if (session->characters[other].available)
        session->active_character = other;
    return true;
}

bool session_switch_character(SessionState *session)
{
    CharacterKind next = session->active_character == CHARACTER_SAMUS
        ? CHARACTER_SOMA : CHARACTER_SAMUS;
    if (!session->characters[next].available)
        return false;
    session->active_character = next;
    return true;
}
