#!/usr/bin/env python3
"""Write a tiny Fallout 3 format plugin and the one mesh it uses, so the engine can be started on a Fallout cell
without any game data.

The plugin has the records of a plain room: a static that is a cube (used as a floor and as a pillar), the marker
static that Fallout cells use for their entry point, an interior cell "OFTestCell" with the lighting of a Fallout 3
cell, a lighting template that the cell takes its fog colour and far fog distance from, and three references that put
them there. It also has a worldspace "OFTestWorld" with a flat exterior cell, a weather and the climate of the
worldspace that says the weather is the one to have (it lists a second weather too, which needs a global that is 0),
and two more exterior cells that are flagged for water: one has water of its own and the other the largest float as
its height, which the games write for a cell without water of its own. The worldspace names a kind of water (WATR),
green and opaque, with no reflection. Both cells have a person (a race and a character with a skeleton) standing
east of the end of the player's walk. The person has the parts of a body: the race names an upper body, two hands and a
head (boxes of different sizes), the person a hair and a suit that covers the upper body and the right hand, so that the
body of the race is there where the suit is not. West of the start there is a wall that is not drawn: a mesh with a
Havok box and nothing to draw (havok_wall.py).
South and west of the start there are three more characters that have AI packages (PACK) or none: one that goes to a
marker and turns the way the marker faces (the first package of its list is for the night, which it ignores at the
time of day the game starts at), one that has no package and must stay where it is put, and one that goes about the
place where it was put.
Two more follow the player: one by a package (Follow, the player as the target) and one that a script tells to.
Two more, far to the south, have packages that depend on scripts: one has two packages with conditions on quest C (one
that never holds, one that holds once the test has set a stage of the quest), and one has none until the script of the
scripted object gives it one (AddScriptPackage).
It holds no Bethesda data. The mesh is an OpenSceneGraph text file, a cube, which the engine can load beside the NIF
files of the games.

    scripts/openfallout/synthetic_fallout_plugin.py --out some/data/folder

writes `OFTest.esm`, `meshes/openfallout/cube.osgt`, the meshes of the person (`meshes/openfallout/person_*.osgt`),
`meshes/openfallout/wall.nif`, `meshes/openfallout/person_foot.nif`, `meshes/characters/_male/skeleton.nif`,
`meshes/characters/_male/mtidle.kf`,
`textures/sky/oftestclouds.dds` and `textures/landscape/oftestground.dds` into the folder.
"""
import argparse
import math
import struct
from pathlib import Path

import fallout_script
import havok_wall
import idle_animation
import placeholder_skeleton
import skinned_box

CUBE_ID = 0x800
CELL_ID = 0x801
FLOOR_ID = 0x802
PILLAR_ID = 0x803
MARKER_ID = 0x804
MARKER_REF_ID = 0x805
TEMPLATE_ID = 0x806
WORLD_ID = 0x807
EXTERIOR_CELL_ID = 0x808
EXTERIOR_LAND_ID = 0x809
EXTERIOR_PILLAR_ID = 0x80A
EXTERIOR_MARKER_REF_ID = 0x80B
WEATHER_ID = 0x80C
CLIMATE_ID = 0x80D
CONDITIONAL_WEATHER_ID = 0x80E
GLOBAL_ID = 0x80F
WATER_CELL_ID = 0x810
WATER_LAND_ID = 0x811
DRY_CELL_ID = 0x812
DRY_LAND_ID = 0x813
WATER_TYPE_ID = 0x814
RACE_ID = 0x815
NPC_ID = 0x816
NPC_REF_ID = 0x817
EXTERIOR_NPC_REF_ID = 0x818
HAIR_ID = 0x819
SUIT_ID = 0x81A
WALL_ID = 0x81B
WALL_REF_ID = 0x81C
EXTERIOR_WALL_REF_ID = 0x81D
TEXTURE_SET_ID = 0x81E
LAND_TEXTURE_ID = 0x81F
CREATURE_ID = 0x820
CREATURE_USER_ID = 0x821
CREATURE_LIST_ID = 0x822
CREATURE_REF_ID = 0x823
CREATURE_USER_REF_ID = 0x824
CREATURE_LIST_REF_ID = 0x825
EXTERIOR_CREATURE_REF_ID = 0x826
EXTERIOR_CREATURE_USER_REF_ID = 0x827
EXTERIOR_CREATURE_LIST_REF_ID = 0x828
IDLER_ID = 0x829
ROAMER_ID = 0x82A
ROAM_PACKAGE_ID = 0x82B
# The character that walks and its packages are one set for each kind of cell, because a package names the reference to
# go to, and a reference is in one cell
WALKER_IDS = {"interior": 0x82C, "exterior": 0x82D}
TRAVEL_PACKAGE_IDS = {"interior": 0x82E, "exterior": 0x82F}
NIGHT_PACKAGE_IDS = {"interior": 0x830, "exterior": 0x831}
# The references of the characters and of the markers they go to, in the interior cell and in the exterior one
PACKAGE_REF_IDS = {"target": (0x832, 0x837), "night": (0x833, 0x838), "walker": (0x834, 0x839),
                   "idler": (0x835, 0x83A), "roamer": (0x836, 0x83B)}
TARGET_MARKER_ID = 0x83C # a static with no model, for the characters to go to, which is not the entry marker of the cell
# Two characters that follow the player: the follower by a package (a Follow package that names the player as its target),
# the companion by a script, which tells it to when the game has run for COMPANION_FROM seconds
FOLLOW_PACKAGE_ID = 0x83D
FOLLOWER_ID = 0x83E
COMPANION_ID = 0x83F
FOLLOW_REF_IDS = {"follower": (0x840, 0x842), "companion": (0x841, 0x843)}
COUNTER_ID = 0x844 # a global the quests change
QUEST_A_ID = 0x845
QUEST_B_ID = 0x846
QUEST_SCRIPT_ID = 0x847
# An activator with a script of an object (each kind of cell has its own, since a script names the references it works
# on), the references that place it and the cube it disables, and the globals the script changes
OBJECT_SCRIPT_IDS = {"interior": 0x848, "exterior": 0x849}
SCRIPTED_IDS = {"interior": 0x84A, "exterior": 0x84B}
SCRIPTED_REF_IDS = {"interior": 0x84C, "exterior": 0x84D}
VICTIM_REF_IDS = {"interior": 0x84E, "exterior": 0x84F}
OBJECT_GLOBALS = {"loads": (0x850, "OFTestObjectLoads"), "frames": (0x851, "OFTestObjectFrames"),
                  "activated": (0x852, "OFTestObjectActivated"), "distance": (0x853, "OFTestObjectDistance"),
                  "victim": (0x854, "OFTestObjectVictimDisabled"), "items": (0x86D, "OFTestIdlerItems"),
                  "rank": (0x86E, "OFTestIdlerRank"), "joined": (0x86F, "OFTestIdlerJoined"),
                  "dead": (0x870, "OFTestIdlerDead"), "player_rank": (0x880, "OFTestPlayerRank"),
                  "listed": (0x881, "OFTestListedItems")}
# Characters whose packages depend on scripts. The conditional one has two packages, each with a condition on quest C: the
# first needs a stage that the quest never reaches, the second the stage that the test sets at GATE_FROM seconds, so it
# stays where it is put until then and goes to the marker of the second. The scripted one has no package: the script of
# the scripted object gives it one when the cell is loaded (AddScriptPackage and EvaluatePackage).
CONDITIONAL_IDS = {"interior": 0x855, "exterior": 0x856}
SCRIPTED_WALKER_IDS = {"interior": 0x857, "exterior": 0x858}
GATED_PACKAGE_IDS = {"interior": 0x859, "exterior": 0x85A}
BLOCKED_PACKAGE_IDS = {"interior": 0x85B, "exterior": 0x85C}
SCRIPT_PACKAGE_IDS = {"interior": 0x85D, "exterior": 0x85E}
GATE_REF_IDS = {"conditional": (0x85F, 0x860), "gated": (0x861, 0x862), "blocked": (0x863, 0x864),
                "scripted": (0x865, 0x866), "scripted_marker": (0x867, 0x868)}
QUEST_C_ID = 0x869
MESSAGE_ID = 0x86A # the message that the script of the scripted object shows when its cell is loaded
# The idler (a character with no packages) is in two factions and holds IDLER_ITEMS suits, by its record. The script of
# the scripted object adds IDLER_ADDS suits, takes IDLER_TAKES, takes the idler out of the second faction and kills it,
# and keeps what it sees of that in globals.
FACTION_IDS = (0x86B, 0x86C)
IDLER_RANKS = (3, 1)
IDLER_ITEMS = 4
IDLER_ADDS = 3
IDLER_TAKES = 2
# The player is the character of form 7 (what PlayerRef names in the games) and is in the first faction by that record;
# the script reads the rank back
PLAYER_BASE_ID = 0x7
PLAYER_RANK = 2
# The beast holds CREATURE_ITEMS suits by its record. The creature that is placed as a levelled list stands for it, so
# the script sees the same suits there
CREATURE_ITEMS = 6
LISTED_REF_IDS = {"interior": CREATURE_LIST_REF_ID, "exterior": EXTERIOR_CREATURE_LIST_REF_ID}
# The mover is a cube. The script of the scripted object puts it next to the target (MoveToMarker with offsets, so it
# turns the way the target does), keeps where it sees it, and, when the test sets MOVE_GLOBALS["now"] to 1, sets its
# height and moves the player to the marker of the other kind of cell (the exterior for the interior start, the
# interior for the exterior one), which the cell of the start has no record of in memory.
MOVER_REF_IDS = {"interior": 0x871, "exterior": 0x872}
MOVE_TARGET_REF_IDS = {"interior": 0x873, "exterior": 0x874}
MOVE_GLOBALS = {"x": (0x875, "OFTestMoverX"), "y": (0x876, "OFTestMoverY"), "angle": (0x877, "OFTestMoverAngle"),
                "now": (0x878, "OFTestMoveNow"), "far": (0x87B, "OFTestFarMoverX"),
                "hop": (0x886, "OFTestHopNow"), "near": (0x887, "OFTestMoverNear"),
                "crossed": (0x88A, "OFTestCrossedCell")}
# A second cube is sent to the marker of the other kind of cell, which is not loaded: the world keeps it there, and the
# script reads where it is from the cell that is
FAR_MOVER_REF_IDS = {"interior": 0x879, "exterior": 0x87A}
FAR_MOVER_PLACE = (1500.0, -800.0)
FAR_OFFSET = (5, 6, 0)
# The script of each kind of cell also moves a cube of the other kind of cell, which is not loaded, to a marker next to
# it: the cube is put there when its cell is loaded (when the test moves the player into it)
SLEEPER_REF_IDS = {"interior": 0x87C, "exterior": 0x87D}
SLEEPER_TARGET_REF_IDS = {"interior": 0x87E, "exterior": 0x87F}
SLEEPER_PLACE = (1800.0, -800.0)
SLEEPER_TARGET_PLACE = (1800.0, -1100.0)
SLEEPER_OFFSET = (7, 8, 0)
SLEEPER_PITCH = 30  # degrees: a turn of the x angle that comes after the move, while the cell is not loaded
MOVER_PLACE = (1200.0, -800.0)
MOVE_TARGET_PLACE = (1200.0, -1100.0)
MOVE_OFFSET = (10, 20, 0)
MOVE_HEIGHT = 321
FAR_MARKER_REF_IDS = {"interior": EXTERIOR_MARKER_REF_ID, "exterior": MARKER_REF_ID}
# A cube that the result script of quest C moves, which the test sets from Lua (so the script runs on the thread of Lua):
# it sets its x and then its y, and each must start from where the one before left it
SETTER_REF_IDS = {"interior": 0x882, "exterior": 0x883}
SETTER_PLACE = (300.0, -1400.0)
SETTER_SET = (700.0, -1700.0) # from the origin of the cell
SETTER_TURN = 180 # degrees: the script of the scripted object only turns it, in the frame of the hop of the player
# A cube that the script sends to the marker of the other kind of cell in the same frame as the player, before the
# player: the player's cell change must not leave it behind
TRAVELLER_REF_IDS = {"interior": 0x884, "exterior": 0x885}
TRAVELLER_PLACE = (1500.0, -2000.0)
# The test asks for a move of the player inside the cell it is in with a global (SetPos on x): the player is moved, not
# taken to the cell again (which fades the screen out and in)
HOP_X = 123.0
# A cube that the script gives an x in the cell next to the one it is in (the dry cell, 1,0) and asks right after whether
# it is in that cell: it is when the cell is an exterior one, and the interior cell has no cell next to it
CROSSER_REF_IDS = {"interior": 0x888, "exterior": 0x889}
CROSSER_PLACE = (1800.0, -1400.0)
CROSS_X = 5000
# Three cubes for the state that scripts keep of references: one is locked by its record (level LOCK_LEVEL) and the script
# opens it, one has no lock and the script locks it (level RELOCK_LEVEL), one is destroyed and marked for deletion. A
# list of forms (by its record the marker that the cubes are moved to) gets the cube added by the script, so the locker
# is in it afterwards and not before.
LOCKED_REF_IDS = {"interior": 0x88B, "exterior": 0x88C}
LOCKER_REF_IDS = {"interior": 0x88D, "exterior": 0x88E}
DOOMED_REF_IDS = {"interior": 0x88F, "exterior": 0x890}
LIST_IDS = {"interior": 0x891, "exterior": 0x892}
STATE_GLOBALS = {"locked": (0x893, "OFTestLockedBefore"), "lock_level": (0x894, "OFTestLockLevel"),
                 "unlocked": (0x895, "OFTestLockedAfter"), "relocked": (0x896, "OFTestRelocked"),
                 "relock_level": (0x897, "OFTestRelockLevel"), "destroyed": (0x898, "OFTestDestroyed"),
                 "doomed_disabled": (0x899, "OFTestDoomedDisabled"), "in_list": (0x89A, "OFTestInListByRecord"),
                 "list_before": (0x89B, "OFTestInListBefore"), "list_after": (0x89C, "OFTestInListAfter")}
LOCK_LEVEL = 50
RELOCK_LEVEL = 25
LOCKED_PLACE = (600.0, -1400.0)
LOCKER_PLACE = (900.0, -1400.0)
DOOMED_PLACE = (1200.0, -1400.0)
# What the story gives the player: the script of the object adds a perk twice to the player, asks for it for the
# teammates (the second argument of HasPerk, not a rank), removes it, adds a note, asks
# for the base form of the locker cube and sets the delay of quest A (whose record has the longer QUEST_RECORD_DELAY, so
# that the quest only gets to its stage in time if SetQuestDelay worked). The perk and the note have no record: scripts
# only name them.
PERK_ID = 0x89D
NOTE_ID = 0x89E
PROGRESS_GLOBALS = {"perk_before": (0x89F, "OFTestPerkBefore"), "perk_after": (0x8A0, "OFTestPerkAfter"),
                    "perk_teammates": (0x8A1, "OFTestPerkTeammates"), "perk_removed": (0x8A2, "OFTestPerkRemoved"),
                    "note_before": (0x8A3, "OFTestNoteBefore"), "note_after": (0x8A4, "OFTestNoteAfter"),
                    "is_id": (0x8A5, "OFTestIsId"), "is_not_id": (0x8A6, "OFTestIsNotId")}
XP_REWARD = 150
KARMA_REWARD = -20
# More of what scripts ask about: the locker cube is linked to the doomed one (XLKR), is in the same cell as it, and is in
# the worldspace only when the cell is the exterior one. The beast has a script, which adds one to a global when it
# loads: it runs for the beast, for the creature placed as the levelled list (which stands for the beast) and for the
# heir, a creature that takes the script from the beast as its template.
TEMPLATE_SCRIPT_ID = 0x8A7
HEIR_ID = 0x8A8
HEIR_REF_IDS = {"interior": 0x8A9, "exterior": 0x8AA}
HEIR_PLACE = (800.0, 740.0)
HEIR_TEMPLATE_FLAGS = 0x40 | 0x80 | 0x200 # the model, the name and the script come from the template
WORLD_QUERY_GLOBALS = {"linked": (0x8AB, "OFTestLinkedRef"), "linked_none": (0x8AC, "OFTestLinkedRefNone"),
                       "same_cell": (0x8AD, "OFTestInSameCell"), "in_worldspace": (0x8AE, "OFTestInWorldspace"),
                       "template_loads": (0x8AF, "OFTestTemplateScriptLoads"), "placed": (0x8B0, "OFTestPlacedRef")}
# PlaceAtMe: the script makes PLACE_COUNT cubes PLACE_DISTANCE units in front of the locker cube (which faces north, so
# they stand that far on in y) and keeps what the call told, which is the reference of the first one made
PLACE_COUNT = 2
PLACE_DISTANCE = 120
# The clock: the game has a global GameHour (13 by its record), which follows the clock of the world instead, as do
# GetCurrentTime and GetGlobalValue of that global. The three readings are the same, and not the 13 of the record.
GAME_HOUR_ID = 0x8B1
GAME_HOUR_RECORD_VALUE = 13.0
CLOCK_GLOBALS = {"hour_command": (0x8B2, "OFTestHourCommand"), "hour_global": (0x8B3, "OFTestHourGlobal"),
                 "global_value": (0x8B4, "OFTestGlobalValue")}
