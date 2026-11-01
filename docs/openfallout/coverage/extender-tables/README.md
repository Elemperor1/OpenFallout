# Command tables read from the extender files Jacob sent (2026-10-20)

One CSV per file, one row per code in the order the plugin registers its commands (code = opcode base + position).
Read statically from the DLLs (nothing was run); help texts are left out. Parameters are `typeID` (`?` = optional),
the labels are the plugin's own words for them. `returns` is the type a typed registration declares (empty = a number
or nothing). Method: docs/openfallout/script-extender-plan.md, section "What the extender files gave".

xnvse_core.csv covers 0x1400 to 0x168D from nvse_1_4.dll (xNVSE 6.4.9); the others are the plugin DLLs.
