# SPDX-License-Identifier: GPL-3.0-only
import unittest
import tomllib
from pathlib import Path
from scripts.world_overview import build_mzm, build_aria, output_rows
from scripts.validate_story_assets import validate_timeline, validate_scene


class WorldOverviewContract(unittest.TestCase):
    def test_aria_preserves_actual_cells_and_flags(self):
        cat={'format':'MV_AOS_WORLD_2','map_cells':[{'engine_area':0,'room':10,'map_x':4,'map_y':8,'save':False,'warp':True},{'engine_area':0,'room':10,'map_x':5,'map_y':8,'save':True,'warp':False}]}
        expected=[(0,10,4,8,0,1),(0,10,5,8,1,0)]
        self.assertEqual(build_aria(cat),expected)
        self.assertIn(b'0|10|5|8|1|0',output_rows('aria',expected))

    def test_mzm_does_not_invent_footprints(self):
        rows=build_mzm('# comment\nR|Brinstar|33|27|7|27|MUSIC_SAVE|1\n')
        self.assertEqual(rows,[(0,33,27,7,1,0)])
        with self.assertRaises(ValueError):build_mzm('R|Brinstar|33|999|7|27|MUSIC_SAVE|1')

    def test_story_rejects_dependency_cycle(self):
        data={'schema':'metroid-vania-timeline','version':1,'events':[{'id':'a','title':'a','track':'shared','world':'interzone','min_order':0,'max_order':1,'sets':['x'],'requires':['y']},{'id':'b','title':'b','track':'metroid','world':'zero_mission','min_order':0,'max_order':1,'sets':['y'],'requires':['x']}]}
        with self.assertRaisesRegex(ValueError,'cycle'):validate_timeline(data)

    def test_same_cutscene_schema_works_for_both_worlds(self):
        c={'schema':'metroid-vania-cutscene','version':1,'id':'common.hello','actors':[{'id':'samus'}],'steps':[{'type':'dialogue','actor':'samus','text':'Hi'}]}
        for world in ('zero_mission','aria','interzone'):
            c['world']=world
            self.assertEqual(validate_scene(c),1)

    def test_initial_authored_scenes_share_the_same_schema(self):
        base=Path(__file__).resolve().parents[1]/'data'/'cutscenes'
        for name in ('interzone_first_meeting','aria_creaking_skull_portal','mzm_deorem_soul_portal'):
            data=tomllib.loads((base/f'{name}.toml').read_text(encoding='utf-8'))
            self.assertGreater(validate_scene(data),0)

    def test_invalid_actions_rejected(self):
        c={'schema':'metroid-vania-cutscene','version':1,'id':'common.hello','world':'aria','actors':[{'id':'samus'}],'steps':[{'type':'teleport_anywhere'}]}
        with self.assertRaisesRegex(ValueError,'unknown action'):validate_scene(c)
