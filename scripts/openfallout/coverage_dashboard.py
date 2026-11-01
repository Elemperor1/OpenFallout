#!/usr/bin/env python3
"""Counts how much of what Fallout 3 and New Vegas define the engine has, and writes it as a treemap page.

    scripts/openfallout/coverage_dashboard.py --html coverage.html --json coverage.json

The page is the project's version of a decompilation progress map: one box for each library, one small square for each
function (or record type) in it, green for the ones the engine has. Read docs/openfallout/coverage-dashboard.md for what
counts as done. The numbers come from the source tree, so run the script again after a change and the page follows:

  * Script commands: every command of the tables that scripts/openfallout/dump_command_table.py read from the two
    executables (components/falloutscript/vanillacommands.inc). A command is done when the engine registers a handler for
    it (setHandler in components/falloutscript and apps/openfallout/mwscript).
  * Script extender functions: every function of the plugin tables in docs/openfallout/coverage/extender-tables, which
    were read from the DLLs of xNVSE and its plugins. Done in the same way.
  * Record types: every record type of the two base plugins (the census table in docs/openfallout/census-results.md). A
    type is done when its loader's record is kept in the engine's record store (apps/openfallout/mwworld/esmstore.hpp).

A unit is in one of three states: done, a stand-in (the engine takes it in and does nothing with it, or answers a
fixed value) or not done. The reasons for the stand-ins are in the source, or in
docs/openfallout/coverage/stand-ins.csv for the ones the source does not say.

Needs Python 3 only (and git, for the commit it prints on the page).
"""
import argparse
import csv
import datetime
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

DONE = "done"
STAND_IN = "stand-in"
TODO = "todo"

# Where the engine registers the handlers of the commands (tests and tools are left out)
HANDLER_SOURCES = [
    "components/falloutscript",
    "apps/openfallout/mwscript",
]
VANILLA_TABLE = "components/falloutscript/vanillacommands.inc"
EXTENDER_ENTRIES = "components/falloutscript/extendercommands.cpp"
SWITCHES = "components/falloutscript/switches.cpp"
EXTENDER_TABLES = "docs/openfallout/coverage/extender-tables"
TTW_CODES = "docs/openfallout/ttw-extension-codes.csv"
STAND_INS = "docs/openfallout/coverage/stand-ins.csv"
CENSUS = "docs/openfallout/census-results.md"
LOADERS = "components/esm4"
STORE = "apps/openfallout/mwworld/esmstore.hpp"

# The extender plugins, in the order of the page: file, name on the page, who makes it
EXTENDER_PLUGINS = [
    ("xnvse_core.csv", "xNVSE core", "the script extender itself"),
    ("jip_nvse.csv", "JIP LN NVSE", "a plugin that Tale of Two Wastelands needs"),
    ("johnnyguitar.csv", "JohnnyGuitar NVSE", "a plugin that Tale of Two Wastelands needs"),
    ("ShowOffNVSE.csv", "ShowOff NVSE", "a plugin that Tale of Two Wastelands needs"),
    ("nvse_stewie_tweaks.csv", "lStewieAl's Tweaks", "a plugin that Tale of Two Wastelands needs"),
    ("kNVSE.csv", "kNVSE", "a plugin that Tale of Two Wastelands needs"),
    ("ttw_nvse.csv", "TTW NVSE", "the plugin of Tale of Two Wastelands"),
]