# The actor values: the locker cube gets Variable01 (62) set to 7, 3 more and 4 damage, which makes 6; 25 damage to its
# health (16), which leaves three quarters of it; and 75 more, which kills it. The player has the karma that was rewarded
# (KARMA_REWARD) when asked for as an actor value (23), and a weapon is as good as new.
AV_VARIABLE = 62
AV_HEALTH = 16
AV_KARMA = 23
AV_STRENGTH = 5
AV_LUCK = 11
AV_SPEED = 21
# What the record of the test person says (person()): the attributes of its DATA (its health and its skills in DNAM are
# written too, and are not what the actor values start at), and the speed and karma of its ACBS
PERSON_HEALTH = 150
PERSON_ATTRIBUTES = (4, 5, 6, 7, 8, 9, 10)
PERSON_SKILLS = tuple(range(20, 34))
PERSON_SPEED = 120
PERSON_KARMA = 25.0
PERSON_LEVEL = 12  # the level of its ACBS (no flag is set, so it is the level of the character)
BEAST_LEVEL = 7  # the level of the beast, which the creature that has the list as its template takes the stats from
DLC_NAME_THERE = "OF Test"  # the plugin the test loads is OFTest.esm
DLC_NAME_MISSING = "Absent Pack"
ACTOR_VALUE_GLOBALS = {"variable": (0x8B5, "OFTestAvVariable"), "share": (0x8B6, "OFTestAvShare"),
                       "dead": (0x8B7, "OFTestAvDead"), "karma": (0x8B8, "OFTestAvKarma"),
                       "weapon": (0x8B9, "OFTestAvWeapon"), "strength": (0x8BA, "OFTestAvStrength"),
                       "luck": (0x8BB, "OFTestAvLuck"), "speed": (0x8BC, "OFTestAvSpeed"),
                       "person_karma": (0x8BD, "OFTestAvPersonKarma"),
                       "dlc_there": (0x8BE, "OFTestAvDlcThere"), "dlc_missing": (0x8BF, "OFTestAvDlcMissing"),
                       "person_level": (0x8C0, "OFTestAvPersonLevel"), "day_of_week": (0x8C1, "OFTestAvDayOfWeek"),
                       "current_package": (0x8C2, "OFTestAvCurrentPackage"), "user_level": (0x8C3, "OFTestAvUserLevel")}
# A conversation: a character with a voice type (and a second voice type that nobody has), the topics of the games (the
# greeting that opens it, the goodbye that closes it, a topic at the top of the list and one that a response adds) and
# their responses, with the globals that the scripts of the responses set
VOICE_ID = 0x8C4
OTHER_VOICE_ID = 0x8C5
TALKER_ID = 0x8C6
TALKER_REF_ID = 0x8C7
GREETING_TOPIC_ID = 0x8C8
GOODBYE_TOPIC_ID = 0x8C9
ASK_TOPIC_ID = 0x8CA
ADDED_TOPIC_ID = 0x8CB
WRONG_GREETING_ID = 0x8CC
GREETING_INFO_ID = 0x8CD
ASK_INFO_ID = 0x8CE
ADDED_INFO_ID = 0x8CF
GOODBYE_INFO_ID = 0x8D0
# The button of a message box with buttons that the end of the conversation shows: the response that ends it shows the
# message, and the script of the talker reads the button into the third global (the number of the button, plus one)
BUTTON_MESSAGE_ID = 0x8D4
BUTTON_SCRIPT_ID = 0x8D5
BUTTON_MESSAGE_TITLE = "OpenFallout Test Message"
BUTTON_MESSAGE_TEXT = "Pick one of the buttons."
# the second button has a condition that the player does not meet (it is not shown); the third is picked by the test
BUTTON_TEXTS = ("First choice", "Hidden choice", "Third choice")
BUTTON_PICKED = 2
# The end script of the question makes the talker say a topic outside of the conversation (Say), and when the talker has
# said it the Begin SayToDone block of its script adds one to a global. The end script of the response that is said sets
# another global, and the block adds one to a third only when that one is set by then: a response ends when its line is
# over, and its end script has run by the time the talker is told that it is done.
SAID_TOPIC_ID = 0x8F3
SAID_INFO_ID = 0x8F4
SAID_TEXT = "Said aloud."
# The blocks of the script name what they are for. SayToDone names the topic that was said, so a block that names another
# topic must not run (the fifth global); MenuMode names the number of a menu: the conversation (1009) and the message box
# (1001) are open at some time in the test and count (the sixth and seventh), the love tester (1074) is not and must not
# (the eighth).
DIALOGUE_GLOBALS = {"begin": (0x8D1, "OFTestDialogueBegin"), "end": (0x8D2, "OFTestDialogueEnd"),
                    "button": (0x8D3, "OFTestDialogueButton"), "said": (0x8F2, "OFTestDialogueSaid"),
                    "wrong": (0x8F5, "OFTestDialogueWrong"), "talk": (0x8F6, "OFTestDialogueTalk"),
                    "box": (0x8F7, "OFTestDialogueBox"), "elsewhere": (0x8F8, "OFTestDialogueElsewhere"),
                    "said_end": (0x913, "OFTestDialogueSaidEnd"), "ordered": (0x914, "OFTestDialogueOrdered")}
MENU_NUMBERS = {"talk": 1009, "box": 1001, "elsewhere": 1074}
TALKER_NAME = "OFTestTalker"
TALKER_TITLE = "OpenFallout Test Talker"
TALKER_PLACE = (-300.0, 300.0)
ASK_TOPIC_NAME = "Ask about the test"
ASK_PROMPT = "Tell me about the test."
ADDED_TOPIC_NAME = "The added topic"
GREETING_TEXT = "Welcome to the test."
WRONG_GREETING_TEXT = "This greeting is for another voice."
ASK_TEXT = "The test goes well."
ADDED_TEXT = "This topic came with the answer before."
# The response of the added topic names a sound record (the sound field of its TRDT) in place of a voice file: the
# record has a file under sound/ that no voice folder holds, and that is the file that is looked up. The record names a
# wav and the data has the mp3 that it was converted to, as the sounds of the games do.
SOUND_ID = 0x8F1
SOUND_NAME = "OFTestAddedSound"
SOUND_FILE = "fx\\oftest\\added.wav"
SOUND_PATH = "sound/fx/oftest/added.mp3"
# The second line of the added topic's response names a sound record that names a folder (a trailing backslash, as some
# records of the games do): the first file of the folder is said.
FOLDER_SOUND_ID = 0x8F9
FOLDER_SOUND_NAME = "OFTestFolderSound"
FOLDER_SOUND_FILE = "fx\\oftestfolder\\"
FOLDER_SOUND_PATH = "sound/fx/oftestfolder/first.wav"
ADDED_MORE_TEXT = "It has a second line."
GOODBYE_TEXT = "Farewell."
# The voice files of the lines: sound/voice/<plugin>/<voice type>/<text>_<form id of the response, 8 hex digits>_<n>.ogg.
# The greeting has a file in the folder of the voice of the talker and one in the folder of another voice (the talker's
# must be the one that is looked up), the question has one only in the other folder (another person's voice, so the
# line must be silent), the added topic's response names a sound record whose file is SOUND_PATH, and the other
# responses have none. The bytes are not a sound: what is under test is which file is looked up.
VOICE_FILES = (
    ("OFTestVoice", "oftest_oftestgreeting", GREETING_INFO_ID),
    ("OFTestOtherVoice", "oftest_oftestgreeting", GREETING_INFO_ID),
    ("OFTestOtherVoice", "oftest_oftestask", ASK_INFO_ID),
)


def voice_path(voice, text, response, number=1):
    return f"sound/voice/oftest.esm/{voice.lower()}/{text}_{response:08x}_{number}.ogg"


# The making of the character: two traits and a perk that is none (PERK), a second playable race, the texts of two
# actor values (AVIF) and a quest whose script opens the menus when the test asks for it, waits until the player is done
# with all of their screens and then reads what the player made into globals
TRAIT_A_ID = 0x8D6
TRAIT_B_ID = 0x8D7
NOT_A_TRAIT_ID = 0x8D8
RACE_TWO_ID = 0x8D9
AVIF_STRENGTH_ID = 0x8DA
AVIF_BARTER_ID = 0x8DB
CHARGEN_QUEST_ID = 0x8DC
CHARGEN_SCRIPT_ID = 0x8DD
CHARGEN_GLOBAL_NAMES = ("go", "state", "during", "after", "female", "race_two", "strength", "perception", "endurance",
                        "charisma", "intelligence", "agility", "luck", "barter", "guns", "survival", "trait_a",
                        "trait_b", "other")
CHARGEN_GLOBALS = {name: (0x8DE + index, "OFTestCg" + name.title().replace("_", ""))
                   for index, name in enumerate(CHARGEN_GLOBAL_NAMES)}
TRAIT_A_NAME, TRAIT_B_NAME, NOT_A_TRAIT_NAME = "Test Trait Alpha", "Test Trait Beta", "Test Perk"
RACE_TWO_NAME = "OFTestRaceTwo"
AVIF_TEXTS = {AVIF_STRENGTH_ID: ("Strength", "Strength of the test", "The test strength description."),
              AVIF_BARTER_ID: ("Barter", "Barter of the test", "The test barter description.")}
AV_PERCEPTION, AV_ENDURANCE, AV_CHARISMA, AV_INTELLIGENCE, AV_AGILITY = 6, 7, 8, 9, 10
AV_BARTER, AV_GUNS, AV_SURVIVAL = 32, 41, 44

# The quest that starts a new game, as CG00 does in Fallout 3: the engine finds it by its editor ID, starts it and runs
# its MenuMode block (the main menu counts as a menu), which sets stage 0 as long as the quest is before stage 5; the
# result script of stage 0 takes the controls of the player, moves the player to a marker (which faces a way) and sets
# stage 5. At stage 5 the script of the quest shows a message box with two buttons and sets stage 6; once the test has
# picked the first button (from Lua, as a click would) it sets stage 10, which gives the controls back and opens the name
# screen.
NEW_GAME_QUEST_NAME = "CG00"
NEW_GAME_QUEST_ID = 0x8FA
NEW_GAME_SCRIPT_ID = 0x8FB
NEW_GAME_PLACE_REF_ID = 0x8FC
NEW_GAME_MESSAGE_ID = 0x8FD
NEW_GAME_PLACE = (300.0, 200.0, 0.0) # in the interior cell
NEW_GAME_YAW = 1.0 # the way the marker faces, radians
NEW_GAME_FIRST_STAGE = 0
NEW_GAME_BOX_STAGE = 5
NEW_GAME_WAIT_STAGE = 6
NEW_GAME_LAST_STAGE = 10
NEW_GAME_BUTTONS = ("First choice", "Second choice")
NEW_GAME_NAME = "Autoplay" # the name autoplay.lua gives the character

LAST_ID = 0x914 # the largest form ID of the plugin, which the next object ID of its header follows
# What the packages tell scripts. The Travel package of the walker has a script for each of its events (its script
# counts them: it is begun once, done once when the walker gets there, and never changed), and so has the package that
# the conditional character follows first, for as long as quest C is below QUEST_C_STAGE (a Guard package at the place
# the character is put, which stays: it is begun once, and changed when the gate opens and another package is on; it is
# never done). The script of the walker has blocks for the events of the packages too: one that names the Travel package
# for its start and one for its end, one that names the package for the night (which is not on) for its end, and one that
# names none for its end (any package).
PACKAGE_EVENT_GLOBALS = {"travel_begin": (0x905, "OFTestTravelBegin"), "travel_end": (0x906, "OFTestTravelEnd"),
                         "travel_change": (0x907, "OFTestTravelChange"), "early_begin": (0x908, "OFTestEarlyBegin"),
                         "early_end": (0x909, "OFTestEarlyEnd"), "early_change": (0x90A, "OFTestEarlyChange"),
                         "block_start": (0x90B, "OFTestBlockStart"), "block_done": (0x90C, "OFTestBlockDone"),
                         "block_other": (0x90D, "OFTestBlockOther"), "block_any": (0x90E, "OFTestBlockAny")}
WATCH_SCRIPT_IDS = {"interior": 0x90F, "exterior": 0x910}
EARLY_PACKAGE_IDS = {"interior": 0x911, "exterior": 0x912}
# The trigger volume: an activator with a script that counts who comes into, stays in and leaves a box (the script has
# OnTriggerEnter and OnTrigger blocks for the player, OnTriggerEnter for anyone, and OnTriggerLeave for the player; when
# the player comes in it also makes a person at the middle of the box with PlaceAtMe)
TRIGGER_GLOBALS = {"enters": (0x8FE, "OFTestTriggerEnters"), "stays": (0x8FF, "OFTestTriggerStays"),
                   "leaves": (0x900, "OFTestTriggerLeaves"), "any": (0x901, "OFTestTriggerAnyEnters")}
TRIGGER_SCRIPT_ID = 0x902
TRIGGER_BASE_ID = 0x903
TRIGGER_REF_ID = 0x904
TRIGGER_PLACE = (-1500.0, -1200.0) # in the interior cell, where the box stands on the floor
TRIGGER_YAW = math.pi / 2 # a quarter turn about z: the long side of the box runs along y
TRIGGER_HALF = (150.0, 20.0, 50.0) # the half sizes the record gives (x, y, z) before the turn
SCRIPTED_PLACE = (600.0, -800.0) # where the scripted object stands, from the start of the player
VICTIM_PLACE = (900.0, -800.0)
CELL_NAME = "OFTestCell"
NPC_NAME = "OFTestPerson"
RACE_NAME = "OFTestRace"
# The skeleton of the character, as the record names it: Fallout characters have theirs in the NPC_ record, not in the
# race. The file is the placeholder skeleton of the engine, which is the one with the bounding box.
SKELETON = "characters\\_male\\skeleton.nif"
# The animation of the character: the file `mtidle.kf` in the folder of its skeleton (idle_animation.py), which moves the
# left foot to the left of the actor, 150 units from where the skeleton has it, in the first second and holds it there.
# The race has a small box skinned to that foot (skinned_box.py), as the head part after the head (the ears), so that a
# ray at what is drawn finds where the animation put it (the other parts of the body are plain models, they do not
# follow the bones).
IDLE_ANIMATION = "characters\\_male\\mtidle.kf"
FOOT_BONE = "Bip01 L Foot"
FOOT_MARKER = (3.0, 3.0, 3.0)
FOOT_MOVE = -150.0
FOOT_PART = "openfallout\\person_foot.nif"
# The animations of walking and running: `mtforward.kf` and `mtfastforward.kf` in the folder `locomotion` below that of the skeleton
# (the files the engine looks for). Each moves the same foot, to a place of its own, and holds it there after a fifth of a
# second, and moves the root of the skeleton forward at the speed of walking and of running, so that the engine can tell
# how fast each travels. A person who is moved plays them: the foot is at WALK_MOVE from the calf while the person
# walks, at RUN_MOVE while it runs, and back at FOOT_MOVE when it stands again.
WALK_ANIMATION = "characters\\_male\\locomotion\\mtforward.kf"
RUN_ANIMATION = "characters\\_male\\locomotion\\mtfastforward.kf"
WALK_MOVE = -70.0
RUN_MOVE = -10.0
WALK_VELOCITY = 100.0
RUN_VELOCITY = 300.0
LOCOMOTION_SECONDS = 0.2
# Where the foot is when the skeleton is at rest: the bones hang from each other (BONES in placeholder_skeleton.py), the
# pelvis 64 units up, the thigh 8 to the left and 4 down from it, and the calf and the foot 26 down each.
FOOT_REST = (-8.0, 0.0, 8.0)
# Three creatures stand east of the person, in view when the player faces east at the end of the walk west: a creature
# with a skeleton, body models (NIFZ) and an idle animation of its own (in the folder `creatures/oftestbeast`, with the
# same body as the person has: a box above the ground and the small box skinned to the left foot), one that takes its model
# and its name from a template that is a levelled list of creatures (which has the first one at level 1), and a levelled
# list of creatures placed as it is, the way the games place the creatures that appear by chance. The creatures stand
# at CREATURE_POSITIONS, east and north of where the player starts. The body of the first is a box of CREATURE_BOUNDS
# in its record.
CREATURE_NAME = "OFTestBeast"
CREATURE_FOLDER = "creatures\\oftestbeast\\"
CREATURE_SKELETON = CREATURE_FOLDER + "skeleton.nif"
CREATURE_IDLE = CREATURE_FOLDER + "mtidle.kf"
CREATURE_BODY = "body.osgt"  # bare names, as NIFZ lists them: the files are in the folder of the skeleton
CREATURE_FOOT = "foot.nif"
CREATURE_BODY_BOX = ((30.0, 15.0, 15.0), (0.0, 0.0, 60.0))
CREATURE_BOUNDS = (-30, -15, 0, 30, 15, 80)
CREATURE_POSITIONS = {"beast": (450.0, 140.0), "user": (450.0, 540.0), "list": (800.0, 340.0)}
# The flags of ACBS that take the model, the name and the stats from the template
CREATURE_TEMPLATE_FLAGS = 0x40 | 0x80 | 0x2
# Characters with AI packages stand south and west of where the player starts, away from where the player walks (x from
# -150 to 300, y from 0 to 600). The walker starts at WALKER_START, and travels (package type Travel) to a marker 300
# units north of it, which faces east, and stops TRAVEL_RADIUS from it; its first package, for the night (22:00 to 02:00, to another marker), is not on
# when the game starts at about nine in the morning, and the second is on at any time. The idler has no packages. The
# roamer has a package that sends it about where it stands, ROAM_RADIUS around it (Wander near the editor location).
# Positions are relative to where the player starts, as the creatures' are.
PACKAGE_PLACES = {"target": (-500.0, -100.0), "night": (-900.0, -400.0), "walker": (-500.0, -400.0),
                  "idler": (-300.0, -700.0), "roamer": (-700.0, -700.0)}
