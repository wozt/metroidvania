# SPDX-License-Identifier: GPL-3.0-only
"""0116: ROM-independent saved project graph and GTK source contracts."""
import tempfile
import unittest
from pathlib import Path
from scripts import project_room_entities as pe
from scripts import project_connection_graph as graph
from scripts import editor_backend
from scripts import editor_cli

ROOT = Path(__file__).resolve().parents[1]


class ConnectionGraph0116(unittest.TestCase):
    def test_reciprocal_missing_simple_interworld_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            a = pe._new('mzm','Brinstar',7,128,128)
            b = pe._new('mzm','Brinstar',8,128,128)
            d1 = pe.door_create(a,0,0,16,16,'A','normal','left')
            d2 = pe.door_create(b,0,0,16,16,'B','normal','right')
            d3 = pe.door_create(a,32,0,16,16,'C','normal','left')
            pe.transition_create(a,d1['id'],'mzm','Brinstar',8,d2['id'],0,0)
            pe.save(root,a);pe.save(root,b)
            rows = graph.connections(root,'mzm','Brinstar',7)
            self.assertEqual({x['door_id']:x['state'] for x in rows},
                             {d1['id']:'simple',d3['id']:'missing'})
            pe.transition_create(b,d2['id'],'mzm','Brinstar',7,d1['id'],0,0)
            pe.save(root,b)
            row = graph.connections(root,'mzm','Brinstar',7)[0]
            self.assertEqual(row['state'],'reciprocal')
            self.assertTrue(row['reciprocal'])
            pe.transition_create(a,d3['id'],'aria','3',5,0,0,0)
            pe.save(root,a)
            self.assertEqual(graph.connections(root,'mzm','Brinstar',7)[1]['state'],
                             'interworld')
            a['transitions'][1]['target_door_id']=42
            pe.save(root,a)
            self.assertEqual(graph.connections(root,'mzm','Brinstar',7)[1]['state'],
                             'invalid')

    def test_no_stage_leak_and_no_source_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            room=pe._new('mzm','Brinstar',7,128,128)
            door=pe.door_create(room,0,0,16,16,'A','normal','left')
            saved=pe.save(root,room)
            original=saved.read_bytes()
            rows=graph.connections(root,'mzm','Brinstar')
            self.assertEqual(len(rows),1)
            self.assertEqual(rows[0]['state'],'missing')
            self.assertEqual(saved.read_bytes(),original)

    def test_backend_and_cli_contract(self):
        self.assertIn('connection-list',editor_backend.COMMAND_FIELDS)
        self.assertIn('connection-list',editor_cli.TSV_COMMANDS)
        c=(ROOT/'editor/world_atlas.c').read_text()
        for phrase in ('PATCH_0116_CONNECTION_VISUALIZATION',
                       'gtk_overlay_set_child(GTK_OVERLAY(map_overlay), grid);',
                       'gtk_widget_set_can_target(w->connection_canvas, FALSE);',
                       'connection_draw_0116(', 'connections_refresh_0116(',
                       'All links in current map', 'Show connections'):
            self.assertIn(phrase,c)

if __name__=='__main__':
    unittest.main()
