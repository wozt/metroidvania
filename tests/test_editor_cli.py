# SPDX-License-Identifier: GPL-3.0-only
"""Functional contracts for the display-independent editor CLI."""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from scripts.editor_cli import CliUsageError, _safe_output

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "scripts/editor_cli.py"


class EditorCliTests(unittest.TestCase):
    def run_cli(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment.pop("DISPLAY", None)
        environment.pop("WAYLAND_DISPLAY", None)
        return subprocess.run(
            [sys.executable, str(CLI), *arguments], cwd=ROOT,
            env=environment, text=True, capture_output=True, check=False)

    def test_project_info_is_clean_json_without_display(self):
        completed = self.run_cli("--command=project-info", "--format=json")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        payload = json.loads(completed.stdout)
        self.assertTrue(payload["success"])
        self.assertEqual(payload["data"]["gameplay_status"], "not_implemented")

    def test_unknown_command_option_is_a_usage_error(self):
        completed = self.run_cli(
            "--command=project-info", "--world=aria", "--format=json")
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(json.loads(completed.stdout)["error"]["type"], "usage")
        self.assertIn("unknown option", completed.stderr)

    def test_unavailable_command_is_explicit(self):
        completed = self.run_cli("--command=audio-list", "--format=json")
        self.assertEqual(completed.returncode, 3)
        payload = json.loads(completed.stdout)
        self.assertEqual(payload["error"]["type"], "unavailable")
        self.assertIn("pending", payload["error"]["message"])

    def test_atomic_dry_run_batch_does_not_create_a_room(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            batch = root / "batch.json"
            batch.write_text(json.dumps({"operations": [{
                "command": "room-create", "world": "aria", "area": 0,
                "slug": "dry_room", "name": "Dry room",
                "width_screens": 1, "height_screens": 1,
            }]}), encoding="utf-8")
            completed = self.run_cli(
                f"--batch={batch}", "--atomic=true", "--dry-run=true",
                f"--root={root}", "--format=json")
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertFalse((root / "assets").exists())

    def test_output_refuses_symlink_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "target"
            target.mkdir()
            link = root / "link"
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaises(CliUsageError):
                _safe_output(link / "result.json", "{}\n")

    def test_batch_refuses_symlink_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "target"
            target.mkdir()
            batch = target / "batch.json"
            batch.write_text('{"operations":[{"command":"project-info"}]}',
                             encoding="utf-8")
            link = root / "link"
            link.symlink_to(target, target_is_directory=True)
            completed = self.run_cli(f"--batch={link / 'batch.json'}", "--format=json")
            self.assertEqual(completed.returncode, 2)


if __name__ == "__main__":
    unittest.main()
