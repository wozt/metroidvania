# SPDX-License-Identifier: GPL-3.0-only
"""Patch 0092: enforce native minimap occupancy and per-screen preview math."""
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import world_overview as overview
from scripts.mzm_room_render import bmp24


class GlobalMap0092Tests(unittest.TestCase):
    def test_minimap_upper_word_bits_do_not_create_phantom_cells(self):
        # Synthetic compressed 32x32 minimap, one non-background tile.
        raw = bytearray([0x40, 0x01] * 1024)
        raw[0:2] = bytes([0x41, 0xF1])  # 0xF141 -> tile ID 0x141
        raw[2:4] = bytes([0x40, 0xF1])  # 0xF140 -> blank, despite palette bits
        # Use the real LZ77 decoder: literal-only stream.
        data = bytearray([0x10, 0, 8, 0])
        for i in range(0, len(raw), 8):
            data.append(0)
            data.extend(raw[i:i+8])
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            file = root / 'metroid/raw/data/menus/pause_screen/brinstar_minimap.tt'
            file.parent.mkdir(parents=True)
            file.write_bytes(data)
            with patch.object(overview, 'MZM_AREAS', ('Brinstar',)), \
                 patch.object(overview, 'OUTPUT', root):
                rows = overview.native_mzm_minimap_cells()
        self.assertEqual(rows, [(0, 999, 0, 0, 0, 0, 3, 0xF141)])

    def test_crop_returns_correct_screen_not_whole_room(self):
        width, height = 544, 224  # two playable 240x160 screens + 32px guard
        rgb = bytearray(width * height * 3)
        for y in range(height):
            for x in range(width):
                i = (y * width + x) * 3
                rgb[i:i+3] = b'\xff\x00\x00' if x < 272 else b'\x00\xff\x00'
        original = bmp24(width, height, rgb)
        one = overview.mzm_screen_preview(original, 0, 0)
        two = overview.mzm_screen_preview(original, 1, 0)
        self.assertIsNotNone(one)
        self.assertIsNotNone(two)
        self.assertNotEqual(one, two)
        self.assertEqual(struct.unpack_from('<ii', one, 18), (60, 40))
        self.assertIsNone(overview.mzm_screen_preview(original, 2, 0))
        self.assertIsNone(overview.mzm_screen_preview(original, -1, 0))

    def test_unique_native_witness_recovers_missing_cell(self):
        anchors = [(0, 1, 3, 3, 0, 0)]
        native = [(0, 999, 3, 3, 0, 0, 3, 0x141),
                  (0, 999, 4, 3, 0, 0, 3, 0x141)]
        rows, report = overview.resolve_mzm_minimap_cells(
            anchors, {}, native, direct_evidence={(0, 1): {(0, 4, 3)}})
        self.assertEqual([row[1] for row in rows[:2]], [1, 1])
        self.assertEqual(report['owned_native_cells'], 2)
        self.assertEqual(report['native_evidence_resolutions'], 1)

    def test_ambiguous_witness_stays_unassigned(self):
        anchors = [(0, 1, 3, 3, 0, 0), (0, 2, 6, 3, 0, 0)]
        native = [(0, 999, 5, 3, 0, 0, 3, 0x141)]
        rows, _ = overview.resolve_mzm_minimap_cells(
            anchors, {}, native,
            direct_evidence={(0, 1): {(0, 5, 3)}, (0, 2): {(0, 5, 3)}})
        self.assertEqual(rows[0][1], 999)

if __name__ == '__main__':
    unittest.main()