# The characters whose packages depend on scripts, and their markers (300 units north of where the characters start)
GATE_PLACES = {"conditional": (-1200.0, -1200.0), "gated": (-1200.0, -900.0), "blocked": (-1500.0, -1200.0),
               "scripted": (1200.0, -1200.0), "scripted_marker": (1200.0, -900.0)}
GATE_FROM = 5.0 # the game time at which the test sets stage QUEST_C_STAGE of quest C, which the second package needs
QUEST_C_NAME = "OFTestQuestC"
QUEST_C_STAGE = 5
QUEST_C_TEXT = "Gate opened"
TARGET_YAW = math.pi / 2
NIGHT_YAW = math.pi
ROAM_RADIUS = 200
TRAVEL_RADIUS = 64 # the walker has arrived when it is this close to the marker (a character stops at the edge of the radius)
# The followers start south of the start, and follow the player at these distances (units): the package names one, the
# script the other
FOLLOW_PLACES = {"follower": (-200.0, -600.0), "companion": (200.0, -600.0)}
FOLLOW_DISTANCES = {"follower": 250, "companion": 200}
COMPANION_FROM = 3.0
PACKAGE_FOLLOW = 1
PLAYER_REFERENCE = 0x14
NIGHT_START = 22
NIGHT_HOURS = 4
PACKAGE_TRAVEL = 6
PACKAGE_WANDER = 5
PACKAGE_GUARD = 14
LOCATION_NEAR_REFERENCE = 0
LOCATION_NEAR_CURRENT = 2
LOCATION_NEAR_EDITOR = 3
# The quests: A starts with the game and its script counts the delays of the quest (QUEST_DELAY seconds each) and sets
# stage 10 on the third. The result script of that stage sets stage 20 of B and adds COUNTER_ADDS to a global, and B
# completes at that stage and completes an objective. The text of each stage goes to the journal.
QUEST_A_NAME = "OFTestQuestA"
QUEST_B_NAME = "OFTestQuestB"
COUNTER_NAME = "OFTestCounter"
QUEST_DELAY = 0.1
QUEST_RECORD_DELAY = 1.0 # the delay quest A has by its record, which the script of the object makes QUEST_DELAY
QUEST_TICKS = 3
COUNTER_ADDS = 7
QUEST_A_STAGE = 10
QUEST_B_STAGE = 20
QUEST_OBJECTIVE = 5
QUEST_A_TEXT = "Stage ten reached"
QUEST_B_TEXT = "Stage twenty reached"
MESSAGE_TEXT = "The object script was here"
QUEST_NEVER_TEXT = "Never told"
COUNTER_NEVER_ADDS = 1000
GET_STAGE_FUNCTION = 58 # the number the games give GetStage in a condition
GET_IS_SEX_FUNCTION = 70
GET_IS_VOICE_TYPE_FUNCTION = 427
CONDITION_AT_LEAST = 0x60 # the comparison "greater than or equal to" of a condition
CONDITION_BELOW = 0x80 # the comparison "less than"
QUEST_OBJECTIVE_TEXT = "Do the thing"
QUEST_FLAG_START_GAME = 0x01
STAGE_FLAG_COMPLETE_QUEST = 0x01
WORLD_NAME = "OFTestWorld"
WEATHER_NAME = "OFTestWeather"
CONDITIONAL_WEATHER_NAME = "OFTestConditionalWeather"
# The size of an exterior cell of Fallout 3 and New Vegas, in game units. The exterior cell 0,0 covers x and y from 0 to
# this. The cell has an entry marker at its centre, which is where the player starts; a cell without a marker is
# entered at its centre too.
EXTERIOR_CELL_SIZE = 4096.0
# The exterior cell north of the start (grid 0,1) is a shallow basin: its flat terrain is at WATER_TERRAIN, below the
# terrain of the start cell, and it has water at WATER_HEIGHT, a little above its terrain and below the eyes of the
# player, who sees it from above as a band just below the horizon when they look north. The cell east of it (grid 1,0)
# is flagged for water as nearly all the cells of New Vegas are, but holds the largest float as its height, so it has
# the default height of the worldspace, far below its terrain: no water to see.
WATER_CELL = (0, 1)
DRY_CELL = (1, 0)
WATER_TERRAIN = -40.0
WATER_HEIGHT = -20.0
# The kind of water of the worldspace (its NAM2), which the cells take as they name none: green and wholly opaque, with
# no reflection, so that the water shows its green (with the fog in front of it), which is far from what the water of
# Morrowind (white, half transparent) shows over the terrain, and which is not what a texture of one colour that the
# engine can not find (magenta, so no green at all) would make of it. The colours are red, green and blue of 0 to 255,
# the opacity is a percentage.
WATER_TYPE_NAME = "OFTestWater"
WATER_SHALLOW = (0, 160, 0)
WATER_DEEP = (0, 160, 0)
WATER_OPACITY = 100
WATER_REFLECTIVITY = 0.0
FLOAT_MAX = 3.4028234663852886e+38
# Edge of the cube in game units. The floor is the cube scaled up so that its top face is at height 0, where the
# player starts, and the pillar is the cube as it is, standing on the floor PILLAR_DISTANCE units north of the start.
CUBE = 256.0
FLOOR_SCALE = 16.0
PILLAR_DISTANCE = 600.0
# How far north of the start the person stands: a little short of the south face of the pillar, so that the player
# who has walked into the pillar and strafes east meets them with the middle of their body.
NPC_DEPTH = PILLAR_DISTANCE - CUBE / 2 - 30.0
# and this many units east of the line of the walk, the player is a box 40 wide and the person a cylinder 40 wide
NPC_DISTANCE = 300.0
# The body of the person: boxes of half extents and centre (x east, y north, z up, from the feet) that are the models of
# the parts. The race has an upper body wider than the suit (so that the suit is seen where both are drawn), a hand on
# each side outside of the cylinder, and a head; the person has a hair on top of it and wears a suit that covers the
# upper body and the right hand (the right hand is the one on the east side, the way the person faces).
PERSON_PARTS = {
    "upperbody": ((18.0, 10.0, 30.0), (0.0, 0.0, 60.0)),
    "lefthand": ((3.0, 3.0, 3.0), (-26.0, 0.0, 50.0)),
    "righthand": ((3.0, 3.0, 3.0), (26.0, 0.0, 50.0)),
    "head": ((8.0, 8.0, 8.0), (0.0, 0.0, 118.0)),
    "hair": ((9.0, 9.0, 3.0), (0.0, 0.0, 130.0)),
    "suit": ((12.0, 8.0, 30.0), (0.0, 0.0, 60.0)),
}
# The face of the person: the head box has a FaceGen file beside it (person_head.egm) with 50 symmetric and 30 asymmetric
# morphs that move nothing but the first, which moves each vertex FACE_SHIFT units east when the coefficient is 1. The
# coefficient is the sum of the race's FGGS (for men) and the person's: RACE_FACE and PERSON_FACE, which only add up to 1
# when the engine puts the face of the race and the numbers of the person together. The head is where the shift puts it
# only when the engine morphs it.
FACE_SHIFT = 20.0
RACE_FACE = 0.75
PERSON_FACE = 0.25
HEAD_VERTICES = 24  # four for each face of the box (box_mesh)
# The file has more vertices than the head, as the head files of the games do (1449 for a head of 1211): the head is
# the first of them
HEAD_FILE_EXTRA_VERTICES = 10

# The biped slots of Fallout 3 that the suit covers: the upper body (0x04) and the right hand (0x10)
SUIT_SLOTS = 0x04 | 0x10

# A wall that is not drawn, a mesh with only Havok collision (havok_wall.py): a box of these half extents, west of where
# the player starts and across the way of a walk west at the depth of the person
WALL_MESH = "openfallout\\wall.nif"
WALL_HALF_EXTENTS = (20.0, 100.0, 100.0)
WALL_DISTANCE = 150.0

# The lighting of the cell, in the order of the XCLL sub-record. The cell inherits the fog colour and the far fog
# distance of its lighting template (inherit flags 0x04 and 0x10), the rest is its own: what the engine should use is
# the cell's ambient and directional colours, the template's fog colour and far distance, and the cell's near distance.
AMBIENT = (90, 90, 100)
DIRECTIONAL = (200, 190, 160)
CELL_FOG = (20, 25, 30)
CELL_FOG_FAR = 6000.0
TEMPLATE_FOG = (120, 110, 100)
TEMPLATE_FOG_FAR = 2500.0
FOG_NEAR = 100.0
INHERIT_FOG_COLOR_AND_FAR = 0x04 | 0x10

# The weather: for each of the ten colour types of NAM0 (sky upper, fog, clouds lower, ambient, sunlight, sun, stars,
# sky lower, horizon, clouds upper) the colours at sunrise, day, sunset and night. Only the types the engine uses are
# distinct, the others are black. It has 4 times of day like the weathers of Fallout 3, those of New Vegas have 6.
WEATHER_SKY = ((200, 120, 60), (30, 80, 200), (210, 90, 40), (0, 0, 40))
WEATHER_FOG = ((190, 150, 110), (200, 190, 170), (180, 120, 90), (10, 10, 20))
WEATHER_AMBIENT = ((90, 70, 60), (70, 80, 90), (80, 60, 60), (10, 10, 30))
WEATHER_SUNLIGHT = ((255, 200, 150), (255, 240, 200), (255, 160, 100), (0, 0, 0))
WEATHER_SUN = ((255, 255, 255), (255, 255, 255), (255, 120, 40), (0, 0, 0))
# The texture of the clouds, as the record names it (a path under textures, the way the games write it), and the image:
# white pixels of one alpha, so that the colour of the clouds on the screen does not depend on where it is looked at
# or how far they have drifted.
CLOUD_TEXTURE = "sky\\OFTestClouds.dds"
CLOUD_ALPHA = 128
# The texture of the ground of the exterior cell: a landscape texture record names a texture set (as the records of
# Fallout 3 and New Vegas do) and has an icon filename that is not where the texture is, the texture set names the file
# relative to the textures directory, and the file is one colour.
GROUND_TEXTURE = "landscape\\OFTestGround.dds"
GROUND_ICON = "landscape\\OFTestIcon.dds"
GROUND_COLOUR = (200, 40, 40)
# Where the fog starts and ends, in game units, by day and by night, then its power by day and by night.
WEATHER_FOG_DAY = (300.0, 12000.0)
WEATHER_FOG_NIGHT = (150.0, 6000.0)
# The wind and the glare of the sun, as bytes (the engine makes a share of 1 of them)
WEATHER_WIND = 51
WEATHER_GLARE = 255
# The chance that the climate gives the weather, and the one it gives a second weather that needs a global that is 0:
# the second weather can not be chosen, so whatever the chances are the first one is.
WEATHER_CHANCE = 7
CONDITIONAL_WEATHER_CHANCE = 93


def sub(code, data=b""):
    return code + struct.pack("<H", len(data)) + data


def zstr(code, text):
    return sub(code, text.encode() + b"\0")


def record(code, form_id, subs, flags=0):
    data = b"".join(subs)
    # Fallout 3 header: size, flags, form ID, revision, form version, version control information (24 bytes).
    return code + struct.pack("<IIIIHH", len(data), flags, form_id, 0, 15, 0) + data


def group(label, group_type, children):
    body = b"".join(children)
    return b"GRUP" + struct.pack("<I", 24 + len(body)) + label + struct.pack("<iHHHH", group_type, 0, 0, 0, 0) + body


def top_group(code, children):
    return group(code, 0, children)


def refr(form_id, base, position, scale=None, rotation=(0.0, 0.0, 0.0), lock_level=None, linked=None,
         primitive=None):
    subs = [sub(b"NAME", struct.pack("<I", base))]
    if primitive is not None:
        # XPRM, 32 bytes: the bounds, the colour, a float for the way the editor draws it and the type (1 a box)
        bounds, kind = primitive
        subs.append(sub(b"XPRM", struct.pack("<3f3ffI", *bounds, 0.0, 0.5, 1.0, 0.1, kind)))
    if lock_level is not None:
        # level, 3 unused bytes, the key (none), 4 bytes of flags and 8 more, as Fallout 3 writes the 20 bytes of a lock
        subs.append(sub(b"XLOC", struct.pack("<B3xI4x8x", lock_level, 0)))
    if linked is not None:
        subs.append(sub(b"XLKR", struct.pack("<I", linked)))
    if scale is not None:
        subs.append(sub(b"XSCL", struct.pack("<f", scale)))
    subs.append(sub(b"DATA", struct.pack("<6f", *position, *rotation)))
    return record(b"REFR", form_id, subs)


def lighting(ambient, directional, fog, fog_near, fog_far):
    """A 40 byte lighting struct as Fallout 3 writes it, in the XCLL of a cell and in the DATA of a lighting template:
    ambient, directional and fog colours (RGBA bytes), fog near and far, the direction as two integers, the fade and
    clip distance of the fog and its power."""
    data = struct.pack("<12Bffiifff", *ambient, 0, *directional, 0, *fog, 0, fog_near, fog_far, 0, 0, 1.0, 4000.0, 1.0)
    assert len(data) == 40
    return data


def weather(form_id=WEATHER_ID, name=WEATHER_NAME):
    """A WTHR record with 4 times of day in NAM0, fog distances, wind and glare."""
    colours = bytearray()
    for kind in (WEATHER_SKY, WEATHER_FOG, ((0, 0, 0),) * 4, WEATHER_AMBIENT, WEATHER_SUNLIGHT, WEATHER_SUN,
                 ((0, 0, 0),) * 4, ((0, 0, 0),) * 4, ((0, 0, 0),) * 4, ((0, 0, 0),) * 4):
        for colour in kind:
            colours += bytes(colour) + b"\0"
    assert len(colours) == 160
    fog = struct.pack("<6f", *WEATHER_FOG_DAY, *WEATHER_FOG_NIGHT, 1.0, 1.0)
    # wind speed, cloud speeds, transition delta, sun glare, sun damage, precipitation and thunder fade, thunder
    # frequency, classification (0: none) and the colour of lightning
    data = struct.pack("<15B", WEATHER_WIND, 0, 0, 4, WEATHER_GLARE, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255)
    # Like NVWastelandClear, three layers of clouds are the blank texture and the clouds are in the fourth
    return record(b"WTHR", form_id, [zstr(b"EDID", name), zstr(b"DNAM", "Sky\\Alpha.dds"),
                                     zstr(b"CNAM", "sky\\alpha.dds"), zstr(b"ANAM", "sky/alpha.dds"),
                                     zstr(b"BNAM", CLOUD_TEXTURE), sub(b"NAM0", bytes(colours)),
                                     sub(b"FNAM", fog), sub(b"DATA", data)])


def cloud_texture(size=4):
    """The image of the clouds, a DDS file of uncompressed 32 bit pixels, white with CLOUD_ALPHA as alpha."""
    return dds_texture((255, 255, 255, CLOUD_ALPHA), size)


def ground_texture(size=4):
    """The image of the ground, a DDS file of one colour (opaque)."""
    return dds_texture((*GROUND_COLOUR, 255), size)


