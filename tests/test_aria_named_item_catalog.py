# SPDX-License-Identifier: GPL-3.0-only
"""0082: native text lookup and per-item menu rows, without distributing a ROM."""
import struct
import unittest
from scripts.aria_item_details import (
    TEXT_POINTER_TABLE, GBA_ROM_BASE, item_name, named_options, _read_name
)


class AriaNamedItemTests(unittest.TestCase):
    @staticmethod
    def rom_with_names():
        # Enough for the upper Aria text table and explicit small name blocks.
        rom = bytearray(0x506B38 + 4 * 0x15C + 6000)
        start = TEXT_POINTER_TABLE - GBA_ROM_BASE
        for tid in range(0x5B, 0x15C):
            # Text names live after the index table, in mock local-ROM bytes.
            off = start + 4 * 0x15C + (tid - 0x5B) * 20
            struct.pack_into('<I', rom, start + 4 * tid, GBA_ROM_BASE + off)
            rom[off:off+2] = b'\x01\x00'
            text = f'Name {tid:03X}'.encode('ascii') + b'\x0A'
            rom[off+2:off+2+len(text)] = text
        return bytes(rom)

    def test_true_item_name_offsets(self):
        rom = self.rom_with_names()
        for subtype, item_id, text_id in (
            (2, 0, 0x5B), (2, 31, 0x7A), (3, 0, 0x7B),
            (3, 58, 0xB5), (4, 0, 0xB6), (4, 44, 0xE2),
            (5, 1, 0xE3), (5, 55, 0x119), (6, 1, 0x11B),
            (7, 35, 0x155), (8, 0, 0x156), (8, 5, 0x15B)):
            with self.subTest(subtype=subtype, item_id=item_id):
                self.assertEqual(item_name(rom, subtype, item_id), f'Name {text_id:03X}')
        with self.assertRaises(ValueError):
            item_name(rom, 3, 59)
        with self.assertRaises(ValueError):
            item_name(rom, 5, 0)

    def test_specific_names_per_id_and_modes(self):
        rows = named_options(self.rom_with_names())
        self.assertGreater(len(rows), 750)
        subset = [r for r in rows if r['native_type'] == 'pickup:03']
        self.assertEqual(len(subset), 59)
        self.assertEqual(subset[0]['name'], 'Name 07B')
        self.assertEqual(subset[-1]['item_id'], 58)
        self.assertTrue(any(r['category'] == 'Hard Mode / Yellow soul' for r in rows))

    def test_corrupted_or_unsupported_text_fail_closed(self):
        rom = bytearray(self.rom_with_names())
        start = TEXT_POINTER_TABLE - GBA_ROM_BASE
        struct.pack_into('<I', rom, start + 4 * 0x5B, 0x09000000)
        with self.assertRaises(ValueError):
            _read_name(bytes(rom), 0x5B)

    def test_gtk_uses_individual_icon_and_item_id_in_dropdown(self):
        from pathlib import Path
        source = (Path(__file__).resolve().parents[1] /
                  'editor/native_workspace.c').read_text(encoding='utf-8')
        self.assertIn('project_row_icon_assign(', source)
        self.assertIn('form->catalog_item_ids', source)
        self.assertIn('sprite_previews/aria/items/%02X_%03d.png', source)
        self.assertIn('form->catalog_ids->len >= 1600', source)


if __name__ == '__main__':
    unittest.main()
