# SPDX-License-Identifier: GPL-3.0-only
"""Native MZM maps use original pause-screen LZ77 cells, not guessed rooms."""
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import world_overview


def literal_lz77(data):
    # Valid GBA 0x10 compressed stream made of literal bytes (test-only).
    result = bytearray(b'\x10' + len(data).to_bytes(3, 'little'))
    for index in range(0, len(data), 8):
        result.append(0)
        result.extend(data[index:index + 8])
    return bytes(result)


class NativeMinimapTest(unittest.TestCase):
    def test_native_occupied_tiles_keep_real_coordinates(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            dest = output / 'raw/metroid/data/menus/pause_screen'
            dest.mkdir(parents=True)
            tiles = [0x140] * 1024
            tiles[0] = 0x141
            tiles[31 * 32 + 20] = 0x15e
            data = struct.pack('<1024H', *tiles)
            (dest / 'brinstar_minimap.tt').write_bytes(literal_lz77(data))
            with patch.object(world_overview, 'OUTPUT', output):
                result = world_overview.native_mzm_minimap_cells()
            self.assertEqual(result, [
                (0, 999, 0, 0, 0, 0, 3, 0x141),
                (0, 999, 20, 31, 0, 0, 3, 0x15e),
            ])

    def test_missing_local_native_data_is_not_invented(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(world_overview, 'OUTPUT', Path(directory)):
                self.assertEqual(world_overview.native_mzm_minimap_cells(), [])

    def test_output_rows_preserve_native_tile_and_legacy_rows(self):
        encoded = world_overview.output_rows('mzm', [
            (0, 2, 1, 1, 0, 0),
            (0, 999, 31, 31, 0, 0, 3, 0x141),
        ]).decode('utf-8')
        self.assertIn('0|2|1|1|0|0|0', encoded)
        self.assertIn('0|999|31|31|0|0|3|321', encoded)

    def test_bad_provenance_rejected(self):
        with self.assertRaises(ValueError):
            world_overview.output_rows('mzm', [(0, 999, 2, 3, 0, 0, 4, 0)])

    def test_mzm_native_tiles_can_coexist_with_room_anchors(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(world_overview, 'OUTPUT', Path(directory)), \
                 patch.object(world_overview, 'native_mzm_minimap_cells',
                              return_value=[(0, 999, 1, 1, 0, 0, 3, 0x141),
                                            (0, 999, 2, 1, 0, 0, 3, 0x141)]), \
                 patch('scripts.mzm_world_atlas.run', return_value=None):
                dest = Path(directory) / 'rooms/metroid/world_atlas.tsv'
                dest.parent.mkdir(parents=True)
                dest.write_text('# atlas\nR|Brinstar|7|1|1|2|music|0\n')
                with patch.object(world_overview, 'write_generated') as write:
                    world_overview.index('mzm')
            rows = write.call_args.args[1].decode()
            self.assertIn('0|7|1|1|0|0|0|0', rows)
            self.assertIn('0|999|2|1|0|0|3|321', rows)
            self.assertNotIn('0|999|1|1|', rows)


if __name__ == '__main__':
    unittest.main()
