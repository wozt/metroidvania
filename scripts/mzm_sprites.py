#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission room sprites and enemy sprite graphics.

Room placements follow ``SpriteLoadRoomSprites`` / ``SpriteInitPrimary``: a
room entry's default enemy data (``.pDefaultSpriteData``) lists
``(y block, x block, slot)`` triples up to ``0xFF``; a slot above 16 selects
entry ``(slot - 1) % 16`` of the room's default spriteset
(``sSpritesetPointers[.defaultSpriteset]``: ``PSPRITE`` id and graphics
slot, ``SpriteLoadSpriteset``), a lower slot is sprite ``slot - 1`` in
graphics slot 0. The first and second event spritesets (hard mode and story
events) are not exported. Everything is parsed from the pinned
decompilation; no table is stored in this repository.

Sprite graphics follow ``SpriteLoadSpriteset`` and ``SpriteDraw``: the
LZ77 graphics of ``sSpritesGraphicsPointers`` go to OBJ VRAM 0x4000 + slot *
0x800 (tile 0x200 + slot * 64), the palette rows of
``sSpritesPalettePointers`` to OBJ rows 8 + slot, and each ``FrameData``
table of the sprite's data file is drawn relative to the sprite position,
mirrored around it for the X and Y flip statuses. Only sprites whose AI is
ported (``PORTED``) are exported, each with the palette row its AI selects.
Enemy drops (``DROPS``) draw from the common sprite graphics instead
(``HudGenericLoadCommonSpriteGfx``: ``sCommonSpritesGfx`` at OBJ VRAM 0x800,
``sCommonSpritesPal`` at OBJ palette rows 2-7), without flips.
Output stays private:

    assets/extracted/metroid/sprites/enemies/runtime/runtime_index.tsv
    assets/extracted/metroid/metadata/sprites.tsv

``sprites.tsv`` holds the ROM tables the sprite engine reads:
``sSpriteRandomNumberTable`` (``random``), ``sSpritesFallingSpeed``
(``falling``) and the ``sPrimarySpriteStats`` row of every primary sprite
(``stats``: name, id, then the SPRITE_STATS columns: health, damage,
weaknesses, drop probabilities), the ``sSecondarySpriteStats`` rows
(``sstats``, same columns) and the signed tables of ported AIs (``table``:
symbol, values).
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import mzm_projectile_compose  # noqa: E402
from scripts.asset_layout import METROID_METADATA, METROID_SPRITES, private_path  # noqa: E402
from scripts.gba_tiles import bgr555  # noqa: E402
from scripts.mzm_room_render import lz77  # noqa: E402
from scripts.mzm_samus_compose import parse_symbols, verify_symbols_against_rom  # noqa: E402
from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset  # noqa: E402
from scripts.sprite_library import LibraryWriter, bmp_from_pixels, write_atomic  # noqa: E402

