/* SPDX-License-Identifier: GPL-3.0-only */
/* The private native workroom commands of the editor backend. */
#ifndef FUSION_EDITOR_BACKEND_WORKSPACE_H
#define FUSION_EDITOR_BACKEND_WORKSPACE_H

#include "common.h"

bool feb_is_workspace_command(const char *command);
/* _require_repository_root(): native workrooms live in the checkout only. */
bool feb_require_repository_root(const char *root, const char *repository, FebError *error);
/* _native_room(): the original room selected by world, area and room. */
bool feb_native_room(const char *root, const char *repository, const FusionJson *options,
                     FusionJson *room, FebError *error);
/* layer-list, tile-get, tile-set and tile-fill. */
bool feb_workspace_command(const char *command, const char *root, const char *repository,
                           const FusionJson *options, bool dry_run, FusionJson *data,
                           FebError *error);

#endif /* FUSION_EDITOR_BACKEND_WORKSPACE_H */
