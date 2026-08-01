# AGENTS.md — dirtoo C++ Port

## Project Overview

**dirtoo** is a Qt-based graphical file manager and a collection of
directory/file utilities. The original implementation is Python 3 +
PyQt6 (~20k lines of Python under `dirtoo-py/`). This repository is
being ported to **modern C++23** with Qt6, CMake, and a Nix flake.

| Path | Role |
|------|------|
| `dirtoo-py/` | Frozen Python reference (read-only for behavior, do not “fix” bugs) |
| `dirtoo/`    | New C++23 codebase (active development) |

License: **GPL-3.0-or-later**. Every source file must use REUSE-style
SPDX headers:

```cpp
// SPDX-FileCopyrightText: 2024–2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
```

(Adjust year/author when contributing new original work.)

---

## Goals of the Port

1. **Functional modular file manager** — not a pixel-perfect clone.
2. Keep the **general UI layout and core functionality** of the Python GUI:
   - Main window with location bar / breadcrumb bar, filter & search fields
   - Icon / small-icon / detail views over a `QGraphicsView`-style or
     equivalent item view
   - Navigation history, parent/home, clipboard (copy/cut/paste)
   - Context menus, properties, rename, create folder/file
   - Directory watching (inotify), thumbnails via freedesktop thumbnailer
   - Optional archive browsing (later phase)
3. **Modularity first**: filesystem operations (copy, move, rename,
   delete, mkdir, …) live in a separate library intended for later
   extraction as **dirops**. GUI must not embed low-level FS logic.
4. **Clean design over hacks**. Prefer correct abstractions. Workarounds
   only as last resort when the problem is fully understood; document
   them. Prefer extra debug logging when stuck.
5. Ignore most CLI utilities under `dirtoo-py/src/dirtoo/programs/`
   unless they are useful for testing the C++ libraries (see TODO.md).

---

## Architecture Snapshot (Python → C++ mapping)

### Python packages (reference)

| Python package | Responsibility | C++ target |
|----------------|----------------|------------|
| `fileview/` | App, controller, main window, graphics file view, actions | `dirtoo` app + UI modules |
| `gui/` | Dialogs, line edits, menus, tool buttons | `dirtoo` UI widgets |
| `filesystem/` | `Location`, `FileInfo`, stdio VFS | `dirtoo::fs` or shared lib |
| `posix/` | Low-level rename/copy/move with verbose/dry-run | **`dirops` library** |
| `filecollection/` | Collection, filter, sorter, grouper | `dirtoo::collection` |
| `watcher/` | inotify + Qt integration | `dirtoo::watcher` |
| `thumbnail/` | D-Bus thumbnailer client | `dirtoo::thumbnail` |
| `archive/` | RAR/7z/libarchive extractors | Later phase |
| `filter/`, `find/`, `expr/` | Filter language, find tool | Low priority / later |
| `programs/` | Many CLI tools | Mostly skip; keep a few test helpers |
| `mime/`, `metadata/`, `image/` | MIME, mediainfo, icons | As needed by UI |

### Intended C++ layout (`dirtoo/`)

```
dirtoo/
  CMakeLists.txt
  flake.nix
  README.md
  libs/
    dirops/          # standalone-ish FS ops (copy/move/rename/delete…)
    dirtoo-fs/       # Location, FileInfo, VFS abstractions
    dirtoo-collection/
    dirtoo-watcher/
    dirtoo-thumbnail/
  apps/
    dirtoo/          # main GUI application
  tools/             # minimal CLIs useful for testing dirops/fs
  tests/
  resources/         # icons, desktop file
```

Use **C++23**, Qt6 (Core, Gui, Widgets; DBus later), CMake 3.25+.

---

## Coding Standards

### Language & style

- C++23: `std::expected`, `std::span`, ranges, `std::filesystem` where
  appropriate, concepts, modules only if the toolchain is solid—prefer
  classic headers initially.
- No exceptions across library API boundaries for ordinary error paths;
  use `std::expected` or explicit error codes. Exceptions only for truly
  unexpected failures.
- Prefer value types and clear ownership (`unique_ptr` / RAII). Avoid
  shared mutable state.
- Logging: structured, leveled (debug/info/warn/error). When diagnosing
  hard bugs, **add debug logs**, do not paper over with hacks.
- No “temporary” silent catches that hide failures.

### Git commit messages

After every coherent series of changes, leave a **detailed suggested
commit message** for the human (subject ≤ ~72 chars, body explaining
why and what). Example:

```
Add dirops::copy_file with conflict policy API

Introduces the first filesystem operation in the dirops library with
explicit overwrite / rename / skip policies. No GUI wiring yet; unit
tests cover basic success and permission-denied paths.
```

### What not to do

- Do **not** port known Python bugs, incomplete experiments, or dead
  code from `experiments/`.
- Do **not** put copy/move logic inside Qt widgets.
- Do **not** depend on Python or PyQt in the C++ tree.
- Do **not** invent workarounds without understanding root cause; log
  and document instead.

---

## Build & environment

- Primary build: **CMake** + Ninja.
- Packaging / dev shell: **Nix flake** (`flake.nix`) providing Qt6,
  cmake, ninja, etc.
- Target platform for development: Linux (inotify, freedesktop
  thumbnailer, XDG). Keep platform-specific code isolated.

---

## Working with the Python tree

- `dirtoo-py/` is a **behavioral reference**, not something to extend.
- When unsure about intended UX, read the corresponding Python module
  under `src/dirtoo/fileview/` or `gui/`.
- CLI programs in `programs/` are mostly out of scope; only port helpers
  that validate C++ libraries (e.g. a thin `dt-move`/`dt-copy` around
  dirops).

---

## Current status

- Python sources extracted to `dirtoo-py/`.
- C++ tree and detailed plan: see `TODO.md`.
- No production C++ GUI yet; scaffolding and library boundaries are the
  first milestones.

---

## Suggested first commits (historical)

When initializing the C++ side, prefer small, reviewable steps:

1. Scaffold `dirtoo/` with CMake + flake + empty libraries.
2. Implement `Location` / `FileInfo` in `dirtoo-fs`.
3. Implement `dirops` rename/copy/move with tests.
4. Minimal Qt main window that lists a directory.
5. Wire navigation, then filter/sort, then thumbnails, etc.

See `TODO.md` for the full phased plan.


## Current C++ layout (dirtoo/)

| Path | Role |
|------|------|
| `libs/dirops` | Qt-free copy/move/rename/remove (future extractable project) |
| `libs/dirtoo-fs` | Location (file + archive URLs), FileInfo, list_directory |
| `libs/dirtoo-collection` | Filtering/sorting (name filter, show hidden) |
| `libs/dirtoo-watcher` | QFileSystemWatcher wrapper |
| `libs/dirtoo-thumbnail` | freedesktop Thumbnailer1 D-Bus client |
| `libs/dirtoo-archive` | ArchiveManager — cache extract via bsdtar/tar/unzip/7z |
| `apps/dirtoo` | Qt GUI (MainWindow, model, clipboard, transfers) |

Python reference remains in `dirtoo-py/` for comparison only.