ENEMIES_RUNTIME = METROID_SPRITES / "enemies" / "runtime"
DECOMP = ROOT / "third_party/mzm"
# Sprites with a ported AI, and the palette row it selects (ZoomerInit:
# the red Zoomer sets paletteRow and absolutePaletteRow to 1; the Rippers
# have their own graphics and palettes).
PORTED = {"PSPRITE_ZOOMER_YELLOW": 0, "PSPRITE_ZOOMER_RED": 1,
          "PSPRITE_RIPPER_BROWN": 0, "PSPRITE_RIPPER_PURPLE": 0,
          "PSPRITE_SKREE_GREEN": 0, "PSPRITE_SKREE_BLUE": 0,
          "PSPRITE_ZEELA": 0, "PSPRITE_ZEELA_RED": 1, "PSPRITE_WAVER": 0,
          "PSPRITE_SIDEHOPPER": 0, "PSPRITE_DESSGEEGA": 0,
          "PSPRITE_DESSGEEGA_AFTER_LONG_BEAM": 0, "PSPRITE_VIOLA_BLUE": 0,
          "PSPRITE_VIOLA_ORANGE": 0, "PSPRITE_SOVA_PURPLE": 0, "PSPRITE_SOVA_ORANGE": 1,
          "PSPRITE_ZEB": 0, "PSPRITE_ZEB_BLUE": 0, "PSPRITE_GEEGA": 0, "PSPRITE_GEEGA_WHITE": 0,
          "PSPRITE_GEEGA_LEADER": 0, "PSPRITE_GEEGA_FOLLOWER": 0, "PSPRITE_ZEBBO_GREEN": 0,
          "PSPRITE_ZEBBO_YELLOW": 0, "PSPRITE_ZEBBO_GREEN_LEADER": 0,
          "PSPRITE_ZEBBO_GREEN_FOLLOWER": 0, "PSPRITE_REO_GREEN_WINGS": 0,
          "PSPRITE_REO_PURPLE_WINGS": 0, "PSPRITE_MELLA": 0, "PSPRITE_HOLTZ": 0,
          "PSPRITE_MULTIVIOLA": 0, "PSPRITE_RINKA_ORANGE": 0, "PSPRITE_RINKA_GREEN": 0,
          "PSPRITE_SPACE_PIRATE": 0, "PSPRITE_SPACE_PIRATE_WAITING1": 0,
          "PSPRITE_SPACE_PIRATE_WAITING2": 0, "PSPRITE_SPACE_PIRATE_WAITING3": 0,
          "PSPRITE_SPACE_PIRATE2": 0, "PSPRITE_SEARCHLIGHT": 0, "PSPRITE_SEARCHLIGHT2": 0,
          "PSPRITE_SEARCHLIGHT3": 0, "PSPRITE_SEARCHLIGHT4": 0,
          "PSPRITE_SECURITY_LASER_VERTICAL": 0, "PSPRITE_SECURITY_LASER_HORIZONTAL": 0,
          "PSPRITE_SECURITY_LASER_VERTICAL2": 0, "PSPRITE_SECURITY_LASER_HORIZONTAL2": 0,
          "PSPRITE_SEARCHLIGHT_EYE": 0, "PSPRITE_SEARCHLIGHT_EYE2": 0}
# s16 (or small u16) tables an AI reads, written as ``table`` lines.
AI_TABLES = ("sSkreeFallingSpeed", "sZeelaEyesFallingFromUpSpeed",
             "sZeelaEyesFallingFromBottomSpeed", "sSpritesFallingCeilingSpeed",
             "sSidehopperHighJumpVelocity", "sSidehopperLowJumpVelocity",
             "sDessgeegaHighJumpYVelocity", "sDessgeegaLowJumpYVelocity",
             "sReoIdleYMovement", "sReoIdleXMovement", "sMellaIdleYMovement",
             "sMellaIdleXMovement", "sMellaGoingDownYMovement", "sMellaGoingUpYMovement",
             "sMellaMovingXMovement", "sHoltzIdleYVelocity", "sSpacePirateWallJumpingVelocity",
             "sSpacePirate_2e1030", "sSpacePirateJumpingVelocity", "sSpacePirate_2e105a",
             "sDebrisBouncingVelocity", "sDebris11YVelocity",
             "sDebris12YVelocity", "sDebris13YVelocity", "sDebris4YVelocity",
             "sDebris5YVelocity", "sDebris6YVelocity", "sDebris7YVelocity",
             "sDebris8YVelocity")
# Particle effects and sprite debris drawn from the common sprite graphics
# (ParticleDraw, SpriteDebrisDraw: no palette row or graphics slot added).
PARTICLES = ("sParticleBombOam", "sParticleSpriteExplosionHugeOam", "sParticleSpriteExplosionSmallOam",
             "sParticleSpriteExplosionMediumOam", "sParticleSpriteExplosionBigOam",
             "sParticleSpriteExplosionSingleThenBigOam", "sParticleScrewAttackDestroyedOam",
             "sParticleShinesparkDestroyedOam", "sParticleSpeedboosterDestroyedOam",
             "sParticleHittingSomethingWithNormalBeamOam",
             "sParticleHittingSomethingInvincibleOam",
             "sParticleHittingSomethingWithMissileOam",
             "sParticleHittingSomethingWithSuperMissileOam", "sParticleMediumDustOam",
             "sParticleTwoMediumDustOam", "sSpriteDebrisOam")
