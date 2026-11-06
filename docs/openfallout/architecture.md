# OpenFallout developer map

This page is a route through the implementation, not a full architecture specification. Follow the code links for behavior and the project guides for detailed scope and evidence. OpenFallout builds on OpenMW code; that relationship does not establish parity with either Fallout game's engine.

## Startup and content

[`main.cpp`](../../apps/openfallout/main.cpp) enters the application and [`engine.cpp`](../../apps/openfallout/engine.cpp) wires together the runtime. During startup the engine creates the world and its record store, then loads the selected content before initializing the Fallout-specific host and continuing into the world.

Fallout plugin selection and ordering are implemented in [`components/esm4/pluginorder.cpp`](../../components/esm4/pluginorder.cpp). [`apps/openfallout/mwworld/esmloader.cpp`](../../apps/openfallout/mwworld/esmloader.cpp) dispatches TES3 and TES4 content formats; TES4 plugins use [`components/esm4/reader.hpp`](../../components/esm4/reader.hpp) and the record readers under `components/esm4/`. [`ESMStore::loadESM4`](../../apps/openfallout/mwworld/esmstore.cpp) routes parsed records into the typed store, and [`World::loadData`](../../apps/openfallout/mwworld/worldimp.cpp) performs the wider content-load sequence and record setup.

The load-order and activation rules are detailed in [Mods and load order](mods-and-load-order.md). Record support starts at the owning record reader (for example, `components/esm4/loadnpc.cpp` for NPC records); the store and [`apps/openfallout/mwclass/`](../../apps/openfallout/mwclass/) connect records to runtime object behavior.

## FalloutScript and its engine host

The reusable compiled-script model and interpreter live in [`components/falloutscript/`](../../components/falloutscript/). [`interpreter.hpp`](../../components/falloutscript/interpreter.hpp) defines the host boundary: the interpreter resolves script variables and commands through a `Host` and registered command handlers. The command table is in [`commandtable.hpp`](../../components/falloutscript/commandtable.hpp), with the vanilla command definitions in [`vanillacommands.inc`](../../components/falloutscript/vanillacommands.inc).

The OpenFallout host is [`OFScript::FalloutScripts`](../../apps/openfallout/mwscript/falloutscripts.hpp), implemented in [`falloutscripts.cpp`](../../apps/openfallout/mwscript/falloutscripts.cpp). It binds command handlers to world, dialogue, mechanics, UI, and other engine services. When changing a command, trace both the component-level interpreter/command contract and the host handler. The [script extender guide](script-extender-plan.md) documents extender scope and the [roadmap](gap-audit-and-roadmap.md) records milestone evidence and known gaps.

## Lua, UI, and saves

The application Lua runtime and C++ bindings live in [`apps/openfallout/mwlua/`](../../apps/openfallout/mwlua/); [`LuaManager::initPreLoad`, `contentFilesLoaded`, and `initPostLoad`](../../apps/openfallout/mwlua/luamanagerimp.cpp) show the load lifecycle. The public Lua API specifications are in [`files/lua_api/openfallout/`](../../files/lua_api/openfallout/), with generated reference pages under [`docs/source/reference/lua-scripting/`](../source/reference/lua-scripting/). UI behavior is split between those bindings, engine GUI/menu code, and Lua-facing menu scripts; start with [`menuscripts.cpp`](../../apps/openfallout/mwlua/menuscripts.cpp) and the [menu guide](menus.md).

Save orchestration is in [`apps/openfallout/mwscript/falloutsave.cpp`](../../apps/openfallout/mwscript/falloutsave.cpp) and [`apps/openfallout/mwstate/statemanagerimp.cpp`](../../apps/openfallout/mwstate/statemanagerimp.cpp). FalloutScript's save codec and versioned section contract are in [`components/falloutscript/savestate.hpp`](../../components/falloutscript/savestate.hpp), [`savescripts.cpp`](../../components/falloutscript/savescripts.cpp), and the `FalloutScripts::saveSections` implementation. Lua state has its own record path in [`LuaManager::write` and `readRecord`](../../apps/openfallout/mwlua/luamanagerimp.cpp). Keep the existing format versions, mapping behavior, and failure handling intact when changing saved state.

## How to read evidence

Source shows what is implemented. Unit tests show only the cases they exercise; find them alongside the owning component, including [`apps/components_tests/esm4/`](../../apps/components_tests/esm4/) and [`apps/openfallout_tests/`](../../apps/openfallout_tests/). Automated in-game evidence is recorded separately in the [harness guide](harness.md) and milestone sections of the [roadmap](gap-audit-and-roadmap.md). Manual play is a separate evidence category and should be claimed only where the project documentation records it. Do not infer game-engine parity from source resemblance, unit tests, or automated runs.