# The topics the script commands are sorted into for the page. The first rule that matches a name takes it. The topics
# are for reading the page: the games' command table has none.
COMMAND_TOPICS = [
    ("Casino and caravan", r"Casino|BlackJack|Roulette|SlotMachine|Caravan|Card|Code(?:Redeemed)?$|RedeemCode|RevokeCode"),
    ("Quests and challenges", r"Quest|Objective|Challenge|Achievement|^(?:Get|Set)Stage|StageDone"),
    ("Screens and menus", r"Menu|Message|Warning|Pipboy|PipBoy|Map|Hud|HUD|Hacking|Terminal|Computers|Screen|Loading|"
                          r"ButtonPressed|Subtitles|ViewerStrings"),
    ("Crime, stealth and law", r"Crime|Trespass|Murder|Expelled|Assault|Alarm|Guard|Stolen|Detect|PCFaction|PlayerEnemy|"
                               r"PCEnemy"),
    ("Dialogue and speech", r"^Say|Topic|Talk|Conversation|Greeting|Persuasion|Disposition|Emotion|Rumors|Radio|Voice|Scene"),
    ("V.A.T.S. and combat", r"VATS|Combat|Weapon|Damage|Kill|Hit|Critical|Limb|Explosive|Fire|Attack|Enemy|Ally|Flee|"
                            r"Cover|Threat|Concuss|Unconscious|Restrained|Ghost|Friend|Detonate|Mines|Grab|Reticle|"
                            r"Destruction|Destroyed|Ash|Cause|Dead|Death|Essential|Resurrect|Health|Stage"),
    ("Sound, magic and effects", r"Sound|Music|Bink|Shader|ImageSpace|Splatter|Flames|Magic|Spell|Dispel|^Cast|Effect|"
                                 r"Alpha|Refraction|Motion|Visuals|FadeSFX|Rumble"),
    ("Items and inventory", r"Item|Equip|Inventory|Container|Note|Merchant|Barter|Gold|Drop|Ingredient|Armor|Recipe|"
                            r"Repair|Leveled|Lockpick|Key|Hardcore|Lock|Clothing|PowerArmor"),
    ("Actor values and progress", r"ActorValue|^(?:Get|Set|Mod)AV$|Perk|Skill|XP|Level|Karma|SPECIAL|Tag|Trait|Reputation|"
                                  r"Faction|Class|Race|Sex|Attribute|Radiation|MiscStat|Fatigue"),
    ("Actors, AI and packages", r"AI|Package|Alert|Sandman|Cannibal|Sitting|Sleep|Furniture|Idle|Anim|Walk|Run|Sneak|"
                                r"Swim|Moving|Turning|Wait|Follow|Teammate|Horse|Knocked|Wake|Evaluate|Look|Group|"
                                r"PickIdle|PlayGroup|LoopGroup|SkipAnim|Reach|Avoidance|Heading|Owner|Ownership|Actor|"
                                r"Creature|Child|Vampire|Disease"),
    ("World, cells and objects", r"Cell|Pos|Angle|Rotate|Scale|Size|Enable|Disable|Place|Marker|Door|Open|Trigger|Zone|"
                                 r"Region|Weather|Wind|Water|Rain|Snow|Cloud|Time|Day|Interior|Worldspace|Parent|Ref|"
                                 r"Self|Distance|LineOfSight|Exists|Move|Reset|Persistent|Purge|Rigid|LOD|Camera|FOV|"
                                 r"Visible|Reachable|Delete|Tracked|Seek|Global|Setting|InList|FormList|Form|Type|ID$|"
                                 r"Random|Activate|Navmesh|NavMesh|Destroy|Remove|Duplicate"),
    ("Maths and system", r"^(?:Sin|Cos|Tan|Sqrt|Log|Abs)$|Xbox|XBox|PS3|Win32|Save|Exit|DLC|Audio|Debug|Autosave|Platform|"
                         r"Print|System|Gore|Version"),
]
TOPIC_FALLBACK = "Other commands"

# The groups the record types are sorted into for the page
RECORD_GROUPS = [
    ("World and cells", "CELL WRLD LAND LTEX REFR ACHR ACRE PGRE PWAT NAVM NAVI REGN GRAS ECZN LGTM LSCR LSCT"),
    ("Objects and statics", "STAT SCOL MSTT TREE ACTI TACT TERM DOOR FURN CONT LIGH ADDN IDLM ANIO TXST DEBR"),
    ("Items", "ARMO ARMA WEAP AMMO ALCH BOOK KEYM MISC NOTE INGR IMOD AMEF CCRD CDCK CHIP CMNY RCPE RCCT"),
    ("Characters and creatures", "NPC_ CREA RACE CLAS FACT EYES HAIR HDPT BPTD RGDL VTYP CSTY LVLC LVLN AVIF PERK REPU"),
    ("Quests, dialogue and rules", "QUST DIAL INFO SCPT PACK GLOB GMST FLST MESG LVLI DOBJ IDLE CHAL CSNO HUNG DEHY SLPD RADS"),
    ("Effects, sound and weather", "MGEF SPEL ENCH EFSH EXPL PROJ IPCT IPDS IMGS IMAD SOUN MUSC MSET ALOC ASPC CAMS CPTH WTHR "
                                   "CLMT WATR MICN"),
]