def dds_texture(colour, size):
    """A DDS file of uncompressed 32 bit pixels of one colour (red, green, blue, alpha), whose bytes are in the order of
    the masks below, blue first."""
    red, green, blue, alpha = colour
    pixels = bytes((blue, green, red, alpha)) * (size * size)
    # magic, header size, flags (caps, height, width, pitch, pixel format), height, width, pitch, depth, mip maps, 11
    # reserved, then the pixel format: size, flags (alpha and rgb), four character code, bits, the masks of red, green,
    # blue and alpha, then the caps (texture) and 4 more words
    header = struct.pack("<4sIIIIIII11I", b"DDS ", 124, 0x100F, size, size, size * 4, 0, 0, *(0,) * 11)
    pixel_format = struct.pack("<IIIIIIII", 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    return header + pixel_format + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + pixels


def water_type():
    """A WATR record with its settings in a DNAM of 196 bytes: the sun power, the reflectivity and the fresnel amount
    after 16 bytes, then the colours (red, green, blue and an unused byte) of shallow water, deep water and the
    reflection, the rest are zeros."""
    visual = (bytes(16) + struct.pack("<fff", 826.0, WATER_REFLECTIVITY, 0.75) + bytes(4) + struct.pack("<ff", 0.0, 0.0)
              + bytes(WATER_SHALLOW) + b"\xff" + bytes(WATER_DEEP) + b"\xff" + bytes((10, 10, 10)) + b"\xff")
    visual += bytes(196 - len(visual))
    return record(b"WATR", WATER_TYPE_ID, [zstr(b"EDID", WATER_TYPE_NAME), sub(b"ANAM", bytes((WATER_OPACITY,))),
                                           sub(b"FNAM", b"\x02"), sub(b"DATA", bytes(2)), sub(b"DNAM", visual)])


def global_variable(form_id=GLOBAL_ID, name="OFTestWeatherGlobal", kind=b"f", value=0.0):
    """A GLOB record, by default a float that is 0 (a short or a long is `s` or `l`, and its value is a float too)."""
    return record(b"GLOB", form_id, [zstr(b"EDID", name), sub(b"FNAM", kind), sub(b"FLTV", struct.pack("<f", value))])


def quest_script():
    """The script of quest A (an SCPT record): on every delay of the quest it adds one to `ticks`, and on the
    QUEST_TICKS-th it sets stage 10 of the quest."""
    script = fallout_script.Script(1)
    ticks = script.variable("ticks")
    quest = script.form(QUEST_A_ID)
    script.add(
        fallout_script.statement(fallout_script.SCRIPT_NAME),
        fallout_script.block(fallout_script.GAME_MODE, b"".join([
            fallout_script.set_to(fallout_script.variable(ticks),
                                  fallout_script.variable(ticks) + fallout_script.number("1")
                                  + fallout_script.operator("+")),
            fallout_script.branch(fallout_script.IF, fallout_script.variable(ticks)
                                  + fallout_script.number(str(QUEST_TICKS)) + fallout_script.operator("==")),
            fallout_script.call(fallout_script.SET_STAGE, fallout_script.form_argument(quest),
                                fallout_script.int_argument(QUEST_A_STAGE)),
            fallout_script.statement(fallout_script.END_IF)])))
    return record(b"SCPT", QUEST_SCRIPT_ID, [zstr(b"EDID", "OFTestQuestScript"), *fallout_script.sub_records(script)])


def message():
    """The MESG record that the scripted object shows: a text and no buttons."""
    return record(b"MESG", MESSAGE_ID, [zstr(b"EDID", "OFTestMessage"), zstr(b"DESC", MESSAGE_TEXT),
                                        sub(b"DNAM", struct.pack("<I", 0)), sub(b"TNAM", struct.pack("<f", 2.0))])


def button_message():
    """The MESG record of a message box with three buttons, the second of which has a condition that nobody meets. The
    flag 1 of DNAM makes it a box and not a note on the screen."""
    subs = [zstr(b"EDID", "OFTestButtonMessage"), zstr(b"DESC", BUTTON_MESSAGE_TEXT), zstr(b"FULL", BUTTON_MESSAGE_TITLE),
            sub(b"DNAM", struct.pack("<I", 1)), sub(b"TNAM", struct.pack("<I", 0))]
    for index, text in enumerate(BUTTON_TEXTS):
        subs.append(zstr(b"ITXT", text))
        if index == 1:
            subs.append(condition(GET_IS_VOICE_TYPE_FUNCTION, OTHER_VOICE_ID, compare=0x00, value=1.0))
    return record(b"MESG", BUTTON_MESSAGE_ID, subs)


def button_script():
    """The script of the talker (an SCPT record of type 0): every frame, until a button has been read, it sets the
    global to the number of the button plus one (GetButtonPressed tells -1 for none, and a button is told once), and
    each time the talker has said what it was told to say it adds one to another global (Begin SayToDone Topic). A
    block for another topic adds to a global that must stay 0, and the blocks of MenuMode add to the globals of the
    menus they name (the conversation and the message box are open at some time, the love tester never is)."""
    sf = fallout_script
    script = sf.Script(0)
    button = script.form(DIALOGUE_GLOBALS["button"][0])

    def count(name):
        target = sf.global_target(script.form(DIALOGUE_GLOBALS[name][0]))
        return sf.set_to(target, target + sf.number("1") + sf.operator("+"))

    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.GAME_MODE, b"".join([
            sf.branch(sf.IF, sf.global_target(button) + sf.number("0") + sf.operator("==")),
            sf.set_to(sf.global_target(button),
                      sf.call_token(sf.GET_BUTTON_PRESSED) + sf.number("1") + sf.operator("+")),
            sf.statement(sf.END_IF)])),
        sf.block(sf.SAY_TO_DONE, b"".join([
            count("said"),
            sf.branch(sf.IF, sf.global_target(script.form(DIALOGUE_GLOBALS["said_end"][0])) + sf.number("1")
                      + sf.operator("==")),
            count("ordered"),
            sf.statement(sf.END_IF)]), names=sf.form_argument(script.form(SAID_TOPIC_ID))),
        sf.block(sf.SAY_TO_DONE, count("wrong"), names=sf.form_argument(script.form(ADDED_TOPIC_ID))),
        *(sf.block(sf.MENU_MODE_BLOCK, count(name), names=sf.int_argument(number))
          for name, number in MENU_NUMBERS.items()))
    return record(b"SCPT", BUTTON_SCRIPT_ID, [zstr(b"EDID", "OFTestButtonScript"), *sf.sub_records(script)])


def trait_perk(form_id, name, title, description, trait):
    """A PERK record with no entries: the flag of a trait (and a name and a text to show) or of a perk that is none."""
    return record(b"PERK", form_id, [zstr(b"EDID", name), zstr(b"FULL", title), zstr(b"DESC", description),
                                     sub(b"DATA", bytes([1 if trait else 0, 0, 1, 1, 0]))])


def actor_value_info(form_id, editor_id, title, description):
    return record(b"AVIF", form_id, [zstr(b"EDID", editor_id), zstr(b"FULL", title), zstr(b"DESC", description)])


def chargen_script():
    """The script of the quest that makes the character (an SCPT record of type 1): when the test sets `go` it opens the
    menus for the race, the name (as the scripts of the games do, one after the other), SPECIAL with the tagged skills
    and the traits; once no menu is open (the game
    stands still while one is, so this only happens after the player is done with all of them) it reads what the player
    made into the globals."""
    sf = fallout_script
    script = sf.Script(1)
    player = script.form(PLAYER_REFERENCE)
    race_two, trait_a, trait_b, other = (script.form(form_id) for form_id in (
        RACE_TWO_ID, TRAIT_A_ID, TRAIT_B_ID, NOT_A_TRAIT_ID))
    names = {name: script.form(form_id) for name, (form_id, _) in CHARGEN_GLOBALS.items()}

    def target(name):
        return sf.global_target(names[name])

    def read(name, code, *arguments, reference=0):
        return sf.set_to(target(name), sf.call_token(code, *arguments, reference=reference))

    attributes = {"strength": AV_STRENGTH, "perception": AV_PERCEPTION, "endurance": AV_ENDURANCE,
                  "charisma": AV_CHARISMA, "intelligence": AV_INTELLIGENCE, "agility": AV_AGILITY, "luck": AV_LUCK,
                  "barter": AV_BARTER, "guns": AV_GUNS, "survival": AV_SURVIVAL}
    start = (
        sf.global_target(names["go"]) + sf.number("1") + sf.operator("==")
        + sf.global_target(names["state"]) + sf.number("0") + sf.operator("==") + sf.operator("&&"))
    done = (sf.global_target(names["state"]) + sf.number("1") + sf.operator("==")
            + sf.call_token(sf.MENU_MODE) + sf.number("0") + sf.operator("==") + sf.operator("&&"))
    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.GAME_MODE, b"".join([
            sf.branch(sf.IF, start),
            sf.call(sf.SET_IN_CHARGEN, sf.int_argument(1)),
            sf.call(sf.SHOW_RACE_MENU),
            sf.call(sf.GET_PLAYER_NAME),
            sf.call(sf.SET_SPECIAL_POINTS, sf.int_argument(40)),
            sf.call(sf.SET_TAG_SKILLS, sf.int_argument(3)),
            sf.call(sf.SHOW_TRAIT_MENU),
            sf.set_to(target("state"), sf.number("1")),
            sf.statement(sf.END_IF),
            sf.branch(sf.IF, done),
            read("during", sf.GET_IN_CHARGEN),
            sf.call(sf.SET_IN_CHARGEN, sf.int_argument(0)),
            read("after", sf.GET_IN_CHARGEN),
            read("female", sf.GET_PC_IS_SEX, sf.int_argument(1)),
            read("race_two", sf.GET_PC_IS_RACE, sf.form_argument(race_two)),
            *(read(name, sf.GET_ACTOR_VALUE, sf.u16(number), reference=player) for name, number in attributes.items()),
            read("trait_a", sf.HAS_PERK, sf.form_argument(trait_a), reference=player),
            read("trait_b", sf.HAS_PERK, sf.form_argument(trait_b), reference=player),
            read("other", sf.HAS_PERK, sf.form_argument(other), reference=player),
            sf.set_to(target("state"), sf.number("2")),
            sf.statement(sf.END_IF)])))
    return record(b"SCPT", CHARGEN_SCRIPT_ID, [zstr(b"EDID", "OFTestChargenScript"), *sf.sub_records(script)])


def chargen_quest():
    return quest(CHARGEN_QUEST_ID, "OFTestChargenQuest", "OpenFallout Test Chargen", QUEST_FLAG_START_GAME, [], script=CHARGEN_SCRIPT_ID)


def new_game_script():
    """The script of the quest that starts a new game (an SCPT record of type 1): `Begin MenuMode`, no number, with
    `if GetStage CG00 < 5` then `SetStage CG00 0`, as the script of CG00 starts the game of Fallout 3; and `Begin
    GameMode`, which at stage 5 shows the message box and sets stage 6, and at stage 6 sets stage 10 once the first
    button has been picked (as the script of CG00 shows its box for the sex of the player and reads the answer)."""
    sf = fallout_script
    script = sf.Script(1)
    quest = script.form(NEW_GAME_QUEST_ID)
    message = script.form(NEW_GAME_MESSAGE_ID)

    def stage_is(stage):
        return (sf.call_token(sf.GET_STAGE, sf.form_argument(quest)) + sf.number(str(stage)) + sf.operator("=="))

    before_box = (sf.call_token(sf.GET_STAGE, sf.form_argument(quest)) + sf.number(str(NEW_GAME_BOX_STAGE))
                  + sf.operator("<"))
    picked_first = sf.call_token(sf.GET_BUTTON_PRESSED) + sf.number("0") + sf.operator("==")
    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.MENU_MODE_BLOCK, b"".join([
            sf.branch(sf.IF, before_box),
            sf.call(sf.SET_STAGE, sf.form_argument(quest), sf.int_argument(NEW_GAME_FIRST_STAGE)),
            sf.statement(sf.END_IF)])),
        sf.block(sf.GAME_MODE, b"".join([
            sf.branch(sf.IF, stage_is(NEW_GAME_BOX_STAGE)),
            sf.call(sf.SHOW_MESSAGE, sf.form_argument(message)),
            sf.call(sf.SET_STAGE, sf.form_argument(quest), sf.int_argument(NEW_GAME_WAIT_STAGE)),
            sf.statement(sf.END_IF),
            sf.branch(sf.IF, stage_is(NEW_GAME_WAIT_STAGE)),
            sf.branch(sf.IF, picked_first),
            sf.call(sf.SET_STAGE, sf.form_argument(quest), sf.int_argument(NEW_GAME_LAST_STAGE)),
            sf.statement(sf.END_IF),
            sf.statement(sf.END_IF)])))
    return record(b"SCPT", NEW_GAME_SCRIPT_ID, [zstr(b"EDID", "OFTestNewGameScript"), *sf.sub_records(script)])


def new_game_message():
    """The MESG record of the box that the quest of the new game shows: two buttons, no condition."""
    subs = [zstr(b"EDID", "OFTestNewGameMessage"), zstr(b"DESC", "Pick the first button."),
            sub(b"DNAM", struct.pack("<I", 1)), sub(b"TNAM", struct.pack("<I", 0))]
    subs += [zstr(b"ITXT", text) for text in NEW_GAME_BUTTONS]
    return record(b"MESG", NEW_GAME_MESSAGE_ID, subs)


def new_game_quest():
    """CG00: stage 0 takes five of the controls of the player (movement, the Pip-Boy, fighting, the view and looking),
    moves the player to the marker and sets stage 5; stage 10 takes fighting on its own, then gives all the controls back
    and opens the screen for the name (the quest is not Start Game Enabled, so only a new game starts it)."""
    sf = fallout_script
    # the references of the result script, in the order of their entries: the player, the marker, the quest
    first = [sf.call(sf.DISABLE_PLAYER_CONTROLS, *[sf.int_argument(1)] * 5),
             sf.call_on(1, sf.MOVE_TO_MARKER, sf.form_argument(2)),
             sf.call(sf.SET_STAGE, sf.form_argument(3), sf.int_argument(NEW_GAME_BOX_STAGE))]
    # taking fighting alone puts away a weapon that the player has drawn, then everything is given back
    last = [sf.call(sf.DISABLE_PLAYER_CONTROLS, sf.int_argument(0), sf.int_argument(0), sf.int_argument(1)),
            sf.call_counted(sf.ENABLE_PLAYER_CONTROLS), sf.call(sf.GET_PLAYER_NAME)]
    return quest(NEW_GAME_QUEST_ID, NEW_GAME_QUEST_NAME, "OpenFallout Test New Game", 0,
                 [(NEW_GAME_FIRST_STAGE, "The game starts", 0, [], first,
                   [PLAYER_REFERENCE, NEW_GAME_PLACE_REF_ID, NEW_GAME_QUEST_ID]),
                  (NEW_GAME_BOX_STAGE, "The game asks", 0, [], [], []),
                  (NEW_GAME_WAIT_STAGE, "The game waits", 0, [], [], []),
                  (NEW_GAME_LAST_STAGE, "The game goes on", 0, [], last, [])],
                 script=NEW_GAME_SCRIPT_ID)


def form_lists():
    """A list of forms for each kind of cell (FLST): the marker that the cubes are moved to."""
    return [record(b"FLST", form_id, [zstr(b"EDID", "OFTestList" + kind.title()),
                                       sub(b"LNAM", struct.pack("<I", TARGET_MARKER_ID))])
            for kind, form_id in LIST_IDS.items()]


