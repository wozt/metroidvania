/* SPDX-License-Identifier: GPL-3.0-only */
/* Weapon and equipment loaders of the Aria of Sorrow room runtime,
 * moved verbatim out of aria_runtime.c (AGENTS.md rule 8,
 * decomposition wave 5). See aria_weapons.h. */
#include "aria_weapons.h"

#include <stdio.h>
#include <string.h>

/* weapons.tsv of src/assets/aos/weapons.c: "none" is the unarmed record. */
bool load_weapon(const char *path, const char *name, AosWeapon *weapon) {
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

bool load_weapon_sprite(const char *name, AriaWeaponSprite *out, SDL_Renderer *renderer) {
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

void free_weapon_sprite(AriaWeaponSprite *sprite) {
    for (int i = 0; i < sprite->count; ++i)
        if (sprite->frames[i].texture) SDL_DestroyTexture(sprite->frames[i].texture);
}

/* Apply the equipment menu's weapon choice to the same runtime objects used
 * at launch. A missing sprite keeps the record equipped but disables its
 * still-unported weapon entity instead of retaining the previous art. */
bool reload_equipped_weapon(uint8_t item, AosSoma *soma,
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
