#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission room sprites and enemy sprite graphics.

Room placements follow ``RoomLoadEntry`` / ``SpriteLoadRoomSprites`` /
``SpriteInitPrimary``: each room entry's default, first-event and
second-event enemy data lists
``(y block, x block, slot)`` triples up to ``0xFF``; a slot above 16 selects
entry ``(slot - 1) % 16`` of the selected spriteset
(``sSpritesetPointers``: ``PSPRITE`` id and graphics
slot, ``SpriteLoadSpriteset``), a lower slot is sprite ``slot - 1`` in
graphics slot 0. The second event has native priority over the first one.
Everything is parsed from the pinned decompilation; no table is stored in
this repository.

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

The message banner (``PSPRITE_MESSAGE_BANNER``) is exported once per English
message as ``Banner/<MESSAGE>/<table>/none``, its OAM tables drawn over the
banner graphics with the message's text (``scripts/mzm_message_text.py``);
``message`` lines give each message's id, name and TextProcessMessageBanner
calls.

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

from scripts import mzm_message_text, mzm_projectile_compose  # noqa: E402
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
          "PSPRITE_KRAID_STATUE": 0, "PSPRITE_RIDLEY_STATUE": 0,
          "PSPRITE_REO_PURPLE_WINGS": 0, "PSPRITE_MELLA": 0, "PSPRITE_HOLTZ": 0,
          "PSPRITE_MULTIVIOLA": 0, "PSPRITE_RINKA_ORANGE": 0, "PSPRITE_RINKA_GREEN": 0,
          "PSPRITE_SPACE_PIRATE": 0, "PSPRITE_SPACE_PIRATE_WAITING1": 0,
          "PSPRITE_SPACE_PIRATE_WAITING2": 0, "PSPRITE_SPACE_PIRATE_WAITING3": 0,
          "PSPRITE_SPACE_PIRATE2": 0, "PSPRITE_SEARCHLIGHT": 0, "PSPRITE_SEARCHLIGHT2": 0,
          "PSPRITE_SEARCHLIGHT3": 0, "PSPRITE_SEARCHLIGHT4": 0,
          "PSPRITE_SECURITY_LASER_VERTICAL": 0, "PSPRITE_SECURITY_LASER_HORIZONTAL": 0,
          "PSPRITE_SECURITY_LASER_VERTICAL2": 0, "PSPRITE_SECURITY_LASER_HORIZONTAL2": 0,
          "PSPRITE_SEARCHLIGHT_EYE": 0, "PSPRITE_SEARCHLIGHT_EYE2": 0,
          "PSPRITE_GERUTA_RED": 0, "PSPRITE_GERUTA_GREEN": 0, "PSPRITE_RIPPERII": 0,
          "PSPRITE_SQUEEPT": 0, "PSPRITE_GAMET_BLUE_SINGLE": 0, "PSPRITE_GAMET_RED_SINGLE": 0,
          "PSPRITE_GAMET_BLUE_LEADER": 0, "PSPRITE_GAMET_BLUE_FOLLOWER": 0,
          "PSPRITE_MORPH_BALL": 0, "PSPRITE_POWER_GRIP": 0, "PSPRITE_CHARGE_BEAM": 0,
          "PSPRITE_SKULTERA": 0, "PSPRITE_DRAGON": 0,
          "PSPRITE_ELEVATOR_PAD": 0, "PSPRITE_STEAM_LARGE": 0, "PSPRITE_STEAM_SMALL": 0,
          "PSPRITE_STEAM_LARGE_DIAGONAL_UP": 0, "PSPRITE_STEAM_SMALL_DIAGONAL_UP": 0,
          "PSPRITE_STEAM_LARGE_DIAGONAL_DOWN": 0, "PSPRITE_STEAM_SMALL_DIAGONAL_DOWN": 0,
          "PSPRITE_GERON_BRINSTAR_ROOM_15": 0, "PSPRITE_GERON_BRINSTAR_ROOM_1C": 0,
          "PSPRITE_GERON_VARIA1": 0, "PSPRITE_GERON_VARIA2": 0, "PSPRITE_GERON_VARIA3": 0,
          "PSPRITE_GERON_NORFAIR": 0, "PSPRITE_PLASMA_BEAM_BLOCK": 0,
          "PSPRITE_GRAVITY_SUIT_BLOCK": 0, "PSPRITE_SPACE_JUMP_BLOCK": 0, "PSPRITE_MAP_STATION": 0,
          "PSPRITE_SAVE_PLATFORM": 0, "PSPRITE_GUNSHIP": 0, "PSPRITE_AREA_BANNER": 0,
          "PSPRITE_MORPH_BALL_LAUNCHER": 0, "PSPRITE_ZIPLINE": 0, "PSPRITE_ZIPLINE_BUTTON": 0,
          "PSPRITE_ZIPLINE_GENERATOR": 0, "PSPRITE_SECURITY_GATE_DEFAULT_OPEN": 0,
          "PSPRITE_PISTON": 0, "PSPRITE_GLASS_TUBE": 0, "PSPRITE_RISING_CHOZO_PILLAR": 0,
          "PSPRITE_PARASITE": 0, "PSPRITE_PARASITE_MULTIPLE": 0,
          "PSPRITE_DEOREM_FIRST_LOCATION": 0, "PSPRITE_DEOREM_SECOND_LOCATION": 0,
          "PSPRITE_ACID_WORM": 0,
          "PSPRITE_GADORA_KRAID": 0, "PSPRITE_GADORA_RIDLEY": 0,
          "PSPRITE_IMAGO_LARVA_RIGHT": 0, "PSPRITE_IMAGO_LARVA_LEFT": 0,
          "PSPRITE_IMAGO_LARVA_RIGHT_SIDE": 0,
          "PSPRITE_IMAGO_COCOON": 0, "PSPRITE_IMAGO_COCOON_AFTER_FIGHT": 0,
          "PSPRITE_MECHA_RIDLEY": 0,
          "PSPRITE_SAVE_PLATFORM_CHOZODIA": 0, "PSPRITE_ATOMIC": 0, "PSPRITE_POLYP": 0, "PSPRITE_CANNON": 0, "PSPRITE_ZEBETITE_ONE_AND_THREE": 0, "PSPRITE_WORKER_ROBOT": 0, "PSPRITE_METROID": 0, "PSPRITE_KRAID": 0, "PSPRITE_RIDLEY": 0, "PSPRITE_MOTHER_BRAIN": 0,
          **{f"PSPRITE_CHOZO_STATUE_{item}{hint}": 0
             for item in ("LONG", "ICE", "WAVE", "BOMB", "SPEEDBOOSTER", "HIGH_JUMP", "SCREW",
                          "VARIA") for hint in ("", "_HINT")},
          "PSPRITE_CHOZO_STATUE_PLASMA_BEAM": 0, "PSPRITE_CHOZO_STATUE_SPACE_JUMP": 0,
          "PSPRITE_CHOZO_STATUE_GRAVITY": 0}
