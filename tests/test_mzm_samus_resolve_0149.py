# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for local symbol-map validation."""
import unittest
from scripts.mzm_samus_resolve import parse_symbols, resolve


class SamusResolve0149(unittest.TestCase):
    def test_map_and_nm_formats(self):
        mapping = parse_symbols("0x08248744 sSamusAnim_PowerSuit_Right_Standing\n"
                                "08248034 T sSamusAnim_PowerSuit_Right_Running\n")
        self.assertEqual(mapping["sSamusAnim_PowerSuit_Right_Standing"], 0x08248744)
        self.assertEqual(len(mapping), 2)

    def test_conflicts_rejected(self):
        with self.assertRaises(ValueError):
            parse_symbols("08248744 sSamusAnim_PowerSuit_Right_Standing\n"
                          "08248754 sSamusAnim_PowerSuit_Right_Standing\n")

    def test_unresolved_is_explicit(self):
        catalog = {"schema": "metroidvania-mzm-samus-symbolic-catalog-v1",
                   "tables": {"example": [{"all_symbols": ["sSamusAnim_Test"]}]}}
        report = resolve(catalog, {}, b"")
        self.assertEqual(report["unresolved"], ["sSamusAnim_Test"])
        self.assertEqual(report["resolved_symbols"], 0)
