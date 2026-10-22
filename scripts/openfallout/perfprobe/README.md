# Perf probe

A developer tool for M9. A player script that stands, turns round and runs for a fixed number of real seconds and logs how
long the frames took. Lines start with `PERF` (counts, milliseconds and form ids only, no text of the game):

- `PERF phase <name> ...` once per phase: `frames`, `mean`, `p50`, `p95`, `p99`, `max` (all in milliseconds), `over33`
  and `over100` (frames longer than 33.4 ms and 100 ms), `fps`, and where the player was (`cell` as the record id, the
  worldspace, the grid cell, the position);
- `PERF window <phase> t=<s> ...` the same for every ten seconds of a phase;
- `PERF hitch <phase> ms=<n> ...` for each frame of more than 100 ms (the first thirty);
- `PERF shot <n> <name>`: a screenshot was taken (the engine writes it to the `screenshots` folder of the user data);
- `PERF turn stalled`: the player ran less than 80 units in a second and turned a quarter to the right.

The phases are `settle` (25 s, a screenshot at the end), `stand` (20 s), `turn` (40 s, a full turn with a screenshot at each
quarter), `run` (90 s) and `end` (6 s, a screenshot). The probe quits the game after the last one.

```
openfallout --skip-menu --start WastelandNV:-18,0 --content FalloutNV.esm \
    --data scripts/openfallout/perfprobe --content perfprobe.omwscripts
```

A data folder that is loaded before this one can hold `scripts/perfprobeconfig.lua`, which returns a table with any of
`phases` (a list of `{ name, secs, shot, turn, run }`), `window` and `stall`; the smoke test uses it to keep the run short.
