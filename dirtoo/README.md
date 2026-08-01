# dirtoo (C++)

Modular Qt6 file manager and filesystem tools, rewritten in **C++23**.

The original Python/PyQt6 implementation lives in `../dirtoo-py/` as a
behavioral reference. This tree is the active port.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build
```

With Nix:

```bash
nix develop
cmake -B build -G Ninja && cmake --build build
```

## Layout

| Path | Purpose |
|------|---------|
| `libs/dirops` | Copy/move/rename/delete/mkdir (Qt-free; future standalone project) |
| `libs/dirtoo-fs` | `Location`, `FileInfo`, directory listing |
| `libs/dirtoo-collection` | Sorted/filtered file list |
| `libs/dirtoo-watcher` | Directory change notifications |
| `libs/dirtoo-thumbnail` | Freedesktop thumbnailer client (stub) |
| `apps/dirtoo` | GUI application |
| `tools/` | Small CLIs for testing dirops |
| `tests/` | Catch2 unit tests |

See `../TODO.md` and `../AGENTS.md` for the port plan and agent rules.

## License

GPL-3.0-or-later (SPDX headers on all sources).
