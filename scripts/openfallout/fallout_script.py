#!/usr/bin/env python3
"""The bytes of compiled Fallout scripts (SCDA) and the sub-records of a script, for the synthetic test plugin.

A statement is the code, the length of its data and the data; an expression is reverse Polish text; a call of a command
is the code of the command (0x1000 and up) with a count of arguments and the arguments. This is the layout the
engine's decoder reads (components/esm4/scriptcode.cpp), written from the documentation of the community tools.
It holds no Bethesda data.
"""
import struct

SCRIPT_NAME = 0x1D
BEGIN = 0x10
END = 0x11
SET_TO = 0x15
IF = 0x16
ELSE = 0x17
ELSE_IF = 0x18
END_IF = 0x19

GAME_MODE = 0  # the type of block that runs every frame (or every delay of a quest)
ON_ACTIVATE = 2  # the types of the blocks of events of objects
ON_LOAD = 21
CALL_ON_REFERENCE = 0x1C  # the code of the statement that calls a command on a reference: ref.Command

# The codes of commands the tests use, as the executables of the games number them
GET_DISTANCE = 0x1001
GET_IS_ID = 0x1048
GET_IN_SAME_CELL = 0x1020
GET_IN_WORLDSPACE = 0x1136
GET_LINKED_REF = 0x116B
PLACE_AT_ME = 0x1025
GET_CURRENT_TIME = 0x1012
GET_DAY_OF_WEEK = 0x10AA
GET_LEVEL = 0x1050
GET_IS_CURRENT_PACKAGE = 0x10A1
GET_ACTOR_VALUE = 0x100E
SET_ACTOR_VALUE = 0x100F
MOD_ACTOR_VALUE = 0x1010
DAMAGE_ACTOR_VALUE = 0x1181
GET_HEALTH_PERCENTAGE = 0x11AF
GET_WEAPON_HEALTH_PERC = 0x11F4
IS_DLC_INSTALLED = 0x126F
GET_GLOBAL_VALUE = 0x104A
GET_IN_CELL = 0x1043
GET_LOCKED = 0x1005
GET_LOCK_LEVEL = 0x1041
LOCK = 0x1072
UNLOCK = 0x1073
GET_DESTROYED = 0x10CB
SET_DESTROYED = 0x10CC
IS_IN_LIST = 0x1174
MARK_FOR_DELETE = 0x11BB
ADD_FORM_TO_FORM_LIST = 0x1233
GET_POS = 0x1006
SET_POS = 0x1007
GET_ANGLE = 0x1008
SET_ANGLE = 0x1009
MOVE_TO_MARKER = 0x109E
ADD_ITEM = 0x1002
DISABLE = 0x1022
GET_DISABLED = 0x1023
GET_DEAD = 0x102E
GET_ITEM_COUNT = 0x102F
GET_IN_FACTION = 0x1047
GET_FACTION_RANK = 0x1049
REMOVE_ITEM = 0x1052
KILL_ACTOR = 0x108B
REMOVE_FROM_FACTION = 0x1180
SHOW_MESSAGE = 0x1059
GET_BUTTON_PRESSED = 0x101F
IS_ACTION_REF = 0x1069
EVALUATE_PACKAGE = 0x105E
ADD_SCRIPT_PACKAGE = 0x1097
ACTIVATE = 0x100D
START_QUEST = 0x1036
SET_STAGE = 0x1039
GET_STAGE = 0x103A
COMPLETE_QUEST = 0x1071
SET_OBJECTIVE_COMPLETED = 0x11A2
SET_OBJECTIVE_DISPLAYED = 0x11A3
SET_QUEST_DELAY = 0x11DC
ADD_PERK = 0x1176
REMOVE_PERK = 0x11A8
HAS_PERK = 0x11C1
ADD_NOTE = 0x117C
GET_HAS_NOTE = 0x117E
REWARD_XP = 0x1177
REWARD_KARMA = 0x1204
MENU_MODE = 0x1024
SHOW_RACE_MENU = 0x10DA
SHOW_NAME_MENU = 0x10DC
GET_PLAYER_NAME = 0x11E1
SET_IN_CHARGEN = 0x10E7
SHOW_CHARGEN_MENU = 0x1197
SET_SPECIAL_POINTS = 0x1218
SET_TAG_SKILLS = 0x121A
SHOW_TRAIT_MENU = 0x1268
GET_IN_CHARGEN = 0x127F
GET_PC_IS_RACE = 0x1082
GET_PC_IS_SEX = 0x1083