# Extra palette rows a ported AI sets (paletteRow), baked as ``/rowN``.
# The tables a sprite's row variants apply to (by prefix; the others keep
# their rows): only the Atomic's body flashes, not its electricity.
ROW_VARIANT_TABLES = {
    "PSPRITE_ATOMIC": ("sAtomicOam_",),
    "PSPRITE_METROID": ("sMetroidOam_Spawning",),
    "PSPRITE_IMAGO_COCOON": ("sImagoCocoonOam_Idle", "sImagoCocoonOam_2e0bb0",
                              "sImagoCocoonOam_Static"),
}
# MechaRidleyLoadMissilesGfx / LoadFireballsGfx: four 4-tile rows of
# sMechaRidleyWeaponsGfx copied to OBJ VRAM 0x4280 + 1024 * row each frame,
# from tile 4 * stage (missiles, stages 0-2, "/w0".."/w2") or 12 + 4 * stage
# (fireballs, stages 0-3, "/w3".."/w6").
MECHA_RIDLEY_WEAPONS = "sMechaRidleyWeaponsGfx"
MECHA_RIDLEY_WEAPON_TABLES = ("sMechaRidleyMissileOam", "sMechaRidleyFireballOam")
MECHA_RIDLEY_WEAPON_VRAM = 0x4280
MECHA_RIDLEY_WEAPON_STAGES = tuple(4 * stage for stage in range(3)) + tuple(
    12 + 4 * stage for stage in range(4))


def mecha_ridley_weapon_vram(vram: bytes, weapons: bytes, first_tile: int) -> bytes:
    """OBJ VRAM after one weapon graphics copy (4 tiles in each of 4 rows)."""
    staged = bytearray(vram)
    for row in range(4):
        source = row * 1024 + first_tile * 32
        target = MECHA_RIDLEY_WEAPON_VRAM + row * 1024
        staged[target:target + 128] = weapons[source:source + 128]
    return bytes(staged)


