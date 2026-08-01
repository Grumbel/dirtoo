# AGENTS.md — dirtoo C++ Port

## Project Overview

**dirtoo** is a Qt-based graphical file manager and a collection of
directory/file utilities. The original implementation is Python 3 +
PyQt6 (~20k lines under `dirtoo-py/`). This repository is the **C++23 /
Qt6** port (CMake, Nix flake).

| Path | Role |
|------|------|
| `dirtoo-py/` | Frozen Python reference (behavior only; do not “fix” bugs) |
| `dirtoo/` | Active C++23 codebase |

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
   - Main window with location / breadcrumb bar, filter & search
   - Detail / Icons (Graphics View) / Small-icons views
   - Navigation history, parent/home, clipboard, context menus
   - Directory watching, freedesktop thumbnails, read-only archives
3. **Modularity first**: FS mutations live in **dirops**; the GUI must not
   embed low-level copy/move logic.
4. **Clean design over hacks**. Prefer correct abstractions. Document
   workarounds. Prefer debug logging when stuck.
5. Ignore most CLI utilities under `dirtoo-py/.../programs/` unless useful
   for testing C++ libraries.

Local **MVP GUI/filter/dialog parity is complete**. See `TODO.md` for the
checklist and explicit out-of-scope items (archive write, remote VFS,
full Python programs surface).

---

## Architecture Snapshot (Python → C++)

| Python | Responsibility | C++ |
|--------|----------------|-----|
| `fileview/` | Main window, graphics file view, actions | `apps/dirtoo` (`MainWindow`, `GraphicsFileView`, …) |
| `gui/` | Dialogs, menus | `apps/dirtoo/*_dialog.*`, menus in `MainWindow` |
| `filesystem/` | Location, FileInfo | `dirtoo-fs` |
| `posix/` | rename/copy/move | **`dirops`** |
| `filecollection/` | filter, sorter, grouper | `dirtoo-collection` |
| `filter/` / expr | Filter language | `dirtoo-filter` |
| `watcher/` | inotify | `dirtoo-watcher` |
| `thumbnail/` | D-Bus thumbnailer | `dirtoo-thumbnail` |
| `archive/` | archive browse | `dirtoo-archive` (read-only) |

### C++ tree (`dirtoo/`)

```
dirtoo/
  CMakeLists.txt
  flake.nix
  README.md
  libs/
    dirops/
    dirtoo-fs/
    dirtoo-collection/
    dirtoo-filter/      # parser, predicates, MediaMetaCache
    dirtoo-watcher/
    dirtoo-thumbnail/
    dirtoo-archive/
  apps/
    dirtoo/             # GUI
  tools/                # dt-copy, dt-move, dt-filter, …
  tests/
  resources/            # icons, badges, dnd cursors, qrc
```

Use **C++23**, Qt6 (Core, Gui, Widgets, DBus as needed), CMake 3.25+.

---

## Coding Standards

### Language & style

- C++23: `std::expected`, `std::span`, ranges, `std::filesystem` where
  appropriate. Prefer classic headers over modules unless the toolchain is
  solid.
- **No exceptions** across library API boundaries for ordinary errors;
  use `std::expected` or error codes.
- Prefer value types and clear ownership (`unique_ptr` / RAII).
- Logging: structured, leveled. When diagnosing hard bugs, **add debug
  logs** — do not paper over with silent catches.

### GUI / I/O rule

**The GUI thread must not perform filesystem or network I/O** for directory
loads, meta probes, thumbnail fetches, or multi-file transfers. Use workers,
signals/slots, and the media meta cache. Bounded reads only for filter
content predicates (see filter implementation).

### Git commit messages

After every coherent series of changes, leave a **detailed suggested commit
message** for the human (subject ≤ ~72 chars, body explaining why and what).

### What not to do

- Do **not** port known Python bugs, incomplete experiments, or dead code.
- Do **not** put copy/move logic inside Qt widgets — call **dirops**.
- Do **not** depend on Python or PyQt in the C++ tree.
- Do **not** invent workarounds without understanding root cause.

---

## Build & environment

- Primary build: **CMake** + Ninja.
- Dev shell / packaging: **Nix flake** (`dirtoo/flake.nix`).
- Target for development: **Linux** (inotify, freedesktop thumbnailer, XDG).
  Keep platform-specific code isolated.

---

## Working with the Python tree

- `dirtoo-py/` is a **behavioral reference**, not something to extend.
- When unsure about UX, read `src/dirtoo/fileview/` or `gui/`.
- CLI programs in `programs/` are mostly out of scope; only port helpers
  that validate C++ libraries (`dt-move`, `dt-copy`, `dt-filter`, …).

---

## Current status (summary)

| Area | State |
|------|--------|
| dirops + CLI tools | done |
| Main window, nav, bookmarks, history | done |
| Filter DSL + recursive search + `dt-filter` | done |
| Detail / Icons (Graphics) / Small icons | done |
| Thumbnails + media badges + meta cache | done |
| Clipboard transfers + conflict/transfer dialogs | done |
| Preferences, properties, about, rename/create | done |
| Archives read-only | done |
| Archive write / remote VFS / programs/* | **out of scope** |

Details and historical checklists: **`TODO.md`**.
User-facing overview: **`dirtoo/README.md`**.
