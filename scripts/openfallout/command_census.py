#!/usr/bin/env python3
"""Ranks the script commands of Fallout 3 and New Vegas by how often the games' own data calls them, and shows which
of them the engine does not have a handler for.

The compiled scripts of the games (object and quest scripts, the results of dialogue and quest stages, terminals,
packages and perks) call commands, and the conditions of dialogue, quests, terminals, packages and perks name
functions, which are commands too. A command that has no handler is stepped over and logged, and a condition that names
one answers as if it were false or true, so a quest or a conversation can stall with no error. This counts how many
places call each command, so that the list of what is missing can be worked in the order the games need it.

The counts come from `esmtool`, which reads the plugins (load order, masters first, and the commands file that
dump_command_table.py writes):

    esmtool scriptcode --commands command_table_fnv.csv --csv nv-scripts.csv FalloutNV.esm DeadMoney.esm ...
    esmtool dialogue   --commands command_table_fnv.csv --csv nv-dialogue.csv FalloutNV.esm DeadMoney.esm ...

and the same for Fallout 3. This script reads those CSV files (one line for each count: kind, code, name, measure,
count) and the state of each command from the source tree, the way the coverage dashboard does:

    scripts/openfallout/command_census.py --fallout3 fo3-scripts.csv fo3-dialogue.csv \\
        --newvegas nv-scripts.csv nv-dialogue.csv \\
        --markdown docs/openfallout/command-census.md --csv docs/openfallout/coverage/tracker/command-usage.csv

It prints the summary; with --markdown it writes the page and with --csv the table of every command. Only codes and
counts are kept: no script, name of a record, text or number from the games.

A "use" is a place that calls the command: a statement or an expression of a script, or a condition. A command that
two conditions of one response name counts twice. Scripts are counted once for each record that holds them (a script
that a later plugin overrides counts in both), so the counts are for ordering the work, not exact tallies of the games.

Needs Python 3 only.
"""
import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coverage_dashboard as dashboard  # noqa: E402

GAMES = ("fallout3", "newvegas")
GAME_TITLES = {"fallout3": "Fallout 3", "newvegas": "New Vegas"}
FIRST_COMMAND = 0x1000
# How many rows of each table the page lists
DEFAULT_TOP = 60

STATE_TITLES = {dashboard.DONE: "handler", dashboard.STAND_IN: "stand-in", dashboard.TODO: "no handler"}


def read_census_csv(path):
    """The counts of one CSV file of esmtool: {code: {(kind, measure): count}}."""
    counts = {}
    with open(path, newline="", encoding="utf-8") as stream:
        reader = csv.reader(stream)
        header = next(reader, None)
        if header is None or [name.strip() for name in header[:5]] != ["kind", "code", "name", "measure", "count"]:
            raise SystemExit("%s: not a census file (the first line is not kind,code,name,measure,count)" % path)
        for number, row in enumerate(reader, 2):
            if not row:
                continue
            if len(row) != 5:
                raise SystemExit("%s line %d: %d fields, expected 5" % (path, number, len(row)))
            try:
                code = int(row[1], 0)
                count = int(row[4])
            except ValueError:
                raise SystemExit("%s line %d: the code or the count is not a number" % (path, number))
            key = (row[0], row[3])
            per_command = counts.setdefault(code, {})
            per_command[key] = per_command.get(key, 0) + count
    return counts


def merge_counts(files):
    """Adds the counts of several census files (the scripts file and the dialogue file of a game)."""
    merged = {}
    for counts in files:
        for code, per_command in counts.items():
            target = merged.setdefault(code, {})
            for key, count in per_command.items():
                target[key] = target.get(key, 0) + count
    return merged


def uses_of(per_command):
    """What the counts of one command in one game come to: places that call it as a statement or in an expression,
    scripts that call it, and conditions that name it."""
    get = per_command.get
    calls = get(("call", "statements"), 0) + get(("call", "expressions"), 0)
    conditions = get(("condition", "uses"), 0) + get(("dialogue_condition", "uses"), 0)
    return {"calls": calls, "scripts": get(("call", "scripts"), 0), "conditions": conditions, "uses": calls + conditions}


