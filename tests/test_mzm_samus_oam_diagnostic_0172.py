# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.mzm_samus_oam_diagnostic_0172 import header


class OamDiagnostic0172Tests(unittest.TestCase):
    def test_empty_header(self):
        rom = bytearray(32)
        self.assertTrue(header(rom, 0x08000000)["empty"])

    def test_valid_flags(self):
        rom = bytearray(struct.pack("<H", 0x2003) + b"\x00" * 30)
        result = header(rom, 0x08000000)
        self.assertEqual(result["parts"], 3)
        self.assertTrue(result["plausible"])

    def test_excessive_header(self):
        rom = bytearray(struct.pack("<H", 129) + b"\x00" * 30)
        self.assertFalse(header(rom, 0x08000000)["plausible"])
