#!/usr/bin/env python3
"""A short summary of NIF files of Gamebryo 20.x (Fallout 3, New Vegas): version, the block types with their counts and
sizes, the vertex counts of the geometry, and the strings of the header (node names, texture and material files). It reads
the header only, plus the first fields of each geometry block, so it runs over a folder of level-of-detail files in
seconds. Nothing of it is game text: the strings are names and paths of files.

    nifsummary.py FILE.nif [FILE.nif ...]
    nifsummary.py --totals FOLDER      (a line per file and the block types of all of them together)
"""
import argparse
import collections
import os
import struct
import sys

GEOMETRY_DATA = ('NiTriShapeData', 'NiTriStripsData', 'BSPackedAdditionalGeometryData', 'NiLinesData')


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def take(self, fmt):
        size = struct.calcsize(fmt)
        if self.pos + size > len(self.data):
            raise ValueError('the file ends inside its header')
        values = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += size
        return values[0] if len(values) == 1 else values

    def text(self, length):
        if self.pos + length > len(self.data):
            raise ValueError('the file ends inside its header')
        raw = self.data[self.pos:self.pos + length]
        self.pos += length
        return raw.decode('latin-1')


def parse(data):
    """Returns a dictionary about the file; raises ValueError when it is not a NIF of 20.x"""
    end = data.find(b'\n', 0, 120)
    if end < 0 or not data.startswith((b'Gamebryo File Format', b'NetImmerse File Format')):
        raise ValueError('not a NIF file')
    reader = Reader(data)
    reader.pos = end + 1
    info = {'header': data[:end].decode('latin-1')}
    version = reader.take('<I')
    info['version'] = '.'.join(str((version >> shift) & 0xFF) for shift in (24, 16, 8, 0))
    if version < 0x14000005:
        raise ValueError('version %s is before 20.0.0.5' % info['version'])
    info['little endian'] = bool(reader.take('<B'))
    info['user version'] = reader.take('<I')
    blocks = reader.take('<I')
    info['bs version'] = reader.take('<I') if info['user version'] >= 3 or version >= 0x14020007 else 0
    for _ in range(3):
        # author, process script and export script: a length byte and the characters
        reader.text(reader.take('<B'))
    types = [reader.text(reader.take('<I')) for _ in range(reader.take('<H'))]
    indices = [reader.take('<H') & 0x7FFF for _ in range(blocks)]
    sizes = [reader.take('<I') for _ in range(blocks)]
    count = reader.take('<I')
    reader.take('<I')
    info['strings'] = [reader.text(reader.take('<I')) for _ in range(count)]
    for _ in range(reader.take('<I')):
        reader.take('<I')
    info['blocks'] = [(types[index], size) for index, size in zip(indices, sizes)]
    # the first fields of a geometry block: the group id and the number of vertices
    offset = reader.pos
    info['vertices'] = []
    for name, size in info['blocks']:
        if name in GEOMETRY_DATA and size >= 6 and offset + 6 <= len(data):
            info['vertices'].append(struct.unpack_from('<H', data, offset + 4)[0])
        offset += size
    info['bytes'] = len(data)
    return info


def describe(path, info, limit=40):
    counts = collections.Counter(name for name, _ in info['blocks'])
    sizes = collections.Counter()
    for name, size in info['blocks']:
        sizes[name] += size
    print('%s: %s, user %d, bs %d, %d bytes, %d blocks' % (
        path, info['version'], info['user version'], info['bs version'], info['bytes'], len(info['blocks'])))
    print('  blocks: ' + ', '.join('%s x%d (%d bytes)' % (name, count, sizes[name])
                                   for name, count in counts.most_common()))
    if info['vertices']:
        print('  geometry: %d shapes, vertices total %d, largest %d' % (
            len(info['vertices']), sum(info['vertices']), max(info['vertices'])))
    strings = info['strings']
    print('  strings (%d): %s%s' % (len(strings), ', '.join(strings[:limit]), ', ...' if len(strings) > limit else ''))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('paths', nargs='+')
    parser.add_argument('--totals', action='store_true', help='the paths are folders: one line per file and the totals')
    args = parser.parse_args()
    status = 0
    if args.totals:
        total = collections.Counter()
        files = 0
        vertices = 0
        for folder in args.paths:
            for root, _, names in os.walk(folder):
                for name in sorted(names):
                    if not name.lower().endswith('.nif'):
                        continue
                    path = os.path.join(root, name)
                    try:
                        with open(path, 'rb') as stream:
                            info = parse(stream.read())
                    except (OSError, ValueError, struct.error) as error:
                        print('%s: %s' % (path, error))
                        status = 1
                        continue
                    files += 1
                    vertices += sum(info['vertices'])
                    total.update(name for name, _ in info['blocks'])
                    print('%s: %d bytes, %d blocks, %d shapes, %d vertices' % (
                        path, info['bytes'], len(info['blocks']), len(info['vertices']), sum(info['vertices'])))
        print('%d files, %d vertices; ' % (files, vertices)
              + ', '.join('%s x%d' % item for item in total.most_common()))
        return status
    for path in args.paths:
        try:
            with open(path, 'rb') as stream:
                describe(path, parse(stream.read()))
        except (OSError, ValueError, struct.error) as error:
            print('%s: %s' % (path, error))
            status = 1
    return status


if __name__ == '__main__':
    sys.exit(main())
