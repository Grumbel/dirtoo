# dirtoo Architecture

This document describes the architecture of **dirtoo**, a modular Qt6 file
manager and filesystem toolkit written in **C++23**. The original Python/PyQt6
application under `dirtoo-py/` is a **behavioral reference** only; the active
codebase is the `dirtoo/` tree.

License: **GPL-3.0-or-later** (REUSE SPDX headers on every source file).

---

## 1. Goals and design principles

1. **Functional modular file manager** — parity with the Python GUI’s layout and
   core workflows, not a pixel-perfect clone.
2. **Library-first modularity** — filesystem identity, listing, filtering,
   watching, thumbnails, archives, and mutations live in separate libraries;
   the GUI composes them and must not embed low-level copy/move logic.
3. **GUI thread stays interactive** — the UI thread must not perform directory
   loads, content-filter evaluation, thumbnail work, media probes, or multi-file
   transfers. Those run on workers and return via signals/slots (or equivalent).
4. **Errors as values** — across library boundaries prefer `std::expected` /
   error codes over exceptions for ordinary failures.
5. **Clean abstractions** — prefer correct design over hacks; document
   workarounds; use structured logging when diagnosing issues.

---

## 2. Repository layout

```
dirtoo/                    # Active C++ codebase
  CMakeLists.txt
  flake.nix
  README.md
  ARCHITECTURE.md          # This document
  libs/
    dirtoo-fs/             # Location, FileInfo, listing primitives
    dirtoo-collection/     # Sorted / filtered / grouped visible list
    dirtoo-filter/         # Filter DSL parser, predicates, MediaMetaCache
    dirtoo-watcher/        # Directory change notifications (inotify)
    dirtoo-thumbnail/      # Freedesktop D-Bus Thumbnailer1 client
    dirtoo-archive/        # Read-only archive TOC + extract-on-demand
    dirtoo-hash/           # Checksum compute + ChecksumStore
    dirtoo-tags/           # TagStore (sha256 identity; never hashes)
    dirops/                # Copy / move / rename / delete / mkdir (Qt-free)
  apps/
    dirtoo/                # GUI application (MainWindow + views + dialogs)
  tools/                   # CLI wrappers (dt-copy, dt-move, dt-filter, …)
  tests/                   # Catch2 unit tests
  resources/               # Icons, badges, DnD cursors, desktop/metainfo

dirtoo-py/                 # Frozen Python reference (behavior only)
```

Build: CMake 3.25+, C++23, Qt6 (Core, Gui, Widgets, DBus as needed). Optional
Nix flake for a reproducible shell.

---

## 3. Layered overview

```
┌─────────────────────────────────────────────────────────────────┐
│  apps/dirtoo  — MainWindow, views, dialogs, workers, settings   │
│  Controllers/chrome: LocationChrome, FilterSearchChrome,        │
│  QuickFilterBar, SidebarController, DevicesController,          │
│  ThumbnailCoordinator, TagController, TransferController, …     │
└────────────┬────────────────────────────┬───────────────────────┘
             │                            │
             ▼                            ▼
┌────────────────────────┐    ┌───────────────────────────────────┐
│  Presentation helpers  │    │  Background workers (QObject)     │
│  FileListModel         │    │  DirectoryLoadWorker              │
│  FileItemDelegate      │    │  FilterWorker / SortWorker        │
│  GraphicsFileView/Item │    │  TransferWorker                   │
│  open_with, clipboard  │    │  SearchWorker, PathCompletion…    │
└────────────┬───────────┘    └───────────────┬───────────────────┘
             │                                │
             ▼                                ▼
┌─────────────────────────────────────────────────────────────────┐
│  libs — domain libraries                                        │
│  collection filter fs watcher thumbnail archive hash tags dirops│
└─────────────────────────────────────────────────────────────────┘
             │
             ▼
        OS: std::filesystem, inotify, D-Bus Thumbnailer1, libarchive (etc.)
```

**Dependency direction:** tools and the GUI depend on libraries; libraries do
not depend on the GUI. `dirops` is intentionally Qt-free so it can ship as a
standalone mutation toolkit.

---

## 4. Core domain libraries

### 4.1 `dirtoo-fs` — identity and listing

