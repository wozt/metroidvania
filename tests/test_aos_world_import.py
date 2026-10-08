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
        descriptor_offset = 0x100 + area * 0x100
        background_offset = descriptor_offset + 0x24
        graphics_offset = descriptor_offset + 0x48
        palette_offset = descriptor_offset + 0x50
        entity_offset = descriptor_offset + 0x58
        metadata_offset = descriptor_offset + 0x80
        struct.pack_into(
            "<I", rom, AREA_DIRECTORY_OFFSET + area * 4,
            GBA_ROM_BASE + table_offset,
        )
        struct.pack_into("<I", rom, table_offset, GBA_ROM_BASE + descriptor_offset)
        struct.pack_into(
            "<HHIIIIII2xH2xH", rom, descriptor_offset,
            0x1F00, 0xFFFF, 0, GBA_ROM_BASE + background_offset,
            GBA_ROM_BASE + graphics_offset, GBA_ROM_BASE + palette_offset,
            GBA_ROM_BASE + entity_offset, GBA_ROM_BASE + descriptor_offset,
            0, area | (area << 7),
        )
        for layer in range(3):
            struct.pack_into(
                "<BBHHHI", rom,
                background_offset + layer * 0xC,
                layer, layer + 1, 0x100 + layer, 0, 0,
                GBA_ROM_BASE + metadata_offset + layer * 2,
            )
            struct.pack_into("<BB", rom, metadata_offset + layer * 2, 1, 1)
        struct.pack_into("<I", rom, graphics_offset, 0)
        struct.pack_into("<I", rom, palette_offset, 0)
        struct.pack_into("<h", rom, entity_offset, 0x7FFF)
    return rom


class AriaWorldImportTests(unittest.TestCase):
    def test_save_and_warp_flags_are_decoded_separately(self):
        rom = synthetic_rom()
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET, 0x8000)
        struct.pack_into("<H", rom, MAP_TABLE_OFFSET + 2, 0x4000 | (1 << 6))
        catalog = decode_world(bytes(rom), verify_hash=False)
        self.assertEqual(catalog["room_counts"], [1] * AREA_COUNT)
        self.assertEqual(catalog["format"], "MV_AOS_WORLD_2")
        self.assertEqual(catalog["native_room_count"], AREA_COUNT)
        self.assertEqual(catalog["transition_count"], 0)
        self.assertEqual(catalog["entity_count"], 0)
        self.assertEqual(catalog["savepoint_count"], 1)
        self.assertEqual(catalog["warp_count"], 1)
        self.assertEqual(catalog["savepoints"][0]["id"], "aria.save.0.0")
        self.assertEqual(catalog["savepoints"][0]["room_pointer"], "0x08000100")
        self.assertFalse(catalog["savepoints"][0]["warp"])
        self.assertFalse(catalog["warps"][0]["save"])
        room = catalog["rooms"][0]
        self.assertEqual(room["resolved_pointer"], "0x08000100")
        self.assertEqual(len(room["backgrounds"]), 3)
        self.assertEqual(room["backgrounds"][0]["width_screens"], 1)
        self.assertEqual(room["map_cells"][0]["map_x"], 0)

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

    def test_room_variant_cycle_is_rejected(self):
        rom = synthetic_rom()
        descriptor_offset = 0x100
        struct.pack_into("<H", rom, descriptor_offset + 2, 0)
        struct.pack_into("<I", rom, descriptor_offset + 4, GBA_ROM_BASE + descriptor_offset)
        with self.assertRaisesRegex(ValueError, "variant cycle"):
            decode_world(bytes(rom), verify_hash=False)

    def test_transition_target_is_resolved_to_native_room(self):
        rom = synthetic_rom()
        descriptor_offset = 0x100
        transition_offset = descriptor_offset - 0x10
        struct.pack_into(
            "<I", rom, descriptor_offset + 0x18,
            GBA_ROM_BASE + transition_offset,
        )
        struct.pack_into(
            "<IbbHHHHH", rom, transition_offset,
            GBA_ROM_BASE + 0x200, -1, 0, 4, 5, 6, 7, 8,
        )
        catalog = decode_world(bytes(rom), verify_hash=False)
        transition = catalog["rooms"][0]["transitions"][0]
        self.assertEqual(catalog["transition_count"], 1)
        self.assertEqual(transition["source_screen_x"], -1)
        self.assertEqual(transition["target_engine_area"], 1)
        self.assertEqual(transition["target_room"], 0)

    def test_unterminated_entity_list_is_rejected(self):
        rom = synthetic_rom()
        descriptor_offset = 0x100
        entity_offset = 0x3000
        struct.pack_into(
            "<I", rom, descriptor_offset + 0x14,
            GBA_ROM_BASE + entity_offset,
        )
        for index in range(512):
            struct.pack_into(
                "<hhBBBBHH", rom, entity_offset + index * 12,
                1, 2, 0, 1, 3, 0, 0, 0,
            )
        with self.assertRaisesRegex(ValueError, "entity list exceeds safety cap"):
            decode_world(bytes(rom), verify_hash=False)

    def test_wrong_revision_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unmodified Aria"):
            decode_world(bytes(synthetic_rom()))


if __name__ == "__main__":
    unittest.main()