TINT_OVERFLOW_ALLOWED = ("PSPRITE_MECHA_RIDLEY",)
ROW_VARIANTS = {"PSPRITE_ATOMIC": (2, 3), "PSPRITE_KRAID": (1, 2, 3), "PSPRITE_RIDLEY": (3, 5), "PSPRITE_GADORA_RIDLEY": (1,), "PSPRITE_IMAGO_LARVA_RIGHT": (1, 2), "PSPRITE_IMAGO_LARVA_LEFT": (1, 2), "PSPRITE_IMAGO_COCOON": (1, 2), "PSPRITE_MOTHER_BRAIN": (2, 3, 4), "PSPRITE_METROID": (1, 2, 3), "PSPRITE_MECHA_RIDLEY": (2, 3, 4), "PSPRITE_SAVE_PLATFORM": (2,), "PSPRITE_SAVE_PLATFORM_CHOZODIA": (2,)}
# The gunship's palette animations: GunshipEntranceFlashingAnim copies
# sGunshipFlashingPal[0x38 + 16 * e] (8 colors) to OBJ row 9 colors 8-15
# (e 0-4) and GunshipFlickerFlames sGunshipFlashingPal[16 * f] to OBJ row 10
# (f 0-2); every combination, each row also untouched (its sGunshipPal
# colors), is baked as ``/eEfF``, ``/eE``, ``/fF`` or no suffix (the gunship
# never flips).
GUNSHIP_FLASHING = "sGunshipFlashingPal"
# MetroidSamusGrabbed copies 8 colors of sMetroidPal_SamusGrabbed row N
# (0-4) to OBJ row 12, which only sMetroidOam_SamusGrabbed's parts use;
# baked as ``/gN`` for that table.
METROID_GRABBED = "sMetroidPal_SamusGrabbed"
# The area banner (PSPRITE_AREA_BANNER) with each location text drawn in its
# graphics (TextDrawLocation: the text in mask mode over 0x800 bytes of
# 0xFF, its two tile rows copied to the graphics slot), baked as ``/ltN``;
# the area names (below LT_SAVE_ROOM) use sAreaBannerLocationTextPal.
LOCATION_TEXTS = 14                     # LT_BRINSTAR ... LT_STARSHIP
LT_SAVE_ROOM = 9
# MultiSpriteData tables (gSubSpriteData1.pMultiOam) of ported sprites, read
# from the decompilation as ``multi`` lines: name, frame, timer, then each
# part's OAM index, y and x offsets.
MULTI_SPRITE_DATA = {
    "sprites/chozo_statue.c": (("sprites_ai/chozo_statue.h", "data/sprites/chozo_statue.h",
                                "structs/sprite.h"),
                               ("sChozoStatueMultiSpriteData_Standing",
                                "sChozoStatueMultiSpriteData_Sitting",
                                "sChozoStatueMultiSpriteData_Seated")),
    "sprites/mother_brain.c": (("sprites_ai/mother_brain.h", "data/sprites/mother_brain.h",
                                "structs/sprite.h"), ("sMotherBrainMultiSpriteData",)),
    "sprites/ridley.c": (("sprites_ai/ridley.h", "data/sprites/ridley.h", "structs/sprite.h"),
                         ("sRidleyMultiSpriteData_Idle", "sRidleyMultiSpriteData_SpittingFireballs",
                          "sRidleyMultiSpriteData_TurningAroundFirstPart",
                          "sRidleyMultiSpriteData_TurningAroundSecondPart",
                          *("sRidleyTailMultiSpriteData_" + name for name in (
                              "Idle", "Dying", "MoveToAttack", "SettingUpDiagonalTailAttack",
                              "ChargingDiagonalTailAttack", "DiagonalTailAttack",
                              "ChargingVerticalTailAttack", "VerticalTailAttack", "LastVerticalAttack",
                              "BackToIdle")))),
    "sprites/kraid.c": (("sprites_ai/kraid.h", "data/sprites/kraid.h", "structs/sprite.h"),
                        tuple("sKraidMultiSpriteData_" + name for name in (
                            "Rising", "Standing", "StandingBetweenSteps", "MovingLeftFeetToRight",
                            "MovingRightFeetToRight", "MovingLeftFeetToLeft", "MovingRightFeetToLeft",
                            "Dying1", "Dying2"))),
    "sprites/imago_larva.c": (("sprites_ai/imago_larva.h", "data/sprites/imago_larva.h",
                                "structs/sprite.h"),
                               tuple("sImagoLarvaMultiSpriteData_" + name for name in (
                                   "Attacking", "Idle", "Retreating", "Dying", "Warning",
                                   "TakingDamage"))),
    "sprites/imago_cocoon.c": (("sprites_ai/imago_cocoon.h", "data/sprites/imago_cocoon.h",
                                 "structs/sprite.h"),
                                ("sImagoCocoonMultiSpriteData_Idle",
                                 "sImagoCocoonMultiSpriteData_Dying")),
    "sprites/mecha_ridley.c": (("sprites_ai/mecha_ridley.h", "data/sprites/mecha_ridley.h",
                                 "structs/sprite.h"),
                                tuple("sMechaRidleyMultiSpriteData_" + name for name in (
                                    "CrawlingForwardLow", "StandingLow", "StandingMiddle",
                                    "StandingHigh", "LowToMiddle", "MiddleToHigh",
                                    "HighToMiddle", "MiddleToLow", "LowToHigh",
                                    "HighToLow", "CrawlingBackwardsLow", "OpeningMouthHigh",
                                    "SpittingFireballsHigh", "ClosingMouthHigh", "DyingStandingLow",
                                    "DyingStandingMiddle", "DyingStandingHigh", "Dying"))),
    "sprites/unknown_item_chozo_statue.c": (
        ("sprites_ai/unknown_item_chozo_statue.h", "data/sprites/unknown_item_chozo_statue.h",
         "structs/sprite.h"),
        ("sUnknownItemChozoStatueMultiSpriteData_Standing",
         "sUnknownItemChozoStatueMultiSpriteData_Sitting",
         "sUnknownItemChozoStatueMultiSpriteData_Seated")),
}
# s16 (or small u16) tables an AI reads, written as ``table`` lines.
AI_TABLES = ("sSkreeFallingSpeed", "sZeelaEyesFallingFromUpSpeed",
             "sZeelaEyesFallingFromBottomSpeed", "sSpritesFallingCeilingSpeed",
             "sSidehopperHighJumpVelocity", "sSidehopperLowJumpVelocity",
             "sDessgeegaHighJumpYVelocity", "sDessgeegaLowJumpYVelocity",
             "sReoIdleYMovement", "sReoIdleXMovement", "sMellaIdleYMovement",
             "sMellaIdleXMovement", "sMellaGoingDownYMovement", "sMellaGoingUpYMovement",
             "sMellaMovingXMovement", "sHoltzIdleYVelocity", "sSqueeptRisingMovement",
             "sSqueeptFallingMovement", "sChargeBeamIdleYMovement",
             "sDragonFireballYMovement", "sDragonFireballOamRotation", "sSpacePirateWallJumpingVelocity",
             "sSpacePirate_2e1030", "sSpacePirateJumpingVelocity", "sSpacePirate_2e105a",
             "sDebrisBouncingVelocity", "sDebris11YVelocity",
             "sDebris12YVelocity", "sDebris13YVelocity", "sDebris4YVelocity",
             "sDebris5YVelocity", "sDebris6YVelocity", "sDebris7YVelocity",
             "sDebris8YVelocity", "sGunshipTakingOffYVelocity", "sGunshipFlyingYVelocity",
             "sAtomicIdleYMovement", "sAtomicIdleXMovement",
             "sPolypProjectileYVelocity", "sWorkerRobotSleepingFallingSpeed",
             "sMetroidSpawningYMovement", "sMetroidSpawningXMovement", "sKraidPal",
             "sRidleyIdleYOffsets", "sRidleySpawningSpittingFireballsYOffsets",
             "sSineTable",
             "sAcidWormSpitCloseYVelocity", "sAcidWormSpitFarYVelocity",
             "sAcidWormSpitMiddleYVelocity", "sAcidWormSwingingMovement",
             "sAcidWormHeadRandomXVelocity",
             "sDeoremSegmentXVelocity", "sDeoremThornYVelocity")
