# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status (revised after full audit)

Core plumbing (libs, filter DSL, multi-window, dirops, read-only archives,
dialog shells, packaging) is in place, but **the GUI is not yet reliable
for large directories or everyday DnD/icon use**. Treat “MVP complete” as
**false** until the Critical queue below is addressed.

### Parity freeze (still intentionally out of scope)

- Archive **write** / modify
- Remote / VFS backends (gio, KIO, …)
- Full Python `programs/*` CLI surface
- Debug mode action
- Kinetic / animated Graphics layout

---

## Critical / high-priority defect queue

Ordered by user impact. File references are under `dirtoo/` unless noted.

### 1. UI freezes / jank on large directories

| Cause | Where | Notes |
|-------|--------|------|
| Extra stats per entry | `libs/dirtoo-fs/src/file_info.cpp`, `location.cpp` | `list_directory` → `FileInfo::from_path` does `symlink_status` + `file_size` + `last_write_time`. `Location::from_path` also runs `weakly_canonical` (more syscalls) **per file**. Prefer `directory_entry` status/size/mtime and skip canonicalization in listing. |
| Full listing still heavy | `directory_load_worker.cpp` | Worker is async, but listing itself is O(n) expensive stats; no progress/cancellation mid-list. |
| Filter I/O on GUI thread | `file_collection.cpp` `rebuild_visible` + `contains*` predicates | `set_name_filter` runs matchers synchronously on the GUI thread. `contains:` / `containsre:` / `containsfuzzy:` read file contents (bounded) **on the UI thread** when filtering a large dir — violates architecture rule. |
| Graphics full scene rebuild | `graphics_file_view.cpp` `rebuild_items` | Every `modelReset` / rows insert/remove clears the scene and recreates **all** `QGraphicsItem`s. Large dirs → multi-second stalls. Need incremental updates or virtualization. |
| Thumbnails for every visible row | `main_window.cpp` `request_thumbnails_for_visible` | Requests D-Bus thumbs for **all** filtered items, not the viewport. Also runs `QMimeDatabase::mimeTypeForFile` on the GUI thread per path. |
| Watcher → full rescan, no debounce | `main_window.cpp` ↔ `directory_watcher.cpp` | Every `directoryChanged` calls `on_directory_changed()` → clear + reload + re-thumb. Busy dirs thrash. Need debounce + incremental add/remove when possible. |
| Double model refresh | `on_directory_loaded` + `on_sort_finished` | Unsorted paint then sorted paint; each `refresh_list` → `beginResetModel` → Graphics rebuild. |

### 2. Drag & drop broken / incomplete

| Issue | Where | Notes |
|-------|--------|------|
| Graphics drag always prefers Copy | `graphics_file_view.cpp` `start_drag` | `drag->exec(Copy\|Move, Qt::CopyAction)` — Move is never the default; keyboard modifiers not applied as default action. |
| Folder drop target only in Graphics | `graphics_file_view.cpp` `dropEvent` vs list/tree | List/tree `dropMimeData` always drops into **current** directory; no “drop on folder row” path (Python does). |
| No Link / symlink DnD | model + transfer | Cursors include link; actions never produce `Qt::LinkAction` or `dirops` symlink. Python pastes/links via `link_files`. |
| Rubber-band vs item drag | Graphics | `setDragMode(RubberBandDrag)` + manual `start_drag` on move — easy to start band drag instead of item drag; selection edge cases. |
| Modifier → action on drop | `on_urls_dropped_to` | Uses `proposedAction` only; verify Shift/Ctrl mapping matches desktop norms (Move/Copy/Link). |
| Same-app internal moves | transfer path | Drop of own selection into subfolder should move/copy correctly; same-path / nested-dest checks exist but are thin. |

### 3. Icon layout and look