def object_script(kind):
    """The script of the scripted object (an SCPT record of type 0): on load it counts the loads, shows the message,
    disables the cube next to it, gives the scripted walker its package, changes the items and factions of the idler
    and kills it, reads the rank of the player in a faction and the suits of a creature placed as a levelled list; every
    frame it counts the frames, measures its distance to the player and keeps whether the cube is disabled; when activated it counts the activations by the player and, the
    second time, lets the object do what it does by default (Activate)."""
    sf = fallout_script
    script = sf.Script(0)
    index = ("interior", "exterior").index(kind)
    victim = script.form(VICTIM_REF_IDS[kind])
    player = script.form(PLAYER_REFERENCE)
    walker = script.form(GATE_REF_IDS["scripted"][index])
    walker_package = script.form(SCRIPT_PACKAGE_IDS[kind])
    greeting = script.form(MESSAGE_ID)
    loads, frames, activated, distance, disabled = (script.form(OBJECT_GLOBALS[name][0])
                                                    for name in ("loads", "frames", "activated", "distance", "victim"))
    items, rank, joined, dead, player_rank, listed_items = (
        script.form(OBJECT_GLOBALS[name][0]) for name in ("items", "rank", "joined", "dead", "player_rank", "listed"))
    listed = script.form(LISTED_REF_IDS[kind])
    idler = script.form(PACKAGE_REF_IDS["idler"][index])
    suit = script.form(SUIT_ID)
    first_faction, second_faction = (script.form(form_id) for form_id in FACTION_IDS)
    mover, move_target, far_marker = (script.form(form_id) for form_id in (
        MOVER_REF_IDS[kind], MOVE_TARGET_REF_IDS[kind], FAR_MARKER_REF_IDS[kind]))
    mover_x, mover_y, mover_angle, move_now, far_x, hop, mover_near = (
        script.form(MOVE_GLOBALS[name][0]) for name in ("x", "y", "angle", "now", "far", "hop", "near"))
    hop_x = int((EXTERIOR_CELL_SIZE / 2 if kind == "exterior" else 0.0) + HOP_X)
    far_mover = script.form(FAR_MOVER_REF_IDS[kind])
    traveller = script.form(TRAVELLER_REF_IDS[kind])
    setter = script.form(SETTER_REF_IDS[kind])
    crosser, dry_cell, crossed = (script.form(form_id) for form_id in (
        CROSSER_REF_IDS[kind], DRY_CELL_ID, MOVE_GLOBALS["crossed"][0]))
    other_kind = "exterior" if kind == "interior" else "interior"
    sleeper, sleeper_target = (script.form(SLEEPER_REF_IDS[other_kind]), script.form(SLEEPER_TARGET_REF_IDS[other_kind]))
    locked, locker, doomed = (script.form(ids[kind]) for ids in (LOCKED_REF_IDS, LOCKER_REF_IDS, DOOMED_REF_IDS))
    form_list, cube = script.form(LIST_IDS[kind]), script.form(CUBE_ID)
    state = {name: script.form(form_id) for name, (form_id, _) in STATE_GLOBALS.items()}
    perk, note = script.form(PERK_ID), script.form(NOTE_ID)
    progress = {name: script.form(form_id) for name, (form_id, _) in PROGRESS_GLOBALS.items()}
    quest_a = script.form(QUEST_A_ID)
    queries = {name: script.form(form_id) for name, (form_id, _) in WORLD_QUERY_GLOBALS.items()}
    world_space = script.form(WORLD_ID)
    game_hour = script.form(GAME_HOUR_ID)
    clock = {name: script.form(form_id) for name, (form_id, _) in CLOCK_GLOBALS.items()}
    values = {name: script.form(form_id) for name, (form_id, _) in ACTOR_VALUE_GLOBALS.items()}
    person_ref = script.form(NPC_REF_ID if kind == "interior" else EXTERIOR_NPC_REF_ID)
    user_ref = script.form(CREATURE_USER_REF_ID if kind == "interior" else EXTERIOR_CREATURE_USER_REF_ID)
    # The actor values, on the locker cube (the player for karma): a number written bare in two bytes, then the amount
    actor_values = b"".join([
        sf.call_on(locker, sf.SET_ACTOR_VALUE, sf.u16(AV_VARIABLE), sf.int_argument(7)),
        sf.call_on(locker, sf.MOD_ACTOR_VALUE, sf.u16(AV_VARIABLE), sf.int_argument(3)),
        sf.call_on(locker, sf.DAMAGE_ACTOR_VALUE, sf.u16(AV_VARIABLE), sf.float_argument(4)),
        sf.set_to(sf.global_target(values["variable"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_VARIABLE), reference=locker)),
        sf.call_on(locker, sf.DAMAGE_ACTOR_VALUE, sf.u16(AV_HEALTH), sf.float_argument(25)),
        sf.set_to(sf.global_target(values["share"]), sf.call_token(sf.GET_HEALTH_PERCENTAGE, reference=locker)),
        sf.call_on(locker, sf.DAMAGE_ACTOR_VALUE, sf.u16(AV_HEALTH), sf.float_argument(75)),
        sf.set_to(sf.global_target(values["dead"]), sf.call_token(sf.GET_DEAD, reference=locker)),
        sf.set_to(sf.global_target(values["karma"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_KARMA), reference=player)),
        sf.set_to(sf.global_target(values["weapon"]), sf.call_token(sf.GET_WEAPON_HEALTH_PERC, reference=player)),
        # The test person starts with what its record says
        sf.set_to(sf.global_target(values["strength"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_STRENGTH), reference=person_ref)),
        sf.set_to(sf.global_target(values["luck"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_LUCK), reference=person_ref)),
        sf.set_to(sf.global_target(values["speed"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_SPEED), reference=person_ref)),
        sf.set_to(sf.global_target(values["person_karma"]),
                  sf.call_token(sf.GET_ACTOR_VALUE, sf.u16(AV_KARMA), reference=person_ref)),
        # IsDLCInstalled takes the name of a plugin, with or without the spaces and the extension
        sf.set_to(sf.global_target(values["dlc_there"]),
                  sf.call_token(sf.IS_DLC_INSTALLED, sf.string_argument(DLC_NAME_THERE))),
        sf.set_to(sf.global_target(values["dlc_missing"]),
                  sf.call_token(sf.IS_DLC_INSTALLED, sf.string_argument(DLC_NAME_MISSING))),
        # The level of the test person is the one of its record; the game begins on a Monday (the 1 of a week that
        # begins on a Sunday)
        sf.set_to(sf.global_target(values["person_level"]), sf.call_token(sf.GET_LEVEL, reference=person_ref)),
        sf.set_to(sf.global_target(values["day_of_week"]), sf.call_token(sf.GET_DAY_OF_WEEK)),
        # A creature whose template is a levelled list has the stats of the creature the list gives, and its level
        sf.set_to(sf.global_target(values["user_level"]), sf.call_token(sf.GET_LEVEL, reference=user_ref)),
    ])
    has_perk = sf.call_token(sf.HAS_PERK, sf.form_argument(perk), reference=player)
    has_note = sf.call_token(sf.GET_HAS_NOTE, sf.form_argument(note))

    def add_one(global_index):
        return sf.set_to(sf.global_target(global_index), sf.global_target(global_index) + sf.number("1")
                         + sf.operator("+"))

    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.ON_LOAD, add_one(loads) + sf.call(sf.SHOW_MESSAGE, sf.form_argument(greeting))
                 + sf.call_on(victim, sf.DISABLE)
                 + sf.call_on(walker, sf.ADD_SCRIPT_PACKAGE, sf.form_argument(walker_package))
                 + sf.call_on(walker, sf.EVALUATE_PACKAGE)
                 + sf.call_on(idler, sf.ADD_ITEM, sf.form_argument(suit), sf.int_argument(IDLER_ADDS))
                 + sf.call_on(idler, sf.REMOVE_ITEM, sf.form_argument(suit), sf.int_argument(IDLER_TAKES))
                 + sf.call_on(idler, sf.REMOVE_FROM_FACTION, sf.form_argument(second_faction))
                 + sf.call_on(idler, sf.KILL_ACTOR, counted=True)
                 + sf.call_on(mover, sf.MOVE_TO_MARKER, sf.form_argument(move_target),
                              *(sf.int_argument(offset) for offset in MOVE_OFFSET))
                 # Right after the move, which the world makes at the start of the next frame: the script sees the
                 # cube at the marker already (the offsets are in the x and y)
                 + sf.set_to(sf.global_target(mover_near),
                             sf.call_token(sf.GET_DISTANCE, sf.form_argument(move_target), reference=mover))
                 # An x in the cell next to this one (the world has not made the move yet): the cube is in that cell
                 # for the script already, when the cell is an exterior one
                 + sf.call_on(crosser, sf.SET_POS, sf.axis_argument("X"), sf.int_argument(CROSS_X))
                 + sf.set_to(sf.global_target(crossed),
                             sf.call_token(sf.GET_IN_CELL, sf.form_argument(dry_cell), reference=crosser))
                 + sf.call_on(far_mover, sf.MOVE_TO_MARKER, sf.form_argument(far_marker),
                              *(sf.int_argument(offset) for offset in FAR_OFFSET))
                 + sf.call_on(sleeper, sf.MOVE_TO_MARKER, sf.form_argument(sleeper_target),
                              *(sf.int_argument(offset) for offset in SLEEPER_OFFSET))
                 + sf.call_on(sleeper, sf.SET_ANGLE, sf.axis_argument("X"), sf.int_argument(SLEEPER_PITCH))
                 # What the record of the locked cube says, then the script opens it; the other cube is locked
                 + sf.set_to(sf.global_target(state["locked"]), sf.call_token(sf.GET_LOCKED, reference=locked))
                 + sf.set_to(sf.global_target(state["lock_level"]), sf.call_token(sf.GET_LOCK_LEVEL, reference=locked))
                 + sf.call_on(locked, sf.UNLOCK, counted=True)
                 + sf.call_on(locker, sf.LOCK, sf.int_argument(RELOCK_LEVEL))
                 + sf.call_on(doomed, sf.SET_DESTROYED, sf.int_argument(1))
                 + sf.call_on(doomed, sf.MARK_FOR_DELETE)
                 # The list has the marker by its record and gets the cube from the script
                 + sf.set_to(sf.global_target(state["list_before"]),
                             sf.call_token(sf.IS_IN_LIST, sf.form_argument(form_list), reference=locker))
                 + sf.call(sf.ADD_FORM_TO_FORM_LIST, sf.form_argument(form_list), sf.form_argument(cube))
                 # The quest script runs on the shorter delay from now on
                 + sf.call(sf.SET_QUEST_DELAY, sf.form_argument(quest_a), sf.float_argument(QUEST_DELAY))
                 + sf.call(sf.REWARD_XP, sf.int_argument(XP_REWARD))
                 + sf.call(sf.REWARD_KARMA, sf.int_argument(KARMA_REWARD))
                 # A perk is added twice and removed (the second argument of HasPerk is not a rank: asked for the
                 # teammates it says yes as well); a note is added; the locker cube is of the base form CUBE_ID and not
                 # of the base form PERK_ID
                 + sf.set_to(sf.global_target(progress["perk_before"]), has_perk)
                 + sf.call_on(player, sf.ADD_PERK, sf.form_argument(perk))
                 + sf.call_on(player, sf.ADD_PERK, sf.form_argument(perk))
                 + sf.set_to(sf.global_target(progress["perk_after"]), has_perk)
                 + sf.set_to(sf.global_target(progress["perk_teammates"]),
                             sf.call_token(sf.HAS_PERK, sf.form_argument(perk), sf.int_argument(1), reference=player))
                 + sf.call_on(player, sf.REMOVE_PERK, sf.form_argument(perk))
                 + sf.set_to(sf.global_target(progress["perk_removed"]), has_perk)
                 + sf.set_to(sf.global_target(progress["note_before"]), has_note)
                 + sf.call(sf.ADD_NOTE, sf.form_argument(note))
                 + sf.set_to(sf.global_target(progress["note_after"]), has_note)
                 + sf.set_to(sf.global_target(progress["is_id"]),
                             sf.call_token(sf.GET_IS_ID, sf.form_argument(cube), reference=locker))
                 + sf.set_to(sf.global_target(progress["is_not_id"]),
                             sf.call_token(sf.GET_IS_ID, sf.form_argument(perk), reference=locker))
                 # The record links the locker cube to the doomed one and the doomed one to nothing; both are in the
                 # same cell; the worldspace is the one of the exterior cell and none for the cell of a building
                 + sf.set_to(sf.global_target(queries["linked"]),
                             sf.call_token(sf.GET_LINKED_REF, reference=locker) + sf.reference_value(doomed)
                             + sf.operator("=="))
                 + sf.set_to(sf.global_target(queries["linked_none"]),
                             sf.call_token(sf.GET_LINKED_REF, reference=doomed))
                 + sf.set_to(sf.global_target(queries["same_cell"]),
                             sf.call_token(sf.GET_IN_SAME_CELL, sf.form_argument(doomed), reference=locker))
                 + sf.set_to(sf.global_target(queries["in_worldspace"]),
                             sf.call_token(sf.GET_IN_WORLDSPACE, sf.form_argument(world_space), reference=locker))
                 + sf.set_to(sf.global_target(clock["hour_command"]), sf.call_token(sf.GET_CURRENT_TIME))
                 + sf.set_to(sf.global_target(clock["hour_global"]), sf.global_target(game_hour))
                 + sf.set_to(sf.global_target(clock["global_value"]),
                             sf.call_token(sf.GET_GLOBAL_VALUE, sf.form_argument(game_hour)))
                 + sf.set_to(sf.global_target(queries["placed"]),
                             sf.call_token(sf.PLACE_AT_ME, sf.form_argument(cube), sf.int_argument(PLACE_COUNT),
                                           sf.float_argument(PLACE_DISTANCE), sf.int_argument(0), reference=locker))
                 + actor_values),
        sf.block(sf.GAME_MODE, b"".join([
            add_one(frames),
            # The scripted walker is on its way to its marker. Setting its height (where it already is) every frame
            # must leave what it does in between alone: it walks on to the marker
            sf.call_on(walker, sf.SET_POS, sf.axis_argument("Z"), sf.int_argument(0)),
            sf.set_to(sf.global_target(distance), sf.call_token(sf.GET_DISTANCE, sf.form_argument(player))),
            sf.set_to(sf.global_target(disabled), sf.call_token(sf.GET_DISABLED, reference=victim)),
            sf.set_to(sf.global_target(items), sf.call_token(sf.GET_ITEM_COUNT, sf.form_argument(suit),
                                                             reference=idler)),
            sf.set_to(sf.global_target(rank), sf.call_token(sf.GET_FACTION_RANK, sf.form_argument(first_faction),
                                                            reference=idler)),
            sf.set_to(sf.global_target(joined), sf.call_token(sf.GET_IN_FACTION, sf.form_argument(second_faction),
                                                              reference=idler)),
            sf.set_to(sf.global_target(dead), sf.call_token(sf.GET_DEAD, reference=idler)),
            # The walker follows the package that the script gave it, once it has looked at its packages
            sf.set_to(sf.global_target(values["current_package"]),
                      sf.call_token(sf.GET_IS_CURRENT_PACKAGE, sf.form_argument(walker_package), reference=walker)),
            sf.set_to(sf.global_target(player_rank), sf.call_token(sf.GET_FACTION_RANK, sf.form_argument(first_faction),
                                                                   reference=player)),
            sf.set_to(sf.global_target(listed_items), sf.call_token(sf.GET_ITEM_COUNT, sf.form_argument(suit),
                                                                    reference=listed)),
            sf.set_to(sf.global_target(mover_x), sf.call_token(sf.GET_POS, sf.axis_argument("X"), reference=mover)),
            sf.set_to(sf.global_target(mover_y), sf.call_token(sf.GET_POS, sf.axis_argument("Y"), reference=mover)),
            sf.set_to(sf.global_target(mover_angle),
                      sf.call_token(sf.GET_ANGLE, sf.axis_argument("Z"), reference=mover)),
            sf.set_to(sf.global_target(far_x), sf.call_token(sf.GET_POS, sf.axis_argument("X"), reference=far_mover)),
            sf.set_to(sf.global_target(state["unlocked"]), sf.call_token(sf.GET_LOCKED, reference=locked)),
            sf.set_to(sf.global_target(state["relocked"]), sf.call_token(sf.GET_LOCKED, reference=locker)),
            sf.set_to(sf.global_target(state["relock_level"]), sf.call_token(sf.GET_LOCK_LEVEL, reference=locker)),
            sf.set_to(sf.global_target(state["destroyed"]), sf.call_token(sf.GET_DESTROYED, reference=doomed)),
            sf.set_to(sf.global_target(state["doomed_disabled"]), sf.call_token(sf.GET_DISABLED, reference=doomed)),
            sf.set_to(sf.global_target(state["in_list"]),
                      sf.call_token(sf.IS_IN_LIST, sf.form_argument(form_list), reference=move_target)),
            sf.set_to(sf.global_target(state["list_after"]),
                      sf.call_token(sf.IS_IN_LIST, sf.form_argument(form_list), reference=locker)),
            # The test asks for the move of the player with a global; the cell of the start is left behind
            sf.branch(sf.IF, sf.global_target(hop) + sf.number("1") + sf.operator("==")),
            sf.set_to(sf.global_target(hop), sf.number("0")),
            sf.call_on(player, sf.SET_POS, sf.axis_argument("X"), sf.int_argument(hop_x)),
            sf.call_on(setter, sf.SET_ANGLE, sf.axis_argument("Z"), sf.int_argument(SETTER_TURN)),
            sf.statement(sf.END_IF),
            sf.branch(sf.IF, sf.global_target(move_now) + sf.number("1") + sf.operator("==")),
            sf.set_to(sf.global_target(move_now), sf.number("0")),
            sf.call_on(mover, sf.SET_POS, sf.axis_argument("Z"), sf.int_argument(MOVE_HEIGHT)),
            sf.call_on(traveller, sf.MOVE_TO_MARKER, sf.form_argument(far_marker)),
            sf.call_on(player, sf.MOVE_TO_MARKER, sf.form_argument(far_marker)),
            sf.statement(sf.END_IF)])),
        # The block replaces what the object does when it is activated; the second activation asks for that
        sf.block(sf.ON_ACTIVATE, b"".join([
            sf.branch(sf.IF, sf.call_token(sf.IS_ACTION_REF, sf.form_argument(player))),
            add_one(activated),
            sf.branch(sf.IF, sf.global_target(activated) + sf.number("2") + sf.operator("==")),
            sf.call_counted(sf.ACTIVATE),
            sf.statement(sf.END_IF),
            sf.statement(sf.END_IF)])))
    return record(b"SCPT", OBJECT_SCRIPT_IDS[kind], [zstr(b"EDID", "OFTestObjectScript" + kind.title()),
                                                     *fallout_script.sub_records(script)])


def scripted_object(kind):
    """The activator with the script (an ACTI record with no model: nothing to see)."""
    return record(b"ACTI", SCRIPTED_IDS[kind], [zstr(b"EDID", "OFTestScripted" + kind.title()),
                                                sub(b"SCRI", struct.pack("<I", OBJECT_SCRIPT_IDS[kind]))])


def object_refs(kind, origin):
    """The references of the scripted object and of the cube its script disables, around the origin of the cell."""
    x, y = origin
    return [refr(SCRIPTED_REF_IDS[kind], SCRIPTED_IDS[kind], (x + SCRIPTED_PLACE[0], y + SCRIPTED_PLACE[1], 0.0)),
            refr(VICTIM_REF_IDS[kind], CUBE_ID, (x + VICTIM_PLACE[0], y + VICTIM_PLACE[1], CUBE / 2)),
            refr(MOVER_REF_IDS[kind], CUBE_ID, (x + MOVER_PLACE[0], y + MOVER_PLACE[1], CUBE / 2)),
            refr(FAR_MOVER_REF_IDS[kind], CUBE_ID, (x + FAR_MOVER_PLACE[0], y + FAR_MOVER_PLACE[1], CUBE / 2)),
            refr(SLEEPER_REF_IDS[kind], CUBE_ID, (x + SLEEPER_PLACE[0], y + SLEEPER_PLACE[1], CUBE / 2)),
            refr(SLEEPER_TARGET_REF_IDS[kind], TARGET_MARKER_ID,
                 (x + SLEEPER_TARGET_PLACE[0], y + SLEEPER_TARGET_PLACE[1], 0.0)),
            refr(MOVE_TARGET_REF_IDS[kind], TARGET_MARKER_ID, (x + MOVE_TARGET_PLACE[0], y + MOVE_TARGET_PLACE[1], 0.0),
                 rotation=(0.0, 0.0, TARGET_YAW)),
            refr(SETTER_REF_IDS[kind], CUBE_ID, (x + SETTER_PLACE[0], y + SETTER_PLACE[1], CUBE / 2)),
            refr(TRAVELLER_REF_IDS[kind], CUBE_ID, (x + TRAVELLER_PLACE[0], y + TRAVELLER_PLACE[1], CUBE / 2)),
            refr(CROSSER_REF_IDS[kind], CUBE_ID, (x + CROSSER_PLACE[0], y + CROSSER_PLACE[1], CUBE / 2)),
            refr(LOCKED_REF_IDS[kind], CUBE_ID, (x + LOCKED_PLACE[0], y + LOCKED_PLACE[1], CUBE / 2),
                 lock_level=LOCK_LEVEL),
            refr(LOCKER_REF_IDS[kind], CUBE_ID, (x + LOCKER_PLACE[0], y + LOCKER_PLACE[1], CUBE / 2),
                 linked=DOOMED_REF_IDS[kind]),
            refr(DOOMED_REF_IDS[kind], CUBE_ID, (x + DOOMED_PLACE[0], y + DOOMED_PLACE[1], CUBE / 2))]


