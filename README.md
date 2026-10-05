OpenFallout
===========

OpenFallout is an open-source game engine project that aims to play Fallout 3, Fallout: New Vegas and Tale of Two Wastelands from your own copy of the games. You need to own the games; none of their content is included or distributed.

It is built on [OpenMW](https://openmw.org), the open-source Morrowind engine, and starts from OpenMW's renderer, physics, audio, Lua scripting and content tools. OpenFallout is an independent project. It is not affiliated with or endorsed by the OpenMW team or Bethesda Softworks.

* Version: 0.52.0 (the OpenMW release it was started from, OpenFallout has not made a release yet)
* License: GPLv3 (see [LICENSE](LICENSE) for more information)
* Source code and bug tracker: https://github.com/Elemperor1/OpenFallout

Font Licenses:
* DejaVuLGCSansMono.ttf: custom (see [files/data/fonts/DejaVuFontLicense.txt](files/data/fonts/DejaVuFontLicense.txt) for more information)
* DemonicLetters.ttf: SIL Open Font License (see [files/data/fonts/DemonicLettersFontLicense.txt](files/data/fonts/DemonicLettersFontLicense.txt) for more information)
* MysticCards.ttf: SIL Open Font License (see [files/data/fonts/MysticCardsFontLicense.txt](files/data/fonts/MysticCardsFontLicense.txt) for more information)

Current Status
--------------

OpenFallout cannot play Fallout yet. What runs today is the OpenMW engine it inherited, which plays Morrowind. The Fallout 3 and New Vegas plugin readers exist and parse many record types, but quests, dialogue, scripts, actors, mechanics and saves for those games are not implemented.

The plan, with the milestones that lead from here to a playable game, is in [docs/openfallout/gap-audit-and-roadmap.md](docs/openfallout/gap-audit-and-roadmap.md). How to build the project and what the build and test baseline looks like is in [docs/openfallout/build-baseline.md](docs/openfallout/build-baseline.md).

The project is being renamed from OpenMW in stages. Programs, configuration files, log files and settings directories already use the OpenFallout name (`openfallout`, `openfallout-launcher`, `openfallout.cfg`, `~/.config/openfallout`), so an existing OpenMW installation and its settings are not shared with OpenFallout. Lua module ids (`openfallout.core`), build options and environment variables (`OPENFALLOUT_*`) and the built-in l10n contexts (`OFEngine`) follow it too, which means Lua mods written for OpenMW need their `require` lines changed. Still to change: the `apps/openmw` directory and the `MW*` namespaces, then shader, asset and file extension names (`omw_*` in shaders, `scripts/omw`, `.omwgame` and `.omwaddon`) and the OpenMW logo and icons.

Acknowledgements
----------------

OpenFallout would not exist without the work of the OpenMW team and its contributors over more than fifteen years. Their names are kept in [AUTHORS.md](AUTHORS.md) and the copyright notices in the source files stay as they are. OpenMW's own project pages are at https://openmw.org and https://gitlab.com/OpenMW/openmw.

Getting Started
---------------

* [Build from source](docs/openfallout/build-baseline.md)
* [Roadmap](docs/openfallout/gap-audit-and-roadmap.md)
* [Report a bug](https://github.com/Elemperor1/OpenFallout/issues)
* The manuals under [docs/source](docs/source) are OpenMW's and still describe the Morrowind engine that OpenFallout starts from. Program and file names in them have been updated.

The data path
-------------

The data path tells OpenFallout where to find your Morrowind files. If you run the launcher, OpenFallout should be able to pick up the location of these files on its own, if both Morrowind and OpenFallout are installed properly (installing Morrowind under WINE is considered a proper install).

Command line options
--------------------

    Syntax: openfallout <options>
    Allowed options:
      --config arg                          additional config directories
      --replace arg                         settings where the values from the
                                            current source should replace those
                                            from lower-priority sources instead of
                                            being appended
      --user-data arg                       set user data directory (used for
                                            saves, screenshots, etc)
      --resources arg (=resources)          set resources directory
      --help                                print help message
      --version                             print version information and quit
      --data arg (=data)                    set data directories (later directories
                                            have higher priority)
      --data-local arg                      set local data directory (highest
                                            priority)
      --fallback-archive arg (=fallback-archive)
                                            set fallback BSA archives (later
                                            archives have higher priority)
      --start arg                           set initial cell
      --content arg                         content file(s): esm/esp, or
                                            omwgame/omwaddon/omwscripts
      --groundcover arg                     groundcover content file(s): esm/esp,
                                            or omwgame/omwaddon
      --no-sound [=arg(=1)] (=0)            disable all sounds
      --script-all [=arg(=1)] (=0)          compile all scripts (excluding dialogue
                                            scripts) at startup
      --script-all-dialogue [=arg(=1)] (=0) compile all dialogue scripts at startup
      --script-console [=arg(=1)] (=0)      enable console-only script
                                            functionality
      --script-run arg                      select a file containing a list of
                                            console commands that is executed on
                                            startup
      --script-warn [=arg(=1)] (=1)         handling of warnings when compiling
                                            scripts
                                            0 - ignore warnings
                                            1 - show warnings but consider script as
                                            correctly compiled anyway
                                            2 - treat warnings as errors
      --load-savegame arg                   load a save game file on game startup
                                            (specify an absolute filename or a
                                            filename relative to the current
                                            working directory)
      --skip-menu [=arg(=1)] (=0)           skip main menu on game startup
      --new-game [=arg(=1)] (=0)            run new game sequence (ignored if
                                            skip-menu=0)
      --encoding arg (=win1252)             Character encoding used in OpenFallout game
                                            messages:

                                            win1250 - Central and Eastern European
                                            such as Polish, Czech, Slovak,
                                            Hungarian, Slovene, Bosnian, Croatian,
                                            Serbian (Latin script), Romanian and
                                            Albanian languages

                                            win1251 - Cyrillic alphabet such as
                                            Russian, Bulgarian, Serbian Cyrillic
                                            and other languages

                                            win1252 - Western European (Latin)
                                            alphabet, used by default
      --fallback arg                        fallback values
      --no-grab [=arg(=1)] (=0)             Don't grab mouse cursor
      --export-fonts [=arg(=1)] (=0)        Export Morrowind .fnt fonts to PNG
                                            image and XML file in current directory
      --activate-dist arg (=-1)             activation distance override
      --random-seed arg (=<impl defined>)   seed value for random number generator