SPRITE_VRAM = 0x4000
# Enemy drops (src/sprites_ai/enemy_drop.c, EnemyDropInit) and their table.
DROPS = {
    "PSPRITE_SMALL_ENERGY_DROP": "sEnemyDropOam_SmallEnergy",
    "PSPRITE_LARGE_ENERGY_DROP": "sEnemyDropOam_LargeEnergy",
    "PSPRITE_MISSILE_DROP": "sEnemyDropOam_Missile",
    "PSPRITE_SUPER_MISSILE_DROP": "sEnemyDropOam_SuperMissile",
    "PSPRITE_POWER_BOMB_DROP": "sEnemyDropOam_PowerBomb",
}
COMMON_GFX_VRAM = 0x800             # VRAM_OBJ + 0x800
COMMON_PALETTE_ROW = 2              # PALRAM_BASE + 0x240: OBJ palette row 2
SPRITESET_ENTRY_TYPES = 16          # MAX_AMOUNT_OF_SPRITE_TYPES (15) + 1


def _c_body(text: str, start: int) -> str:
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise ValueError("unterminated C initializer")


def psprite_ids(header: str) -> dict[str, int]:
    """Values of the PrimarySprite enum (consecutive from PSPRITE_UNUSED0)."""
    return enum_ids(header, "PrimarySprite")


def ssprite_ids(header: str) -> dict[str, int]:
    """Values of the SecondarySprite enum."""
    return enum_ids(header, "SecondarySprite")


def enum_ids(header: str, enum: str) -> dict[str, int]:
    match = re.search(rf"MAKE_ENUM\(u8,\s*{enum}\)", header)
    if not match:
        raise ValueError(enum + " enum not found")
    ids = {}
    value = 0
    for item in _c_body(header, match.end()).split(","):
        item = re.sub(r"//[^\n]*|/\*.*?\*/", "", item, flags=re.S).strip()
        if not item:
            continue
        name, _, explicit = item.partition("=")
        if explicit.strip():
            value = int(explicit.strip(), 0)
        ids[name.strip()] = value
        value += 1
    return ids


def pointer_table(source: str, table: str) -> dict[str, str]:
    """``[PSPRITE_OFFSET_FOR_GRAPHICS(PSPRITE_X)] = symbol`` entries."""
    match = re.search(rf"\b{table}\s*\[[^\]]*\]\s*=", source)
    if not match:
        raise ValueError(table + " not found")
    return dict(re.findall(r"\[PSPRITE_OFFSET_FOR_GRAPHICS\((PSPRITE_\w+)\)\]\s*=\s*(\w+)",
                           _c_body(source, match.end())))


def spritesets(sprite_c: str, spriteset_c: str, ids: dict[str, int]) -> list[list[tuple[str, int]]]:
    """``sSpritesetPointers`` order, each a list of (PSPRITE name, gfx slot)."""
    match = re.search(r"\bsSpritesetPointers\s*\[[^\]]*\]\s*=", sprite_c)
    if not match:
        raise ValueError("sSpritesetPointers not found")
    order = re.findall(r"\w+", _c_body(sprite_c, match.end()))
    names = {value: name for name, value in ids.items()}
    contents = {}
    for found in re.finditer(r"const\s+u8\s+(sSpriteset\w+)\s*\[[^\]]*\]\s*=", spriteset_c):
        body = _c_body(spriteset_c, found.end()).replace("SPRITESET_EMPTY", "0, 0")
        tokens = [token.strip() for token in body.split(",") if token.strip()]
        entries = []
        for name, slot in zip(tokens[0::2], tokens[1::2]):
            value = ids[name] if name in ids else int(name, 0)
            if value == 0:
                break
            entries.append((names.get(value, name), int(slot, 0)))
        contents[found.group(1)] = entries
    return [contents.get(name, []) for name in order]


