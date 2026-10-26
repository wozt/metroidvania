/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_EDITOR_BACKEND_COMMANDS_ROOM_H
#define FUSION_EDITOR_BACKEND_COMMANDS_ROOM_H

#include "common.h"

bool feb_is_room_command(const char *command);
/* The project-room, story and project-validation commands. */
bool feb_room_command(const char *command, const FusionJson *options, const char *root,
                      const char *repository, bool dry_run, FusionJson *data, FebError *error);

/* _project_validation(): the project-validate report into ``data``. */
bool feb_project_validation(const char *root, FusionJson *data, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_COMMANDS_ROOM_H */
