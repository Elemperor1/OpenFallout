#!/usr/bin/env python3
"""List the script commands that a Fallout 3 or New Vegas executable knows: their codes, names and parameters.

The compiled scripts of the games (SCDA) call a command by a number, and the bytes after it hold its arguments, whose
layout depends on the parameters the command takes. The games keep that in a table in the executable, an array of
structures that start with the name of the command (the layout xNVSE and FOSE document for these tables):

    0x00 const char *longName     0x10 u16 needsParent       0x18 execute function
    0x04 const char *shortName    0x12 u16 numParams         0x1C parse function
    0x08 u32 opcode               0x14 ParamInfo *params     0x20 eval function
    0x0C const char *helpText                                0x24 u32 flags

and each ParamInfo is 12 bytes: const char *typeStr, u32 typeID, u32 isOptional.

This finds the tables without knowing where they are: it looks for runs of such structures in the data of a 32 bit
Windows executable (a structure is taken to be one when its name pointers lead to short printable strings, its function
pointer leads into the image, and its parameter table is well formed). The executable is only read. It prints how many
tables and entries it found, and writes one CSV line per entry:

    table,opcode,name,short name,needs parent,parameters,flags

where the parameters are `typeID` or `typeID?` for an optional one, separated by spaces. The help text is left out on
purpose: it is text of the game, and nothing here needs it.

    scripts/openfallout/dump_command_table.py FalloutNV.exe --csv command_table_fnv.csv

Needs Python 3 only. An executable that is packed (the Steam copies are wrapped in a protection layer) has no readable
table: use the one from the GOG installer.
"""
import argparse
import csv
import struct
import sys


class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise SystemExit("not a Windows executable (no MZ header)")
        (pe_offset,) = struct.unpack_from("<I", data, 0x3C)
        if data[pe_offset:pe_offset + 4] != b"PE\0\0":
            raise SystemExit("not a Windows executable (no PE header)")
        coff = pe_offset + 4
        machine, sections, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data, coff)
        if machine != 0x14C:
            raise SystemExit("not a 32 bit executable (machine 0x%X)" % machine)
        optional = coff + 20
        (magic,) = struct.unpack_from("<H", data, optional)
        if magic != 0x10B:
            raise SystemExit("not a PE32 image")
        (self.image_base,) = struct.unpack_from("<I", data, optional + 28)
        (self.image_size,) = struct.unpack_from("<I", data, optional + 56)
        table = optional + optional_size
        self.sections = []
        for i in range(sections):
            name, virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
                "<8sIIII", data, table + i * 40
            )
            self.sections.append(
                (name.rstrip(b"\0").decode("ascii", "replace"), self.image_base + virtual_address,
                 max(virtual_size, raw_size) if raw_size else virtual_size, raw_pointer, raw_size)
            )

    def to_offset(self, va):
        """File offset of a virtual address, or None when no section has the bytes of it in the file."""
        for _, start, size, raw_pointer, raw_size in self.sections:
            if start <= va < start + size:
                delta = va - start
                if delta < raw_size and raw_pointer + delta < len(self.data):
                    return raw_pointer + delta
                return None
        return None

    def in_image(self, va):
        return self.image_base <= va < self.image_base + self.image_size

    def string(self, va, max_length=96):
        """The printable ASCII string at a virtual address, or None."""
        offset = self.to_offset(va)
        if offset is None:
            return None
        end = self.data.find(b"\0", offset, offset + max_length + 1)
        if end < 0:
            return None
        raw = self.data[offset:end]
        if any(b < 0x20 or b > 0x7E for b in raw):
            return None
        return raw.decode("ascii")


class Entry:
    def __init__(self, offset, name, short_name, opcode, needs_parent, params, flags):
        self.offset = offset
        self.name = name
        self.short_name = short_name
        self.opcode = opcode
        self.needs_parent = needs_parent
        self.params = params  # list of (type id, optional)
        self.flags = flags


