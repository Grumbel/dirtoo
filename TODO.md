# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status

Core local file-manager **MVP is done**. Filter DSL, multi-window,
dirops, read-only archives, Graphics View icons mode, dialog polish, and
packaging hooks are in place.

### Parity freeze

Intentionally **out of scope** unless explicitly revisited:

- Archive **write** / modify
- Remote / VFS backends (gio, KIO, …)
- Full Python `programs/*` CLI surface
- Debug mode action
- Kinetic / animated Graphics layout

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
| Filter show/hide + pin, history, Escape | **done** |
| Filter DSL (`and`/`or`/`not`/`()`, glob, regex, size, type) | **done** |
| `contains:` / `Contains:` (bounded I/O) | **done** |
| `date:` / `length:` / `time:` / `weekday:` | **done** |
| `containsre` / fuzzy / random / charset | **done** (charset limited) |
| `pages` / `filecount` + media width/height/duration/fps | **done** |
| Filter help | **done** |
| CLI `dt-filter` | **done** |
| Recursive search UI | **done** |

### View & chrome

| Python behaviour | C++ status |
|------------------|------------|
| Detail + icons + zoom + leap | **done** |
| Small-icon / compact list | **done** |
| Graphics View icon scene | **done** (grid, DnD, folder-tile drop) |
| Group by day / directory / duration | **done** (headers in list/detail; flat grid in Graphics) |
| Message area | **done** |
| Async path completion | **done** |
| Save file list as… | **done** |

### Ops & dialogs

| Item | Status |
|------|--------|
| dirops + tools | **done** |
| Clipboard cut/copy/paste | **done** |
| Conflict dialog (info + apply-to-all) | **done** |
| Transfer dialog (bytes, time, pause, log) | **done** |
| Properties (owner, times, perms RO, media cache) | **done** |
| About | **done** |
| Rename / New Folder name dialog | **done** |
| Preferences (view, zoom, group, crop, dirs-first, …) | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus + status badges | **done** |

### Optional polish still open

| Item | Notes |
|------|--------|
| Group section labels in **Graphics** view | List/detail only today |
| Viewport-limited thumbnail requests | Currently requests all filtered items |
| Folder-as-drop-target on **list/tree** | Graphics path only |
| Editable permissions in Properties | Display-only |
| Safer `file_clock` → system_clock conversion in conflict UI | `clock_cast` portability |
| Keyboard focus navigation in Graphics scene | Leap exists globally |

---

## Architecture notes (must keep)

**GUI thread must not perform filesystem or network I/O** for listings,
metadata probes, or bulk transfers. Use:

- Directory / sort / search / transfer **workers**
- `MediaMetaCache` (SQLite, async probes: ffprobe / pdfinfo / bsdtar)
- Bounded content reads for `contains*` predicates

`dirops` remains Qt-free for eventual standalone extraction.

### View tech

| Mode | Implementation |
|------|----------------|
| Detail | `QTreeView` + `FileListModel` |
| Icons | `GraphicsFileView` + `GraphicsFileItem` |
| Small icons | `QListView` list mode |

### C++ advantages vs Python

- Modular installable libraries + Nix flake outputs
- Explicit no-GUI-thread I/O architecture
- Async load/sort workers
- Hand-written filter parser (no pyparsing runtime)

---

## Historical near-term queue (all done)

1. [x] Leap, history, middle-click, filter pin
2. [x] Filter DSL + `dt-filter` + recursive search
3. [x] `contains` / date / length / time / weekday family
4. [x] Group by + small icons + badges
5. [x] Preferences + thumbnail prepare/reload
6. [x] Dialog polish (conflict, transfer, properties, about, names)
7. [x] Graphics View icons + DnD + folder drop
8. [x] Save file list as…

---

## Working process

- Suggest a detailed commit message after each change series.
- Keep `dirtoo-py/` as reference only.
- Prefer small, reviewable commits; do not bulk-reformat unrelated code.
