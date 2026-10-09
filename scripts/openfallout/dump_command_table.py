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

    table,opcode,name,short name,needs parent,parameters,flags,parse

where the parameters are `typeID` or `typeID?` for an optional one, separated by spaces, and parse is the address of the
function that reads the arguments (most commands share one; the few that do not, read their arguments in another way).
The summary lists how many commands use each parse function and names the commands of the rare ones. The help text is
left out on purpose: it is text of the game, and nothing here needs it.

    scripts/openfallout/dump_command_table.py FalloutNV.exe --csv command_table_fnv.csv

A command that scripts call and no table lists (the census shows them) can be looked for with --probe 0x1177,0x116B,
which prints every place where the data holds that code where the code of a command would be, the words of the
structure there and why it is or is not taken for a command.

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
    def __init__(self, offset, name, short_name, opcode, needs_parent, params, flags, parse):
        self.offset = offset
        self.name = name
        self.short_name = short_name
        self.opcode = opcode
        self.needs_parent = needs_parent
        self.params = params  # list of (type id, optional)
        self.flags = flags
        self.parse = parse


def check_entry(pe, offset, stride):
    """The Entry at an offset and None, or None and the reason the structure there is not a command."""
    data = pe.data
    if offset + stride > len(data):
        return None, "runs past the end of the file"
    name_ptr, short_ptr, opcode, help_ptr, needs_parent, num_params, params_ptr, execute = struct.unpack_from(
        "<IIIIHHII", data, offset
    )
    name = pe.string(name_ptr)
    if name is None:
        return None, "the name pointer 0x%X does not lead to a short printable string" % name_ptr
    short_name = ""
    if short_ptr:
        short_name = pe.string(short_ptr)
        if short_name is None:
            return None, "the short name pointer 0x%X does not lead to a short printable string" % short_ptr
    if opcode > 0xFFFF or needs_parent > 1 or num_params > 40:
        return None, "the code, the needs-parent flag (%d) or the number of parameters (%d) is out of range" % (
            needs_parent, num_params)
    if help_ptr and not pe.in_image(help_ptr):
        return None, "the help text pointer 0x%X is outside the image" % help_ptr
    if not pe.in_image(execute):
        return None, "the execute function pointer 0x%X is outside the image" % execute
    parse = struct.unpack_from("<I", data, offset + 0x1C)[0] if stride >= 0x20 else 0
    if parse and not pe.in_image(parse):
        return None, "the parse function pointer 0x%X is outside the image" % parse
    flags = struct.unpack_from("<I", data, offset + 0x24)[0] if stride >= 0x28 else 0
    params = []
    if num_params:
        table = pe.to_offset(params_ptr)
        if table is None or table + 12 * num_params > len(data):
            return None, "the parameter table 0x%X is not in the file" % params_ptr
        for i in range(num_params):
            type_ptr, type_id, optional = struct.unpack_from("<III", data, table + 12 * i)
            if (type_ptr and pe.string(type_ptr) is None) or type_id > 0x100 or optional > 1:
                return None, "parameter %d is malformed (type %d, optional %d)" % (i, type_id, optional)
            params.append((type_id, optional))
    return Entry(offset, name, short_name, opcode, needs_parent, params, flags, parse), None


def read_entry(pe, offset, stride):
    return check_entry(pe, offset, stride)[0]


def probe(pe, opcodes, stride):
    """Says where the data of the executable holds each of these codes at the place of the code of a command
    (the third word of a structure), and why that structure is or is not taken for a command. For the codes that a
    script calls but no table lists. Prints names, never help text."""
    wanted = set(opcodes)
    found = {code: 0 for code in wanted}
    for _, _, _, raw_pointer, raw_size in pe.sections:
        end = min(raw_pointer + raw_size, len(pe.data))
        for offset in range(raw_pointer + 8, end - 3, 4):
            (value,) = struct.unpack_from("<I", pe.data, offset)
            if value not in wanted:
                continue
            start = offset - 8
            entry, reason = check_entry(pe, start, stride)
            found[value] += 1
            words = struct.unpack_from("<10I", pe.data, start) if start + 40 <= len(pe.data) else ()
            print("code 0x%04X at file offset 0x%X: %s" % (value, start, "a command" if entry else "not a command"))
            if words:
                print("  words: " + " ".join("%08X" % w for w in words))
            if entry:
                print("  %s (%s), %d parameters, parse 0x%X" % (entry.name, entry.short_name, len(entry.params), entry.parse))
            else:
                print("  rejected: " + reason)
            for name, delta in (("before", -stride), ("after", stride)):
                around = start + delta
                if 0 <= around and around + 12 <= len(pe.data):
                    neighbour = struct.unpack_from("<I", pe.data, around + 8)[0]
                    print("  %s: code word 0x%X" % (name, neighbour))
    for code in sorted(wanted):
        if not found[code]:
            print("code 0x%04X: no structure with this code" % code)


