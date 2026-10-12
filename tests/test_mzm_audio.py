# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the Zero Mission sound data exporter."""
from pathlib import Path
import unittest

from scripts import mzm_audio, mzm_c_tables, mzm_runtime_room

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


class LinkMapTests(unittest.TestCase):
    MAP = (" .rodata        0x0808cf70     0x2000 sound/song_a.o\n"
           " .rodata        0x08090000      0x100 sound/song_b.o\n"
           " .rodata        0x08100000      0x100 src/other.o\n"
           "                0x0808cf70                sSoundDataEntries\n")

    def test_sound_range_spans_the_sound_objects(self):
        self.assertEqual(mzm_audio.sound_range(self.MAP), (0x0808CF70, 0x08090100))

    def test_symbol(self):
        self.assertEqual(mzm_audio.symbol(self.MAP, "sSoundDataEntries"), 0x0808CF70)
        with self.assertRaises(ValueError):
            mzm_audio.symbol(self.MAP, "sMissing")

    def test_no_sound_section(self):
        with self.assertRaises(ValueError):
            mzm_audio.sound_range(" .rodata 0x08000000 0x10 src/a.o\n")


@unittest.skipUnless((DECOMP / "src/data/audio.c").is_file(),
                     "pinned MZM decompilation missing")
class EngineTableTests(unittest.TestCase):
    def tables(self):
        result = {}
        for line in mzm_audio.table_lines(DECOMP):
            kind, name, values = line.split("\t")
            self.assertEqual(kind, "table")
            result[name] = [int(v) for v in values.split(",")]
        return result

    def test_engine_tables(self):
        tables = self.tables()
        self.assertEqual(list(tables), list(mzm_audio.TABLES))
        self.assertEqual(len(tables["sClockTable"]), 49)
        self.assertEqual(tables["sClockTable"][24], 24)
        self.assertEqual(tables["sClockTable"][-1], 96)
        self.assertEqual(len(tables["sUnk_808cad0"]), 128)
        self.assertEqual(len(tables["sArray_808cce2"]), 512)
        # freqIndex 4: 13379 Hz, 224 samples a frame (SAMPLES_PER_SECOND in
        # single precision).
        self.assertEqual(tables["sNativeSampleRate"][4], 13379)
        self.assertEqual(tables["sPcmSamplesPerVBlankTable"][4], 224)

    def test_samples_per_second(self):
        self.assertEqual(mzm_c_tables.evaluate("SAMPLES_PER_SECOND(224)", {}), 13379)

    def test_music_aliases_and_default(self):
        names = mzm_c_tables.enum_values(DECOMP)
        lines = mzm_audio.music_lines(DECOMP)
        self.assertIn(f"alias\t{names['MUSIC_MAP_ROOM_2']}\t{names['MUSIC_MAP_ROOM']}", lines)
        self.assertEqual(len([line for line in lines if line.startswith("alias")]), 5)
        self.assertEqual(lines[-1], f"default\t{names['SOUND_A9']}")

    def test_room_music(self):
        self.assertEqual(mzm_runtime_room.room_music({"musicTrack": "MUSIC_BRINSTAR"}), 1)
        self.assertEqual(mzm_runtime_room.room_music({}), 0)


if __name__ == "__main__":
    unittest.main()