# Particle effects and sprite debris drawn from the common sprite graphics
# (ParticleDraw, SpriteDebrisDraw: no palette row or graphics slot added).
PARTICLES = ("sParticleBombOam", "sParticleFreezingSpriteWithIceOam",
             "sParticleFreezingSpriteWithChargedIceBeamOam", "sParticleBeamTrailingOam",
             "sParticleHittingSomethingWithIceBeamOam",
             "sParticleHittingSomethingWithWaveBeamOam",
             "sParticleHittingSomethingWithPlasmaBeamOam", "sParticleMissileTrailOam",
             "sParticleSuperMissileTrailOam", "sParticleChargedLongBeamTrailOam",
             "sParticleChargedIceBeamTrailOam", "sParticleChargedWaveBeamTrailOam",
             "sParticleChargedPlasmaBeamTrailOam", "sParticleChargedFullBeamTrailOam", "sParticleSpriteExplosionHugeOam", "sParticleMainBossDeathOam", "sParticleSpriteExplosionSmallOam",
             "sParticleSpriteExplosionMediumOam", "sParticleSpriteExplosionBigOam",
             "sParticleSpriteExplosionSingleThenBigOam", "sParticleScrewAttackDestroyedOam",
             "sParticleShinesparkDestroyedOam", "sParticleSpeedboosterDestroyedOam",
             "sParticleHittingSomethingWithNormalBeamOam",
             "sParticleHittingSomethingWithLongBeamOam", "sParticlePseudoScrewDestroyedOam",
             "sParticleHittingSomethingInvincibleOam",
             "sParticleHittingSomethingWithMissileOam",
             "sParticleHittingSomethingWithSuperMissileOam", "sParticleMediumDustOam",
             "sParticleTwoMediumDustOam", "sSpriteDebrisOam")