def path(relative):
    if os.path.isabs(relative):
        return relative
    return os.path.join(REPO, *relative.split("/"))


def read_text(relative):
    with open(path(relative), encoding="utf-8", errors="replace") as stream:
        return stream.read()


# --------------------------------------------------------------------------------------------------------------------
# What the source says


def strip_comments(text):
    """The C++ text without // comments, so that a handler written in a comment does not count."""
    return re.sub(r"//[^\n]*", "", text)


def source_files(directories):
    for directory in directories:
        for root, _, names in os.walk(path(directory)):
            for name in sorted(names):
                if name.endswith(".cpp"):
                    yield os.path.join(root, name)


def read_vanilla_commands(text):
    """The commands of vanillacommands.inc: dicts with code, name, short name and whether Fallout 3 has it."""
    pattern = re.compile(
        r'^\{\s*(0x[0-9A-Fa-f]+),\s*"([^"]*)",\s*"([^"]*)",\s*(true|false),\s*(0x[0-9A-Fa-f]+),\s*"([^"]*)",\s*(true|false)',
        re.M,
    )
    commands = []
    for match in pattern.finditer(text):
        commands.append(
            {
                "code": int(match.group(1), 16),
                "name": match.group(2),
                "short": match.group(3),
                "fallout3": match.group(7) == "true",
            }
        )
    return commands


def read_handlers(directories):
    """The commands the engine registers a handler for: {lower case name or code: file}. A name is what
    Interpreter::setHandler takes (long or short, any case), a code is a number."""
    names = {}
    codes = {}
    call = re.compile(r'setHandler\(\s*(?:"([^"]+)"|(0x[0-9A-Fa-f]+))')
    for file in source_files(directories):
        relative = os.path.relpath(file, REPO).replace(os.sep, "/")
        with open(file, encoding="utf-8", errors="replace") as stream:
            text = strip_comments(stream.read())
        for match in call.finditer(text):
            if match.group(1):
                names.setdefault(match.group(1).lower(), relative)
            else:
                codes.setdefault(int(match.group(2), 16), relative)
    return names, codes


def read_listed_commands(text, array):
    """The names and reasons in one of the arrays of switches.cpp that the engine registers in a loop."""
    match = re.search(r"\b" + array + r"\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    if not match:
        raise SystemExit("%s: the array %s was not found" % (SWITCHES, array))
    entries = {}
    for entry in re.finditer(r'\{\s*"(\w+)",\s*"([^"]+)"', strip_comments(match.group(1))):
        entries[entry.group(1).lower()] = entry.group(2)
    return entries


def read_extender_entries(text):
    """The extender codes the engine puts in its command table: {code: name}."""
    entries = {}
    for match in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+),\s*"(\w+)"', strip_comments(text)):
        entries[int(match.group(1), 16)] = match.group(2)
    return entries


def read_fixed_answers(directories):
    """Handlers that take no captures and return a number: {lower case name: file}. The games give most of these a
    real answer, so they are candidates for a stand-in (stand-ins.csv says which are not)."""
    pattern = re.compile(
        r'setHandler\(\s*"(\w+)",\s*\[\]\((?:\w+::)?CallContext&\s*\w*\)\s*\{\s*return\s+-?[0-9.]+;\s*\}\)'
    )
    found = {}
    for file in source_files(directories):
        with open(file, encoding="utf-8", errors="replace") as stream:
            text = strip_comments(stream.read())
        for match in pattern.finditer(text):
            found[match.group(1).lower()] = os.path.relpath(file, REPO).replace(os.sep, "/")
    return found


