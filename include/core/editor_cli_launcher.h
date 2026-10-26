/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_EDITOR_CLI_LAUNCHER_H
#define FUSION_EDITOR_CLI_LAUNCHER_H

#include <stdbool.h>

/* Runs the editor CLI: the native backend handles the ported commands and
 * the process is replaced with the remaining Python CLI for the others.
 * When remove_headless is true, exact --headless arguments are omitted. */
int fusion_editor_cli_launch(int argc, char **argv, bool remove_headless);
/* The built editor CLI program the GTK editor runs its backend commands
 * through. */
const char *fusion_editor_cli_program(void);
/* The built world overview indexer of the GTK world atlas. */
const char *fusion_world_overview_program(void);

#endif
