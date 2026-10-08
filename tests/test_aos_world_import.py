# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.import_aos_world import (
    AREA_COUNT,
    AREA_DIRECTORY_OFFSET,
    GBA_ROM_BASE,
    MAP_HEIGHT,
    MAP_TABLE_OFFSET,
    MAP_WIDTH,
    decode_world,
)


def synthetic_rom() -> bytearray:
    rom = bytearray(AREA_DIRECTORY_OFFSET + AREA_COUNT * 4)
    rom[MAP_TABLE_OFFSET:MAP_TABLE_OFFSET + MAP_WIDTH * MAP_HEIGHT * 2] = (
        b"\xff\xff" * MAP_WIDTH * MAP_HEIGHT
    )
    table_start = AREA_DIRECTORY_OFFSET - AREA_COUNT * 4
    for area in range(AREA_COUNT):
        table_offset = table_start + area * 4
        struct.pack_into(
            "<I", rom, AREA_DIRECTORY_OFFSET + area * 4,
            GBA_ROM_BASE + table_offset,
        )
        struct.pack_into("<I", rom, table_offset, GBA_ROM_BASE + 0x100 + area * 4)
    return rom


class AriaWorldImportTests(unittest.TestCase):
    def test_save_and_warp_flags_are_decoded_separately(self):
        rom = synthetic_rom()
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET, 0x8000)
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET + 2, 0x4000 | (1 << 6))
        catalog = decode_world(bytes(rom), verify_hash=False)
        self.assertEqual(catalog["room_counts"], [1] * AREA_COUNT)
        self.assertEqual(catalog["savepoint_count"], 1)
        self.assertEqual(catalog["warp_count"], 1)
        self.assertEqual(catalog["savepoints"][0]["id"], "aria.save.0.0")
        self.assertEqual(catalog["savepoints"][0]["room_pointer"], "0x08000100")
        self.assertFalse(catalog["savepoints"][0]["warp"])
        self.assertFalse(catalog["warps"][0]["save"])

    def test_invalid_map_room_is_rejected(self):
        rom = synthetic_rom()
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET, 0x8001)
        with self.assertRaisesRegex(ValueError, "invalid room"):
            decode_world(bytes(rom), verify_hash=False)

    def test_invalid_room_pointer_is_rejected(self):
        rom = synthetic_rom()
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET, 0x8000)
        table_start = AREA_DIRECTORY_OFFSET - AREA_COUNT * 4
        struct.pack_into("<I", rom, table_start, 0x07000000)
        with self.assertRaisesRegex(ValueError, "invalid GBA ROM pointer"):
            decode_world(bytes(rom), verify_hash=False)

    def test_wrong_revision_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unmodified Aria"):
            decode_world(bytes(synthetic_rom()))


if __name__ == "__main__":
    unittest.main()
