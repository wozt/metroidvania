# SPDX-License-Identifier: GPL-3.0-only
"""0111 regression: GLib owns the environment vector after g_environ_setenv.

The GTK room-open scenario also exercises this through the existing C tests,
but the actual crash needs a ROM-backed import and cannot run in unit tests.
"""
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "editor/native_workspace.c"


class ProjectCommandEnvironment0111(unittest.TestCase):
    def test_environment_vector_has_one_owner(self):
        source = SOURCE.read_text(encoding="utf-8")
        body = source.split("static gboolean project_command(", 1)[1]
        body = body.split("static guint project_collision_type(", 1)[0]
        start = body.index("gchar **environment = g_get_environ();")
        end = body.index("gboolean success = launched", start)
        block = body[start:end]
        self.assertIn('environment = g_environ_setenv(environment, "MV_EDITOR_ROOM_STAGE",', block)
        self.assertIn('g_spawn_sync(NULL, (gchar **)args->pdata, environment,', block)
        self.assertEqual(block.count("g_strfreev(environment);"), 1)
        self.assertGreater(block.index("g_strfreev(environment);"),
                           block.index("g_spawn_sync("))
        self.assertNotIn("gchar **staged = g_environ_setenv", block)


if __name__ == "__main__":
    unittest.main()
