/* SPDX-License-Identifier: GPL-3.0-only */
/* The runtime's Zero Mission save file (see mzm_save.h). One field walk
 * serves both directions: the header "MVZMSAVE", a version, the payload
 * length, the payload and its CRC-32. */
#include "mzm_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VERSION = 2, HEADER = 8 + 4 + 4, TRAILER = 4 };
static const char MAGIC[8] = {'M', 'V', 'Z', 'M', 'S', 'A', 'V', 'E'};

typedef struct {
    uint8_t *bytes;         /* NULL while measuring */
    const uint8_t *in;      /* decoding */
    size_t at, size;
    bool ok;
    uint32_t version;
} Io;

static void raw(Io *io, void *value, size_t size) {
    if (io->in) {
        if (io->at + size > io->size) {
            io->ok = false;
            memset(value, 0, size);
        } else {
            memcpy(value, io->in + io->at, size);
        }
    } else if (io->bytes) {
        memcpy(io->bytes + io->at, value, size);
    }
    io->at += size;
}

/* Little-endian integers of `size` bytes (1, 2 or 4). */
static void integer(Io *io, void *value, size_t size) {
    uint8_t buffer[4] = {0};
    uint32_t v = 0;
    if (!io->in) {
        if (size == 1) v = *(uint8_t *)value;
        else if (size == 2) v = *(uint16_t *)value;
        else v = *(uint32_t *)value;
        for (size_t i = 0; i < size; ++i) buffer[i] = (uint8_t)(v >> (8 * i));
    }
    raw(io, buffer, size);
    if (!io->in) return;
    for (size_t i = 0; i < size; ++i) v |= (uint32_t)buffer[i] << (8 * i);
    if (size == 1) *(uint8_t *)value = (uint8_t)v;
    else if (size == 2) *(uint16_t *)value = (uint16_t)v;
    else *(uint32_t *)value = v;
}

static void u8(Io *io, uint8_t *v) { integer(io, v, 1); }
static void i8(Io *io, int8_t *v) { integer(io, v, 1); }
static void u32(Io *io, uint32_t *v) { integer(io, v, 4); }
static void i32(Io *io, int32_t *v) { integer(io, v, 4); }

static void flag(Io *io, bool *v) {
    uint8_t b = *v ? 1 : 0;
    u8(io, &b);
    *v = b != 0;
}

static void int_field(Io *io, int *v) {
    int32_t x = (int32_t)*v;
    i32(io, &x);
    *v = (int)x;
}

static void text(Io *io, char *s, size_t size) {
    raw(io, s, size);
    s[size - 1] = '\0';
}

static void block(Io *io, MzmSaveRoomBlock *b) {
    text(io, b->alias, sizeof b->alias);
    i32(io, &b->x);
    i32(io, &b->y);
}

static void equipment(Io *io, MzmEquipment *e) {
    int32_t suit = (int32_t)e->suit;
    i32(io, &suit);
    e->suit = (MzmSuitType)suit;
    u32(io, &e->items);
    int_field(io, &e->energy);
    int_field(io, &e->max_energy);
    int_field(io, &e->missiles);
    int_field(io, &e->max_missiles);
    int_field(io, &e->super_missiles);
    int_field(io, &e->max_super_missiles);
    int_field(io, &e->power_bombs);
    int_field(io, &e->max_power_bombs);
    u32(io, &e->unknown_items);
}

