#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.import_mzm_rooms import decode_room_descriptors


class NativeRoomDescriptorsTest(unittest.TestCase):
    source = """const struct RoomEntryRom sBrinstarRoomEntries[2] = {
[0] = {
.tileset = 8,
.pBg1Data = sBrinstar_0_Bg1,
.pBg2Data = sBrinstar_0_Bg2,
.pClipData = sBrinstar_0_Clipdata,
.pDefaultSpriteData = sBrinstar_0_Spriteset0,
.defaultSpriteset = 2,
.mapX = 5,
.mapY = 7,
.musicTrack = MUSIC_BRINSTAR,
},
[1] = {
.tileset = 27,
.pBg1Data = sBrinstar_1_Bg1,
.pBg2Data = sBrinstar_1_Bg2,
.pClipData = sBrinstar_1_Clipdata,
.pDefaultSpriteData = sBrinstar_1_Spriteset0,
.defaultSpriteset = 12,
.mapX = 8,
.mapY = 9,
.musicTrack = MUSIC_SAVE_ELEVATOR_ROOM,
},
};"""

    def test_descriptors(self):
        rooms = decode_room_descriptors(self.source)
        self.assertEqual(len(rooms), 2)
        self.assertEqual(rooms[0]["id"], "mzm:brinstar:000")
        self.assertEqual(rooms[1]["fields"]["musicTrack"], "MUSIC_SAVE_ELEVATOR_ROOM")

    def test_missing_room(self):
        with self.assertRaises(ValueError):
            decode_room_descriptors(self.source.replace("[1] = {", "[3] = {"))

    def test_incomplete_table(self):
        with self.assertRaises(ValueError):
            decode_room_descriptors(self.source.replace("RoomEntries[2]", "RoomEntries[3]"))

    def test_missing_field(self):
        with self.assertRaises(ValueError):
            decode_room_descriptors(self.source.replace(".tileset = 8,", ""))

    def test_no_tables(self):
        with self.assertRaises(ValueError):
            decode_room_descriptors("")


if __name__ == "__main__":
    unittest.main()
