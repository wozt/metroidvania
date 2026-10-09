# SPDX-License-Identifier: GPL-3.0-only
"""0104: original Aria/MZM doors can acquire private editable overrides."""
import tempfile
import unittest
from pathlib import Path

from scripts import project_room_entities as entities
from scripts.editor_backend import execute


class NativeDoorAdoption0104(unittest.TestCase):
    def test_roundtrip_source_bound_door(self):
        document = entities._new('mzm', 'Brinstar', 10, 640, 480)
        source = {'index': 27, 'variant': 'native', 'native_type': '27'}
        door = entities.door_adopt(document, source, 32, 80, 16, 64)
        self.assertEqual(door['native_source'], source)
        self.assertEqual(entities.door_adopt(document, source, 32, 80, 16, 64)['id'], door['id'])
        self.assertEqual(len(document['doors']), 1)
        entities.door_update(document, door['id'], x=48, y=96,
            width=16, height=64, label='My override', door_type='portal', facing='right')
        self.assertEqual(document['doors'][0]['native_source'], source)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entities.save(root, document)
            restored = entities.load(root, 'mzm', 'Brinstar', 10, 640, 480)
            self.assertEqual(restored['doors'][0]['native_source'], source)
            entities.door_delete(restored, door['id'])
            self.assertEqual(restored['doors'], [])

    def test_aria_native_source_and_legacy_custom_door(self):
        document = entities._new('aria', '3', 7, 1024, 512)
        entities.door_create(document, 0, 0, 16, 16, 'Custom', 'normal', 'left')
        self.assertNotIn('native_source', document['doors'][0])
        source = {'index': 2, 'variant': 'screen-anchor', 'native_type': '2'}
        adopted = entities.door_adopt(document, source, 1008, 256, 16, 16)
        self.assertEqual(adopted['facing'], 'right')
        self.assertEqual(len(document['doors']), 2)
        document['doors'][0]['native_source'] = source
        with self.assertRaisesRegex(ValueError, 'multiple project overrides'):
            entities.validate(document, 'aria', '3', 7, 1024, 512)

    def test_backend_requires_original_private_annotation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            opts = dict(world='zero_mission', area='Brinstar', room=10,
                        width=640, height=480, native_index=26,
                        native_variant='native', native_type='26')
            path = root/'assets/extracted/rooms/metroid/annotations/brinstar_010.tsv'
            path.parent.mkdir(parents=True)
            with self.assertRaises(ValueError):
                execute('door-adopt', opts, root=root)
            path.write_text('# private annotation\n'
                'DOOR|26|32|80|16|64|native|26|Door 26|native original\n')
            result = execute('door-adopt', opts, root=root)
            self.assertTrue(result['persisted'])
            self.assertEqual(result['door']['native_source']['index'], 26)
            repeat = execute('door-adopt', opts, root=root)
            self.assertEqual(repeat['door']['id'], result['door']['id'])
            self.assertEqual(len(entities.load(root,'mzm','Brinstar',10,640,480)['doors']),1)
            opts['native_index'] = 28
            with self.assertRaises(ValueError):
                execute('door-adopt', opts, root=root)


if __name__ == '__main__':
    unittest.main()
