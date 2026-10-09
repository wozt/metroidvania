# SPDX-License-Identifier: GPL-3.0-only
"""0115: validate a saved forward link and prepare an undoable reverse draft."""
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import editor_backend as backend
from scripts import project_room_entities as pe

ROOT = Path(__file__).resolve().parents[1]

class ReciprocalLink0115(unittest.TestCase):
    def test_reverse_plan_is_read_only_and_target_can_be_staged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            a = pe._new('mzm', 'Brinstar', 7, 128, 128)
            b = pe._new('mzm', 'Brinstar', 8, 128, 128)
            left = pe.door_create(a, 16, 32, 16, 32, 'A', 'normal', 'right')
            right = pe.door_create(b, 48, 64, 16, 32, 'B', 'normal', 'left')
            pe.transition_create(a, left['id'], 'mzm', 'Brinstar', 8, right['id'], 48, 64)
            pa, pb = pe.save(root, a), pe.save(root, b)
            olda, oldb = pa.read_bytes(), pb.read_bytes()
            with patch.object(backend, '_filter_native_rooms',
                              return_value=[{'room': 7}, {'room': 8}]):
                plan = backend.execute('door-return-plan', {
                    'world': 'zero_mission', 'area': 'Brinstar', 'room': 7,
                    'source_door_id': left['id']}, root=root)
                self.assertEqual(plan['source_room'], 8)
                self.assertEqual(plan['source_door_id'], right['id'])
                self.assertEqual(plan['target_room'], 7)
                self.assertEqual(plan['target_door_id'], left['id'])
                self.assertEqual((plan['spawn_x'], plan['spawn_y']), (16, 32))
                self.assertEqual(pa.read_bytes(), olda)
                self.assertEqual(pb.read_bytes(), oldb)
                # The real destination editor uses the same backend with a
                # unique stage token; this NEVER writes the saved target file.
                with patch.dict(os.environ, {'MV_EDITOR_ROOM_STAGE':
                    '00000000-0000-4000-8000-000000000115'}):
                    result = backend.execute('door-link', {
                        'world': plan['source_world'], 'area': plan['source_area'],
                        'room': plan['source_room'], 'width': 128, 'height': 128,
                        'source_door_id': plan['source_door_id'],
                        'target_world': plan['target_world'],
                        'target_area': plan['target_area'],
                        'target_room': plan['target_room'],
                        'target_door_id': plan['target_door_id'],
                        'spawn_x': plan['spawn_x'], 'spawn_y': plan['spawn_y'],
                    }, root=root)
                    self.assertEqual(result['transition']['target_door_id'], left['id'])
                    self.assertEqual(pb.read_bytes(), oldb)
                    self.assertEqual(len(pe.load(root,'mzm','Brinstar',8,128,128)['transitions']),1)
            self.assertEqual(pa.read_bytes(), olda)
            self.assertEqual(pb.read_bytes(), oldb)

    def test_return_plan_rejects_uncommitted_forward_and_busy_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            a=pe._new('mzm','Brinstar',7,128,128)
            b=pe._new('mzm','Brinstar',8,128,128)
            da=pe.door_create(a,16,16,16,32,'A','normal','left')
            db=pe.door_create(b,32,32,16,32,'B','normal','right')
            pa=pe.save(root,a)
            pb=pe.save(root,b)
            scope={'world':'zero_mission','area':'Brinstar','room':7,
                   'source_door_id':da['id']}
            with patch.object(backend,'_filter_native_rooms',
                              return_value=[{'room':7},{'room':8}]):
                with self.assertRaisesRegex(ValueError,'forward destination'):
                    backend.execute('door-return-plan',scope,root=root)
                pe.transition_create(a,da['id'],'mzm','Brinstar',8,db['id'],32,32)
                pe.save(root,a)
                pe.transition_create(b,db['id'],'mzm','Brinstar',7,da['id'],16,16)
                pe.save(root,b)
                with self.assertRaisesRegex(ValueError,'already has a transition'):
                    backend.execute('door-return-plan',scope,root=root)

    def test_gtk_handoff_is_explicit_and_undoable(self):
        s=(ROOT/'editor/native_workspace.c').read_text(encoding='utf-8')
        for term in ('PATCH_0115_RECIPROCAL_GTK', 'Prepare return link',
                     '--command=door-return-plan', 'pending_reverse_0115',
                     'project_reverse_apply_0115(doc);',
                     'project_command(doc, "door-link", opts, NULL)',
                     'First save the forward link with the source room diskette.'):
            self.assertIn(term,s)

if __name__ == '__main__':
    unittest.main()
