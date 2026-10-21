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
            path = os.path.join(DATA, 'textures', name + '.dds')
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

    def test_build_copies_them(self):
        with open(os.path.join(DATA, 'CMakeLists.txt')) as stream:
            listed = set(re.findall(r'^\s+textures/(\S+)$', stream.read(), re.MULTILINE))
        for name in self.images:
            self.assertIn(name + '.dds', listed, '%s.dds is not in files/data/CMakeLists.txt' % name)


if __name__ == '__main__':
    unittest.main()
