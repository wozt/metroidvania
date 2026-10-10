# SPDX-License-Identifier: GPL-3.0-only
"""Count playable body frames independently of ELF symbol extent."""
import unittest
from scripts.mzm_samus_compositions_0160 import validate_pair


class Composition0162Tests(unittest.TestCase):
    def setUp(self):
        self.symbols = {"body": (0x08248034, 192),
                        "cannon": (0x08234120, 80),
                        "upper": (0x0823236c, 64),
                        "lower": (0x082324ac, 64)}

    def test_terminator_padding_accepted(self):
        _, _, count = validate_pair("body", "cannon", ("upper", "lower"),
                                    self.symbols, 10)
        self.assertEqual(count, 10)

    def test_short_cannon_rejected(self):
        with self.assertRaises(ValueError):
            validate_pair("body", "cannon", ("upper", "lower"),
                          self.symbols, 11)

    def test_overlong_body_rejected(self):
        self.symbols["body"] = (0x08248034, 257 * 16)
        with self.assertRaises(ValueError):
            validate_pair("body", "cannon", ("upper", "lower"),
                          self.symbols, 10)
