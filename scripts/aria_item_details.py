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
# PATCH_0083_ARIA_NATIVE_DECODERS: decode native AoS text control sequences.
# These additional US character codes follow DSVEdit's AOS_SPECIAL_CHARACTERS.
SPECIAL = {
    0x90: "Œ", 0x91: "œ", 0xA7: "§", 0xAA: "ᵃ", 0xAB: "«",
    0xBA: "°", 0xBB: "»", 0xC0: "À", 0xC1: "Á", 0xC2: "Â",
    0xC4: "Ä", 0xC7: "Ç", 0xC8: "È", 0xC9: "É", 0xCA: "Ê",
    0xCB: "Ë", 0xD6: "Ö", 0xD8: "Œ", 0xDB: "Û", 0xDC: "Ü",
    0xDF: "ß", 0xE0: "à", 0xE2: "â", 0xE4: "ä", 0xE7: "ç",
    0xE8: "è", 0xE9: "é", 0xEA: "ê", 0xEB: "ë", 0xEE: "î",
    0xEF: "ï", 0xF4: "ô", 0xF6: "ö", 0xF9: "ù", 0xFB: "û",
    0xFC: "ü",
}


def _read_name(rom: bytes, text_id: int) -> str:
    """Read a bounded native Aria text string, handling known control opcodes.

    AoS strings have a two-byte 01 00 prefix (not part of the name). DSVEdit
    recognizes byte values 01/02/03/07/08 as two-byte control commands and
    other control bytes as stand-alone codes. Previously these were rejected
    for every affected name. The decoder ignores presentation controls, NOT
    arbitrary unknown bytes or malformed pointers.
    """
    if type(text_id) is not int or not 0 <= text_id <= 0xB4E:
        raise ValueError("Aria text ID out of range")
    pointer_offset = TEXT_POINTER_TABLE - GBA_ROM_BASE + text_id * 4
    if pointer_offset < 0 or pointer_offset + 4 > len(rom):
        raise ValueError("Aria name pointer table outside ROM")
    address = struct.unpack_from('<I', rom, pointer_offset)[0]
    pos = address - GBA_ROM_BASE
    if pos < 0 or pos + 3 > len(rom):
        raise ValueError(f"Aria text {text_id:03X}: invalid source pointer {address:08X}")
    header = rom[pos:pos + 2]
    if header != b'\x01\x00':
        raise ValueError(f"Aria text {text_id:03X}: invalid string header {header.hex()}")
    chars = []
    cursor = pos + 2
    end = min(len(rom), cursor + 160)
    while cursor < end:
        code = rom[cursor]
        cursor += 1
        if code == 0x0A:
            value = ''.join(chars).strip()
            if not value or len(value) > 80 or not any(c.isalnum() for c in value):
                raise ValueError(f"Aria text {text_id:03X}: empty/invalid item name")
            return value
        if code in (1, 2, 3, 7, 8):
            # Formatting/choice/portrait/text-color command and its one-byte
            # operand. Never interpret its parameter as a name character.
            if cursor >= end:
                break
            cursor += 1
        elif code == 6:  # Native newline / spacing between word groups.
            if chars and chars[-1] != ' ':
                chars.append(' ')
        elif code in (4, 5, 9) or 0x0B <= code <= 0x1A:
            continue  # Known single-byte control / button marker.
        elif 0x20 <= code <= 0x7E:
            chars.append(chr(code))
        elif code in SPECIAL:
            chars.append(SPECIAL[code])
        else:
            raise ValueError(f"Aria text {text_id:03X}: unknown opcode {code:02X}")
    raise ValueError(f"Aria text {text_id:03X}: unterminated string")


def diagnose_names(rom: bytes) -> tuple[int, int, list[str]]:
    """Give actionable diagnostics instead of silently labeling everything unknown."""
    correct = total = 0
    failures = []
    for family, (_, count, first_id) in _ITEM_RANGES.items():
        for item_id in range(first_id, first_id + count):
            total += 1
            try:
                item_name(rom, family, item_id)
                correct += 1
            except ValueError as exc:
                if len(failures) < 8:
                    failures.append(f"subtype={family:02X} item={item_id:03d}: {exc}")
    return correct, total, failures


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


# PATCH_0084_ARIA_GTK_LIFETIMES: CLI entrypoint MUST follow item_name/named_options.
if __name__ == '__main__':
    from scripts.import_game_assets import verified_rom
    from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1
    verified = verified_rom(DEFAULT_ROM, EXPECTED_SHA1)
    okay, total, errors = diagnose_names(verified)
    print(f'Aria item/soul names decoded: {okay}/{total}')
    for failure in errors:
        print('  ', failure)
    if okay < total:
        raise SystemExit(1)