def command_states():
    """The state of every command of the table in the source tree: {code: {name, state, note, fallout3}}. The state is
    the coverage dashboard's (done, stand-in or todo)."""
    handlers = dashboard.read_handlers(dashboard.HANDLER_SOURCES)
    switches_text = dashboard.read_text(dashboard.SWITCHES)
    listed = {
        "ignored": dashboard.read_listed_commands(switches_text, "ignored"),
        "switches": dashboard.read_listed_commands(switches_text, "switchCommands"),
    }
    stand_ins = dashboard.read_stand_ins(dashboard.read_text(dashboard.STAND_INS))
    fixed = dashboard.read_fixed_answers(dashboard.HANDLER_SOURCES)
    fixed = {name: file for name, file in fixed.items() if stand_ins.get(name, ("", ""))[0] != "exact"}
    topics, _, _ = dashboard.command_libraries(handlers, listed, stand_ins, fixed)
    states = {}
    for units in topics.values():
        for item in units:
            states[int(item["c"], 16)] = {
                "name": item["n"],
                "state": item["s"],
                "note": item.get("t", ""),
                "fallout3": item["w"] != "New Vegas only",
            }
    return states


def build_rows(games, states):
    """One row for each command of the table, and for each code the data calls that the table does not have:
    {code, name, state, note, fallout3 (does the game have it), per game {calls, scripts, conditions, uses}, uses}."""
    codes = set(states)
    for counts in games.values():
        codes.update(counts)
    rows = []
    for code in sorted(codes):
        state = states.get(code)
        row = {
            "code": code,
            "name": state["name"] if state else "",
            "state": state["state"] if state else "unknown",
            "note": state["note"] if state else "not in the table of commands",
            "fallout3": state["fallout3"] if state else False,
        }
        total = 0
        for game in GAMES:
            row[game] = uses_of(games.get(game, {}).get(code, {}))
            total += row[game]["uses"]
        row["uses"] = total
        rows.append(row)
    return rows


def ranked(rows, state):
    return sorted((r for r in rows if r["state"] == state and r["uses"] > 0), key=lambda r: (-r["uses"], r["code"]))


def share(part, whole):
    return "%.1f%%" % (100.0 * part / whole) if whole else "0%"


def summary_lines(rows):
    """The first lines of the page: how the uses split over the states of the commands."""
    table = [r for r in rows if r["state"] != "unknown"]
    lines = []
    total_uses = sum(r["uses"] for r in table)
    for state in (dashboard.DONE, dashboard.STAND_IN, dashboard.TODO):
        group = [r for r in table if r["state"] == state]
        used = [r for r in group if r["uses"] > 0]
        uses = sum(r["uses"] for r in group)
        lines.append((STATE_TITLES[state], len(group), len(used), uses, share(uses, total_uses)))
    return lines


def render_markdown(rows, top, games_given):
    table = [r for r in rows if r["state"] != "unknown"]
    unknown = [r for r in rows if r["state"] == "unknown"]
    out = []
    out.append("# Which script commands the games call\n")
    out.append(
        "How often the plugins of the games call each script command, and which of them the engine has no handler for. "
        "It is for ordering the work on the rules of the world (milestone 16) and for finding the commands that stall a "
        "quest. The counts come from `esmtool scriptcode` and `esmtool dialogue` (see "
        "`scripts/openfallout/command_census.py`); they hold codes and counts only.\n"
    )
    out.append(
        "A **use** is a place that calls the command: a statement or an expression in a compiled script (object and "
        "quest scripts, the results of dialogue and quest stages, terminals, packages and perks), or a condition that "
        "names it (dialogue, quests, terminals, packages and perks). A script is counted for each record that holds it, "
        "so the figures are for ranking, not exact tallies.\n"
    )
    out.append("Games counted: " + ", ".join(GAME_TITLES[g] for g in games_given) + ".\n")

    out.append("## How the uses split\n")
    out.append("| State of the command | Commands | Called at all | Uses | Share of the uses |")
    out.append("|---|---:|---:|---:|---:|")
    for title, commands, used, uses, percent in summary_lines(rows):
        out.append("| %s | %d | %d | %d | %s |" % (title, commands, used, uses, percent))
    out.append("")
    unused = [r for r in table if r["state"] == dashboard.TODO and r["uses"] == 0]
    out.append(
        "%d commands with no handler are called by no script and no condition of the plugins counted; they can wait "
        "for the games' own gaps (the extenders, the console) or never be built.\n" % len(unused)
    )

    def listing(title, state, note_column):
        group = ranked(rows, state)
        out.append("## %s\n" % title)
        if not group:
            out.append("None.\n")
            return
        whole = sum(r["uses"] for r in group)
        header = "| # | Code | Command | FO3 calls | FO3 conditions | NV calls | NV conditions | Uses | Running share |"
        out.append(header + (" Note |" if note_column else ""))
        out.append("|--:|---|---|--:|--:|--:|--:|--:|--:|" + ("---|" if note_column else ""))
        running = 0
        for rank, r in enumerate(group[:top], 1):
            running += r["uses"]
            cells = "| %d | 0x%04X | %s | %d | %d | %d | %d | %d | %s |" % (
                rank,
                r["code"],
                r["name"],
                r["fallout3"]["calls"],
                r["fallout3"]["conditions"],
                r["newvegas"]["calls"],
                r["newvegas"]["conditions"],
                r["uses"],
                share(running, whole),
            )
            out.append(cells + (" %s |" % r["note"] if note_column else ""))
        if len(group) > top:
            out.append("")
            out.append(
                "%d more commands with uses are in `docs/openfallout/coverage/tracker/command-usage.csv`; the "
                "table is %d commands with %d uses in all.\n" % (len(group) - top, len(group), whole)
            )
        else:
            out.append("")

    listing("Commands with no handler, by uses", dashboard.TODO, False)
    listing("Stand-ins, by uses", dashboard.STAND_IN, True)

    if unknown:
        out.append("## Codes the table of commands does not have\n")
        out.append(
            "These codes are called by the data and are in no command table (the commands of the script extenders "
            "start at 0x1400): "
            + ", ".join("0x%04X (%d)" % (r["code"], r["uses"]) for r in sorted(unknown, key=lambda r: -r["uses"])[:40])
            + ".\n"
        )
    return "\n".join(out)


