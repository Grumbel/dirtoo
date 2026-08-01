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

Large-directory mitigations (cheap listing, filter worker, Graphics item reuse, viewport thumbs, soft watcher reload), DnD/Link, and core parity features are in place. See **`TODO.md`** for residual polish (virtualization, incremental FS deltas) and explicit out-of-scope items (archive write, remote VFS, full Python `programs/*`).

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
loads, meta probes, thumbnail fetches, multi-file transfers, **or content
filter evaluation** (`contains*` / `containsre` / `containsfuzzy`). Use
workers, signals/slots, and the media meta cache. Bounded content reads
belong on a worker thread, not in `FileCollection::rebuild_visible` on the
UI thread.

Prior GUI-thread I/O violations have mitigations (see `TODO.md`). Residual risks: full directory rescan on watch (soft), no list virtualization, non-content filters still apply on the UI thread.

### Git commit messages

After every coherent series of changes, leave a **detailed suggested commit
message** for the human (subject ≤ ~72 chars, body explaining why and what).


### Documentation (required)

Always keep **`TODO.md`** and **`AGENTS.md`** accurate when you change behavior,
fix defects, or close/open work items:

- Update the relevant **status tables / checklists** in `TODO.md` in the same
  change series (mark items done, note mitigations, add new residual risks).
- Refresh the **Current status** summary in this file when the overall picture
  shifts (not on every micro-fix).
- Do not leave stale “missing / broken” rows after features land.

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
| dirops + CLI tools | **done** (`create_file` / `create_symlink` / swap) |
| Main window, nav, bookmarks, history | **done** |
| Filter DSL + recursive search + `dt-filter` | **done**; content filters via `FilterWorker` |
| Detail / Icons (Graphics) / Small icons | **improved** — Graphics viewport-windowed tiles; even small-icon grid |
| Thumbnails + media badges + meta cache | **improved** — viewport batch; directory montages (explicit action) |
| Clipboard transfers + conflict/transfer dialogs | **done** including Link paste |
| Preferences, properties, about, rename/new folder/file | **done** |
| Archives read-only | **done** |
| DnD | **done** — Move/Copy/Link modifiers; folder drop on list/tree |
| Select All / Swap Names / Show Full Paths / Time Gaps | **done** |
| Graphics viewport virtualization | **done**; soft watcher **merge_items**; Detail virt / inotify **open** |
| Archive write / remote VFS / programs/* | **out of scope** |

Priority residual queue and parity matrix: **`TODO.md`**.  
User-facing overview: **`dirtoo/README.md`**.
