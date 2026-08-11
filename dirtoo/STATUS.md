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
Async location-bar path completion, dual view, zoom, DND, clipboard (dirtoo + uri-list + GNOME), background
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


## Version number handling

- **Source of truth:** top-level `VERSION` file only (currently `0.2.0-dev`).
- Inside git the string keeps a `-dev` suffix until release.
- CMake reads `VERSION` → `PROJECT_VERSION_FULL` and defines `DIRTOO_VERSION`.
- Packaging may pass `-DPROJECT_VERSION_FULL=...`. The flake builds:

      `{VERSION}.{revCount}+g{shortRev}`

  e.g. `0.2.0-dev.1509+g2fdf60f` (`self.revCount` + `self.shortRev`).
- GUI: Help → About and `dirtoo --version`.
- CLI tools: `--version` / `-V`.
- Release: drop `-dev`, commit `VERSION`, tag `vX.Y.Z` matching the file with a `v` prefix.


## Flake package outputs

| Attribute | Contents |
|-----------|----------|
| `default` / `dirtoo` | Full app + libs + tools + tests |
| `dirops` | Qt-free file-ops library + `dt-*` tools |
| `dirtoo-fs` | Location / FileInfo |
| `dirtoo-collection` | Filter/sort collection (+ fs) |
| `dirtoo-watcher` | Directory watcher (Qt) |
| `dirtoo-thumbnail` | Thumbnailer D-Bus client (Qt) |
| `dirtoo-archive` | Archive index/manager (Qt) |
| `all-libs` | symlinkJoin of all libraries |

```bash
nix build .#dirops
nix build .#dirtoo-fs
nix build .#all-libs
nix build .#dirtoo
```


## Independent library packages

Each directory under `libs/` is a standalone CMake project that installs a
`*Config.cmake` package. The top-level `dirtoo` project does **not**
`add_subdirectory` the libs; it uses `find_package`:

- `dirops::dirops`
- `dirtoo-fs::dirtoo-fs`
- `dirtoo-collection::dirtoo-collection`
- `dirtoo-watcher::dirtoo-watcher`
- `dirtoo-thumbnail::dirtoo-thumbnail`
- `dirtoo-archive::dirtoo-archive`

The flake builds libraries first, then configures the app with those
packages on `CMAKE_PREFIX_PATH` (via Nix `buildInputs`).
