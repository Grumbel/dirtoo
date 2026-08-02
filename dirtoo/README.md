# dirtoo (C++)

Modular **Qt6** file manager and filesystem tools, rewritten in **C++23**.

The original Python/PyQt6 implementation lives in `../dirtoo-py/` as a
**behavioral reference** (do not “fix” Python bugs here). This tree is the
active codebase.

## Status

Local GUI **MVP parity** is in place: navigation, filter DSL, recursive search,
thumbnails, clipboard transfers, dialogs, archives (read-only), three view modes,
bookmarks, location history, and Recently Opened (file open history).

See **`../TODO.md`** for residual work and **`../AUDIT.md`** for the full source
audit / parity matrix. Intentional out-of-scope items: archive write, remote VFS,
full Python `programs/*`.

**Operations history (planned):** log rename/move/copy/delete and related
mutations with timestamps — **not** a full Undo stack (rollback deferred).

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

Run the GUI:

```bash
./build/apps/dirtoo/dirtoo
# or the target name produced by your build (e.g. dirtoo-app)
```

## Layout

| Path | Purpose |
|------|---------|
| `libs/dirops` | Copy/move/rename/delete/mkdir (Qt-free; future standalone project) |
| `libs/dirtoo-fs` | `Location`, `FileInfo`, directory listing |
| `libs/dirtoo-collection` | Sorted / filtered / grouped file list |
| `libs/dirtoo-filter` | Filter DSL parser + predicates + media meta cache |
| `libs/dirtoo-watcher` | Directory change notifications (QFileSystemWatcher) |
| `libs/dirtoo-thumbnail` | Freedesktop D-Bus thumbnailer client |
| `libs/dirtoo-archive` | Read-only archive TOC + extract-on-demand |
| `apps/dirtoo` | GUI application |
| `tools/` | CLIs: `dt-copy/move/rename/mkdir/mkfile/rm/symlink/swap`, `dt-filter`, `dt-mediainfo`, `dt-archiveinfo` |
| `tests/` | Catch2 unit tests |
| `resources/` | Icons, badges, DnD cursors, `.qrc` |

Agent rules and port history: `../AGENTS.md`, `../TODO.md`.

## Views

| Mode | Widget |
|------|--------|
| **Detail** | `QTreeView` + model |
| **Icons** | `GraphicsFileView` (`QGraphicsScene` tiles) |
| **Small icons** | `QListView` list mode |

Icons support zoom, crop/letterbox thumbnails, media badges (size, duration,
fps, pages), themed drag cursors, and drop onto folder tiles.

## Filter DSL (subset)

Examples:

```text
*.png
size:>1M type:file
contains:hello
date:2024
time:>=12:00 weekday:sat
group expressions: (a or b) and not c
```

See **Help → Filter expression help** in the app, or `dt-filter --help`.

## Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| F2 | Rename |
| F3 | Properties |
| F5 | Refresh |
| Backspace / Alt+Up | Parent directory |
| Alt+Home | Home |
| Alt+Left / Alt+Right | History back / forward |
| Ctrl+L | Focus location bar |
| Ctrl+C / X / V | Copy / Cut / Paste |
| Delete | Delete selection |
| Ctrl++ / Ctrl+- | Zoom icons |
| Ctrl+N | New window |
| Ctrl+Shift+S | Save file list as… |

## Transfers & dialogs

- Clipboard cut/copy/paste with conflict resolution (Replace / Rename / Skip,
  apply-to-all, source vs destination size/mtime).
- Background transfer dialog: progress, bytes, elapsed time, pause/resume,
  activity log, close-when-finished.
- Properties: MIME, ownership, timestamps, read-only permissions, cached media
  fields when available.
- Preferences: view mode, zoom, icon detail, group-by, crop, directories-first,
  hidden files, filter pin.

## Archives

Double-click a supported archive (zip, tar, tar.gz, 7z, rar, …) to browse its
contents. Extraction uses external tools into a user cache directory.
**Archive views are read-only.**

## Windows & navigation

- **File → New Window** (Ctrl+N).
- **Middle-click** breadcrumb, directory, history entry, or Parent toolbar
  control to open a path in a new window.
- Bookmarks menu (file-backed store).
- Async path completion on the location bar.
- Leap widget (type-ahead jump).

## Architecture notes

- GUI thread must **not** do filesystem or network I/O for listings, meta
  probes, or transfers; workers + signals own that work.
- Media metadata uses a process-local **SQLite cache** (`MediaMetaCache`) with
  async probing (ffprobe / pdfinfo / bsdtar).
- `dirops` stays Qt-free so it can be extracted later as a standalone library.

## License

**GPL-3.0-or-later**. Every source file uses REUSE-style SPDX headers:

```cpp
// SPDX-FileCopyrightText: 2024–2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
```
