# SPDX-License-Identifier: GPL-3.0-only
"""0114: verified target project door selection, without ROM assets."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import project_room_entities as entities
from scripts import editor_backend as backend

ROOT = Path(__file__).resolve().parents[1]


class TargetDoorPicker0114(unittest.TestCase):
    def test_saved_target_door_and_refused_nonexistent_id(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = entities._new('mzm', 'Brinstar', 7, 128, 64)
            door = entities.door_create(target, 0, 0, 16, 32, 'Return', 'normal', 'left')
            entities.save(root, target)
            options = {'target_world': 'zero_mission', 'target_area': 'Brinstar',
                       'target_room': 7}
            with patch.object(backend, '_filter_native_rooms',
                              return_value=[{'room': 7}]):
                doors = backend.execute('door-target-list', options, root=root)['doors']
                self.assertEqual([d['id'] for d in doors], [door['id']])
                self.assertEqual(doors[0]['label'], 'Return')
                scope = {**options, 'target_door_id': door['id']}
                self.assertEqual(backend._transition_target(scope, root)['target_door_id'], door['id'])
                with self.assertRaisesRegex(ValueError, 'not present'):
                    backend._transition_target({**scope, 'target_door_id': door['id'] + 10}, root)

    def test_missing_saved_room_has_no_door_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            options = {'target_world': 'aria', 'target_area': 3, 'target_room': 5}
            with patch.object(backend, '_filter_native_rooms',
                              return_value=[{'room': 5}]):
                data = backend.execute('door-target-list', options, root=Path(directory))
                self.assertEqual(data['doors'], [])
                with self.assertRaisesRegex(ValueError, 'not present'):
                    backend._transition_target({**options, 'target_door_id': 4}, Path(directory))

    def test_gtk_picker_and_cli_contract(self):
        c = (ROOT / 'editor/native_workspace.c').read_text()
        cli = (ROOT / 'scripts/editor_cli.py').read_text()
        for text in ('project_door_list_targets_0114(', 'project_door_target_chosen_0114(',
                     'List saved target doors', '--command=door-target-list',
                     'project_door_link_0102(', 'form->target_choice_ids'):
            self.assertIn(text, c)
        self.assertIn('"door-target-list"', cli)


if __name__ == '__main__':
    unittest.main()