| Issue | Where | Notes |
|-------|--------|------|
| Hardcoded scene background | `graphics_file_view.cpp` | `QColor(250,250,250)` ignores palette / dark themes. |
| Caption / tile geometry | `graphics_file_item.cpp`, `apply_icon_zoom` | Tile height = icon + fixed text rows; elide/multi-line is basic; no Python-style shadow/outline renderer. Overflow or sparse gaps at some zoom levels. |
| Group headers missing in Graphics | layout | Day/directory/duration headers exist in detail/list delegate only. |
| Dual icon paths | MainWindow | Icons mode prefers `GraphicsFileView` but `QListView` IconMode still configured via `apply_icon_zoom` — keep one canonical path. |
| Small-icons grid | `apply_icon_zoom` | `setGridSize(QSize())` + wrapping TopToBottom — layout can look uneven vs Python SequenceMode. |
| No directory composite thumbs | missing | Python `DirectoryThumbnailer` builds folder montage thumbs; C++ only theme/mime icons for dirs. |

### 4. Filter correctness (partially fixed)

| Issue | Status |
|-------|--------|
| `Contains:` / `Containsre:` shadowed by lowercased dispatch | **Fixed** in `parser.cpp` (check capital forms first). Same fix applied to Glob/Regex/Fuzzy. |
| Charset predicate limited | ascii / utf-8 / latin1 only — documented limitation. |
| Content filters on GUI thread | Still open (see Critical §1). |

---

## Python features present in `dirtoo-py/`, missing or weaker in C++

### Should port (user-visible, local FS)

| Feature | Python | C++ |
|---------|--------|-----|
| **Select All** (Ctrl+A) | `actions.edit_select_all` | **Missing** (no menu/shortcut; views may have default only for some) |
| **Create empty file** | context menu / `create_file` | **Missing** (New Folder only) |
| **Symlink / Link paste & DnD** | `link_files`, Link drop | **Missing** |
| **Directory thumbnails** | `DirectoryThumbnailer` | **Missing** |
| **Time gaps** in list | `toggle_timegaps` | **Missing** |
| **Show abspath vs basename** | view toggles | **Missing** |
| Filter line **history** | Python filter toolbar history | Pin/show only; history weak/absent |
| Transfer **error** / **request** dialogs | dedicated dialogs | Partially folded into transfer/conflict |
| Undo menu entries | present (may be stub) | **Missing** |

### Acceptable gaps / out of scope

| Feature | Notes |
|---------|------|
| `programs/*` CLIs | Out of scope except library test helpers |
| Archive write | Out of scope |
| Remote VFS | Out of scope |
| Kinetic layout animation | Out of scope |
| Face-detect / experiment code | `dirtoo-py/experiments/*` not product |

---

## Python UI → C++ parity (checklist)

### Navigation & mouse

| Python behaviour | C++ status |
|------------------|------------|
| Middle-click breadcrumb / directory / archive → new window | **done** |
| Middle-click Parent toolbar | **done** |
| Middle-click History menu → new window | **done** |
| Location bar ↔ line edit, path completer | **done** |
| Location history menu | **done** |
| Bookmarks menu (file store, middle-click) | **done** |

### Filter & search

| Python behaviour | C++ status |
|------------------|------------|
| Filter show/hide + pin, Escape | **done** |
| Filter history | **partial** |
| Filter DSL (`and`/`or`/`not`/`()`, glob, regex, size, type) | **done** |
| `contains:` / `Contains:` (bounded I/O) | **done** (case fix applied; still runs on GUI thread) |
| `date:` / `length:` / `time:` / `weekday:` | **done** |
| `containsre` / fuzzy / random / charset | **done** (charset limited) |
| `pages` / `filecount` + media width/height/duration/fps | **done** |
| Filter help | **done** |
| CLI `dt-filter` | **done** |
| Recursive search UI | **done** |

### View & chrome

| Python behaviour | C++ status |
|------------------|------------|
| Detail + icons + zoom + leap | **done** (Graphics look/perf issues) |
| Small-icon / compact list | **done** (layout polish needed) |
| Graphics View icon scene | **partial** — grid works; rebuild cost, theme, captions weak |
| Group by day / directory / duration | **partial** — headers in list/detail only |
| Message area | **done** |
| Async path completion | **done** |
| Save file list as… | **done** |
| Select all | **missing** |
| Time gaps | **missing** |
| Directory thumbnails | **missing** |

### Ops & dialogs

