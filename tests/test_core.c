#include "core/save.h"
#include "core/session.h"
#include "core/sha1.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_separate_health_and_ko(void)
{
    SessionState session;
    session_init(&session);
    assert(session_damage_active(&session, 20));
    assert(session.characters[CHARACTER_SAMUS].hp == 79);
    assert(session.characters[CHARACTER_SOMA].hp == 120);
    assert(session_damage_active(&session, 100));
    assert(!session.characters[CHARACTER_SAMUS].available);
    assert(session.active_character == CHARACTER_SOMA);
    assert(!session_switch_character(&session));
}

static void test_character_switch_preserves_world(void)
{
    SessionState session;
    session_init(&session);
    session.worlds[WORLD_METROID].target_hp = 31;
    assert(session_switch_character(&session));
    assert(session.active_character == CHARACTER_SOMA);
    assert(session.worlds[WORLD_METROID].target_hp == 31);
}

static void test_world_persistence_and_save(void)
{
    const char *path = "/tmp/fusion-core-test-save.bin";
    SessionState session;
    SessionState loaded;
    char error[128];
    session_init(&session);
    session.worlds[WORLD_METROID].target_hp = 42;
    session.worlds[WORLD_CASTLEVANIA].door_open = true;
    session.active_world = WORLD_CASTLEVANIA;
    assert(save_session(path, &session, error, sizeof(error)));
    memset(&loaded, 0, sizeof(loaded));
    assert(load_session(path, &loaded, error, sizeof(error)));
    assert(loaded.worlds[WORLD_METROID].target_hp == 42);
    assert(loaded.worlds[WORLD_CASTLEVANIA].door_open);
    assert(loaded.active_world == WORLD_CASTLEVANIA);
    unlink(path);
}

int main(void)
{
    test_separate_health_and_ko();
    test_character_switch_preserves_world();
    test_world_persistence_and_save();
    puts("Tous les tests coeur sont passés.");
    return 0;
}
