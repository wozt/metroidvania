# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.aos_room_render import (
    GBA_ROM_BASE,
    composite_backgrounds,
    decode_background,
    lz77_at,
    render_background,
    resource_payload,
)


class AriaRoomRenderTests(unittest.TestCase):
    def test_lz77_pointer_decode_and_compressed_resource_header(self):
        rom = bytearray(0x100)
        stream = bytes([0x10, 8, 0, 0, 0, 0, 4, 0x20, 0, 0, 1, 2, 3])
        rom[0x40:0x40 + len(stream)] = stream
        self.assertEqual(lz77_at(bytes(rom), GBA_ROM_BASE + 0x40), b"\0\x04 \0\0\1\2\3")
        struct.pack_into("<BBBBI", rom, 0x20, 1, 4, 1, 0, GBA_ROM_BASE + 0x40)
        payload, header = resource_payload(bytes(rom), GBA_ROM_BASE + 0x20)
        self.assertEqual(payload, b"\0\1\2\3")
        self.assertEqual(header["encoding"], 1)

    def test_background_blocks_and_collision_are_expanded(self):
        rom = bytearray(0x800)
        metadata = GBA_ROM_BASE + 0x100
        blocks = GBA_ROM_BASE + 0x200
        collision = GBA_ROM_BASE + 0x300
        block_map = GBA_ROM_BASE + 0x400
        struct.pack_into("<BBHIII", rom, 0x100, 1, 1, 0, blocks, collision, block_map)
        struct.pack_into("<16H", rom, 0x200, *range(16))
        rom[0x300:0x310] = bytes(range(16))
        struct.pack_into("<H", rom, 0x400, 0x4001)
        background = decode_background(bytes(rom), {
            "layer": 1,
            "metadata_pointer": f"0x{metadata:08x}",
            "control": 0,
            "field_0": 22,
        })
        self.assertEqual(background["width_tiles"], 32)
        self.assertEqual(background["height_tiles"], 32)
        self.assertEqual(background["tiles"][:4], [0x403, 0x402, 0x401, 0x400])
        self.assertEqual(background["collision"][:4], [3, 2, 1, 0])

    def test_4bpp_render_and_depth_composite(self):
        vram = bytearray(0x10000)
        palette = bytearray(0x200)
        vram[32:64] = b"\x11" * 32
        struct.pack_into("<H", palette, 2, 0x001F)
        background = {
            "status": "DECODED_TEXT_BACKGROUND",
            "layer": 1,
            "width_tiles": 1,
            "height_tiles": 1,
            "control": 0,
            "depth_key": 22,
            "tiles": [1],
        }
        rgba, unresolved = render_background(background, bytes(vram), bytes(palette))
        self.assertEqual(unresolved, 0)
        self.assertEqual(rgba[:4], bytes([255, 0, 0, 255]))

        rear = dict(background, layer=2, depth_key=31)
        rear_rgba = bytes([0, 0, 255, 255]) * 64
        width, height, rgb = composite_backgrounds([
            (background, rgba),
            (rear, rear_rgba),
        ])
        self.assertEqual((width, height), (8, 8))
        self.assertEqual(rgb[:3], bytes([255, 0, 0]))

    def test_composite_without_bg1_uses_original_available_layer(self):
        bg = {"layer": 2, "width_tiles": 1, "height_tiles": 1, "depth_key": 2}
        width, height, rgb = composite_backgrounds([(bg, bytes([0, 255, 0, 255]) * 64)])
        self.assertEqual((width, height), (8, 8))
        self.assertEqual(rgb[:3], bytes([0, 255, 0]))
        with self.assertRaisesRegex(ValueError, "no supported text background"):
            composite_backgrounds([])


if __name__ == "__main__":
    unittest.main()
