#include "core/save.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t payload_size;
} SaveHeader;

static bool fail(char *error, size_t size, const char *message)
{
    if (size > 0)
        snprintf(error, size, "%s", message);
    return false;
}

bool save_session(const char *path, const SessionState *session,
                  char *error, size_t error_size)
{
    static const char magic[8] = {'F','U','S','I','O','N','1','\0'};
    SaveHeader header;
    FILE *file;

    memcpy(header.magic, magic, sizeof(magic));
    header.version = FUSION_SAVE_VERSION;
    header.payload_size = (uint32_t)sizeof(*session);
    file = fopen(path, "wb");
    if (file == NULL)
        return fail(error, error_size, strerror(errno));
    if (fwrite(&header, sizeof(header), 1, file) != 1 ||
        fwrite(session, sizeof(*session), 1, file) != 1) {
        fclose(file);
        return fail(error, error_size, "incomplete save write");
    }
    if (fclose(file) != 0)
        return fail(error, error_size, strerror(errno));
    return true;
}

bool load_session(const char *path, SessionState *session,
                  char *error, size_t error_size)
{
    static const char magic[8] = {'F','U','S','I','O','N','1','\0'};
    SaveHeader header;
    SessionState loaded;
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return fail(error, error_size, strerror(errno));
    if (fread(&header, sizeof(header), 1, file) != 1 ||
        memcmp(header.magic, magic, sizeof(magic)) != 0 ||
        header.version != FUSION_SAVE_VERSION ||
        header.payload_size != sizeof(loaded)) {
        fclose(file);
        return fail(error, error_size, "unknown or incompatible save format");
    }
    if (fread(&loaded, sizeof(loaded), 1, file) != 1) {
        fclose(file);
        return fail(error, error_size, "truncated save file");
    }
    /* Reject unexpected trailing bytes (including partial extra records). */
    if (fgetc(file) != EOF || ferror(file)) {
        fclose(file);
        return fail(error, error_size, "trailing or unreadable save data");
    }
    if (fclose(file) != 0)
        return fail(error, error_size, "save close failed");
    if (loaded.version != FUSION_SAVE_VERSION ||
        loaded.active_world >= FUSION_WORLD_COUNT ||
        loaded.active_character >= FUSION_CHARACTER_COUNT)
        return fail(error, error_size, "invalid save values");
    for (size_t i = 0; i < FUSION_CHARACTER_COUNT; ++i) {
        const CharacterState *c = &loaded.characters[i];
        if (c->max_hp <= 0 || c->hp < 0 || c->hp > c->max_hp ||
            c->available != (c->hp > 0))
            return fail(error, error_size, "invalid character health");
    }
    for (size_t i = 0; i < FUSION_WORLD_COUNT; ++i) {
        const WorldState *w = &loaded.worlds[i];
        if (w->target_max_hp <= 0 || w->target_hp < 0 ||
            w->target_hp > w->target_max_hp)
            return fail(error, error_size, "invalid world target health");
    }
    if (!loaded.characters[loaded.active_character].available &&
        (loaded.characters[CHARACTER_SAMUS].available ||
         loaded.characters[CHARACTER_SOMA].available))
        return fail(error, error_size, "active character unavailable");
    *session = loaded;
    return true;
}
