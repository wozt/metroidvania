/* SPDX-License-Identifier: GPL-3.0-only */
/* The native room render audit of the editor. */
#ifndef FUSION_EDITOR_BACKEND_AUDIT_H
#define FUSION_EDITOR_BACKEND_AUDIT_H

#include "common.h"

/* audit_world(): the MV_ROOM_RENDER_AUDIT_1 report of one world
 * (zero_mission or aria), optionally of one area. */
bool feb_audit_world(const char *repository, const char *world, const FusionJson *area,
                     int64_t workers, FusionJson *report, FebError *error);
/* write_private_report(): assets/extracted/audits/``name``; ``path``
 * receives the written file. */
bool feb_audit_write_report(const char *repository, const FusionJson *report, const char *name,
                            char *path, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_AUDIT_H */
