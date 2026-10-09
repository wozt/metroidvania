/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/editor_cli_launcher.h"

int main(int argc, char **argv)
{
    return fusion_editor_cli_launch(argc, argv, false);
}