def read_stand_ins(text):
    """stand-ins.csv: name, state (stand-in or exact), reason. The state exact says a fixed answer is what the games
    give."""
    entries = {}
    lines = [line for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")]
    for row in csv.DictReader(lines):
        if row["state"] not in ("stand-in", "exact"):
            raise SystemExit("%s: the state of %s is %r" % (STAND_INS, row["name"], row["state"]))
        entries[row["name"].lower()] = (row["state"], row["reason"])
    return entries


def read_extender_table(file):
    rows = []
    with open(path(EXTENDER_TABLES + "/" + file), newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["name"] in ("EmptyCommand", "NONE"):
                # Slots the plugin reserves and gives no function
                continue
            rows.append({"code": int(row["code"], 16), "name": row["name"], "short": row["short_name"]})
    return rows


def read_ttw_calls():
    """The extender codes that the scripts of Tale of Two Wastelands reach, with how often they call each: {code: calls}."""
    calls = {}
    with open(path(TTW_CODES), newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            # A code that only a condition names has no count of calls
            calls[int(row["code"], 16)] = int(row["calls"]) if row["calls"].strip().isdigit() else 0
    return calls


def read_census_types(text):
    """The record types of the two base plugins with how many records each game has: [(code, fallout3, newvegas)]."""
    section = text.split("## Every record type in the two base plugins")[1].split("\n## ")[0]
    rows = []
    for match in re.finditer(r"^\| `([A-Z_0-9]{4})` \| (?:yes|no) \| ([\d,\-]+) \| ([\d,\-]+) \|", section, re.M):
        count = lambda value: 0 if value == "-" else int(value.replace(",", ""))
        rows.append((match.group(1), count(match.group(2)), count(match.group(3))))
    return rows


def read_loaders():
    """The record structs of the loaders by the record type they declare: {code: struct name}."""
    structs = {}
    directory = path(LOADERS)
    for name in sorted(os.listdir(directory)):
        if not (name.startswith("load") and name.endswith(".hpp")):
            continue
        with open(os.path.join(directory, name), encoding="utf-8", errors="replace") as stream:
            text = stream.read()
        for match in re.finditer(r"sRecordId\s*=\s*(?:ESM::)?(?:RecNameInts::)?REC_([A-Z_0-9]{3,4})4\b", text):
            before = re.findall(r"^    (?:struct|class)\s+(\w+)\b", text[: match.start()], re.M)
            if before:
                structs[match.group(1)] = before[-1]
    return structs


def read_stored_records():
    """The ESM4 structs that ESMStore keeps a store for."""
    return set(re.findall(r"Store<ESM4::(\w+)>", read_text(STORE)))


# --------------------------------------------------------------------------------------------------------------------
# Sorting into libraries


def command_topic(name):
    for topic, pattern in COMMAND_TOPICS:
        if re.search(pattern, name):
            return topic
    return TOPIC_FALLBACK


def unit(name, state, note="", extra=None):
    result = {"n": name, "s": state}
    if note:
        result["t"] = note
    if extra:
        result.update(extra)
    return result


def command_libraries(handlers, listed, stand_ins, fixed):
    """The libraries of vanilla script commands and a list of handler names the table does not know."""
    names, codes = handlers
    commands = read_vanilla_commands(read_text(VANILLA_TABLE))
    ignored = listed["ignored"]
    switches = listed["switches"]
    libraries = {}
    known = set()
    for command in commands:
        keys = [command["name"].lower()] + ([command["short"].lower()] if command["short"] else [])
        known.update(keys)
        file = next((names[key] for key in keys if key in names), codes.get(command["code"]))
        reason = next((ignored[key] for key in keys if key in ignored), None)
        switch = next((switches[key] for key in keys if key in switches), None)
        if file is None and (reason is not None or switch is not None):
            # Registered in a loop over the arrays of switches.cpp
            file = SWITCHES
        override = next((stand_ins[key] for key in keys if key in stand_ins), None)
        where = "New Vegas only" if not command["fallout3"] else "Fallout 3 and New Vegas"
        if file is None:
            state, note = TODO, "no handler"
        elif reason is not None:
            state, note = STAND_IN, "accepted and does nothing: " + reason
        elif switch is not None:
            state, note = STAND_IN, "keeps the switch and does nothing with it: " + switch
        elif override is not None and override[0] == "stand-in":
            state, note = STAND_IN, override[1]
        elif override is None and any(key in fixed for key in keys):
            state, note = STAND_IN, "answers a fixed value (not checked against the games)"
        else:
            state, note = DONE, file.rsplit("/", 1)[-1]
        extra = {"c": "0x%04X" % command["code"], "w": where}
        libraries.setdefault(command_topic(command["name"]), []).append(unit(command["name"], state, note, extra))
    unknown = sorted(name for name in names if name not in known)
    return libraries, unknown, len(commands)


def extender_libraries(handlers, entries, ignored, stand_ins, ttw):
    """One library for each extender plugin."""
    names, codes = handlers
    libraries = []
    for file, title, maker in EXTENDER_PLUGINS:
        units = []
        for row in read_extender_table(file):
            code = row["code"]
            key = row["name"].lower()
            registered = entries.get(code, "").lower() == key
            has_handler = registered and (key in names or code in codes)
            if registered and key in ignored:
                state, note = STAND_IN, "accepted and does nothing: " + ignored[key]
            elif has_handler:
                state, note = DONE, (names.get(key) or codes.get(code)).rsplit("/", 1)[-1]
            elif registered:
                state, note = TODO, "in the command table with its arguments, no handler"
            else:
                state, note = TODO, "not in the command table"
            extra = {"c": "0x%04X" % code}
            if code in ttw:
                extra["ttw"] = ttw[code]
            units.append(unit(row["name"], state, note, extra))
        libraries.append({"name": title, "family": "extender", "about": maker, "units": units})
    return libraries


def record_libraries(census, structs, stored):
    libraries = {name: [] for name, _ in RECORD_GROUPS}
    group_of = {}
    for name, codes in RECORD_GROUPS:
        for code in codes.split():
            group_of[code] = name
    for code, fallout3, new_vegas in census:
        struct = structs.get(code)
        if struct is None:
            state, note = TODO, "no loader"
        elif struct in stored:
            state, note = DONE, "loaded and kept in the record store (ESM4::%s)" % struct
        else:
            state, note = STAND_IN, "loaded, then dropped: no record store (ESM4::%s)" % struct
        counts = "Fallout3.esm %s, FalloutNV.esm %s records" % (format(fallout3, ","), format(new_vegas, ","))
        group = group_of.get(code)
        if group is None:
            raise SystemExit("The record type %s is in no group of RECORD_GROUPS" % code)
        libraries[group].append(unit(code, state, note + "; " + counts))
    return libraries


def count(units):
    totals = {DONE: 0, STAND_IN: 0, TODO: 0}
    for item in units:
        totals[item["s"]] += 1
    return totals


def build():
    handlers = read_handlers(HANDLER_SOURCES)
    switches_text = read_text(SWITCHES)
    listed = {
        "ignored": read_listed_commands(switches_text, "ignored"),
        "switches": read_listed_commands(switches_text, "switchCommands"),
    }
    stand_ins = read_stand_ins(read_text(STAND_INS))
    fixed = read_fixed_answers(HANDLER_SOURCES)
    fixed = {name: file for name, file in fixed.items() if stand_ins.get(name, ("", ""))[0] != "exact"}
    entries = read_extender_entries(read_text(EXTENDER_ENTRIES))
    ttw = read_ttw_calls()

    libraries = []
    topics, unknown, command_total = command_libraries(handlers, listed, stand_ins, fixed)
    for topic in [t for t, _ in COMMAND_TOPICS] + [TOPIC_FALLBACK]:
        if topic in topics:
            libraries.append({"name": topic, "family": "command", "about": "script commands", "units": topics[topic]})
    libraries.extend(extender_libraries(handlers, entries, listed["ignored"], stand_ins, ttw))
    records = record_libraries(read_census_types(read_text(CENSUS)), read_loaders(), read_stored_records())
    for group, _ in RECORD_GROUPS:
        libraries.append({"name": group, "family": "record", "about": "record types", "units": records[group]})

    for library in libraries:
        library["units"].sort(key=lambda item: ({DONE: 0, STAND_IN: 1, TODO: 2}[item["s"]], item["n"].lower()))
        library["count"] = count(library["units"])
    return {
        "generated": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        "commit": git_commit(),
        "libraries": libraries,
        "unknownHandlers": unknown,
        "ttwTotal": len(ttw),
    }


def git_commit():
    try:
        out = subprocess.run(
            ["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True, text=True, check=True
        ).stdout.strip()
        dirty = subprocess.run(
            ["git", "-C", REPO, "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True, check=True
        ).stdout.strip()
        return out + (" (with changes)" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def summary(data):
    lines = []
    families = {}
    for library in data["libraries"]:
        total = sum(library["count"].values())
        done = library["count"][DONE]
        lines.append(
            "%-36s %5d units  %5d done  %4d stand-in  %5.1f%%"
            % (library["name"], total, done, library["count"][STAND_IN], 100.0 * done / total if total else 0)
        )
        family = families.setdefault(library["family"], {DONE: 0, STAND_IN: 0, TODO: 0})
        for key, value in library["count"].items():
            family[key] += value
    lines.append("")
    for family, totals in families.items():
        total = sum(totals.values())
        lines.append(
            "%-36s %5d units  %5d done  %4d stand-in  %5.1f%%"
            % (family, total, totals[DONE], totals[STAND_IN], 100.0 * totals[DONE] / total)
        )
    if data["unknownHandlers"]:
        lines.append("")
        lines.append("Handlers for names that are in no command table: " + ", ".join(data["unknownHandlers"]))
    return "\n".join(lines)


PAGE_START = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<style>body { margin: 0; } [hidden] { display: none !important; }</style>
"""
PAGE_END = "</head>\n<body>\n"


def render_html(data, template_path, fragment=False):
    """The page with the data in it. The page of the file is a fragment (a title, styles, markup and a script, with no
    html, head or body), which is the form the artifact host takes; unless fragment is asked for, it is wrapped to
    open in a browser by itself."""
    with open(template_path, encoding="utf-8") as stream:
        template = stream.read()
    payload = json.dumps(data, separators=(",", ":"), ensure_ascii=False).replace("</", "<\\/")
    marker = "/*COVERAGE_DATA*/null"
    if marker not in template:
        raise SystemExit("%s has no %s to put the data in" % (template_path, marker))
    page = template.replace(marker, payload)
    if fragment:
        return page
    # The title, meta and style belong in the head and the rest in the body
    split = page.index("</style>") + len("</style>")
    return PAGE_START + page[:split] + PAGE_END + page[split:] + "</body>\n</html>\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--html", help="write the page here")
    parser.add_argument("--json", help="write the data here")
    parser.add_argument(
        "--template",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "coverage_dashboard.html"),
        help="the page the data goes into (default: coverage_dashboard.html next to this script)",
    )
    parser.add_argument(
        "--fragment", action="store_true", help="write the page without the html, head and body tags (for an artifact)"
    )
    parser.add_argument("--quiet", action="store_true", help="do not print the table of numbers")
    args = parser.parse_args()
    data = build()
    if not args.quiet:
        print(summary(data))
    if args.json:
        with open(args.json, "w", encoding="utf-8") as stream:
            json.dump(data, stream, indent=1, ensure_ascii=False)
    if args.html:
        with open(args.html, "w", encoding="utf-8") as stream:
            stream.write(render_html(data, args.template, args.fragment))
    return 0


if __name__ == "__main__":
    sys.exit(main())
