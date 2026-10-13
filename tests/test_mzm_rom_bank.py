# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import struct
import unittest

from scripts import mzm_rom_bank

ROOT = Path(__file__).resolve().parents[1]
ELF = ROOT / "third_party/mzm/mzm_us.elf"


def tiny_elf(symbols: list[tuple[str, int, int, int]]) -> bytes:
    """An ELF32 file with a .symtab of (name, value, size, type) and its
    string table."""
    names = b"\0"
    entries = [struct.pack("<IIIBBH", 0, 0, 0, 0, 0, 0)]
    for name, value, size, kind in symbols:
        entries.append(struct.pack("<IIIBBH", len(names), value, size, kind, 0, 1))
        names += name.encode() + b"\0"
    symtab = b"".join(entries)
    header_size, section_size = 0x34, 40
    symtab_at = header_size
    strtab_at = symtab_at + len(symtab)
    sections_at = strtab_at + len(names)
    header = bytearray(header_size)
    header[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<I", header, 0x20, sections_at)
    struct.pack_into("<HH", header, 0x2E, section_size, 3)
    sections = struct.pack("<IIIIIIIIII", *[0] * 10)
    sections += struct.pack("<IIIIIIIIII", 0, 2, 0, 0, symtab_at, len(symtab), 2, 0, 4, 16)
    sections += struct.pack("<IIIIIIIIII", 0, 3, 0, 0, strtab_at, len(names), 0, 0, 1, 0)
    return bytes(header) + symtab + names + sections


class ElfSymbolTests(unittest.TestCase):
    def test_sized_objects_only(self):
        elf = tiny_elf([("sTable", 0x08000010, 8, 1), ("Function", 0x08000000, 4, 2),
                        ("sEmpty", 0x08000020, 0, 1), ("gRam", 0x03000000, 4, 1)])
        self.assertEqual(mzm_rom_bank.elf_symbols(elf),
                         [("sTable", 0x08000010, 8), ("gRam", 0x03000000, 4)])

    def test_rom_range_and_duplicates(self):
        lines, duplicates = mzm_rom_bank.symbol_lines(
            [("b", 0x08000004, 4), ("a", 0x08000000, 4), ("b", 0x08000008, 2),
             ("gRam", 0x03000000, 4)], 16)
        self.assertEqual(lines, [f"symbol\ta\t{0x08000000}\t4", f"symbol\tb\t{0x08000004}\t4"])
        self.assertEqual(duplicates, ["b"])
        with self.assertRaises(ValueError):
            mzm_rom_bank.symbol_lines([("past", 0x0800000C, 8)], 16)

    def test_not_an_elf(self):
        with self.assertRaises(ValueError):
            mzm_rom_bank.elf_symbols(b"MZ\0\0")

    @unittest.skipUnless(ELF.exists(), "the reference ELF is not built")
    def test_reference_build_has_the_hud_data(self):
        symbols = {name: (address, size)
                   for name, address, size in mzm_rom_bank.elf_symbols(ELF.read_bytes())}
        self.assertEqual(symbols["sCommonSpritesPal"][1], 192)
        self.assertEqual(symbols["sMissileDigitsGfx"][1], 2304)
        self.assertEqual(symbols["sAreaRoomEntryPointers"][1], 28)
        self.assertEqual(symbols["sBrinstarRoomEntries"][1], 42 * 60)


if __name__ == "__main__":
    unittest.main()
