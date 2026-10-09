# SPDX-License-Identifier: GPL-3.0-only
"""0084 Aria CLI entrypoint and GTK4 widget-lifetime regression tests.

No copyrighted files; the native ROM is replaced with synthetic text pointers.
"""
import contextlib
import io
import runpy
import struct
import unittest
import warnings
from pathlib import Path
from unittest.mock import patch

from scripts import aria_item_details as names


class AriaCliAndGtkLifetime0084(unittest.TestCase):
    @staticmethod
    def fake_rom():
        # Provide a real pointer-table layout with one shared native string.
        base = names.TEXT_POINTER_TABLE - names.GBA_ROM_BASE
        endpoint = base + (0x160 * 4)
        rom = bytearray(endpoint + 32)
        rom[endpoint:endpoint + 9] = b'\x01\x00Potion\x0A'
        for first, count, initial in names._ITEM_RANGES.values():
            for item_id in range(initial, initial + count):
                text_index = first + item_id - initial
                struct.pack_into('<I', rom, base + 4 * text_index,
                                 names.GBA_ROM_BASE + endpoint)
        return bytes(rom)

    def test_python_module_main_runs_after_definitions(self):
        fake_rom = self.fake_rom()
        expected = sum(count for _, count, _ in names._ITEM_RANGES.values())
        with patch('scripts.import_game_assets.verified_rom', return_value=fake_rom):
            with warnings.catch_warnings(), contextlib.redirect_stdout(io.StringIO()) as out:
                warnings.simplefilter('ignore', RuntimeWarning)
                runpy.run_module('scripts.aria_item_details', run_name='__main__')
        self.assertIn(f'Aria item/soul names decoded: {expected}/{expected}',
                      out.getvalue())
        self.assertEqual(names.item_name(fake_rom, 2, 0), 'Potion')

    def test_gui_tracks_room_data_widgets(self):
        c = (Path(__file__).resolve().parents[1] / 'editor/native_workspace.c').read_text()
        self.assertIn('document_track_widget(doc, &doc->annotations_list, data_list);', c)
        self.assertIn('document_track_widget(doc, &doc->annotations_status,', c)
        self.assertIn('g_object_add_weak_pointer(G_OBJECT(*borrowed[i])', c)
        self.assertIn('g_object_remove_weak_pointer(G_OBJECT(*slots[i])', c)
        self.assertIn('g_signal_handlers_disconnect_matched(form->native_type', c)
        self.assertIn('!form->native_type || !form->catalog_description', c)
        self.assertIn('!form->native_type || !form->selected_icon', c)

    def test_name_decoder_diagnostic_works_with_bad_texts(self):
        rom = bytearray(self.fake_rom())
        base = names.TEXT_POINTER_TABLE - names.GBA_ROM_BASE
        struct.pack_into('<I', rom, base + 0x5b * 4, 0)
        ok, total, errors = names.diagnose_names(rom)
        self.assertEqual(ok + 1, total)
        self.assertEqual(len(errors), 1)
        self.assertIn('invalid source pointer', errors[0])


if __name__ == '__main__':
    unittest.main()
