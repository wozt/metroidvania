# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_compositions_0160 import parse_sized_symbols, validate_pair


class Compositions0160Tests(unittest.TestCase):
    def test_parser(self):
        result = parse_sized_symbols("08248034 000000a0 R sSamusAnim_Test\n"
                                     "08234120 00000050 R sArmCannonAnim_Test\n")
        self.assertEqual(result["sSamusAnim_Test"], (0x08248034, 160))

    def test_matching_counts(self):
        symbols = {
            "body": (0x08248034, 160), "cannon": (0x08234120, 80),
            "upper": (0x0823236c, 64), "lower": (0x082324ac, 64),
        }
        self.assertEqual(validate_pair("body", "cannon", ("upper", "lower"), symbols)[2], 10)

    def test_mismatch_rejected(self):
        symbols = {
            "body": (0x08248034, 160), "cannon": (0x08234120, 72),
            "upper": (0x0823236c, 64), "lower": (0x082324ac, 64),
        }
        with self.assertRaises(ValueError):
            validate_pair("body", "cannon", ("upper", "lower"), symbols)