def trigger_script():
    """The script of the trigger (an SCPT record of type 0): OnTriggerEnter for the player counts the times the player
    comes in (and makes a person at the middle of the box, who comes in too, a pass later), OnTriggerEnter for anyone
    counts everybody that does, OnTrigger for the player counts the passes while the player is in (not the one in which
    the player came in), OnTriggerLeave for the player counts the times the player goes out."""
    sf = fallout_script
    script = sf.Script(0)
    player = script.form(PLAYER_REFERENCE)
    trigger = script.form(TRIGGER_REF_ID)
    person_base = script.form(NPC_ID)
    counts = {name: script.form(form_id) for name, (form_id, _) in TRIGGER_GLOBALS.items()}

    def add_one(global_index):
        return sf.set_to(sf.global_target(global_index), sf.global_target(global_index) + sf.number("1")
                         + sf.operator("+"))

    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.ON_TRIGGER_ENTER,
                 add_one(counts["enters"])
                 + sf.call_on(trigger, sf.PLACE_AT_ME, sf.form_argument(person_base), sf.int_argument(1),
                              sf.float_argument(0.0), sf.int_argument(0)),
                 names=sf.form_argument(player)),
        sf.block(sf.ON_TRIGGER_ENTER, add_one(counts["any"])),
        sf.block(sf.ON_TRIGGER, add_one(counts["stays"]), names=sf.form_argument(player)),
        sf.block(sf.ON_TRIGGER_LEAVE, add_one(counts["leaves"]), names=sf.form_argument(player)))
    return record(b"SCPT", TRIGGER_SCRIPT_ID, [zstr(b"EDID", "OFTestTriggerScript"), *sf.sub_records(script)])


def trigger_base():
    """The activator of the trigger: no model, the script above."""
    return record(b"ACTI", TRIGGER_BASE_ID, [zstr(b"EDID", "OFTestTrigger"),
                                             sub(b"SCRI", struct.pack("<I", TRIGGER_SCRIPT_ID))])


def trigger_reference():
    """The reference of the trigger: a box on the floor (the reference is at the middle of it, one half height up),
    turned a quarter about z."""
    x, y = TRIGGER_PLACE
    return refr(TRIGGER_REF_ID, TRIGGER_BASE_ID, (x, y, TRIGGER_HALF[2]), rotation=(0.0, 0.0, TRIGGER_YAW),
                primitive=(TRIGGER_HALF, 1))


def stage_script(statements, references):
    """The sub-records of the script that runs when a stage is set: a list of statements with no block."""
    script = fallout_script.Script(1)
    for form_id in references:
        script.form(form_id)
    script.add(*statements)
    return fallout_script.sub_records(script)


def condition(function, param1=0, param2=0, compare=0x00, value=0.0, flags=0):
    """A CTDA sub-record as Fallout 3 and New Vegas write it, 28 bytes. `function` is the number the games give the
    function (the code of its command less 0x1000), `compare` one of the six comparisons (0x00 equal, 0x20 not equal,
    0x40 greater ...), and `flags` 0x01 for OR with the next condition."""
    return sub(b"CTDA", struct.pack("<BxxxfIIIII", compare | flags, value, function, param1, param2, 0, 0))


def quest(form_id, name, title, flags, entries, script=None, objective=None, delay=QUEST_DELAY):
    """A QUST record. Each entry is (stage, text, stage flags, conditions, result script statements, forms the result
    script names): the log entries of a stage follow each other, and the conditions say which of them applies."""
    subs = [zstr(b"EDID", name), zstr(b"FULL", title)]
    if script is not None:
        subs.append(sub(b"SCRI", struct.pack("<I", script)))
    subs.append(sub(b"DATA", struct.pack("<BBHf", flags, 0, 0, delay)))
    stage = None
    for index, text, stage_flags, conditions, result, references in entries:
        if index != stage:
            subs.append(sub(b"INDX", struct.pack("<H", index)))
            stage = index
        subs += [sub(b"QSDT", bytes([stage_flags])), *conditions, zstr(b"CNAM", text)]
        subs += stage_script(result, references)
    if objective is not None:
        subs += [sub(b"QOBJ", struct.pack("<i", objective[0])), zstr(b"NNAM", objective[1])]
    return record(b"QUST", form_id, subs)


def quests():
    """Quest A, quest B and quest C, in the order they are in the file."""
    sf = fallout_script
    # Stage 10 of A: set stage 20 of B, show its objective and add COUNTER_ADDS to the global
    a_result = [
        sf.call(sf.SET_STAGE, sf.form_argument(1), sf.int_argument(QUEST_B_STAGE)),
        sf.call(sf.SET_OBJECTIVE_DISPLAYED, sf.form_argument(1), sf.int_argument(QUEST_OBJECTIVE),
                sf.int_argument(1)),
        sf.set_to(sf.global_target(2), sf.global_target(2) + sf.number(str(COUNTER_ADDS)) + sf.operator("+"))]
    # Stage 20 of B has two log entries, which the conditions tell apart: the first applies when quest A is at its stage
    # 10 (it is, as the script of A set it before this) and completes the quest and the objective. The second applies
    # when quest A is at stage 99, which it never is: its text must stay out of the journal and its result script, which
    # adds a thousand to the global, must not run.
    b_result = [sf.call(sf.SET_OBJECTIVE_COMPLETED, sf.form_argument(1), sf.int_argument(QUEST_OBJECTIVE),
                        sf.int_argument(1))]
    never_result = [sf.set_to(sf.global_target(1), sf.global_target(1) + sf.number(str(COUNTER_NEVER_ADDS))
                              + sf.operator("+"))]
    a_at_ten = condition(GET_STAGE_FUNCTION, QUEST_A_ID, compare=0x00, value=float(QUEST_A_STAGE))
    a_at_ninety_nine = condition(GET_STAGE_FUNCTION, QUEST_A_ID, compare=0x00, value=99.0)
    # Stage of C: SetPos on x and then on y of the cube of each kind of cell (the one in the cell that is not loaded is
    # sent where its cell will put it when it is loaded)
    c_result = []
    for index, kind in enumerate(("interior", "exterior"), 1):
        origin = EXTERIOR_CELL_SIZE / 2 if kind == "exterior" else 0.0
        c_result += [sf.call_on(index, sf.SET_POS, sf.axis_argument("X"), sf.int_argument(int(origin + SETTER_SET[0]))),
                     sf.call_on(index, sf.SET_POS, sf.axis_argument("Y"), sf.int_argument(int(origin + SETTER_SET[1])))]
    return [
        quest(QUEST_A_ID, QUEST_A_NAME, "OpenFallout Test Quest A", QUEST_FLAG_START_GAME,
              [(QUEST_A_STAGE, QUEST_A_TEXT, 0, [], a_result, [QUEST_B_ID, COUNTER_ID])], script=QUEST_SCRIPT_ID,
              delay=QUEST_RECORD_DELAY),
        quest(QUEST_B_ID, QUEST_B_NAME, "OpenFallout Test Quest B", 0,
              [(QUEST_B_STAGE, QUEST_B_TEXT, STAGE_FLAG_COMPLETE_QUEST, [a_at_ten], b_result, [QUEST_B_ID]),
               (QUEST_B_STAGE, QUEST_NEVER_TEXT, 0, [a_at_ninety_nine], never_result, [COUNTER_ID])],
              objective=(QUEST_OBJECTIVE, QUEST_OBJECTIVE_TEXT)),
        # The quest that opens the gate of the packages: it has the one stage, which the test sets from Lua
        quest(QUEST_C_ID, QUEST_C_NAME, "OpenFallout Test Quest C", 0,
              [(QUEST_C_STAGE, QUEST_C_TEXT, 0, [], c_result, [SETTER_REF_IDS["interior"], SETTER_REF_IDS["exterior"]])]),
    ]
def climate():
    """A CLMT record that lists the weather and one that needs a global that is 0, with the times of sunrise and
    sunset."""
    weathers = (struct.pack("<IiI", WEATHER_ID, WEATHER_CHANCE, 0)
                + struct.pack("<IiI", CONDITIONAL_WEATHER_ID, CONDITIONAL_WEATHER_CHANCE, GLOBAL_ID))
    return record(b"CLMT", CLIMATE_ID, [zstr(b"EDID", "OFTestClimate"), sub(b"WLST", weathers),
                                        sub(b"TNAM", struct.pack("<6B", 36, 42, 108, 114, 0, 0))])


def land(form_id=EXTERIOR_LAND_ID, height=0.0, textured=False):
    """A LAND record: a flat terrain at the height over the whole cell, 33 by 33 vertices with normals pointing up, and
    with the base texture of GROUND_TEXTURE on each quadrant when `textured` (otherwise none, and the engine uses the
    default one of the game)."""
    flags = 0x1 | 0x2  # has normals and heights
    normals = bytes((0, 0, 127)) * (33 * 33)
    # The height of the first vertex in steps of 8 units, then for each vertex the difference to the one before it (a
    # signed byte, in steps of 8 units), then 3 bytes of padding.
    heights = struct.pack("<f", height / 8) + bytes(33 * 33) + bytes(3)
    base_textures = [sub(b"BTXT", struct.pack("<IBBH", LAND_TEXTURE_ID, quadrant, 0, 0)) for quadrant in range(4)]
    return record(b"LAND", form_id, [sub(b"DATA", struct.pack("<I", flags)), sub(b"VNML", normals),
                                     sub(b"VHGT", heights)] + (base_textures if textured else []))


def land_texture():
    """A LTEX record as Fallout has them: the filename of an icon (which is not the texture), the texture set that has
    the texture, the havok data and the specular exponent."""
    return record(b"LTEX", LAND_TEXTURE_ID, [zstr(b"EDID", "OFTestGround"), zstr(b"ICON", GROUND_ICON),
                                             sub(b"TNAM", struct.pack("<I", TEXTURE_SET_ID)),
                                             sub(b"HNAM", bytes((2, 30, 30))), sub(b"SNAM", bytes((30,)))])


def texture_set():
    """A TXST record with the diffuse texture of the ground, which a landscape texture names."""
    return record(b"TXST", TEXTURE_SET_ID, [zstr(b"EDID", "OFTestGroundSet"), zstr(b"TX00", GROUND_TEXTURE),
                                            sub(b"DNAM", struct.pack("<H", 0))])


def part_path(name):
    """The path of the mesh of a part of the person, as a record names it (under meshes)."""
    return "openfallout\\person_%s.osgt" % name


def part(index, name, path=None):
    """The INDX and MODL of a part of the body or head of a race."""
    return [sub(b"INDX", struct.pack("<I", index)), zstr(b"MODL", path or part_path(name))]


def race(form_id=RACE_ID, name=RACE_NAME):
    """A race with the data that the loader needs of one and the parts of a body that Fallout 3 gives it: the head (part
    0 of the 8 of the head data) and the upper body and the two hands (parts 0 to 2 of the 4 of the body data), for men.
    The skeleton is in the NPC_ record."""
    skills = struct.pack("<16B", *([0] * 16))
    data = skills + struct.pack("<4fI", 1.0, 1.0, 1.0, 1.0, 1)
    return record(b"RACE", form_id, [zstr(b"EDID", name), sub(b"DATA", data),
                                     sub(b"NAM0"), sub(b"MNAM"), *part(0, "head"), *part(1, "foot", FOOT_PART),
                                     sub(b"FGGS", struct.pack("<50f", RACE_FACE, *([0.0] * 49))), sub(b"FNAM"),
                                     sub(b"NAM1"), sub(b"MNAM"), *part(0, "upperbody"), *part(1, "lefthand"),
                                     *part(2, "righthand"), sub(b"FNAM")])


def hair():
    """A HAIR record with a model."""
    return record(b"HAIR", HAIR_ID, [zstr(b"EDID", "OFTestHair"), zstr(b"FULL", "OpenFallout Test Hair"),
                                     zstr(b"MODL", part_path("hair")), zstr(b"ICON", "openfallout\\hair.dds"),
                                     sub(b"DATA", b"\x00")])


def suit():
    """An ARMO record of Fallout 3: the model of a man in MODL, the biped slots and the general flags in BMDT, then the
    value, health and weight."""
    return record(b"ARMO", SUIT_ID, [zstr(b"EDID", "OFTestSuit"), zstr(b"FULL", "OpenFallout Test Suit"),
                                     zstr(b"MODL", part_path("suit")), sub(b"BMDT", struct.pack("<II", SUIT_SLOTS, 0)),
                                     sub(b"DATA", struct.pack("<IIf", 1, 1, 1.0))])


def faction(form_id, name):
    """A FACT record: a name and nothing else."""
    return record(b"FACT", form_id, [zstr(b"EDID", name), zstr(b"FULL", name)])


def person(form_id=NPC_ID, name=NPC_NAME, packages=(), dressed=True, factions=(), items=(), voice=None,
           title="OpenFallout Test Person", script=None):
    """A character of that race, with the skeleton of the placeholder, as a Fallout record names it (MODL), following
    the packages (in order of priority). One that is not `dressed` has no suit, hair or face."""
    # ACBS of Fallout 3 and New Vegas: flags, fatigue, barter gold, level, calc min, calc max, speed multiplier, karma,
    # disposition base, template flags (24 bytes); none of the flags is set, so the record has its own traits.
    acbs = struct.pack("<IHHhHHHfhH", 0, 0, 0, PERSON_LEVEL, 1, 1, PERSON_SPEED, PERSON_KARMA, 0, 0)
    subs = [zstr(b"EDID", name), zstr(b"FULL", title), zstr(b"MODL", SKELETON),
            sub(b"ACBS", acbs), sub(b"RNAM", struct.pack("<I", RACE_ID)),
            sub(b"DATA", struct.pack("<i7B", PERSON_HEALTH, *PERSON_ATTRIBUTES)),
            sub(b"DNAM", struct.pack("<14B14B", *PERSON_SKILLS, *([0] * 14)))]
    if dressed:
        subs += [sub(b"CNTO", struct.pack("<II", SUIT_ID, 1)), sub(b"HNAM", struct.pack("<I", HAIR_ID)),
                 sub(b"FGGS", struct.pack("<50f", PERSON_FACE, *([0.0] * 49)))]
    subs += [sub(b"CNTO", struct.pack("<II", item, count)) for item, count in items]
    subs += [sub(b"SNAM", struct.pack("<IBxxx", faction_id, rank)) for faction_id, rank in factions]
    subs += [sub(b"PKID", struct.pack("<I", package)) for package in packages]
    if voice is not None:
        subs.append(sub(b"VTCK", struct.pack("<I", voice)))
    if script is not None:
        subs.append(sub(b"SCRI", struct.pack("<I", script)))
    return record(b"NPC_", form_id, subs)


def voice_type(form_id, name):
    """A VTYP record: the voice a character speaks with (the folder of its voice files is named by the editor id)."""
    return record(b"VTYP", form_id, [zstr(b"EDID", name), sub(b"DNAM", b"\x00")])


def topic(form_id, name, title, kind, flags, priority=50.0):
    """A DIAL record of Fallout: the editor id, the words the player sees, the priority and the type and flags."""
    return record(b"DIAL", form_id, [zstr(b"EDID", name), zstr(b"FULL", title), sub(b"QSTI", struct.pack("<I", QUEST_A_ID)),
                                     sub(b"PNAM", struct.pack("<f", priority)), sub(b"DATA", bytes([kind, flags]))])


def response(form_id, text, conditions=(), flags=0, adds=(), prompt=None, begin=None, end=None, show=None, sound=0,
             say=None, more=()):
    """An INFO record with one line (and the lines of `more`, each a text and the sound record it names). The scripts
    are lists of statements (with the global they set): the one that runs when the response starts and the one that
    runs when it is over, which shows the message `show` when it is given and makes the talker say the topic `say` when
    that is given. `sound` is the sound record the response names."""
    subs = [sub(b"DATA", struct.pack("<BBH", 0, 0, flags)), sub(b"QSTI", struct.pack("<I", QUEST_A_ID))]
    subs += [sub(b"NAME", struct.pack("<I", added)) for added in adds]
    subs += [sub(b"TRDT", struct.pack("<IiIB3xIB3x", 0, 0, 0, 1, sound, 0)), zstr(b"NAM1", text), zstr(b"NAM2", "")]
    for number, (more_text, more_sound) in enumerate(more, 2):
        subs += [sub(b"TRDT", struct.pack("<IiIB3xIB3x", 0, 0, 0, number, more_sound, 0)), zstr(b"NAM1", more_text),
                 zstr(b"NAM2", "")]
    subs += list(conditions)
    if prompt is not None:
        subs.append(zstr(b"RNAM", prompt))
    if begin is not None:
        subs += stage_script([fallout_script.set_to(fallout_script.global_target(1), fallout_script.number("1"))],
                             [begin])
    if end is not None:
        subs += [sub(b"NEXT")]
        statements = [fallout_script.set_to(fallout_script.global_target(1), fallout_script.number("1"))]
        references = [end]
        if say is not None:
            statements.append(fallout_script.call_on(2, fallout_script.SAY, fallout_script.form_argument(3)))
            references += [TALKER_REF_ID, say]
        subs += stage_script(statements, references)
    elif show is not None:
        subs += [sub(b"NEXT")]
        subs += stage_script([fallout_script.call(fallout_script.SHOW_MESSAGE, fallout_script.form_argument(1))],
                             [show])
    return record(b"INFO", form_id, subs)


def topic_group(form_id, responses):
    return group(struct.pack("<I", form_id), 7, responses)


