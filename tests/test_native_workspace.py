import unittest
from scripts.mzm_native_workspace import serialize,parse
class WorkspaceTest(unittest.TestCase):
    def test_round_trip_and_failures(self):
        room={'id':'mzm:brinstar:033','fields':{'tileset':'27'}}
        layers={'Bg1':(2,2,(0,1,2,65535)),'Bg2':(1,1,(7,))}
        blob=serialize(room,1024,layers,'rooms/metroid/tilesets/27_atlas.bmp')
        result=parse(blob)
        self.assertEqual(result['layers']['BG1'],(2,2,[0,1,2,65535]))
        for invalid in (blob+b'extra\n',blob.replace(b'0001',b'ZZZZ'),blob.replace(b'MVNATIVE 1',b'MVNATIVE 0')):
            with self.assertRaises(ValueError):parse(invalid)
    def test_invalid_source_dimensions(self):
        room={'id':'mzm:brinstar:033','fields':{'tileset':'27'}}
        with self.assertRaises(ValueError):
            serialize(room,4,{'Bg1':(1,1,[0]),'Bg2':(2,2,[1])},'rooms/metroid/tilesets/27_atlas.bmp')
if __name__=='__main__':unittest.main()
