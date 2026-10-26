/* SPDX-License-Identifier: GPL-3.0-only */
/* The world overview index and preview batches of the GTK world atlas. */
#ifndef FUSION_EDITOR_BACKEND_WORLD_OVERVIEW_H
#define FUSION_EDITOR_BACKEND_WORLD_OVERVIEW_H

#include "common.h"

/* The fusion_world_overview command line: --world {mzm,aria} [--area N]
 * [--budget 0..32], writing below ``repository``/assets/extracted. */
int feb_world_overview_main(int argc, char **argv, const char *repository);

#endif /* FUSION_EDITOR_BACKEND_WORLD_OVERVIEW_H */