def dialogue():
    """The topics and their responses, as the children of the top group of DIAL. The greeting has two responses: the
    first is for a voice that nobody has and must not be said, the second is said to a man with the voice of the talker
    and runs a script when it starts. The topic at the top of the list has one response that adds a topic and
    runs a script when it starts and one when it ends; the added topic and the goodbye end the conversation."""
    sf = fallout_script
    is_voice = condition(GET_IS_VOICE_TYPE_FUNCTION, VOICE_ID, compare=0x00, value=1.0)
    is_other_voice = condition(GET_IS_VOICE_TYPE_FUNCTION, OTHER_VOICE_ID, compare=0x00, value=1.0)
    is_man = condition(GET_IS_SEX_FUNCTION, 0, compare=0x00, value=1.0)
    begin, end = DIALOGUE_GLOBALS["begin"][0], DIALOGUE_GLOBALS["end"][0]
    return [
        topic(GREETING_TOPIC_ID, "GREETING", "", 0, 0, 0.0),
        topic_group(GREETING_TOPIC_ID, [
            response(WRONG_GREETING_ID, WRONG_GREETING_TEXT, [is_other_voice]),
            response(GREETING_INFO_ID, GREETING_TEXT, [is_voice, is_man], begin=begin)]),
        topic(GOODBYE_TOPIC_ID, "GOODBYE", "Goodbye.", 1, 0x02, 0.0),
        topic_group(GOODBYE_TOPIC_ID, [response(GOODBYE_INFO_ID, GOODBYE_TEXT, flags=0x0001)]),
        topic(ASK_TOPIC_ID, "OFTestAsk", ASK_TOPIC_NAME, 0, 0x02, 90.0),
        topic_group(ASK_TOPIC_ID, [
            response(ASK_INFO_ID, ASK_TEXT, adds=[ADDED_TOPIC_ID], prompt=ASK_PROMPT, begin=begin, end=end,
                     say=SAID_TOPIC_ID)]),
        topic(SAID_TOPIC_ID, "OFTestSaid", "", 0, 0),
        topic_group(SAID_TOPIC_ID, [response(SAID_INFO_ID, SAID_TEXT, end=DIALOGUE_GLOBALS["said_end"][0])]),
        topic(ADDED_TOPIC_ID, "OFTestAdded", ADDED_TOPIC_NAME, 0, 0),
        topic_group(ADDED_TOPIC_ID, [response(ADDED_INFO_ID, ADDED_TEXT, flags=0x0001, show=BUTTON_MESSAGE_ID,
                                              sound=SOUND_ID, more=((ADDED_MORE_TEXT, FOLDER_SOUND_ID),))]),
    ]


def talker_reference():
    return achr(TALKER_REF_ID, (*TALKER_PLACE, 0.0), base=TALKER_ID)


def add_one_to_global():
    """The statements of a script that counts: the first form it names is a global, which it adds 1 to."""
    sf = fallout_script
    return [sf.set_to(sf.global_target(1), sf.global_target(1) + sf.number("1") + sf.operator("+"))]


def package_event(marker, global_name):
    """The marker of an event of a package (POBA when it begins, POEA when it ends, POCA when it changes) and the script
    that runs for it, which adds 1 to the global."""
    return [sub(marker), *stage_script(add_one_to_global(), [PACKAGE_EVENT_GLOBALS[global_name][0]])]


def package(form_id, name, kind, location=None, radius=0, time=0xff, duration=0, target=None, distance=0,
            conditions=(), events=()):
    """A PACK record of Fallout 3: the editor ID, the data (the flags, the kind of package and 2 bytes the games add), the
    location as a type, a form ID or 0 and a radius, the schedule (any month, any day, no date, the hour or 0xff for
    any, how many hours it lasts) and the target as a type and a form ID (0: a reference), the distance to keep and a float (16 bytes in Fallout),
    then the conditions (CTDA sub-records) that must hold, then the events (the marker of each and the global its script
    counts)."""
    subs = [zstr(b"EDID", name), sub(b"PKDT", struct.pack("<IBBH", 0, kind, 0, 0))]
    if location is not None:
        subs.append(sub(b"PLDT", struct.pack("<iIi", *location, radius)))
    subs.append(sub(b"PSDT", struct.pack("<BBBBI", 0xff, 0xff, 0, time, duration)))
    if target is not None:
        subs.append(sub(b"PTDT", struct.pack("<iIif", *target, distance, 0.0)))
    subs += conditions
    for marker, global_name in events:
        subs += package_event(marker, global_name)
    return record(b"PACK", form_id, subs)


def packages():
    """The packages of the walker (two for each kind of cell), of the roamer and of the follower."""
    records = [package(ROAM_PACKAGE_ID, "OFTestRoam", PACKAGE_WANDER, (LOCATION_NEAR_EDITOR, 0), ROAM_RADIUS),
               package(FOLLOW_PACKAGE_ID, "OFTestFollow", PACKAGE_FOLLOW, target=(0, PLAYER_REFERENCE),
                       distance=FOLLOW_DISTANCES["follower"])]
    for index, kind in enumerate(("interior", "exterior")):
        records.append(package(NIGHT_PACKAGE_IDS[kind], "OFTestNight" + kind.capitalize(), PACKAGE_TRAVEL,
                               (LOCATION_NEAR_REFERENCE, PACKAGE_REF_IDS["night"][index]), 0, NIGHT_START, NIGHT_HOURS))
        records.append(package(TRAVEL_PACKAGE_IDS[kind], "OFTestTravel" + kind.capitalize(), PACKAGE_TRAVEL,
                               (LOCATION_NEAR_REFERENCE, PACKAGE_REF_IDS["target"][index]), TRAVEL_RADIUS,
                               events=((b"POBA", "travel_begin"), (b"POEA", "travel_end"),
                                       (b"POCA", "travel_change"))))
        # Quest C is at its stage when the test sets it (GATE_FROM): at least that stage, or stage 99 which it never has
        gate_open = condition(GET_STAGE_FUNCTION, QUEST_C_ID, compare=CONDITION_AT_LEAST, value=float(QUEST_C_STAGE))
        gate_never = condition(GET_STAGE_FUNCTION, QUEST_C_ID, compare=0x00, value=99.0)
        # The package that the conditional character follows until the gate opens: it stays where it is put
        gate_closed = condition(GET_STAGE_FUNCTION, QUEST_C_ID, compare=CONDITION_BELOW, value=float(QUEST_C_STAGE))
        records.append(package(EARLY_PACKAGE_IDS[kind], "OFTestEarly" + kind.capitalize(), PACKAGE_GUARD,
                               (LOCATION_NEAR_CURRENT, 0), TRAVEL_RADIUS, conditions=[gate_closed],
                               events=((b"POBA", "early_begin"), (b"POEA", "early_end"), (b"POCA", "early_change"))))
        records.append(package(BLOCKED_PACKAGE_IDS[kind], "OFTestBlocked" + kind.capitalize(), PACKAGE_TRAVEL,
                               (LOCATION_NEAR_REFERENCE, GATE_REF_IDS["blocked"][index]), TRAVEL_RADIUS,
                               conditions=[gate_never]))
        records.append(package(GATED_PACKAGE_IDS[kind], "OFTestGated" + kind.capitalize(), PACKAGE_TRAVEL,
                               (LOCATION_NEAR_REFERENCE, GATE_REF_IDS["gated"][index]), TRAVEL_RADIUS,
                               conditions=[gate_open]))
        records.append(package(SCRIPT_PACKAGE_IDS[kind], "OFTestScripted" + kind.capitalize(), PACKAGE_TRAVEL,
                               (LOCATION_NEAR_REFERENCE, GATE_REF_IDS["scripted_marker"][index]), TRAVEL_RADIUS))
    return records


def watch_script(kind):
    """The script of the walker (an SCPT record of type 0), which counts the events of its packages: the start and the end
    of the Travel package, the end of the package for the night, which is not on, and the end of any package."""
    sf = fallout_script
    script = sf.Script(0)
    travel, night = (script.form(ids[kind]) for ids in (TRAVEL_PACKAGE_IDS, NIGHT_PACKAGE_IDS))
    counts = {name: script.form(form_id) for name, (form_id, _) in PACKAGE_EVENT_GLOBALS.items()}

    def add_one(name):
        return sf.set_to(sf.global_target(counts[name]),
                         sf.global_target(counts[name]) + sf.number("1") + sf.operator("+"))

    script.add(
        sf.statement(sf.SCRIPT_NAME),
        sf.block(sf.ON_PACKAGE_START, add_one("block_start"), names=sf.form_argument(travel)),
        sf.block(sf.ON_PACKAGE_DONE, add_one("block_done"), names=sf.form_argument(travel)),
        sf.block(sf.ON_PACKAGE_DONE, add_one("block_other"), names=sf.form_argument(night)),
        sf.block(sf.ON_PACKAGE_DONE, add_one("block_any")))
    return record(b"SCPT", WATCH_SCRIPT_IDS[kind], [zstr(b"EDID", "OFTestWatchScript" + kind.title()),
                                                    *sf.sub_records(script)])


def package_people():
    """The walker of each kind of cell, the idler and the roamer."""
    people = [person(WALKER_IDS[kind], "OFTestWalker" + kind.capitalize(),
                     [NIGHT_PACKAGE_IDS[kind], TRAVEL_PACKAGE_IDS[kind]], dressed=False,
                     script=WATCH_SCRIPT_IDS[kind])
              for kind in ("interior", "exterior")]
    people.append(person(IDLER_ID, "OFTestIdler", dressed=False, items=[(SUIT_ID, IDLER_ITEMS)],
                         factions=list(zip(FACTION_IDS, IDLER_RANKS))))
    people.append(person(PLAYER_BASE_ID, "OFTestPlayer", dressed=False, factions=[(FACTION_IDS[0], PLAYER_RANK)]))
    people.append(person(ROAMER_ID, "OFTestRoamer", [ROAM_PACKAGE_ID], dressed=False))
    people.append(person(FOLLOWER_ID, "OFTestFollower", [FOLLOW_PACKAGE_ID], dressed=False))
    people.append(person(COMPANION_ID, "OFTestCompanion", dressed=False))
    for kind in ("interior", "exterior"):
        people.append(person(CONDITIONAL_IDS[kind], "OFTestConditional" + kind.capitalize(),
                             [EARLY_PACKAGE_IDS[kind], BLOCKED_PACKAGE_IDS[kind], GATED_PACKAGE_IDS[kind]],
                             dressed=False))
        people.append(person(SCRIPTED_WALKER_IDS[kind], "OFTestScriptedWalker" + kind.capitalize(), dressed=False))
    return people


def package_refs(index, origin):
    """The markers and the placed characters that follow packages, a script or the player, of a cell (0 for the interior
    cell, 1 for the exterior one), whose origin is where the player starts in it."""
    ids = {name: refs[index] for name, refs in PACKAGE_REF_IDS.items()}
    kind = ("interior", "exterior")[index]

    def place(name):
        x, y = PACKAGE_PLACES[name]
        return (origin[0] + x, origin[1] + y, 0.0)

    def gate_place(name):
        x, y = GATE_PLACES[name]
        return (origin[0] + x, origin[1] + y, 0.0)

    gate_ids = {name: refs[index] for name, refs in GATE_REF_IDS.items()}
    gate_refs = [achr(gate_ids["conditional"], gate_place("conditional"), CONDITIONAL_IDS[kind]),
                 refr(gate_ids["gated"], TARGET_MARKER_ID, gate_place("gated"), rotation=(0.0, 0.0, TARGET_YAW)),
                 refr(gate_ids["blocked"], TARGET_MARKER_ID, gate_place("blocked"), rotation=(0.0, 0.0, NIGHT_YAW)),
                 achr(gate_ids["scripted"], gate_place("scripted"), SCRIPTED_WALKER_IDS[kind]),
                 refr(gate_ids["scripted_marker"], TARGET_MARKER_ID, gate_place("scripted_marker"),
                      rotation=(0.0, 0.0, TARGET_YAW))]
    return gate_refs + [refr(ids["target"], TARGET_MARKER_ID, place("target"), rotation=(0.0, 0.0, TARGET_YAW)),
            refr(ids["night"], TARGET_MARKER_ID, place("night"), rotation=(0.0, 0.0, NIGHT_YAW)),
            achr(ids["walker"], place("walker"), WALKER_IDS[kind]), achr(ids["idler"], place("idler"), IDLER_ID),
            achr(ids["roamer"], place("roamer"), ROAMER_ID)] + [
        achr(FOLLOW_REF_IDS[name][index], (origin[0] + x, origin[1] + y, 0.0), base)
        for name, base, (x, y) in (("follower", FOLLOWER_ID, FOLLOW_PLACES["follower"]),
                                   ("companion", COMPANION_ID, FOLLOW_PLACES["companion"]))]


def head_morphs():
    """The FaceGen morph file of the head: a header of 64 bytes (FREGM002, the numbers of vertices and of morphs, a number,
    40 bytes that are not used), then each morph with its scale and three deltas for each vertex. The scale of the first
    symmetric morph is 0.5, so its deltas are twice the shift."""
    vertices = HEAD_VERTICES + HEAD_FILE_EXTRA_VERTICES
    data = b"FREGM002" + struct.pack("<4I", vertices, 50, 30, 0) + bytes(40)
    for index in range(80):
        move = int(round(FACE_SHIFT * 2)) if index == 0 else 0
        data += struct.pack("<f", 0.5) + struct.pack("<3h", move, 0, 0) * vertices
    return data


def creature_folder_file(name):
    """The path of a file in the folder of the creature, as a record names it (under meshes)."""
    return CREATURE_FOLDER + name


def creature(form_id, name, subs, level=1, template_flags=0):
    """A CREA record of Fallout 3 with the ACBS that the loader reads (24 bytes, template flags last)."""
    acbs = struct.pack("<IHHhHHHfhH", 0, 0, 0, level, 1, 1, 100, 0.0, 0, template_flags)
    return record(b"CREA", form_id, [zstr(b"EDID", name), sub(b"ACBS", acbs), *subs])


def creatures():
    """The creature with a model of its own, the one that takes the model from a levelled list, and the list."""
    beast = creature(CREATURE_ID, CREATURE_NAME, [
        zstr(b"FULL", "OpenFallout Test Beast"), zstr(b"MODL", CREATURE_SKELETON),
        sub(b"NIFZ", CREATURE_BODY.encode("ascii") + b"\0" + CREATURE_FOOT.encode("ascii") + b"\0"),
        sub(b"OBND", struct.pack("<6h", *CREATURE_BOUNDS)), sub(b"CNTO", struct.pack("<II", SUIT_ID, CREATURE_ITEMS)),
        sub(b"SCRI", struct.pack("<I", TEMPLATE_SCRIPT_ID))], level=BEAST_LEVEL)
    # the record of one that takes its model, and with it its name and stats, from the template and keeps nothing of its
    # own
    user = creature(CREATURE_USER_ID, CREATURE_NAME + "User", [
        zstr(b"FULL", "Not Shown"), sub(b"TPLT", struct.pack("<I", CREATURE_LIST_ID))],
        template_flags=CREATURE_TEMPLATE_FLAGS)
    # LVLD (the chance of nothing), LVLF (the flags) and one LVLO (level 1, the creature, a count of 1)
    levelled = record(b"LVLC", CREATURE_LIST_ID, [
        zstr(b"EDID", CREATURE_NAME + "List"), sub(b"LVLD", b"\x00"), sub(b"LVLF", b"\x00"),
        sub(b"LVLO", struct.pack("<hHIhH", 1, 0, CREATURE_ID, 1, 0))])
    return beast, user, levelled


def heir():
    """A creature that takes its model, its name and its script from the beast, which is its template."""
    return creature(HEIR_ID, CREATURE_NAME + "Heir", [zstr(b"FULL", "Not Shown"),
                                                      sub(b"TPLT", struct.pack("<I", CREATURE_ID))],
                    template_flags=HEIR_TEMPLATE_FLAGS)


def template_script():
    """The script of the beast (an SCPT record): on load it adds one to a global."""
    sf = fallout_script
    script = sf.Script(0)
    loads = script.form(WORLD_QUERY_GLOBALS["template_loads"][0])
    script.add(sf.statement(sf.SCRIPT_NAME),
               sf.block(sf.ON_LOAD, sf.set_to(sf.global_target(loads), sf.global_target(loads) + sf.number("1")
                                              + sf.operator("+"))))
    return record(b"SCPT", TEMPLATE_SCRIPT_ID, [zstr(b"EDID", "OFTestTemplateScript"), *sf.sub_records(script)])


def acre(form_id, base, position):
    """A placed creature: the base record (a creature or a levelled list) and where it stands."""
    return record(b"ACRE", form_id, [sub(b"NAME", struct.pack("<I", base)),
                                     sub(b"DATA", struct.pack("<6f", *position, 0.0, 0.0, 0.0))])


def placed_creatures(origin, ids):
    """The three placed creatures of a cell, whose origin is where the player starts in it."""
    bases = {"beast": CREATURE_ID, "user": CREATURE_USER_ID, "list": CREATURE_LIST_ID}
    return [acre(ids[name], bases[name], (origin[0] + x, origin[1] + y, 0.0))
            for name, (x, y) in CREATURE_POSITIONS.items()]


def achr(form_id, position, base=NPC_ID):
    """A placed character: the base record and where it stands."""
    return record(b"ACHR", form_id, [sub(b"NAME", struct.pack("<I", base)),
                                     sub(b"DATA", struct.pack("<6f", *position, 0.0, 0.0, 0.0))])


