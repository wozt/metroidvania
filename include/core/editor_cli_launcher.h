/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_EDITOR_CLI_LAUNCHER_H
#define FUSION_EDITOR_CLI_LAUNCHER_H

#include <stdbool.h>

/* Replace the current process with the shared Python editor CLI.
 * When remove_headless is true, exact --headless arguments are omitted. */
int fusion_editor_cli_launch(int argc, char **argv, bool remove_headless);

#endif
