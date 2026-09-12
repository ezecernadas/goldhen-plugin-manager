# GoldHEN Plugin Manager

A minimal PlayStation 4 homebrew application for [GoldHEN](https://github.com/GoldHEN/GoldHEN)
that manages which plugins are loaded, globally and per game, without editing `plugins.ini`
by hand over FTP.

It does one thing only: toggle plugins on and off. No cheats, no downloads, no updater.

## Screens

1. **Targets** — the fixed **Global** entry (the `[default]` section) followed by every installed
   game/app with its icon and name. D-pad to move, Cross to open, Circle to exit.
2. **Plugins** — one checkbox per `.prx` found in `/data/GoldHEN/plugins/`. Cross toggles a
   plugin for the selected target; Circle saves and goes back. On the `gamepad_helper.prx` row,
   Square opens the Gamepad Helper screen.
3. **Gamepad Helper** — edits the `gamepad.ini` section of the selected target (`[default]` for
   Global): dead zones, touchpad buttons, button remapping and vibration intensity. Left/Right
   change a value (L1/R1 step the dead zones by 10), Cross cycles the on/off rows (default, on, off) and steps every other row forward; L1/R1 move the dead-zone values by 10, Circle saves
   and goes back. The value `default` means the key is absent from the file, so the plugin uses
   its own default.

GoldHEN reads `plugins.ini` when a game process is launched, so changes apply the next time
the game is (re)launched.

## Files

| Path | Purpose |
|---|---|
| `/data/GoldHEN/plugins.ini` | Regenerated in full on every save (written to `plugins.ini.tmp`, then renamed). |
| `/data/GoldHEN/plugins.ini.bak` | One-time backup of the original file, taken before the first write. |
| `/data/GoldHEN/plugin_manager.json` | Source of truth: every target, every plugin (enabled or not) and its arguments. |
| `/data/GoldHEN/plugin_manager.json.broken` | Where an unreadable sidecar is moved before re-importing `plugins.ini`. |
| `/data/GoldHEN/gamepad.ini` | Gamepad Helper config, regenerated in full when saved from its screen (tmp + rename). Unknown keys are preserved; comment lines are dropped. |
| `/data/GoldHEN/gamepad.ini.bak` | One-time backup of the original `gamepad.ini`, taken before the first write. |

On first run the existing `plugins.ini` is imported into the sidecar without losing anything:
plugin arguments (`plugin.prx ; args`), sections of games that are not installed, and plugins
that are no longer on disk are all kept in the JSON even though they are not shown.

### Notes on `plugins.ini`

- Plugin arguments after `;` on the same line are preserved verbatim when a plugin is disabled
  and re-enabled.
- Because `;` introduces arguments, a `;` at the start of a line **cannot** be used to comment out
  a plugin: such a line has an empty path and is dropped. Disabled plugins are simply omitted from
  the regenerated file and remembered in `plugin_manager.json`.
- Lines starting with `#` are treated as comments and are not preserved.
- After every save the app applies `chmod 0777` to `plugins.ini` and to every file in
  `/data/GoldHEN/plugins/`.

### Installed game detection

Games are read from `/system_data/priv/mms/app.db` (`tbl_appinfo`, keys `TITLE` and `CATEGORY`).
The `CATEGORY` values used to keep only games and user apps (`gd` = games, `gde` = homebrew and
user apps; `gda`, `gdi`, `gdn` are system applications) were verified against a real `app.db` and
are defined in `include/app.h` as `APPDB_USER_CATEGORIES`.

## Building

Requirements:

- [Open Orbis SDK](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/) (`OO_PS4_TOOLCHAIN` must point to it)
- [SDL2](https://github.com/bucanero/SDL-PS4/tree/ps4) library. The `libSDL2.a` bundled with the
  OpenOrbis toolchain release crashes inside `SDL_Init()` on the console (verified); build the
  `ps4` branch of the fork and copy its `libSDL2.a` over the toolchain one.
- [libjbc](https://github.com/bucanero/ps4-libjbc) library
- [SQLite](https://github.com/bucanero/libSQLite-ps4) library, with `patches/libSQLite-ps4-libc-vfs.patch`
  applied before `make install` (see below)
- [dbglogger](https://github.com/bucanero/dbglogger) library

```
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
make            # builds IV0000-PLGM00001_00-GOLDHENPLUGINMGR.pkg
make test       # runs the host-side unit tests of the ini/json model (needs only a C compiler)
```

`PkgTool.Core` (shipped with the toolchain) is a .NET Core 3.0 binary that needs OpenSSL 1.1.
On distributions that only ship OpenSSL 3 (Ubuntu 24.04 and later), extract the `libssl1.1`
package somewhere and point the Makefile at it:

```
export PKGTOOL_LIBSSL_DIR=/path/to/libssl1.1/usr/lib/x86_64-linux-gnu
```

On Ubuntu, the Debian-packaged `llvm-ar` is only available versioned; add
`/usr/lib/llvm-18/bin` (or your LLVM version) to `PATH` before building the ported libraries.

### Notes on the ported libraries

- **SQLite VFS patch.** GoldHEN injects the plugins listed under `[default]` into every process,
  including this application. At least one of them hooks `sceKernelOpen`, and calling it from
  this process crashes with CE-34878-0 (the libc `open()` in the toolchain's static `libc.a`
  is a raw syscall wrapper and is not affected). `patches/libSQLite-ps4-libc-vfs.patch` makes the
  `orbis_rw` VFS use `open/read/lseek/write/close` from libc instead of the `sceKernel*`
  wrappers. Apply it in the `libSQLite-ps4` checkout with `patch -p1 < .../libSQLite-ps4-libc-vfs.patch`.
- **SDL-PS4 with a recent clang.** The fork's build expects clang 12. With clang 18 pass
  `CFLAGS="-Wno-declaration-after-statement -Wno-error"` to `cmake`, drop the
  `-Werror=declaration-after-statement` flag from its `CMakeLists.txt`, and use CMake 3.x
  (CMake 4 rejects the fork's minimum-version declaration).

`make test` compiles `source/ini_model.c` with the host compiler (`HOSTCC`, default `cc`) and
runs `tests/test_ini_model.c`; it does not need the PS4 toolchain.

To enable debug logging, build with `make DEBUGLOG=1`. The application then sends debug messages
to UDP multicast address `239.255.0.100:30000`:

```
$ socat udp4-recv:30000,ip-add-membership=239.255.0.100:0.0.0.0 -
```

## Credits

This project is a fork of [PS4 Cheats Manager](https://github.com/bucanero/PS4CheatsManager) by
[Bucanero](https://github.com/bucanero), stripped down to plugin management. The rendering, input,
dialog and privilege-escalation code comes from that project.

- [Bucanero](https://github.com/bucanero): PS4 Cheats Manager, SDL-PS4, libjbc, libSQLite-ps4, dbglogger
- [SiSTRo](https://github.com/SiSTR0): [GoldHEN](https://github.com/GoldHEN/GoldHEN)
- [Dnawrkshp](https://github.com/Dnawrkshp/): [Artemis PS3](https://github.com/Dnawrkshp/ArtemisPS3) (original GUI code)
- [Berion](https://www.psx-place.com/members/berion.1431/): GUI design
- [OpenOrbis](https://github.com/OpenOrbis): PS4 toolchain

## License

GoldHEN Plugin Manager is based on PS4 Cheats Manager, Copyright (C) 2022-2024 Damian Parrino.

This program is free software: you can redistribute it and/or modify it under the terms of the
[GNU General Public License](LICENSE) as published by the Free Software Foundation, either
version 3 of the License, or (at your option) any later version.
