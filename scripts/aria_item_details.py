# SPDX-License-Identifier: GPL-3.0-only
"""Named Aria items read from the user's verified US ROM (DSVEdit source layout).

Native item-family constructors are NOT item identities. Preserve the Aria
native_type token plus a separate item_id for schema-v1 compatibility.
No copyrighted tables or source images are distributed with this module.
"""
from __future__ import annotations
import struct

from scripts.import_aos_world import GBA_ROM_BASE

TEXT_POINTER_TABLE = 0x08506B38
# DSVEdit constants/aos_constants.rb: name-text ranges and native item counts.
_ITEM_RANGES = {2: (0x5B, 32, 0), 3: (0x7B, 59, 0), 4: (0xB6, 45, 0),
                5: (0xE3, 55, 1), 6: (0x11B, 24, 1),
                7: (0x133, 35, 1), 8: (0x156, 6, 0)}
FAMILY_NAMES = {1: "Money", 2: "Consumable", 3: "Weapon",
                4: "Armor / accessory", 5: "Red soul", 6: "Blue soul",
                7: "Yellow soul", 8: "Ability soul"}
MODES = (("pickup", "Normal"), ("hard-mode-pickup", "Hard Mode"),
         ("all-souls-reward", "All Souls"))
SPECIAL = {0x90: "Œ", 0x91: "œ", 0xA7: "§", 0xBA: "°",
           0xC0: "À", 0xC7: "Ç", 0xC9: "É", 0xCA: "Ê",
           0xD6: "Ö", 0xE0: "à", 0xE7: "ç", 0xE9: "é"}


def _read_name(rom: bytes, text_id: int) -> str:
    """Bounded AoS encoded string: pointer -> 01 00 header -> bytes -> 0A.

    The text codec follows DSVEdit's decode_string_aos: ASCII 0x20..0x7E
    plus explicitly mapped non-ASCII characters. Unknown commands fail closed.
    """
    pointer_offset = TEXT_POINTER_TABLE - GBA_ROM_BASE + text_id * 4
    if pointer_offset < 0 or pointer_offset + 4 > len(rom):
        raise ValueError("Aria name pointer table outside ROM")
    address = struct.unpack_from('<I', rom, pointer_offset)[0]
    pos = address - GBA_ROM_BASE
    if pos < 0 or pos + 3 > len(rom) or rom[pos:pos+2] != b'\x01\x00':
        raise ValueError(f"Aria text {text_id:03x}: bad pointer or header")
    result = []
    for code in rom[pos+2:min(len(rom), pos+2+96)]:
        if code == 0x0A:
            name = ''.join(result).strip()
            if not name or len(name) > 80 or any(ord(c) < 32 for c in name):
                raise ValueError(f"Aria text {text_id:03x}: invalid name")
            return name
        if 0x20 <= code <= 0x7E:
            result.append(chr(code))
        elif code in SPECIAL:
            result.append(SPECIAL[code])
        else:
            raise ValueError(f"Aria text {text_id:03x}: unsupported code {code:02x}")
    raise ValueError(f"Aria text {text_id:03x}: unterminated string")


def item_name(rom: bytes, subtype: int, item_id: int) -> str:
    if subtype not in _ITEM_RANGES:
        raise ValueError("Aria item family has no indexed text names")
    first, count, start_index = _ITEM_RANGES[subtype]
    if type(item_id) is not int or not start_index <= item_id < start_index + count:
        raise ValueError("Aria item ID has no native name text entry")
    return _read_name(rom, first + item_id - start_index)


def named_options(rom: bytes) -> list[dict]:
    """One menu row per named item per mode, not just one per pickup family."""
    options = []
    for prefix, mode in MODES:
        options.append({"native_type": f"{prefix}:01", "name": "Money pickup",
                        "category": f"{mode} / Money", "item_id": 0})
        for subtype, (first, count, start) in _ITEM_RANGES.items():
            for item_id in range(start, start + count):
                # Individual unsupported/broken text names are retained by ID
                # with honest diagnostics; never fabricate item identities.
                try:
                    name = item_name(rom, subtype, item_id)
                except ValueError:
                    name = f"[Name not decoded: {subtype:02X}/{item_id:02X}]"
                options.append({"native_type": f"{prefix}:{subtype:02X}",
                                "name": name,
                                "category": f"{mode} / {FAMILY_NAMES[subtype]}",
                                "item_id": item_id})
    return options
