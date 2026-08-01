# dirtoo C++ port — status

## Layout
- `dirtoo-py/` — original Python/Qt reference (do not “fix” its bugs)
- `dirtoo/` — C++23 / Qt6 rewrite

## Modules
| Library | Purpose |
|---------|---------|
| dirops | copy/move/rename/remove (Qt-free) |
| dirtoo-fs | Location (file + archive URLs), FileInfo |
| dirtoo-collection | filter / sort / show-hidden |
| dirtoo-watcher | directory change notifications |
| dirtoo-thumbnail | Thumbnailer1 D-Bus client |
| dirtoo-archive | TOC listing + optional full/member extract |

## GUI highlights
Dual view, zoom, DND, clipboard (dirtoo + uri-list + GNOME), background
transfers, conflict dialogs, properties, open-with, menus, QSettings,
read-only archive browse (list-first).

## Build
```bash
cd dirtoo
nix develop   # or install Qt6 + CMake + Ninja + libarchive tools
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## License
GPL-3.0-or-later (SPDX headers on sources).