def find_tables(pe, stride, minimum):
    """Runs of entries one stride apart. Returns a list of lists of Entry."""
    found = {}
    for _, _, _, raw_pointer, raw_size in pe.sections:
        # A section can say it holds more bytes than the file has
        end = min(raw_pointer + raw_size, len(pe.data))
        for offset in range(raw_pointer, end - stride + 1, 4):
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
    attach_strays(tables, found, stride)
    return tables


# How far from a table (in entries) and from its codes an entry may lie to be taken as part of it
STRAY_DISTANCE = 64
STRAY_CODES = 64


def attach_strays(tables, found, stride):
    """Adds to the tables the entries that stand between or beside their runs on the same grid and have codes that
    fit. The executables have entries around which the neighbours are not taken for commands (the dummies that hold the
    places of removed commands), so the run is cut short there and a short run is not told from chance. A stray is not
    taken for a part of a table when it is far away or its code is, so what looks like a command by chance elsewhere
    stays out."""
    used = {e.offset for table in tables for e in table}
    for offset in sorted(found):
        if offset in used:
            continue
        entry = found[offset]
        for table in tables:
            # The strays are added in the order of their offsets, so the ends of the table are not the first and the
            # last of its list: one before the run is added after it
            first = min(e.offset for e in table)
            last = max(e.offset for e in table)
            if (offset - first) % stride:
                continue
            if not first - STRAY_DISTANCE * stride <= offset <= last + STRAY_DISTANCE * stride:
                continue
            codes = [e.opcode for e in table]
            if not min(codes) - STRAY_CODES <= entry.opcode <= max(codes) + STRAY_CODES:
                continue
            table.append(entry)
            used.add(offset)
            break
    for table in tables:
        table.sort(key=lambda e: e.offset)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("executable")
    parser.add_argument("--csv", help="write the entries to this file")
    parser.add_argument("--minimum", type=int, default=8, help="shortest run that counts as a table (default 8)")
    parser.add_argument("--stride", type=lambda v: int(v, 0), help="size of an entry (default: try 0x28, 0x24, 0x20)")
    parser.add_argument(
        "--probe", type=lambda v: [int(c, 0) for c in v.split(",")],
        help="codes (comma separated) to look for where a table should hold them, and say why a structure there is "
             "or is not a command; for commands that scripts call and no table lists")
    args = parser.parse_args(argv)

    with open(args.executable, "rb") as stream:
        pe = PE(stream.read())
    print("image base 0x%X, %d sections" % (pe.image_base, len(pe.sections)))
    if args.probe:
        probe(pe, args.probe, args.stride or 0x28)
        return 0

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

    parsers = {}
    for _, e in rows:
        parsers.setdefault(e.parse, []).append(e.name)
    for parse in sorted(parsers, key=lambda p: -len(parsers[p])):
        names = parsers[parse]
        print("parse function 0x%X: %d commands%s" % (parse, len(names), (": " + " ".join(names)) if len(names) <= 30 else ""))

    if args.csv:
        with open(args.csv, "w", newline="") as stream:
            writer = csv.writer(stream)
            for number, e in rows:
                params = " ".join(("%d?" if optional else "%d") % type_id for type_id, optional in e.params)
                writer.writerow([number, "0x%04X" % e.opcode, e.name, e.short_name, e.needs_parent, params,
                                 "0x%X" % e.flags, "0x%X" % e.parse])
        print("wrote %d entries to %s" % (len(rows), args.csv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