SPRITE_VRAM = 0x4000
# The message banner's graphics and the text the banner draws into them.
BANNER_SYMBOLS = ("sMessageBannerGfx", "sMessageBannerPal", "sMessageTextPointers",
                  "sCharactersGfx", "sCharacterWidths")
# Enemy drops (src/sprites_ai/enemy_drop.c, EnemyDropInit) and their table.
DROPS = {
    "PSPRITE_SMALL_ENERGY_DROP": ("sEnemyDropOam_SmallEnergy",),
    "PSPRITE_LARGE_ENERGY_DROP": ("sEnemyDropOam_LargeEnergy",),
    "PSPRITE_MISSILE_DROP": ("sEnemyDropOam_Missile",),
    "PSPRITE_SUPER_MISSILE_DROP": ("sEnemyDropOam_SuperMissile",),
    "PSPRITE_POWER_BOMB_DROP": ("sEnemyDropOam_PowerBomb",),
    "PSPRITE_MULTIPLE_LARGE_ENERGY": (
        "sEnemyDropOam_MultipleLargeEnergySlot1",
        "sEnemyDropOam_MultipleLargeEnergySlot2",
        "sEnemyDropOam_MultipleLargeEnergySlot3",
    ),
}
# Sprites drawn with the common sprite graphics (particle_data.c tables).
COMMON_SPRITES = {
    "PSPRITE_WATER_DROP": ("sWaterDropOam_Spawning", "sWaterDropOam_Falling",
                           "sWaterDropOam_Splashing"),
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


def _resolve_room_sprites(data: list[int], ids: dict[str, int],
                          members: list[tuple[str, int]]) -> list[dict]:
    """Resolve one RoomEntryRom enemy-data array against its spriteset."""
    names = {value: name for name, value in ids.items()}
    sprites = []
    for index in range(0, len(data) - 2, 3):
        y, x, slot = data[index:index + 3]
        if y == 0xFF:
            break
        slot &= 0x7F
        if slot > SPRITESET_ENTRY_TYPES:
            choice = (slot - 1) % SPRITESET_ENTRY_TYPES
            if choice >= len(members):
                continue
            sprite, gfx_slot = members[choice]
            gfx_slot &= 7
        else:
            sprite, gfx_slot = names.get(slot - 1, f"PSPRITE_{slot - 1}"), 0
        sprites.append({"y": y, "x": x, "slot": slot, "sprite": sprite,
                        "gfx_slot": gfx_slot, "room_slot": index // 3})
    return sprites


def room_sprite_variants(area: str, number: int, decomp: Path = DECOMP) -> list[dict]:
    """The three RoomLoadEntry sprite choices in native priority order.

    Variant 0 is always the default. Variants 1 and 2 carry the numeric
    event that selects them; ``RoomLoadEntry`` checks variant 2 first.
    """
    entry, ids, sets, default_set = _room_entry_and_spriteset(area, number, decomp)
    events = enum_ids((decomp / "include/constants/event.h").read_text(encoding="utf-8"),
                      "Event")
    choices = [
        (0, 0, entry["pDefaultSpriteData"], default_set),
        (1, events.get(entry.get("firstSpritesetEvent", "EVENT_NONE"), 0),
         entry.get("pFirstSpriteData", "sEnemyRoomData_Empty"),
         int(entry.get("firstSpriteset", str(default_set)), 0)),
        (2, events.get(entry.get("secondSpritesetEvent", "EVENT_NONE"), 0),
         entry.get("pSecondSpriteData", "sEnemyRoomData_Empty"),
         int(entry.get("secondSpriteset", str(default_set)), 0)),
    ]
    variants = []
    for variant, event, symbol, spriteset in choices:
        if variant and event == 0:
            continue
        if spriteset >= len(sets) - 1:
            spriteset = len(sets) - 1
        members = [(name, slot & 7) for name, slot in sets[spriteset]]
        data = enemy_data(decomp / "src/data/rooms", symbol)
        variants.append({"variant": variant, "event": event, "spriteset": spriteset,
                         "members": members,
                         "sprites": _resolve_room_sprites(data, ids, sets[spriteset])})
    return variants


def room_spriteset(area: str, number: int, decomp: Path = DECOMP) -> tuple[int, list]:
    """The room's default spriteset (SpriteLoadSpriteset: gSpriteset and
    gSpritesetSpritesId): its index and (PSPRITE name, graphics slot) entries."""
    _, _, sets, spriteset = _room_entry_and_spriteset(area, number, decomp)
    return spriteset, [(name, slot & 7) for name, slot in sets[spriteset]]


def room_sprites(area: str, number: int, decomp: Path = DECOMP) -> list[dict]:
    """Default sprites of a room: block position, PSPRITE and graphics slot."""
    return room_sprite_variants(area, number, decomp)[0]["sprites"]


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


def shifted_palettes(palette: list, common: list) -> dict[str, list]:
    """The frozen and stun looks: SpriteDraw adds the sprite's paletteRow to
    each part's row, and ProjectileFreezeSprite / SPRITE_GET_STUN_PALETTE set
    it to NBR_OF_PALETTE_ROWS - gfx slot - 0 or 1, so a part of row ``b``
    shows OBJ row (b + 15) or (b + 14) mod 16 (the common frozen and stun
    rows for row 8; frozenPaletteRowOffset 0)."""
    full = [common[i] if common[i] is not None else palette[i] for i in range(256)]
    result = {}
    for suffix, shift in (("/frozen", 15), ("/stun", 14)):
        shifted = list(palette)
        for bank in range(8, 16):
            source = (bank + shift) % 16
            for index in range(16):
                shifted[bank * 16 + index] = full[source * 16 + index]
        result[suffix] = shifted
    return result


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
    wanted.add(MECHA_RIDLEY_WEAPONS)
    wanted.update(["sSecondarySpriteStats", "sHazeData", GUNSHIP_FLASHING, METROID_GRABBED, *AI_TABLES,
                   "sLocationTextPointers", "sAreaBannerLocationTextPal"])
    wanted.update(["sCommonSpritesGfx", "sCommonSpritesPal",
                   *(table for tables in DROPS.values() for table in tables), *PARTICLES])
    wanted.update(table for tables in COMMON_SPRITES.values() for table in tables)
    banner_source = (decomp / "src/data/sprites/message_banner.c").read_text(encoding="utf-8")
    banner_tables = [t for t in frame_tables(banner_source)
                     if t.startswith(("sMessageBannerOam", "sSaveYesNoCursorOam"))]
    wanted.update([*BANNER_SYMBOLS, *banner_tables])
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

    messages = {name: value for name, value in enum_ids(
        (decomp / "include/constants/text.h").read_text(encoding="utf-8"), "MessageId").items()
        if 0 <= value < enum_ids((decomp / "include/constants/text.h").read_text(encoding="utf-8"),
                                 "MessageId")["MESSAGE_COUNT"]}
    banner_vram, banner_palette = stage(rom, symbols, "sMessageBannerGfx", "sMessageBannerPal", 0)
    font = mzm_message_text.Font(
        rom[rom_offset(symbols["sCharactersGfx"][0], rom):][:symbols["sCharactersGfx"][1]],
        rom[rom_offset(symbols["sCharacterWidths"][0], rom):][:symbols["sCharacterWidths"][1]])
    pointers = struct.unpack_from("<I", rom, rom_offset(symbols["sMessageTextPointers"][0], rom) +
                                  4 * mzm_message_text.LANGUAGE_ENGLISH)[0]
    banners = []
    for name, value in sorted(messages.items(), key=lambda item: item[1]):
        text_pointer = struct.unpack_from("<I", rom, rom_offset(pointers, rom) + 4 * value)[0]
        text = mzm_message_text.read_text(rom, text_pointer, rom_offset)
        vram = mzm_message_text.Vram(banner_vram)
        calls = mzm_message_text.render_message(vram, font, text, SPRITE_VRAM)
        lines.append(f"message\t{value}\t{name[len('MESSAGE_'):]}\t{calls}")
        banners.append((name[len('MESSAGE_'):], vram.to_bytes()))
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
    # sHazeData[EFFECT_HAZE_COUNT][4]: haze value, damage effect, water
    # movement and blending kind of each room effect (HazeSetBackgroundEffect).
    lines.append("\t".join(["haze", *map(str, words("sHazeData", False, 1))]))
    from scripts import mzm_c_tables
    for source, (headers, names) in MULTI_SPRITE_DATA.items():
        for name in names:
            for index, (timer, parts) in enumerate(
                    mzm_c_tables.multi_sprite_data(source, name, list(headers), decomp)):
                lines.append("\t".join(["multi", name, str(index), str(timer),
                                         *(",".join(map(str, part)) for part in parts)]))
    folder = private_path(Path(root), METROID_METADATA, create=True)
    folder.mkdir(parents=True, exist_ok=True)
    write_atomic(folder / "sprites.tsv", "\n".join(lines) + "\n")
    library = LibraryWriter(root, ENEMIES_RUNTIME)
    _, common_palette = stage_common(rom, symbols)
    for sprite, row, gfx, palette_symbol, tables in plan:
        vram, palette = stage(rom, symbols, gfx, palette_symbol, row)
        variants = {"": palette}
        # Statues are never frozen or stunned and only flip in X. Only Chozo
        # statues use the native flashing palette-row variants.
        chozo_statue = sprite.startswith("PSPRITE_CHOZO_STATUE")
        statue = (chozo_statue or
                  sprite in ("PSPRITE_KRAID_STATUE", "PSPRITE_RIDLEY_STATUE"))
        if not statue:
            variants.update(shifted_palettes(palette, common_palette))
        elif chozo_statue:
            # sChozoStatueFlashingPaletteRows: the statue drawn with its
            # palette rows 1 to 3 (paletteRow added to each part's row).
            for flash in (1, 2, 3):
                variants[f"/row{flash}"] = stage(rom, symbols, gfx, palette_symbol, row + flash)[1]
        # SavePlatformSaving flashes the platform and its top with palette
        # row 2.
        for flash in ROW_VARIANTS.get(sprite, ()):
            variants[f"/row{flash}"] = stage(rom, symbols, gfx, palette_symbol, row + flash)[1]
        flips = ("none", "x") if statue else mzm_projectile_compose.FLIPS
        if sprite == "PSPRITE_GUNSHIP":
            address, size = symbols[GUNSHIP_FLASHING]
            flashing = [bgr555(value) for value in struct.unpack_from(
                f"<{size // 2}H", rom, rom_offset(address, rom))]
            variants = {}
            for entrance in (None, 0, 1, 2, 3, 4):
                for flames in (None, 0, 1, 2):
                    colours = list(palette)
                    suffix = ""
                    if entrance is not None:
                        colours[9 * 16 + 8:9 * 16 + 16] = flashing[0x38 + 16 * entrance:
                                                                   0x38 + 16 * entrance + 8]
                        suffix += f"e{entrance}"
                    if flames is not None:
                        colours[10 * 16:11 * 16] = flashing[16 * flames:16 * flames + 16]
                        suffix += f"f{flames}"
                    variants["/" + suffix if suffix else ""] = colours
            flips = ("none",)
        if sprite == "PSPRITE_METROID":
            address, size = symbols[METROID_GRABBED]
            grabbed = [bgr555(value) for value in struct.unpack_from(
                f"<{size // 2}H", rom, rom_offset(address, rom))]
            for index in range(size // 32):
                colours = list(palette)
                colours[12 * 16:12 * 16 + 8] = grabbed[16 * index:16 * index + 8]
                variants[f"/g{index}"] = colours
        for table in tables:
            entries = mzm_projectile_compose.frames(rom, symbols, table)
            for suffix, colours in variants.items():
                if suffix.startswith("/g") and table != "sMetroidOam_SamusGrabbed":
                    continue
                if (suffix.startswith("/row") and sprite in ROW_VARIANT_TABLES and
                        not table.startswith(ROW_VARIANT_TABLES[sprite])):
                    continue
                for flip in flips:
                    frames = []
                    try:
                        for pointer, duration in entries:
                            try:
                                pixels = mzm_projectile_compose.render(rom, vram, colours, pointer, flip)
                            except ValueError as exc:
                                raise ValueError(
                                    f"{sprite}/{table}{suffix}/{flip}: {exc}") from exc
                            bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                            frames.append((bmp, duration, left, top))
                    except ValueError:
                        # A Mecha Ridley damage tint shifting a part past the
                        # sprite's eight rows reads another sprite's OBJ row
                        # natively; that table keeps its untinted look.
                        if sprite in TINT_OVERFLOW_ALLOWED and suffix.startswith("/row"):
                            continue
                        raise
                    library.add(f"Sprite/{sprite[len('PSPRITE_'):]}/{table[1:]}/{flip}{suffix}",
                                frames)
            if sprite == "PSPRITE_MECHA_RIDLEY" and table in MECHA_RIDLEY_WEAPON_TABLES:
                address, size = symbols[MECHA_RIDLEY_WEAPONS]
                weapons = rom[rom_offset(address, rom):rom_offset(address, rom) + size]
                for index, first_tile in enumerate(MECHA_RIDLEY_WEAPON_STAGES):
                    staged = mecha_ridley_weapon_vram(vram, weapons, first_tile)
                    frames = []
                    for pointer, duration in entries:
                        pixels = mzm_projectile_compose.render(rom, staged, palette, pointer, "none")
                        bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                        frames.append((bmp, duration, left, top))
                    library.add(f"Sprite/MECHA_RIDLEY/{table[1:]}/none/w{index}", frames)
    # The message banner (PSPRITE_MESSAGE_BANNER) with each message's text
    # in its graphics (TextProcessMessageBanner over SpriteLoadGfx's tiles).
    for name, vram in banners:
        for table in banner_tables:
            frames = []
            for pointer, duration in mzm_projectile_compose.frames(rom, symbols, table):
                pixels = mzm_projectile_compose.render(rom, vram, banner_palette, pointer, "none")
                bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                frames.append((bmp, duration, left, top))
            library.add(f"Banner/{name}/{table[1:]}/none", frames)
    # The area banner's location texts.
    pointers = struct.unpack_from("<I", rom, rom_offset(symbols["sLocationTextPointers"][0], rom) +
                                  4 * mzm_message_text.LANGUAGE_ENGLISH)[0]
    banner_plan = next(item for item in plan if item[0] == "PSPRITE_AREA_BANNER")
    for lt in range(LOCATION_TEXTS):
        palette_symbol = banner_plan[3] if lt >= LT_SAVE_ROOM else "sAreaBannerLocationTextPal"
        vram, palette = stage(rom, symbols, banner_plan[2], palette_symbol, 0)
        text_pointer = struct.unpack_from("<I", rom, rom_offset(pointers, rom) + 4 * lt)[0]
        text = mzm_message_text.read_text(rom, text_pointer, rom_offset)
        buffer = mzm_message_text.Vram(b"\xff" * 0x800)
        mzm_message_text.draw_location(buffer, font, text)
        drawn = bytearray(vram)
        raw = buffer.to_bytes()
        drawn[SPRITE_VRAM:SPRITE_VRAM + 0x380] = raw[:0x380]
        drawn[SPRITE_VRAM + 0x400:SPRITE_VRAM + 0x780] = raw[0x400:0x780]
        for table in banner_plan[4]:
            frames = []
            for pointer, duration in mzm_projectile_compose.frames(rom, symbols, table):
                pixels = mzm_projectile_compose.render(rom, bytes(drawn), palette, pointer, "none")
                bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                frames.append((bmp, duration, left, top))
            library.add(f"Sprite/AREA_BANNER/{table[1:]}/none/lt{lt}", frames)
    vram, palette = stage_common(rom, symbols)
    for sprite, tables in DROPS.items():
        for table in tables:
            frames = []
            for pointer, duration in mzm_projectile_compose.frames(rom, symbols, table):
                pixels = mzm_projectile_compose.render(rom, vram, palette, pointer, "none")
                bmp, left, top = bmp_from_pixels(pixels or {(0, 0): (0, 0, 0)})
                frames.append((bmp, duration, left, top))
            library.add(f"Sprite/{sprite[len('PSPRITE_'):]}/{table[1:]}/none", frames)
    for sprite, sprite_tables in COMMON_SPRITES.items():
        for table in sprite_tables:
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
