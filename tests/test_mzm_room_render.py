# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_room_render import (bmp24, graphic_base, lz77, metatiles,
                                     render_layer, rle_room,
                                     tileset_resource_indices)

class RenderTest(unittest.TestCase):
    def test_lz77_literals_and_overlap(self):
        self.assertEqual(lz77(bytes([0x10,4,0,0,0,1,2,3,4])),bytes([1,2,3,4]))
        self.assertEqual(lz77(bytes([0x10,7,0,0,0x40,65,0x30,0])),b'A'*7)
    def test_lz_reject(self):
        for blob in (b'',b'\x10\x01\x00\x00\x80\x00\x00',b'\x10\xff\xff\xff'):
            with self.assertRaises(ValueError): lz77(blob)
    def test_native_rle(self):
        # One 2x2 room, low bytes 1,1,2,2 and high bytes 0.
        blob = bytes([2,2,1,0x82,1,0x82,2,0,1,0x84,0,0])
        self.assertEqual(rle_room(blob),(2,2,(1,1,2,2)))
        self.assertEqual(rle_room(blob + b'room metadata'),(2,2,(1,1,2,2)))
        with self.assertRaises(ValueError): rle_room(blob[:-2])
    def test_metatile_and_render(self):
        # Header + one four-entry metatile. First byte of gfx makes opaque palette value 1.
        table=metatiles(bytes([0,0]) + struct.pack('<4H',*(0x3000+192,)*4))
        self.assertEqual(len(table),1)
        self.assertEqual(graphic_base(list(table[0]),1)[0],192)
        gfx=bytes([0x11]*32)
        palette=bytearray(14*32)
        palette[16*2+2:16*2+4] = (0x001f).to_bytes(2,'little')
        rgb,miss,paint=render_layer(1,1,(0,),table,gfx,bytes(palette),192)
        self.assertEqual(len(rgb),16*16*3)
        self.assertEqual(miss,0)
        self.assertEqual(paint,256)
        self.assertEqual(rgb[:3],bytes([255,0,0]))
        self.assertEqual(bmp24(16,16,rgb)[:2],b'BM')
    def test_invalid_room(self):
        with self.assertRaises(ValueError):rle_room(bytes([0,0,1,0,1,0]))
        with self.assertRaises(ValueError):metatiles(b'\0\0\0')
    def test_tileset_aliases_follow_the_native_table(self):
        resources = tileset_resource_indices()
        self.assertEqual(len(resources), 79)
        self.assertEqual(resources[41], (40, 40, 40))
        self.assertEqual(resources[78], (78, 42, 78))

if __name__=='__main__':unittest.main()
