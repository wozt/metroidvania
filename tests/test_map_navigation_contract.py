# SPDX-License-Identifier: GPL-3.0-only
"""World map UI hook regression: no claims of gameplay map placement."""
import unittest
from pathlib import Path


class GlobalMapNavigation(unittest.TestCase):
    def test_shared_world_map_has_drag_and_right_click_actions(self):
        source = Path('editor/world_atlas.c').read_text(encoding='utf-8')
        for symbol in ('map_drag_begin', 'map_drag_update',
                       'map_context_pressed', 'map_context_enable',
                       'GDK_BUTTON_SECONDARY', 'gtk_popover_popup',
                       'Copy cell coordinates', 'mv-span-width',
                       'mv-span-height'):
            self.assertIn(symbol, source)

    def test_authoring_is_explicitly_project_draft_only(self):
        source = Path('editor/world_atlas.c').read_text(encoding='utf-8')
        self.assertIn('map_creator_build(center)', source)
        self.assertIn('mv-create-room-action', source)
        self.assertIn('placement of new drafts', source)
        self.assertIn('Reuse the', source)
        self.assertIn('existing authoring page', source)


if __name__ == '__main__':
    unittest.main()