| Item | Status |
|------|--------|
| dirops + tools | **done** |
| Clipboard cut/copy/paste | **done** (no link) |
| Conflict dialog (info + apply-to-all) | **done** |
| Transfer dialog (bytes, time, pause, log) | **done** |
| Properties (owner, times, perms RO, media cache) | **done** (perms display-only) |
| About | **done** |
| Rename / New Folder | **done** |
| New empty file | **missing** |
| Preferences | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus + status badges | **done** (all-visible request; no dir montage) |
| Symlink create / link paste | **missing** |

### Optional polish / follow-ups

| Item | Notes |
|------|--------|
| Group section labels in **Graphics** view | List/detail only today |
| Viewport-limited thumbnail requests | Currently all filtered items + mime DB on GUI |
| Folder-as-drop-target on **list/tree** | Graphics path only |
| Editable permissions in Properties | Display-only |
| Safer `file_clock` → system_clock in conflict UI | `clock_cast` portability |
| Keyboard focus navigation in Graphics scene | Leap exists globally |
| Debounced directory watcher | Full rescan every event |
| Incremental Graphics item updates | Full scene rebuild on reset |
| Cheap directory listing (use `directory_entry`) | Avoid `weakly_canonical` per file |
| Off-GUI-thread content filter evaluation | `contains*` currently UI-thread |
| Theme-aware Graphics background / selection | Hardcoded light gray |
| DnD default action + Link support | Copy-biased; no symlink |

---

## Architecture notes (must keep)

**GUI thread must not perform filesystem or network I/O** for listings,
metadata probes, bulk transfers, or **content-filter evaluation**.

Use:

- Directory / sort / search / transfer / **filter-apply** workers
- `MediaMetaCache` (SQLite, async probes: ffprobe / pdfinfo / bsdtar)
- Bounded content reads for `contains*` predicates **off the GUI thread**
- Viewport-scoped thumbnail + mime work

`dirops` remains Qt-free for eventual standalone extraction.

### View tech

| Mode | Implementation |
|------|----------------|
| Detail | `QTreeView` + `FileListModel` |
| Icons | `GraphicsFileView` + `GraphicsFileItem` (primary) |
| Small icons | `QListView` list mode |

### Known architecture violations (fix these)

1. `FileCollection::rebuild_visible` + content predicates on GUI thread  
2. `request_thumbnails_for_visible` mime probing on GUI thread  
3. Watcher-driven full reload without debounce  
4. Graphics `rebuild_items` synchronous mass allocation on GUI thread  

### C++ advantages vs Python

- Modular installable libraries + Nix flake outputs
- Explicit no-GUI-thread I/O architecture (when honored)
- Async load/sort workers
- Hand-written filter parser (no pyparsing runtime)

---

## Suggested work order

1. [x] **Listing cost**: `list_directory` via `directory_entry`; `from_path_unchecked` (no per-child `weakly_canonical`).  
2. [x] **Watcher debounce** (200ms single-shot).  
3. **Graphics**: incremental model sync; don’t recreate all items on every reset.  
4. [x] **Thumbnails**: viewport-scoped batch (cap 64); skip `QMimeDatabase` on GUI.  
5. **Filter**: evaluate `contains*` on a worker; apply results by generation.  
6. [x] **DnD**: Shift→Move; folder drop on list/tree; Graphics modifier handling.  
7. [partial] **Icons**: palette base background; **GraphicsFileView is now constructed** (was never created). Caption/group headers still open.  
8. [partial] **Parity**: Select All (Ctrl+A) done; New File / symlink / dir thumbs still open.

---

## Historical near-term queue (mostly done; regress where noted)

1. [x] Leap, history, middle-click, filter pin  
2. [x] Filter DSL + `dt-filter` + recursive search  
3. [x] `contains` / date / length / time / weekday family (**case-sensitive Contains fixed**)  
4. [x] Group by + small icons + badges  
5. [x] Preferences + thumbnail prepare/reload  
6. [x] Dialog polish (conflict, transfer, properties, about, names)  
7. [x] Graphics View icons + DnD + folder drop (**perf/look/DnD action still open**)  
8. [x] Save file list as…  

---

## Working process

- Suggest a detailed commit message after each change series.  
- Keep `dirtoo-py/` as reference only.  
- Prefer small, reviewable commits; do not bulk-reformat unrelated code.  
- When fixing freezes, measure with directories of 10k+ entries.  
