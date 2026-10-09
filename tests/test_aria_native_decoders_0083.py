# SPDX-License-Identifier: GPL-3.0-only
"""0083: ROM-free fixtures for the actual Aria text/GfxWrapper format."""
from __future__ import annotations
import io
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from scripts import native_sprite_thumbnails as thumbs
from scripts import aria_item_details as names


class NativeAriaDecoderRegression(unittest.TestCase):
    @staticmethod
    def fixture() -> bytearray:
        end = max(*thumbs.GFX_BANKS, thumbs.PALETTE,
                  *(row[1] for row in thumbs.ITEM_TABLES),
                  names.TEXT_POINTER_TABLE) - thumbs.GBA_ROM_BASE
        rom = bytearray(end + 0x11000)
        for ptr in thumbs.GFX_BANKS:
            off = ptr - thumbs.GBA_ROM_BASE
            rom[off:off + 4] = bytes([0, 4, 0, 16])
            rom[off + 4:off + 4 + 0x2000] = b'\x11' * 0x2000
        pp = thumbs.PALETTE - thumbs.GBA_ROM_BASE
        rom[pp:pp+4] = bytes([0, 0, 5, 0])
        struct.pack_into('<H', rom, pp+4+2, 0x001F)  # red, palette 0
        struct.pack_into('<H', rom, pp+4+32+2, 0x03E0)  # green, palette 1
        # Consumable #0 uses first real icon (encoded 0x0401).
        for subtype, pointer, count, stride in thumbs.ITEM_TABLES:
            for i in range(count):
                offset = pointer - thumbs.GBA_ROM_BASE + i*stride
                struct.pack_into('<H', rom, offset+2, 0x0401)
        return rom

    def test_raw_wrapper_and_packed_icon_palette(self):
        rom = self.fixture()
        pages = [thumbs._page(rom, p) for p in thumbs.GFX_BANKS]
        self.assertEqual([len(p) for p in pages], [8192] * 3)
        # Decode PNG with stdlib only: inspect the RGBA scanline for true red.
        png = thumbs._decode_icon(rom, pages, 0)
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        start = png.index(b'IDAT') + 4
        length = int.from_bytes(png[start-8:start-4], 'big')
        rows = zlib.decompress(png[start:start+length])
        self.assertEqual(rows[1:5], bytes([255, 0, 0, 255]))
        png2 = thumbs._decode_icon(rom, pages, 0, 1)
        at = png2.index(b'IDAT') + 4
        size = int.from_bytes(png2[at-8:at-4], 'big')
        self.assertEqual(zlib.decompress(png2[at:at+size])[1:5], bytes([0, 255, 0, 255]))

    def test_reject_unrecognized_wrappers_and_bad_backreferences(self):
        rom = self.fixture()
        ptr = thumbs.GFX_BANKS[0] - thumbs.GBA_ROM_BASE
        rom[ptr] = 4
        with self.assertRaises(ValueError):
            thumbs._page(rom, thumbs.GFX_BANKS[0])
        rom[ptr:ptr+4] = bytes([1,4,0,0])
        compressed = thumbs.GBA_ROM_BASE + len(rom) - 64
        struct.pack_into('<I', rom, ptr+4, compressed)
        at = compressed - thumbs.GBA_ROM_BASE
        rom[at:at+6] = b'\x10\x00\x20\x00\x80\x00'
        with self.assertRaises(ValueError):
            thumbs._page(rom, thumbs.GFX_BANKS[0])

    def test_compressed_gba_wrapper_uses_lz10_pointer(self):
        # Two zero literals and 455 overlapping 18-byte backreferences:
        # exactly 8192 decompressed bytes, not a guessed raw gfx page.
        rom = self.fixture()
        off = thumbs.GFX_BANKS[0] - thumbs.GBA_ROM_BASE
        packed = bytearray(b'\x10\x00\x20\x00')
        tokens = [(0, b'\x00'), (0, b'\x00')] + [(1, b'\xF0\x00')] * 455
        for start in range(0, len(tokens), 8):
            batch = tokens[start:start + 8]
            flag = sum(bit << (7 - j) for j, (bit, _) in enumerate(batch))
            packed.append(flag)
            for _, data in batch:
                packed.extend(data)
        address = thumbs.GBA_ROM_BASE + len(rom) - 2048
        place = address - thumbs.GBA_ROM_BASE
        self.assertLess(len(packed), 2048)
        rom[place:place + len(packed)] = packed
        rom[off:off + 4] = bytes([1, 4, 0, 0])
        struct.pack_into('<I', rom, off + 4, address)
        self.assertEqual(thumbs._page(rom, thumbs.GFX_BANKS[0]), b'\x00' * 8192)

    def test_generate_private_icons_without_rom_mutation(self):
        rom = bytes(self.fixture())
        with tempfile.TemporaryDirectory() as tmp:
            total = thumbs.generate_aria_item_thumbnails(rom, Path(tmp))
            self.assertGreater(total, 130)
            path = Path(tmp) / 'items' / '03_000.png'
            self.assertTrue(path.is_file())
            self.assertTrue(path.read_bytes().startswith(b'\x89PNG'))
            self.assertEqual(thumbs.generate_aria_item_thumbnails(rom, Path(tmp)), 0)
        self.assertEqual(rom, bytes(self.fixture()))

    def test_aria_name_decoding_skips_native_display_commands(self):
        rom = self.fixture()
        off = names.TEXT_POINTER_TABLE - names.GBA_ROM_BASE
        target = len(rom) - 128
        struct.pack_into('<I', rom, off + 0x5B*4, names.GBA_ROM_BASE + target)
        rom[target:target+16] = b'\x01\x00\x08\x02\x04Potion \xE9\x0A'
        self.assertEqual(names.item_name(rom, 2, 0), 'Potion é')
        decoded, total, errors = names.diagnose_names(rom)
        self.assertGreater(total, 200)
        self.assertGreaterEqual(decoded, 1)
        self.assertLessEqual(len(errors), 8)
        struct.pack_into('<I', rom, off + 0x5B*4, 0x0A000000)
        with self.assertRaises(ValueError):
            names.item_name(rom, 2, 0)


if __name__ == '__main__':
    unittest.main()
