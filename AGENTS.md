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

Large-directory mitigations (cheap listing, filter worker, Graphics item reuse, viewport thumbs, soft watcher reload), DnD/Link, and core parity features are in place. See **`TODO.md`** for residual polish and **`AUDIT.md`** for the full file inventory and parity matrix. Explicit out-of-scope items include archive write, remote VFS, and full Python `programs/*`.

**Operations history (not Undo):** every successful or failed filesystem mutation
the GUI performs (rename, move, copy, delete, mkdir, mkfile, symlink, swap, and
future permission changes) should be append-only logged with timestamps and
paths. **Do not** promise transactional rollback — complex ops are not reliably
reversible; a history browser is enough until a careful, limited “revert this
one rename” design exists.

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


### Dialog button order (GNOME)

Use **GNOME button order** for dialogs: dismissive actions on the left,
affirmative on the right — e.g. **`[Cancel] [OK]`**, not Windows-style
`[OK] [Cancel]`. Prefer `QDialogButtonBox` so roles map correctly; the app
installs a style hint (`QDialogButtonBox::GnomeLayout`) at startup so this
holds for `QDialogButtonBox` and `QMessageBox` even when the platform theme
would use a Windows-like layout. Do not hard-code right-to-left button rows
that fight the style.

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
| Main window, nav, bookmarks, history | **done** (nav no longer double-loads via watcher) |
| Filter DSL + recursive search + `dt-filter` | **done**; content filters via `FilterWorker` |
| Detail / Icons (Graphics) / Small icons | **improved** — Graphics viewport-windowed + selection persistence |
| Thumbnails + media badges + meta cache | **improved** — viewport batch; directory montages |
| Clipboard transfers + conflict/transfer dialogs | **done** including Link paste |
| Preferences, properties, about, rename/new folder/file | **done** |
| Archives read-only | **done** |
| DnD | **done** — modifiers, folder drop, nested-drop guard |
| Select All / Swap Names / Full Paths / Time Gaps | **done** (Graphics Select All = all rows) |
| Soft watcher merge | **done** (`merge_items`); full readdir still used |
| Detail virtualization / inotify per-entry | **open** (optional) |
| Archive write / remote VFS / programs/* | **out of scope** |

Priority residual queue and parity matrix: **`TODO.md`**.  
User-facing overview: **`dirtoo/README.md`**.