| Type | Role |
|------|------|
| **`Location`** | Logical place: ordinary path, archive URL (`archive://…`), parent/join, human string, URL form. |
| **`FileInfo`** | One entry: path/location, basename, size, mtime, type flags (file/dir/symlink), ownership, etc. Factories: `from_path`, `from_location`, `from_directory_entry` (hot path), `synthetic` (archive members). |

Listing a directory produces a vector of `FileInfo` without applying filter
UI rules; that is the collection layer’s job.

### 4.2 `dirtoo-collection` — visible file list

**`FileCollection`** holds the full set of entries for the current location and
derives a **visible** ordered list after:

- **Show hidden** toggle
- **Filter** expression (compiled predicates from `dirtoo-filter`)
- **Sort** (`Sorter`: name/size/mtime/type, ascending/descending, directories-first)
- **Group** (`GroupMode`: none / day / directory / duration)

Watcher-driven updates can **merge** items rather than always replacing the
whole set (soft reload). Content-heavy predicates must not be evaluated on the
GUI thread during `rebuild_visible`; see filter worker below.

### 4.3 `dirtoo-filter` — DSL and media metadata

- **Parser** — turns a string into a predicate tree (`*.png`, `size:>1M`,
  `contains:…`, `date:…`, boolean groups, etc.).
- **Predicates** — pure (or bounded) match functions over `FileInfo` / path.
- **`MediaMetaCache`** — shared cache for duration, dimensions, page counts,
  etc., filled by probes off the UI thread.
- **Search** — recursive directory walk with cancellation (`search.hpp`), used
  by the recursive search UI.

CLI: `dt-filter` exercises the same parser without the GUI.

### 4.4 `dirtoo-watcher` — change notification

Thin Qt-friendly wrapper around filesystem monitoring (inotify-backed). The GUI
connects signals to a **soft reload** of the current directory (merge into the
collection) rather than blocking the UI on every event.

### 4.5 `dirtoo-thumbnail` — Freedesktop thumbnails

Client for the **org.freedesktop.thumbnails.Thumbnailer1** D-Bus service:
queue requests, map to cache paths, fall back when the daemon is absent. The
app requests thumbnails for **visible** rows only and stores status on the
model (`Pending` / `Ready` / `Failed`).

### 4.6 `dirtoo-archive` — read-only archives

Browse archive tables of contents as synthetic `FileInfo` under archive
`Location`s; extract on demand for open/thumbnail when needed. **Write** into
archives is out of scope for the current architecture.

### 4.7 `dirops` — mutations

Copy, move, rename, delete, mkdir, symlink, swap-names, conflict policies.
Returns structured errors (`std::expected`). Used by:

- GUI `TransferWorker` (progress, pause, cancel, conflict dialogs)
- CLI tools (`dt-copy`, `dt-move`, `dt-rename`, …)

The GUI must not reimplement recursive copy/move; it only schedules work and
presents progress/errors.

---

## 5. Application architecture (`apps/dirtoo`)

### 5.1 Entry and shell

- **`main.cpp`** — `QApplication`, logging handler, CLI args, constructs
  **`MainWindow`**.
- **`MainWindow`** — central controller: location stack/history, collection +
  model, view stack, toolbars/menus, filter/search chrome, transfer lifecycle,
  preferences, context menus.

Persistent state lives in **`AppSettings`** (`QSettings` org/app `dirtoo`):
view mode, zoom, icon detail, group mode, filter pin, **size unit style**
(SI vs IEC), window geometry, location history, etc.

### 5.2 Data flow: open a directory

```
User navigates (path, breadcrumb, history, parent, archive open)
        │
        ▼
MainWindow::open_location(Location)
        │
        ├─► cancel previous load / filter jobs
        ├─► DirectoryLoadWorker (background)
        │         lists FileInfo[] via fs (+ archive if needed)
        │
        ▼
FileCollection::set_items / merge
        │
        ├─► optional FilterWorker if expression needs content I/O
        ├─► sort / group (async SortWorker when heavy)
        │
        ▼
FileListModel::reset / dataChanged
        │
        ▼
Active view(s) refresh  +  request_thumbnails_for_visible()
```

Navigation updates the location bar / button bar, window title, and history.
Watcher events trigger a soft rescan and merge.

