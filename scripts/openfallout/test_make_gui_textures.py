#!/usr/bin/env python3
"""Checks make_gui_textures.py: the images are well-formed DDS files, the committed ones are what the script makes, every
texture that the layouts and skins of the interface name is there, and the build copies them."""
import os
import re
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
DATA = os.path.join(ROOT, 'files', 'data')

sys.path.insert(0, HERE)
import make_gui_textures  # noqa: E402


class GuiTexturesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.images = make_gui_textures.build_all()

    def test_dds_headers(self):
        for name, image in self.images.items():
            data = image.to_dds()
            self.assertEqual(data[:4], b'DDS ', name)
            size, flags, height, width, pitch = struct.unpack_from('<5I', data, 4)
            self.assertEqual(size, 124, name)
            self.assertEqual((width, height), (image.width, image.height), name)
            self.assertEqual(len(data), 128 + width * height * 4, name)
            self.assertEqual(struct.unpack_from('<I', data, 76)[0], 32, name)

    def test_committed_files_are_current(self):
        for name, image in self.images.items():
            path = os.path.join(DATA, make_gui_textures.relative_path(name))
            self.assertTrue(os.path.exists(path), '%s is not committed: run make_gui_textures.py' % name)
            with open(path, 'rb') as stream:
                self.assertEqual(stream.read(), image.to_dds(), '%s is out of date' % name)

    def test_layouts_find_their_textures(self):
        have = {name.lower() for name in os.listdir(os.path.join(DATA, 'textures'))}
        mygui = os.path.join(DATA, 'mygui')
        pattern = re.compile(r'textures?[\\/]+([A-Za-z0-9_\-.]+\.(?:dds|tga|png))', re.IGNORECASE)
        missing = set()
        for name in os.listdir(mygui):
            with open(os.path.join(mygui, name), errors='replace') as stream:
                for match in pattern.finditer(stream.read()):
                    if match.group(1).lower() not in have:
                        missing.add(match.group(1))
        self.assertEqual(sorted(missing), [])

    def test_build_copies_them_below_the_archives(self):
        # they go to resources/vfs-fallback, so that the image of a game that has one of the same name shadows them
        with open(os.path.join(DATA, 'CMakeLists.txt')) as stream:
            text = stream.read()
        match = re.search(r'set\(FALLBACK_TEXTURES\n(.*?)\n\)', text, re.DOTALL)
        self.assertIsNotNone(match, 'files/data/CMakeLists.txt has no FALLBACK_TEXTURES')
        listed = set(re.findall(r'^\s+((?:textures|icons)/\S+)$', match.group(1), re.MULTILINE))
        wanted = {make_gui_textures.relative_path(name) for name in self.images}
        for path in wanted:
            self.assertIn(path, listed, '%s is not in FALLBACK_TEXTURES' % path)
        self.assertEqual(sorted(listed), sorted(wanted))
        self.assertIn('resources/vfs-fallback/${f}', text)
        builtin = re.search(r'set\(BUILTIN_DATA_FILES\n(.*?)\n\)', text, re.DOTALL).group(1)
        for name in self.images:
            path = make_gui_textures.relative_path(name)
            self.assertNotIn(path, builtin, '%s would be installed over the archives' % path)

    def test_the_main_menu_has_what_it_asks_for(self):
        # apps/openfallout/mwgui/mainmenu.cpp names the picture and, for each button, three images
        self.assertIn('menu_morrowind', self.images)
        for button in ('return', 'newgame', 'savegame', 'loadgame', 'options', 'credits', 'exitgame'):
            for suffix in ('', '_over', '_pressed'):
                self.assertIn('menu_%s%s' % (button, suffix), self.images)
        # the buttons are 192 across and 64 down: the menu takes 64 as the height of the image of a button and cuts off 16
        for name, image in self.images.items():
            if name.startswith('menu_') and name.split('_')[1] in ('return', 'newgame', 'savegame', 'loadgame', 'options',
                                                                   'credits', 'exitgame'):
                self.assertEqual((image.width, image.height), (192, 64), name)

    def test_the_hud_and_the_screens_have_their_signs(self):
        # hud.cpp, quickkeysmenu.cpp, levelupdialog.cpp, windowmanagerimp.cpp and openmw_hud.layout name these
        for name in ('icons/k/stealth_sneak', 'icons/k/stealth_handtohand', 'icons/tx_goldicon', 'player_hit_01'):
            self.assertIn(name, self.images)
        with open(os.path.join(DATA, 'mygui', 'openmw_hud.layout'), errors='replace') as stream:
            self.assertIn('icons\\k\\stealth_sneak.dds', stream.read())

    def test_text_is_drawn_in_the_letters_that_are_there(self):
        for _, label in make_gui_textures.MENU_BUTTONS:
            for letter in label.replace(' ', ''):
                self.assertIn(letter, make_gui_textures.FONT, label)
        for rows in make_gui_textures.FONT.values():
            self.assertEqual(len(rows), 7)
            for row in rows:
                self.assertEqual(len(row), 5)


if __name__ == '__main__':
    unittest.main()