def read_entry(pe, offset, stride):
    data = pe.data
    if offset + stride > len(data):
        return None
    name_ptr, short_ptr, opcode, help_ptr, needs_parent, num_params, params_ptr, execute = struct.unpack_from(
        "<IIIIHHII", data, offset
    )
    name = pe.string(name_ptr)
    if name is None:
        return None
    short_name = ""
    if short_ptr:
        short_name = pe.string(short_ptr)
        if short_name is None:
            return None
    if opcode > 0xFFFF or needs_parent > 1 or num_params > 40:
        return None
    if help_ptr and not pe.in_image(help_ptr):
        return None
    if not pe.in_image(execute):
        return None
    flags = struct.unpack_from("<I", data, offset + 0x24)[0] if stride >= 0x28 else 0
    params = []
    if num_params:
        table = pe.to_offset(params_ptr)
        if table is None or table + 12 * num_params > len(data):
            return None
        for i in range(num_params):
            type_ptr, type_id, optional = struct.unpack_from("<III", data, table + 12 * i)
            if (type_ptr and pe.string(type_ptr) is None) or type_id > 0x100 or optional > 1:
                return None
            params.append((type_id, optional))
    return Entry(offset, name, short_name, opcode, needs_parent, params, flags)


def find_tables(pe, stride, minimum):
    """Runs of entries one stride apart. Returns a list of lists of Entry."""
    found = {}
    for _, start, size, raw_pointer, raw_size in pe.sections:
        # The tables are data: skip sections that hold only code
        for offset in range(raw_pointer, raw_pointer + raw_size - stride + 1, 4):
            if offset in found:
                continue
            # a cheap test before the full one: the first word must point into the image
            (first,) = struct.unpack_from("<I", pe.data, offset)
            if not pe.in_image(first):
                continue
            entry = read_entry(pe, offset, stride)
            if entry is not None:
                found[offset] = entry
    tables = []
    seen = set()
    for offset in sorted(found):
        if offset in seen or (offset - stride) in found:
            continue
        run = []
        at = offset
        while at in found:
            run.append(found[at])
            seen.add(at)
            at += stride
        if len(run) >= minimum:
            tables.append(run)
    return tables


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("executable")
    parser.add_argument("--csv", help="write the entries to this file")
    parser.add_argument("--minimum", type=int, default=8, help="shortest run that counts as a table (default 8)")
    parser.add_argument("--stride", type=lambda v: int(v, 0), help="size of an entry (default: try 0x28, 0x24, 0x20)")
    args = parser.parse_args(argv)

    with open(args.executable, "rb") as stream:
        pe = PE(stream.read())
    print("image base 0x%X, %d sections" % (pe.image_base, len(pe.sections)))

    tables = []
    strides = [args.stride] if args.stride else [0x28, 0x24, 0x20]
    for stride in strides:
        tables = find_tables(pe, stride, args.minimum)
        if tables:
            print("entries are 0x%X bytes apart" % stride)
            break
    if not tables:
        print("no table found: the executable may be packed")
        return 1

    rows = []
    for number, run in enumerate(tables):
        opcodes = [e.opcode for e in run]
        increasing = all(b > a for a, b in zip(opcodes, opcodes[1:]))
        consecutive = all(b == a + 1 for a, b in zip(opcodes, opcodes[1:]))
        print(
            "table %d: %d entries at file offset 0x%X, codes 0x%X to 0x%X%s"
            % (number, len(run), run[0].offset, min(opcodes), max(opcodes),
               ", consecutive" if consecutive else (", increasing" if increasing else ""))
        )
        for e in run:
            rows.append((number, e))

    kinds = {}
    for _, e in rows:
        for type_id, _ in e.params:
            kinds[type_id] = kinds.get(type_id, 0) + 1
    print("parameter types used: " + " ".join("%d:%d" % (k, kinds[k]) for k in sorted(kinds)))

    if args.csv:
        with open(args.csv, "w", newline="") as stream:
            writer = csv.writer(stream)
            for number, e in rows:
                params = " ".join(("%d?" if optional else "%d") % type_id for type_id, optional in e.params)
                writer.writerow([number, "0x%04X" % e.opcode, e.name, e.short_name, e.needs_parent, params,
                                 "0x%X" % e.flags])
        print("wrote %d entries to %s" % (len(rows), args.csv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