### 5.3 `FileListModel` — Qt adapter

`QAbstractTableModel` over the collection’s visible rows.

| Concern | Mechanism |
|---------|-----------|
| Columns | Name, Size, Modified, Type (`FileListColumn`) |
| Icons / thumbs | `DecorationRole`; thumbnail path/status via custom roles |
| Captions | Display text; icon-mode multi-line captions driven by **icon detail level** |
| Groups | `IsGroupStartRole`, `GroupLabelRole` |
| Time gaps | `TimeGapSecondsRole` (detail separators) |
| Style flags | `icon_style_active()`, crop thumbnails, detail level |

Size strings go through shared **`format_byte_size()`** (`size_format`) so SI
(KB/MB) vs IEC (KiB/MiB) is consistent app-wide.

### 5.4 View modes — three backends, one model

`MainWindow` keeps a **`QStackedWidget`** of three views sharing **`FileListModel`**:

| Mode | Widget | Painting |
|------|--------|----------|
| **Detail** | `FileTreeView` (`QTreeView`) | Columns + **`FileItemDelegate`** (non-icon style): name, size, date, type; group headers and time-gap rows. |
| **Small Icons** | `FileListView` (`QListView`, IconMode) | Same **`FileItemDelegate`** with `set_icon_style(true)` — tile + short caption. |
| **Icons** | **`GraphicsFileView`** (`QGraphicsView`) | **`GraphicsFileItem`** tiles on a `QGraphicsScene`. **Does not** use the delegate. |

Switching (`set_view_mode`):

- Detail → tree, icon style off  
- Small Icons → list, icon style on, detail level forced low  
- Icons → graphics view, icon style on, richer caption LOD; `sync_from_model()`

**GraphicsFileView** specifics:

- Precomputes a **grid of slot positions** (group-aware column breaks).
- **Viewport windowing**: only rows near the visible scroll range are
  materialized as `GraphicsFileItem` instances; selection is tracked in a
  persistent row set so off-screen selections survive.
- **File cursor** — keyboard focus tile independent of multi-select (arrows,
  Shift-extend, Ctrl+Space toggle, Enter activate), painted as a light outline
  (Python `_cursor_item` parity).
- Zoom / crop / compact spacing adjust tile size and relayout.

**Shared look, duplicated paint:** badges (duration, resolution, …), selection
tint, and folder emblems are implemented both in `FileItemDelegate` and
`GraphicsFileItem` so list and graphics stay visually close without a single
renderer class.

```
                    FileListModel
                         │
         ┌───────────────┼────────────────┐
         ▼               ▼                ▼
   FileTreeView    FileListView    GraphicsFileView
   + Delegate      + Delegate      + GraphicsFileItem[]
   (Detail)        (Small Icons)   (Icons, windowed)
```

### 5.5 Chrome and navigation UI

- **Toolbar** — Parent, Home, Back/Forward, Reload, Prepare Thumbnails, Show
  Hidden, Sort/Group menus, view modes, zoom, icon detail, crop (ordered to
  mirror the Python toolbar where practical).
- **Location bar** — editable path + **`LocationButtonBar`** breadcrumbs.
- **Filter bar** — bottom row; expression applied to the collection; history
  via Up/Down; pin/show actions.
- **Search** — recursive search UI + `SearchWorker`; results can drive the list.
- **Leap widget** — type-ahead jump overlay (focus name prefix).
- **Message area** — transient errors/info without modal spam.

### 5.6 Selection, clipboard, transfers

- Selection: Qt selection models on list/tree; row set + scene selection on
  graphics.
- **Clipboard** — internal MIME + `text/uri-list` + GNOME-compatible payloads
  (`clipboard.cpp`); modes Copy / Cut / Link.
- **TransferWorker** + **TransferDialog** — background dirops with progress,
  pause/cancel, **ConflictDialog** policies.
- Paste into folder from the item context menu targets a directory row’s path.

### 5.7 Context menus (Python-aligned)

| Target | Menu role |
|--------|-----------|
| **Background** | Directory operations: create folder/file, paste, terminal here, select all, properties of **current** location. |
| **Item(s)** | Open folder/archive, **Open With &lt;default apps&gt;**, Open with… (other associations), containing folder, terminal (dirs), cut/copy/paste-into, rename/delete, thumbnail actions, properties. |