def room_entries(rooms_c: str, area: str) -> dict[int, dict[str, str]]:
    """Fields of ``s<Area>RoomEntries`` by room index."""
    match = re.search(rf"\bs{area}RoomEntries\s*\[[^\]]*\]\s*=", rooms_c)
    if not match:
        raise ValueError(f"s{area}RoomEntries not found")
    body = _c_body(rooms_c, match.end())
    entries = {}
    for found in re.finditer(r"\[(\d+)\]\s*=\s*\{", body):
        fields = dict(re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", _c_body(body, found.end() - 1)))
        entries[int(found.group(1))] = {key: value.strip() for key, value in fields.items()}
    return entries


def enemy_data(rooms_dir: Path, symbol: str) -> list[int]:
    """The bytes of a room's enemy data array, up to the terminator."""
    pattern = re.compile(rf"const\s+u8\s+{re.escape(symbol)}\s*\[[^\]]*\]\s*=")
    for path in sorted(rooms_dir.rglob("*.c")) + [rooms_dir.parent / "rooms_data.c"]:
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        found = pattern.search(text)
        if not found:
            continue
        body = _c_body(text, found.end()).replace("ROOM_SPRITE_DATA_TERMINATOR",
                                                   "255, 255, 255")
        values = []
        for token in (item.strip() for item in body.split(",")):
            if not token:
                continue
            index = re.fullmatch(r"SPRITESET_IDX\((\d+)\)", token)
            values.append(16 + int(index.group(1)) + 1 if index else int(token, 0))
        return values
    raise ValueError("enemy data not found: " + symbol)


def _room_entry_and_spriteset(area: str, number: int, decomp: Path):
    rooms_c = (decomp / "src/data/rooms_data.c").read_text(encoding="utf-8")
    entry = room_entries(rooms_c, area).get(number)
    if entry is None:
        raise ValueError(f"no room entry {area} {number}")
    header = (decomp / "include/constants/sprite.h").read_text(encoding="utf-8")
    ids = psprite_ids(header)
    sets = spritesets((decomp / "src/sprite.c").read_text(encoding="utf-8"),
                      (decomp / "src/data/spriteset.c").read_text(encoding="utf-8"), ids)
    spriteset = int(entry.get("defaultSpriteset", "0"), 0)
    if spriteset >= len(sets) - 1:
        spriteset = len(sets) - 1
    return entry, ids, sets, spriteset


def room_spriteset(area: str, number: int, decomp: Path = DECOMP) -> tuple[int, list]:
    """The room's default spriteset (SpriteLoadSpriteset: gSpriteset and
    gSpritesetSpritesId): its index and (PSPRITE name, graphics slot) entries."""
    _, _, sets, spriteset = _room_entry_and_spriteset(area, number, decomp)
    return spriteset, [(name, slot & 7) for name, slot in sets[spriteset]]


def room_sprites(area: str, number: int, decomp: Path = DECOMP) -> list[dict]:
    """Default sprites of a room: block position, PSPRITE and graphics slot."""
    entry, ids, sets, spriteset = _room_entry_and_spriteset(area, number, decomp)
    names = {value: name for name, value in ids.items()}
    data = enemy_data(decomp / "src/data/rooms", entry["pDefaultSpriteData"])
    sprites = []
    for index in range(0, len(data) - 2, 3):
        y, x, slot = data[index:index + 3]
        if y == 0xFF:
            break
        slot &= 0x7F
        if slot > SPRITESET_ENTRY_TYPES:
            choice = (slot - 1) % SPRITESET_ENTRY_TYPES
            members = sets[spriteset]
            if choice >= len(members):
                continue
            sprite, gfx_slot = members[choice]
            gfx_slot &= 7
        else:
            sprite, gfx_slot = names.get(slot - 1, f"PSPRITE_{slot - 1}"), 0
        sprites.append({"y": y, "x": x, "slot": slot, "sprite": sprite, "gfx_slot": gfx_slot,
                        "room_slot": index // 3})
    return sprites


def data_file(decomp: Path, gfx_symbol: str) -> Path:
    pattern = re.compile(rf"const\s+u32\s+{re.escape(gfx_symbol)}\s*\[")
    for path in sorted((decomp / "src/data/sprites").glob("*.c")):
        if pattern.search(path.read_text(encoding="utf-8")):
            return path
    raise ValueError("no sprite data file defines " + gfx_symbol)


def frame_tables(source: str) -> list[str]:
    return re.findall(r"^(?:static\s+)?const\s+struct\s+FrameData\s+(s\w+)\s*\[", source, re.M)


def stage(rom: bytes, symbols, gfx: str, palette_symbol: str, palette_row: int):
    """OBJ VRAM with the sprite in graphics slot 0, and the OBJ palette with
    its palette row `palette_row` at row 8 (SpriteDraw adds the row)."""
    vram = bytearray(0x8000)
    address, size = symbols[gfx]
    tiles = lz77(rom[rom_offset(address, rom):rom_offset(address, rom) + size])
    if SPRITE_VRAM + len(tiles) > len(vram):
        raise ValueError(gfx + " overflows OBJ VRAM")
    vram[SPRITE_VRAM:SPRITE_VRAM + len(tiles)] = tiles
    address, size = symbols[palette_symbol]
    colors = rom[rom_offset(address, rom):rom_offset(address, rom) + size]
    palette = [None] * 256
    rows = size // 32
    for row in range(rows - palette_row):
        for index in range(16):
            value = struct.unpack_from("<H", colors, ((palette_row + row) * 16 + index) * 2)[0]
            palette[(8 + row) * 16 + index] = bgr555(value)
    return bytes(vram), palette


def stage_common(rom: bytes, symbols):
    """OBJ VRAM and palette as HudGenericLoadCommonSpriteGfx and the room
    load leave them for the common sprites (enemy drops)."""
    vram = bytearray(0x8000)
    address, size = symbols["sCommonSpritesGfx"]
    vram[COMMON_GFX_VRAM:COMMON_GFX_VRAM + size] = rom[rom_offset(address, rom):
                                                       rom_offset(address, rom) + size]
    address, size = symbols["sCommonSpritesPal"]
    colors = rom[rom_offset(address, rom):rom_offset(address, rom) + size]
    palette = [None] * 256
    for index in range(size // 2):
        palette[COMMON_PALETTE_ROW * 16 + index] = bgr555(struct.unpack_from("<H", colors,
                                                                            index * 2)[0])
    return bytes(vram), palette


def produce(root: Path, rom_path: Path, elf: Path, decomp: Path = DECOMP,
            nm: str = "arm-none-eabi-nm", objcopy: str = "arm-none-eabi-objcopy") -> dict:
    from scripts.mzm_samus_pipeline import _elf_image, _run_nm
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    sprite_c = (decomp / "src/sprite.c").read_text(encoding="utf-8")
    graphics = pointer_table(sprite_c, "sSpritesGraphicsPointers")
    palettes = pointer_table(sprite_c, "sSpritesPalettePointers")
    plan = []
    wanted = set()
    for sprite, row in PORTED.items():
        source = data_file(decomp, graphics[sprite]).read_text(encoding="utf-8")
        tables = frame_tables(source)
        plan.append((sprite, row, graphics[sprite], palettes[sprite], tables))
        wanted.update([graphics[sprite], palettes[sprite], *tables])
    tables = ("sSpriteRandomNumberTable", "sSpritesFallingSpeed", "sPrimarySpriteStats")
    wanted.update(tables)
    wanted.update(["sSecondarySpriteStats", *AI_TABLES])
    wanted.update(["sCommonSpritesGfx", "sCommonSpritesPal", *DROPS.values(), *PARTICLES])
    nm_output = _run_nm(nm, elf)
    symbols = {name: value for name, value in parse_symbols(nm_output, tuple(wanted)).items()
               if name in wanted}
    missing = wanted - symbols.keys()
    if missing:
        raise ValueError("reference ELF lacks " + ", ".join(sorted(missing)[:5]))
    verify_symbols_against_rom(rom, _elf_image(objcopy, elf), symbols)
    header = (decomp / "include/constants/sprite.h").read_text(encoding="utf-8")
    ids = psprite_ids(header)
    secondary = ssprite_ids(header)
    lines = ["schema\tmetroidvania-mzm-sprite-tables-v1"]

    def words(symbol: str, signed: bool, width: int) -> list[int]:
        address, size = symbols[symbol]
        data = rom[rom_offset(address, rom):rom_offset(address, rom) + size]
        code = {1: "b", 2: "h"}[width] if signed else {1: "B", 2: "H"}[width]
        return list(struct.unpack(f"<{size // width}{code}", data))

    lines.append("random\t" + "\t".join(map(str, words("sSpriteRandomNumberTable", False, 1))))
    lines.append("falling\t" + "\t".join(map(str, words("sSpritesFallingSpeed", True, 2))))
    stats = words("sPrimarySpriteStats", False, 2)
    count = ids.get("PSPRITE_COUNT", 0)
    columns = len(stats) // count if count else 0
    if not columns or columns * count != len(stats):
        raise ValueError("sPrimarySpriteStats does not match the PSPRITE count")
    for name, value in sorted(ids.items(), key=lambda item: item[1]):
        if name == "PSPRITE_COUNT":
            continue
        row = stats[value * columns:(value + 1) * columns]
        lines.append("\t".join(["stats", name[len("PSPRITE_"):], str(value), *map(str, row)]))
    stats = words("sSecondarySpriteStats", False, 2)
    count = secondary.get("SSPRITE_COUNT", 0)
    if not count or columns * count != len(stats):
        raise ValueError("sSecondarySpriteStats does not match the SSPRITE count")
    for name, value in sorted(secondary.items(), key=lambda item: item[1]):
        if name == "SSPRITE_COUNT":
            continue
        row = stats[value * columns:(value + 1) * columns]
        lines.append("\t".join(["sstats", name[len("SSPRITE_"):], str(value), *map(str, row)]))
    for table in AI_TABLES:
        lines.append("\t".join(["table", table, *map(str, words(table, True, 2))]))
    folder = private_path(Path(root), METROID_METADATA, create=True)
    folder.mkdir(parents=True, exist_ok=True)
    write_atomic(folder / "sprites.tsv", "\n".join(lines) + "\n")
    library = LibraryWriter(root, ENEMIES_RUNTIME)
    for sprite, row, gfx, palette_symbol, tables in plan:
        vram, palette = stage(rom, symbols, gfx, palette_symbol, row)
        for table in tables:
            entries = mzm_projectile_compose.frames(rom, symbols, table)
            for flip in mzm_projectile_compose.FLIPS:
                frames = []
                for pointer, duration in entries:
                    pixels = mzm_projectile_compose.render(rom, vram, palette, pointer, flip)
                    bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                    frames.append((bmp, duration, left, top))
                library.add(f"Sprite/{sprite[len('PSPRITE_'):]}/{table[1:]}/{flip}", frames)
    vram, palette = stage_common(rom, symbols)
    for sprite, table in DROPS.items():
        frames = []
        for pointer, duration in mzm_projectile_compose.frames(rom, symbols, table):
            pixels = mzm_projectile_compose.render(rom, vram, palette, pointer, "none")
            bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
            frames.append((bmp, duration, left, top))
        library.add(f"Sprite/{sprite[len('PSPRITE_'):]}/{table[1:]}/none", frames)
    for table in PARTICLES:
        frames = []
        for pointer, duration in mzm_projectile_compose.frames(rom, symbols, table):
            pixels = mzm_projectile_compose.render(rom, vram, palette, pointer, "none")
            bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
            frames.append((bmp, duration, left, top))
        library.add(f"Particle/{table[1:]}/none", frames)
    return library.finish()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path, default=ROOT / "roms/Metroid - Zero Mission (USA).gba")
    parser.add_argument("--elf", type=Path, default=DECOMP / "mzm_us.elf")
    parser.add_argument("--decomp", type=Path, default=DECOMP)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom, args.elf, args.decomp, args.nm, args.objcopy)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    print(f"Zero Mission enemy sprites: {result['sequences']} sequences, "
          f"{result['frames']} frames, {result['unique_bmps']} unique BMPs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