def write_csv(rows, path):
    with open(path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(
            [
                "code",
                "name",
                "state",
                "fo3_calls",
                "fo3_scripts",
                "fo3_conditions",
                "nv_calls",
                "nv_scripts",
                "nv_conditions",
                "uses",
            ]
        )
        for r in sorted(rows, key=lambda r: (-r["uses"], r["code"])):
            writer.writerow(
                [
                    "0x%04X" % r["code"],
                    r["name"],
                    r["state"],
                    r["fallout3"]["calls"],
                    r["fallout3"]["scripts"],
                    r["fallout3"]["conditions"],
                    r["newvegas"]["calls"],
                    r["newvegas"]["scripts"],
                    r["newvegas"]["conditions"],
                    r["uses"],
                ]
            )


def refers_to_any(path, inputs):
    """Whether `path` is the same file as one of `inputs` (the same name, or a link or hard link to it)."""
    for other in inputs:
        try:
            if os.path.samefile(path, other):
                return True
        except OSError:
            # a path that is not there yet is none of the inputs
            continue
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fallout3", nargs="+", metavar="CSV", default=[], help="the census files of the Fallout 3 plugins")
    parser.add_argument("--newvegas", nargs="+", metavar="CSV", default=[], help="the census files of the New Vegas plugins")
    parser.add_argument("--markdown", help="write the page here")
    parser.add_argument("--csv", help="write the table of every command here")
    parser.add_argument("--top", type=int, default=DEFAULT_TOP, help="how many rows each list of the page has")
    args = parser.parse_args()
    if not args.fallout3 and not args.newvegas:
        parser.error("give the census files of at least one game (--fallout3, --newvegas)")
    # Writing a report over a census file would replace what the report is made of
    for option, output in (("--markdown", args.markdown), ("--csv", args.csv)):
        if output and refers_to_any(output, args.fallout3 + args.newvegas):
            parser.error("%s names one of the census files: the report would replace it" % option)

    games = {
        "fallout3": merge_counts([read_census_csv(path) for path in args.fallout3]),
        "newvegas": merge_counts([read_census_csv(path) for path in args.newvegas]),
    }
    given = [game for game in GAMES if (args.fallout3 if game == "fallout3" else args.newvegas)]
    rows = build_rows(games, command_states())
    for title, commands, used, uses, percent in summary_lines(rows):
        print("%-11s %4d commands, %4d called, %7d uses (%s)" % (title, commands, used, uses, percent))
    if args.markdown:
        with open(args.markdown, "w", encoding="utf-8") as stream:
            stream.write(render_markdown(rows, args.top, given))
    if args.csv:
        write_csv(rows, args.csv)
    return 0


if __name__ == "__main__":
    sys.exit(main())