**Open With** (`open_with.cpp`) resolves MIME types, reads `mimeapps.list`
defaults/associations, and also scans `.desktop` `MimeType=` fields so
registered applications appear even when mime cache entries are sparse.

### 5.8 Workers (GUI process)

All inherit `QObject` and run on a worker thread or `QtConcurrent`/`QThread`
pattern as implemented:

| Worker | Responsibility |
|--------|----------------|
| `DirectoryLoadWorker` | List directory / archive TOC |
| `FilterWorker` | Content predicates (`contains*`, fuzzy, …) |
| `SortWorker` | Heavy sort off UI thread |
| `TransferWorker` | dirops batch with progress/conflicts |
| `SearchWorker` | Recursive search |
| `PathCompletionWorker` | Location bar completion |
| `DirectoryThumbnailWorker` | Folder collage / prepare thumbs |

Rule of thumb: if it touches the disk for more than a trivial `stat`, it
belongs on a worker.

---

## 6. Comparison with dirtoo-py

| Area | Python (`dirtoo-py`) | C++ (`dirtoo`) |
|------|----------------------|----------------|
| Main canvas | Single **`FileView`** (`QGraphicsView`) for ICON / SMALLICON / DETAIL | **Three** widgets: Tree, List, Graphics |
| Item rendering | **`FileItem`** + **`FileItemRenderer`** switched by `FileItemStyle` | Delegate (detail/small) **or** `GraphicsFileItem` (icons) |
| Detail view | Graphics items drawn as rows | Real `QTreeView` columns |
| Large directories | Full scene item set + layout modes | Graphics **viewport windowing**; list/tree rely on Qt item views |
| FS mutations | `posix/` helpers | **`dirops`** library + CLI |
| Filter | Python expr engine | **`dirtoo-filter`** C++ DSL + worker for content I/O |
| Thumbnails | D-Bus / cache | **`dirtoo-thumbnail`** + model status |
| Cursor keys | `_cursor_item` on `FileView` | Graphics file cursor; list/tree use Qt current index |
| Architecture | App-centric packages under `fileview/`, `gui/`, … | Explicit **libs/** + thin **apps/dirtoo** |

The C++ port deliberately uses stock Qt views for Detail and Small Icons
(columns, accessibility, less custom layout) while keeping a graphics path for
rich icon tiles and large-directory scaling—the closest visual analogue to
Python’s Icons mode.

---

## 7. Cross-cutting concerns

### 7.1 Size formatting

`size_format` provides `format_byte_size()` with process-wide **SI** (base 1000)
or **IEC** (base 1024) style, chosen in Preferences and applied to the list,
captions, properties, conflicts, transfers, and status bar.

### 7.2 Settings and preferences

`load_settings` / `save_settings` / `MainWindow::apply_settings` /
`persist_settings` keep UI state coherent across sessions. Preferences dialog
edits the same `AppSettings` struct the main window uses at runtime.

### 7.3 Testing and tools

- **Catch2** tests under `tests/` cover location, filter, collection merge/group,
  dirops policies, etc.
- **tools/** link the same libraries the GUI uses, for scriptable verification
  without Widgets.

### 7.4 Explicit non-goals (current)

- Writing into archives  
- Remote VFS (SMB/SFTP as first-class locations)  
- Porting the bulk of Python `programs/*` utilities  

See project `TODO.md` / `STATUS.md` for residual polish (true incremental
watcher deltas, further virtualization, etc.).

---

## 8. Mental model for contributors

1. **Identity** is `Location` + `FileInfo` (`dirtoo-fs`).
2. **What the user sees** is `FileCollection` → `FileListModel` → one of three
   views.
3. **What changes the disk** goes through **`dirops`** on a worker, never
   inline on the GUI thread for bulk work.
4. **What filters the list** is `dirtoo-filter`; content-touching filters are
   async.
5. **What looks like Python Icons mode** is `GraphicsFileView` +
   `GraphicsFileItem`; Detail/Small Icons are intentionally different Qt
   backends sharing the same model.

When adding a feature, prefer extending a library and a small GUI surface area
over growing `MainWindow` indefinitely; keep the GUI-thread I/O rule intact.
