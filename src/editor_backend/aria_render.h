/* SPDX-License-Identifier: GPL-3.0-only */
/* The Aria room renderer and editable workroom export of the editor. */
#ifndef FUSION_EDITOR_BACKEND_ARIA_RENDER_H
#define FUSION_EDITOR_BACKEND_ARIA_RENDER_H

#include "common.h"

#include "fusion/assets/rom.h"

/* render_room(): the render report, writing the previews on request. */
bool feb_aria_render_room(const char *repository, int64_t area, int64_t number,
                          bool write_outputs, FusionJson *result, FebError *error);
/* The verified ROM and its native world catalog (MV_AOS_WORLD_2). */
bool feb_aria_open_world(const char *repository, FusionRom *rom, FusionJson *world,
                         FebError *error);
/* render_room() against an opened ROM and world catalog. */
bool feb_aria_render_loaded(const char *repository, const FusionRom *rom,
                            const FusionJson *world, int64_t area, int64_t number,
                            bool write_outputs, FusionJson *result, FebError *error);
/* export(): the workroom, atlas, BG3 and collision previews and the room
 * annotations; ``info`` is the workspace description. */
bool feb_aria_workspace_export(const char *repository, int64_t area, int64_t number,
                               FusionJson *info, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_ARIA_RENDER_H */