static void visit(Io *io, MzmSaveData *d) {
    text(io, d->room, sizeof d->room);
    i32(io, &d->samus_x);
    i32(io, &d->samus_y);
    i8(io, &d->facing);
    equipment(io, &d->equipment);
    raw(io, d->events, sizeof d->events);
    for (int a = 0; a < MZM_HUD_AREAS; ++a)
        for (int i = 0; i < MZM_MINIMAP_SIZE; ++i) {
            u32(io, &d->minimap.visited[a][i]);
            u32(io, &d->minimap.obtained[a][i]);
        }
    u8(io, &d->minimap.downloaded_maps);
    for (int a = 0; a < MZM_MAX_AREAS; ++a) {
        int_field(io, &d->items.count[a]);
        for (int i = 0; i < MZM_MAX_ITEMS_PER_AREA; ++i) {
            u8(io, &d->items.items[a][i].room);
            u8(io, &d->items.items[a][i].type);
            u8(io, &d->items.items[a][i].x);
            u8(io, &d->items.items[a][i].y);
        }
    }
    for (int a = 0; a < MZM_SAVE_HATCH_AREAS; ++a)
        for (int i = 0; i < 8; ++i) u32(io, &d->hatches_opened[a][i]);
    i32(io, &d->hatch_destination_count);
    for (int i = 0; i < MZM_SAVE_HATCH_DESTINATIONS; ++i) block(io, &d->hatch_destinations[i]);
    i32(io, &d->never_reform_count);
    for (int i = 0; i < MZM_SAVE_NEVER_REFORM; ++i) block(io, &d->never_reform[i]);
    flag(io, &d->escaped_zebes);
    if (io->version < 2) {
        d->area = 0;
        d->difficulty = 1;
        return;
    }
    u8(io, &d->area);
    u8(io, &d->difficulty);
    u8(io, &d->igt.hours);
    u8(io, &d->igt.minutes);
    u8(io, &d->igt.seconds);
    u8(io, &d->igt.frames);
    flag(io, &d->max_igt);
    u8(io, &d->completed_game);
}

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) crc = crc >> 1 ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint8_t *mzm_save_encode(const MzmSaveData *data, size_t *size) {
    MzmSaveData copy = *data;
    Io measure = {NULL, NULL, 0, 0, true, VERSION};
    visit(&measure, &copy);
    size_t total = HEADER + measure.at + TRAILER;
    uint8_t *bytes = malloc(total);
    if (!bytes) return NULL;
    memcpy(bytes, MAGIC, sizeof MAGIC);
    put32(bytes + 8, VERSION);
    put32(bytes + 12, (uint32_t)measure.at);
    Io io = {bytes + HEADER, NULL, 0, 0, true, VERSION};
    visit(&io, &copy);
    put32(bytes + HEADER + measure.at, crc32(bytes + HEADER, measure.at));
    *size = total;
    return bytes;
}

bool mzm_save_decode(const uint8_t *bytes, size_t size, MzmSaveData *data) {
    if (size < HEADER + TRAILER || memcmp(bytes, MAGIC, sizeof MAGIC) != 0) return false;
    uint32_t version = get32(bytes + 8);
    if (version < 1 || version > VERSION) return false;
    size_t payload = get32(bytes + 12);
    if (payload != size - HEADER - TRAILER) return false;
    if (crc32(bytes + HEADER, payload) != get32(bytes + HEADER + payload)) return false;
    MzmSaveData parsed;
    memset(&parsed, 0, sizeof parsed);
    Io io = {NULL, bytes + HEADER, 0, payload, true, version};
    visit(&io, &parsed);
    if (!io.ok || io.at != payload) return false;
    if (parsed.hatch_destination_count < 0 || parsed.hatch_destination_count > MZM_SAVE_HATCH_DESTINATIONS ||
        parsed.never_reform_count < 0 || parsed.never_reform_count > MZM_SAVE_NEVER_REFORM)
        return false;
    for (int a = 0; a < MZM_MAX_AREAS; ++a)
        if (parsed.items.count[a] < 0 || parsed.items.count[a] > MZM_MAX_ITEMS_PER_AREA) return false;
    *data = parsed;
    return true;
}

bool mzm_save_write(const char *path, const MzmSaveData *data) {
    size_t size = 0;
    uint8_t *bytes = mzm_save_encode(data, &size);
    if (!bytes) return false;
    char temporary[4096];
    int n = snprintf(temporary, sizeof temporary, "%s.tmp", path);
    bool ok = n > 0 && (size_t)n < sizeof temporary;
    FILE *file = ok ? fopen(temporary, "wb") : NULL;
    ok = file && fwrite(bytes, 1, size, file) == size;
    if (file && fclose(file) != 0) ok = false;
    free(bytes);
    if (ok) ok = rename(temporary, path) == 0;
    else if (file) remove(temporary);
    return ok;
}

bool mzm_save_read(const char *path, MzmSaveData *data) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    uint8_t *bytes = NULL;
    size_t size = 0;
    bool ok = fseek(file, 0, SEEK_END) == 0;
    long length = ok ? ftell(file) : -1;
    ok = length > 0 && length < (16L << 20) && fseek(file, 0, SEEK_SET) == 0;
    if (ok) {
        size = (size_t)length;
        bytes = malloc(size);
        ok = bytes && fread(bytes, 1, size, file) == size;
    }
    fclose(file);
    ok = ok && mzm_save_decode(bytes, size, data);
    free(bytes);
    return ok;
}
