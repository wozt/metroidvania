# SPDX-License-Identifier: GPL-3.0-only
"""Native event lock slots must not be confused with door table indices."""
import unittest
from pathlib import Path
from scripts.mzm_hatch_events import parse_hatch_lock_events, preview_lock_class
from scripts.mzm_hatch_preview import HATCH_BASES, hatch_metatile_index


class MzmHatchEvents0100(unittest.TestCase):
    @staticmethod
    def source() -> str:
        def item(room, event, kind, slots):
            fields = [f'.room = {room}', f'.event = {event}', f'.type = {kind}']
            fields.extend(f'.hatchesToLock_{slot} = {"TRUE" if slot in slots else "FALSE"}'
                          for slot in range(16))
            return '{' + ',\n'.join(fields) + '}'
        first = item(9, 'EVENT_VIEWED_STATUE_ROOM', 'HATCH_LOCK_EVENT_TYPE_BEFORE', {1})
        second = item(9, 'EVENT_SECOND', 'HATCH_LOCK_EVENT_TYPE_AFTER_UNLOCKABLE', {0})
        return f'const struct HatchLockEvent sHatchLockEventsBrinstar[2] = {{\n{first},\n{second}\n}};'

    def test_exact_slots_and_original_event_conditions(self):
        actual = parse_hatch_lock_events(self.source(), 'Brinstar', 9)
        self.assertEqual(actual[1], [('EVENT_VIEWED_STATUE_ROOM', 'BEFORE', 'PERMANENT')])
        self.assertEqual(actual[0], [('EVENT_SECOND', 'AFTER', 'UNLOCKABLE')])
        self.assertEqual(preview_lock_class(actual[1], False), 'PERMANENT')
        self.assertIsNone(preview_lock_class(actual[1], True))
        self.assertIsNone(preview_lock_class(actual[0], False))
        self.assertEqual(preview_lock_class(actual[0], True), 'UNLOCKABLE')
        self.assertEqual(parse_hatch_lock_events(self.source(), 'Kraid', 9), {})

    def test_multiple_rules_preserve_stricter_permanent_lock(self):
        rules = [('EVENT_A', 'AFTER', 'UNLOCKABLE'),
                 ('EVENT_B', 'AFTER', 'PERMANENT')]
        self.assertEqual(preview_lock_class(rules, True), 'PERMANENT')

    def test_exact_permanent_lock_metatiles(self):
        self.assertEqual(HATCH_BASES['locked_navigation'], 0x2A)
        self.assertEqual(hatch_metatile_index('locked_navigation', False), 0x2A)
        self.assertEqual(hatch_metatile_index('locked_navigation', True), 0x2B)

    def test_incomplete_table_is_not_silently_accepted(self):
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            parse_hatch_lock_events(self.source().replace('[2]', '[3]'), 'Brinstar', 9)

    def test_widget_offers_explicitly_hypothetical_modes(self):
        code = (Path(__file__).resolve().parents[1] / 'editor/native_workspace.c').read_text()
        self.assertIn('All native events: OFF (preview)', code)
        self.assertIn('All native events: ON (preview)', code)
        self.assertIn('hatch_lock_rules=', code)
        self.assertIn('gtk_drop_down_get_selected(GTK_DROP_DOWN(object))', code)
        self.assertIn('hatch_preview_toggled', code)


if __name__ == '__main__':
    unittest.main()