def worldspace():
    """The WRLD record of OFTestWorld followed by its children: the exterior cell 0,0 in an exterior block and sub-block
    (both labelled with the grid 0,0), with its references in the temporary children group of the cell."""
    land_level, water_level = -2700.0, -14000.0
    wrld = record(b"WRLD", WORLD_ID, [zstr(b"EDID", WORLD_NAME), zstr(b"FULL", "OpenFallout Test World"),
                                      sub(b"NAM0", struct.pack("<ff", -4096.0, -4096.0)),
                                      sub(b"NAM9", struct.pack("<ff", 8192.0, 8192.0)),
                                      sub(b"CNAM", struct.pack("<I", CLIMATE_ID)),
                                      sub(b"NAM2", struct.pack("<I", WATER_TYPE_ID)), sub(b"DATA", b"\x00"),
                                      sub(b"DNAM", struct.pack("<ff", land_level, water_level))])
    # DATA of an exterior cell is its flags (0: not an interior, no water), XCLC the grid and the flags of the land.
    cell = record(b"CELL", EXTERIOR_CELL_ID, [zstr(b"EDID", WORLD_NAME + "Cell"), sub(b"DATA", b"\x00"),
                                              sub(b"XCLC", struct.pack("<iiI", 0, 0, 0))])
    # Two more cells with flat terrain and water flagged: one with a height of its own, one with the largest float.
    water_cell = record(b"CELL", WATER_CELL_ID, [zstr(b"EDID", WORLD_NAME + "WaterCell"), sub(b"DATA", b"\x02"),
                                                 sub(b"XCLC", struct.pack("<iiI", *WATER_CELL, 0)),
                                                 sub(b"XCLW", struct.pack("<f", WATER_HEIGHT))])
    dry_cell = record(b"CELL", DRY_CELL_ID, [zstr(b"EDID", WORLD_NAME + "DryCell"), sub(b"DATA", b"\x02"),
                                             sub(b"XCLC", struct.pack("<iiI", *DRY_CELL, 0)),
                                             sub(b"XCLW", struct.pack("<f", FLOAT_MAX))])
    half = EXTERIOR_CELL_SIZE / 2
    refs = group(struct.pack("<I", EXTERIOR_CELL_ID), 9, [
        land(textured=True),
        refr(EXTERIOR_PILLAR_ID, CUBE_ID, (half, half + PILLAR_DISTANCE, CUBE / 2)),
        refr(EXTERIOR_MARKER_REF_ID, MARKER_ID, (half, half, 0.0)),
        achr(EXTERIOR_NPC_REF_ID, (half + NPC_DISTANCE, half + NPC_DEPTH, 0.0)),
        refr(EXTERIOR_WALL_REF_ID, WALL_ID, (half - WALL_DISTANCE, half + NPC_DEPTH, 0.0)),
        *placed_creatures((half, half), {"beast": EXTERIOR_CREATURE_REF_ID, "user": EXTERIOR_CREATURE_USER_REF_ID,
                                         "list": EXTERIOR_CREATURE_LIST_REF_ID}),
        acre(HEIR_REF_IDS["exterior"], HEIR_ID, (half + HEIR_PLACE[0], half + HEIR_PLACE[1], 0.0)),
        *package_refs(1, (half, half)),
        *object_refs("exterior", (half, half)),
    ])
    children = group(struct.pack("<I", EXTERIOR_CELL_ID), 6, [refs])
    water_children = group(struct.pack("<I", WATER_CELL_ID), 6, [
        group(struct.pack("<I", WATER_CELL_ID), 9, [land(WATER_LAND_ID, WATER_TERRAIN)])])
    dry_children = group(struct.pack("<I", DRY_CELL_ID), 6, [
        group(struct.pack("<I", DRY_CELL_ID), 9, [land(DRY_LAND_ID)])])
    grid = struct.pack("<hh", 0, 0)
    sub_block = group(grid, 5, [cell, children, water_cell, water_children, dry_cell, dry_children])
    block = group(grid, 4, [sub_block])
    return [wrld, group(struct.pack("<I", WORLD_ID), 1, [block])]


def plugin():
    hedr = struct.pack("<fiI", 0.94, 18, LAST_ID + 1)
    header = record(b"TES4", 0, [sub(b"HEDR", hedr), zstr(b"CNAM", "OpenFallout"),
                                 zstr(b"SNAM", "synthetic test plugin")], flags=1)
    half = int(CUBE / 2)
    cube = record(b"STAT", CUBE_ID, [zstr(b"EDID", "OFTestCube"),
                                     sub(b"OBND", struct.pack("<6h", -half, -half, -half, half, half, half)),
                                     zstr(b"MODL", "openfallout\\cube.osgt")])
    wall = record(b"STAT", WALL_ID, [zstr(b"EDID", "OFTestWall"), zstr(b"MODL", WALL_MESH)])
    # The static that real cells use to say where to enter them, with no model.
    marker = record(b"STAT", MARKER_ID, [zstr(b"EDID", "COCMarkerHeading")])
    target_marker = record(b"STAT", TARGET_MARKER_ID, [zstr(b"EDID", "OFTestTargetMarker")])
    template = record(b"LGTM", TEMPLATE_ID, [
        zstr(b"EDID", "OFTestLighting"),
        sub(b"DATA", lighting((1, 2, 3), (4, 5, 6), TEMPLATE_FOG, 1.0, TEMPLATE_FOG_FAR))])
    xcll = lighting(AMBIENT, DIRECTIONAL, CELL_FOG, FOG_NEAR, CELL_FOG_FAR)
    cell = record(b"CELL", CELL_ID, [zstr(b"EDID", CELL_NAME), zstr(b"FULL", "OpenFallout Test Cell"),
                                     sub(b"DATA", b"\x01"), sub(b"XCLL", xcll),
                                     sub(b"LTMP", struct.pack("<I", TEMPLATE_ID)),
                                     sub(b"LNAM", struct.pack("<I", INHERIT_FOG_COLOR_AND_FAR))])
    refs = group(struct.pack("<I", CELL_ID), 9, [
        refr(FLOOR_ID, CUBE_ID, (0.0, 0.0, -CUBE * FLOOR_SCALE / 2), FLOOR_SCALE),
        refr(PILLAR_ID, CUBE_ID, (0.0, PILLAR_DISTANCE, CUBE / 2)),
        refr(MARKER_REF_ID, MARKER_ID, (0.0, 0.0, 0.0)),
        refr(NEW_GAME_PLACE_REF_ID, TARGET_MARKER_ID, NEW_GAME_PLACE, rotation=(0.0, 0.0, NEW_GAME_YAW)),
        achr(NPC_REF_ID, (NPC_DISTANCE, NPC_DEPTH, 0.0)),
        talker_reference(),
        refr(WALL_REF_ID, WALL_ID, (-WALL_DISTANCE, NPC_DEPTH, 0.0)),
        *placed_creatures((0.0, 0.0), {"beast": CREATURE_REF_ID, "user": CREATURE_USER_REF_ID,
                                      "list": CREATURE_LIST_REF_ID}),
        acre(HEIR_REF_IDS["interior"], HEIR_ID, (*HEIR_PLACE, 0.0)),
        *package_refs(0, (0.0, 0.0)),
        *object_refs("interior", (0.0, 0.0)),
        trigger_reference(),
    ])
    children = group(struct.pack("<I", CELL_ID), 6, [refs])
    sub_block = group(struct.pack("<i", CELL_ID // 10 % 10), 3, [cell, children])
    block = group(struct.pack("<i", CELL_ID % 10), 2, [sub_block])
    return (header + top_group(b"STAT", [cube, marker, wall, target_marker]) + top_group(b"LGTM", [template])
            + top_group(b"GLOB", [global_variable(), global_variable(COUNTER_ID, COUNTER_NAME, b"l"),
                                  *(global_variable(form_id, name, b"l") for form_id, name in OBJECT_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l") for form_id, name in TRIGGER_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l")
                                    for form_id, name in PACKAGE_EVENT_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l") for form_id, name in DIALOGUE_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l") for form_id, name in CHARGEN_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l") for form_id, name in STATE_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l") for form_id, name in PROGRESS_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l")
                                    for form_id, name in WORLD_QUERY_GLOBALS.values()),
                                  global_variable(GAME_HOUR_ID, "GameHour", b"f", GAME_HOUR_RECORD_VALUE),
                                  *(global_variable(form_id, name, b"f") for form_id, name in CLOCK_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"f")
                                    for form_id, name in ACTOR_VALUE_GLOBALS.values()),
                                  *(global_variable(form_id, name, b"l" if key in ("now", "hop") else b"f")
                                    for key, (form_id, name) in MOVE_GLOBALS.items())])
            + top_group(b"FLST", form_lists())
            + top_group(b"ACTI", [scripted_object("interior"), scripted_object("exterior"), trigger_base()])
            + top_group(b"MESG", [message(), button_message(), new_game_message()])
            + top_group(b"SCPT", [quest_script(), template_script(), object_script("interior"),
                                                 object_script("exterior"), button_script(), chargen_script(),
                                                 new_game_script(), trigger_script(), watch_script("interior"),
                                                 watch_script("exterior")])
            + top_group(b"QUST", [*quests(), chargen_quest(), new_game_quest()])
            + top_group(b"PERK", [trait_perk(TRAIT_A_ID, "OFTestTraitA", TRAIT_A_NAME, "The first trait.", True),
                                  trait_perk(TRAIT_B_ID, "OFTestTraitB", TRAIT_B_NAME, "The second trait.", True),
                                  trait_perk(NOT_A_TRAIT_ID, "OFTestPerkNotTrait", NOT_A_TRAIT_NAME, "No trait.", False)])
            + top_group(b"AVIF", [actor_value_info(form_id, *texts[:1], *texts[1:])
                                  for form_id, texts in AVIF_TEXTS.items()])
            + top_group(b"WTHR", [weather(), weather(CONDITIONAL_WEATHER_ID, CONDITIONAL_WEATHER_NAME)])
            + top_group(b"CLMT", [climate()]) + top_group(b"WATR", [water_type()])
            + top_group(b"LTEX", [land_texture()]) + top_group(b"TXST", [texture_set()])
            + top_group(b"RACE", [race(), race(RACE_TWO_ID, RACE_TWO_NAME)]) + top_group(b"HAIR", [hair()]) + top_group(b"ARMO", [suit()])
            + top_group(b"FACT", [faction(form_id, "OFTestFaction" + "AB"[i]) for i, form_id in enumerate(FACTION_IDS)])
            + top_group(b"PACK", packages())
            + top_group(b"NPC_", [person(), *package_people(),
                                  person(TALKER_ID, TALKER_NAME, dressed=False, voice=VOICE_ID, title=TALKER_TITLE,
                                         script=BUTTON_SCRIPT_ID)])
            + top_group(b"VTYP", [voice_type(VOICE_ID, "OFTestVoice"), voice_type(OTHER_VOICE_ID, "OFTestOtherVoice")])
            + top_group(b"SOUN", [record(b"SOUN", SOUND_ID, [zstr(b"EDID", SOUND_NAME), zstr(b"FNAM", SOUND_FILE)]),
                                record(b"SOUN", FOLDER_SOUND_ID, [zstr(b"EDID", FOLDER_SOUND_NAME),
                                                                  zstr(b"FNAM", FOLDER_SOUND_FILE)])])
            + top_group(b"DIAL", dialogue())
            + top_group(b"CREA", [*creatures()[:2], heir()])
            + top_group(b"LVLC", [creatures()[2]])
            + top_group(b"CELL", [block]) + top_group(b"WRLD", worldspace()))


def box_mesh(half_extents=(CUBE / 2,) * 3, center=(0.0, 0.0, 0.0)):
    """An OpenSceneGraph text file with one box of the half extents around the centre; with the defaults, a cube of edge
    CUBE centred on its origin."""
    x, y, z = half_extents
    cx, cy, cz = center
    corners = [(-x, -y, -z), (x, -y, -z), (x, y, -z), (-x, y, -z), (-x, -y, z), (x, -y, z), (x, y, z), (-x, y, z)]
    corners = [(a + cx, b + cy, c + cz) for a, b, c in corners]
    # Corner indices of each face, counter-clockwise seen from outside, with the normal of that face.
    faces = [((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)), ((0, 1, 5, 4), (0, -1, 0)),
             ((2, 3, 7, 6), (0, 1, 0)), ((1, 2, 6, 5), (1, 0, 0)), ((3, 0, 4, 7), (-1, 0, 0))]
    vertices, normals, indices = [], [], []
    for face, normal in faces:
        base = len(vertices)
        vertices.extend(corners[i] for i in face)
        normals.extend([normal] * 4)
        indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))

    def array(name, array_id, rows):
        return (["      %s TRUE {" % name, "        osg::Vec3Array {", "          UniqueID %d" % array_id,
                 "          Binding BIND_PER_VERTEX", "          vector %d {" % len(rows)]
                + ["            %g %g %g" % row for row in rows] + ["          }", "        }", "      }"])

    lines = ["#Ascii Scene", "#Version 161", "#Generator OpenFallout synthetic_fallout_plugin.py", "",
             "osg::Geode {", "  UniqueID 1", "  Drawables 1 {", "    osg::Geometry {", "      UniqueID 2",
             "      PrimitiveSetList 1 {", "        osg::DrawElementsUShort {", "          UniqueID 3",
             "          Mode TRIANGLES", "          vector %d {" % len(indices)]
    lines += ["            " + " ".join(str(i) for i in indices[n:n + 6]) for n in range(0, len(indices), 6)]
    lines += ["          }", "        }", "      }"]
    lines += array("VertexArray", 4, vertices) + array("NormalArray", 5, normals)
    lines += ["    }", "  }", "}", ""]
    return "\n".join(lines)


def write(out, animation=True, spline=True):
    """Write the plugin and its files into the folder. `animation` is false for the files of the person to have no
    animation file. The animation moves the foot with the control points of a B-spline, as most bones of the files of
    the games are, or with two keys when `spline` is false."""
    out = Path(out)
    mesh = out / "meshes" / "openfallout" / "cube.osgt"
    mesh.parent.mkdir(parents=True, exist_ok=True)
    for voice, text, response in VOICE_FILES:
        voice_file = out / voice_path(voice, text, response)
        voice_file.parent.mkdir(parents=True, exist_ok=True)
        voice_file.write_bytes(b"OggS not a sound")
    sound_file = out / SOUND_PATH
    sound_file.parent.mkdir(parents=True, exist_ok=True)
    sound_file.write_bytes(b"OggS not a sound")
    folder_file = out / FOLDER_SOUND_PATH
    folder_file.parent.mkdir(parents=True, exist_ok=True)
    folder_file.write_bytes(b"OggS not a sound")
    mesh.write_text(box_mesh(), encoding="ascii")
    for name, (half_extents, center) in PERSON_PARTS.items():
        (out / "meshes" / Path(part_path(name).replace("\\", "/"))).write_text(box_mesh(half_extents, center),
                                                                           encoding="ascii")
    (out / "meshes" / Path(part_path("head").replace("\\", "/")).with_suffix(".egm")).write_bytes(head_morphs())
    (out / "meshes" / Path(WALL_MESH.replace("\\", "/"))).write_bytes(
        havok_wall.wall(WALL_HALF_EXTENTS, (0.0, 0.0, WALL_HALF_EXTENTS[2])))
    texture = out / "textures" / "sky" / "oftestclouds.dds"
    texture.parent.mkdir(parents=True, exist_ok=True)
    texture.write_bytes(cloud_texture())
    ground = out / "textures" / Path(GROUND_TEXTURE.replace("\\", "/")).parent / "oftestground.dds"
    ground.parent.mkdir(parents=True, exist_ok=True)
    ground.write_bytes(ground_texture())
    skeleton = out / "meshes" / Path(SKELETON.replace("\\", "/"))
    skeleton.parent.mkdir(parents=True, exist_ok=True)
    skeleton.write_bytes(placeholder_skeleton.skeleton())
    (out / "meshes" / Path(FOOT_PART.replace("\\", "/"))).write_bytes(
        skinned_box.skinned_box(FOOT_BONE, FOOT_REST, FOOT_REST, FOOT_MARKER))
    creature_folder = out / "meshes" / Path(CREATURE_FOLDER.replace("\\", "/"))
    creature_folder.mkdir(parents=True, exist_ok=True)
    (creature_folder / "skeleton.nif").write_bytes(placeholder_skeleton.skeleton())
    (creature_folder / CREATURE_BODY).write_text(box_mesh(*CREATURE_BODY_BOX), encoding="ascii")
    (creature_folder / CREATURE_FOOT).write_bytes(skinned_box.skinned_box(FOOT_BONE, FOOT_REST, FOOT_REST, FOOT_MARKER))
    if animation:
        (creature_folder / "mtidle.kf").write_bytes(
            idle_animation.idle_animation(node=FOOT_BONE, offset=FOOT_MOVE, spline=spline))
        (out / "meshes" / Path(IDLE_ANIMATION.replace("\\", "/"))).write_bytes(
            idle_animation.idle_animation(node=FOOT_BONE, offset=FOOT_MOVE, spline=spline))
        for path, name, move, velocity in ((WALK_ANIMATION, "MTForward", WALK_MOVE, WALK_VELOCITY),
                                           (RUN_ANIMATION, "MTFastForward", RUN_MOVE, RUN_VELOCITY)):
            target = out / "meshes" / Path(path.replace("\\", "/"))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(idle_animation.idle_animation(
                name=name, node=FOOT_BONE, offset=move, stop=LOCOMOTION_SECONDS, spline=spline, root_speed=velocity))
    (out / "OFTest.esm").write_bytes(plugin())
    return out / "OFTest.esm"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, type=Path, help="folder to write into (created if missing)")
    print(write(parser.parse_args().out))