def u16(value):
    return struct.pack("<H", value)


def u32(value):
    return struct.pack("<I", value)


def statement(code, data=b""):
    return u16(code) + u16(len(data)) + data


def variable(index, kind="s"):
    """A local variable in an expression or as the target of Set: `s` for a short or a long, `f` for a float."""
    return kind.encode() + u16(index)


def global_target(reference):
    """A global variable, named by the index of its entry in the table of references (1 is the first)."""
    return b"G" + u16(reference)


def number(text):
    return text.encode() + b" "


def operator(text):
    return text.encode()


def int_argument(value):
    return b"n" + struct.pack("<i", value)


def reference_value(reference):
    """A reference as a value in an expression, named by the index of its entry in the table of references."""
    return b"Z" + u16(reference)


def string_argument(text):
    """A text, as a parameter that is a string holds it: its length in two bytes, then the characters."""
    data = text.encode()
    return u16(len(data)) + data


def float_argument(value):
    """A number with a fraction, as the compiler writes a literal for a parameter that is a float."""
    return b"z" + struct.pack("<d", value)


def axis_argument(letter):
    """An axis (X, Y or Z), written as the bare byte of its letter."""
    return letter.encode()


def form_argument(reference):
    """A form, named by the index of its entry in the table of references (1 is the first)."""
    return b"r" + u16(reference)


def call(code, *arguments):
    """A call of a command as a statement. A command with no arguments has no data at all."""
    if not arguments:
        return statement(code)
    return statement(code, u16(len(arguments)) + b"".join(arguments))


def call_counted(code, *arguments):
    """A call of a command that has parameters, when it gives none of them: the data is the count of arguments (0)."""
    return statement(code, u16(len(arguments)) + b"".join(arguments))


def call_on(reference, code, *arguments, counted=False):
    """A call of a command on a reference (ref.Command) as a statement: the 0x1C statement holds the index of the
    reference in its length field, then the call. A command that has parameters, all of them optional, still has the
    count of arguments (`counted`) when it is given none."""
    data = u16(len(arguments)) + b"".join(arguments) if arguments or counted else b""
    return u16(CALL_ON_REFERENCE) + u16(reference) + u16(code) + u16(len(data)) + data


def call_token(code, *arguments, reference=0):
    """A call of a command in an expression, on a reference when one is named (ref.Command)."""
    data = u16(len(arguments)) + b"".join(arguments) if arguments else b""
    prefix = b"r" + u16(reference) if reference else b""
    return prefix + b"X" + u16(code) + u16(len(data)) + data


def set_to(target, expression):
    return statement(SET_TO, target + u16(len(expression)) + expression)


def branch(code, expression, jump=0):
    return statement(code, u16(jump) + u16(len(expression)) + expression)


def block(block_type, body):
    """Begin <type> ... End: the length in the Begin counts from its end to the end of the End."""
    end = statement(END)
    return statement(BEGIN, u16(block_type) + u32(len(body) + len(end))) + body + end


class Script:
    """Compiled code and the tables that go with it. `type` is 0 for the script of an object, 1 for a quest."""

    def __init__(self, script_type=0):
        self.type = script_type
        self.code = b""
        self.references = []  # form ids (SCRO)
        self.variables = []  # (index, is_integer, name) (SLSD, SCVR)

    def form(self, form_id):
        """The index the bytecode uses for the form in the table of references."""
        if form_id not in self.references:
            self.references.append(form_id)
        return self.references.index(form_id) + 1

    def variable(self, name, is_integer=True):
        self.variables.append((len(self.variables) + 1, is_integer, name))
        return len(self.variables)

    def add(self, *statements):
        self.code += b"".join(statements)
        return self


def sub(code, data=b""):
    return code + struct.pack("<H", len(data)) + data


def sub_records(script):
    """SCHR, SCDA, SLSD and SCVR for each variable, and SCRO for each form: the sub-records of a script."""
    header = struct.pack("<IIIIHH", 0, len(script.references), len(script.code), len(script.variables), script.type, 1)
    subs = [sub(b"SCHR", header), sub(b"SCDA", script.code)]
    for index, is_integer, name in script.variables:
        # index, 12 unused bytes, the type (1 for a short or a long), 7 unused bytes
        subs.append(sub(b"SLSD", struct.pack("<I12xB7x", index, 1 if is_integer else 0)))
        subs.append(sub(b"SCVR", name.encode() + b"\0"))
    for form_id in script.references:
        subs.append(sub(b"SCRO", struct.pack("<I", form_id)))
    return subs
