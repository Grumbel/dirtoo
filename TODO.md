# TODO — Porting dirtoo from Python/PyQt6 to C++23

Python reference: `dirtoo-py/`. Active codebase: `dirtoo/`.

Principles: modular design, **dirops** separate, C++23, CMake + Nix, GPLv3+
SPDX, no hacks, VERSION file is sole version source. Archives are **read-only**.

---

## Milestone checklist (MVP)

- [x] Build with CMake + flake (`PROJECT_VERSION_FULL` / `DIRTOO_VERSION`)
- [x] Open home / directory; list files (detail + icon views)
- [x] Navigate parent/home/history; location bar + **breadcrumb buttons**
- [x] Rename, mkdir, delete via dirops
- [x] Copy/move with conflict dialog, progress, background worker
- [x] Directory auto-refresh (QFileSystemWatcher)
- [x] MIME icons + Thumbnailer1 D-Bus client
- [x] Context menu; open with default app / Open with… / terminal
- [x] Drag-and-drop, clipboard (dirtoo + uri-list + GNOME)
- [x] QSettings (geometry, view mode, zoom, hidden, last location)
- [x] About dialog; `--version`; desktop file + icons
- [x] Archive browse (TOC list-first, on-demand member extract, nested)
- [x] Unit tests (location, dirops, collection, clipboard, archive index)

---

## Phase status

### Phase 0 — Repository & build skeleton — **done**

- [x] Tree: libs, apps, tools, tests, resources
- [x] CMake C++23, Qt6, warnings
- [x] flake.nix (qt6, catch2_3, archive tools); no FetchContent
- [x] VERSION → `PROJECT_VERSION_FULL` / `DIRTOO_VERSION`; flake `+g<rev>`
- [x] Install rules (binary, desktop, icons)

### Phase 1 — `dirtoo-fs` — **done** (archives extended)

- [x] `Location` (file + `archive:///path!/entry`)
- [x] `FileInfo` (+ synthetic for archive members)
- [x] `list_directory`

### Phase 2 — `dirops` — **done**

- [x] copy/move/rename/remove/mkdir/swap
- [x] conflict policies, progress, cancel, dry-run
- [x] CLI tools: `dt-copy`, `dt-move`, `dt-rename`, `dt-mkdir`, `dt-swap`

### Phase 3 — `dirtoo-collection` — **done**

- [x] sort by name/size/mtime; name filter; show-hidden

### Phase 4 — `dirtoo-watcher` — **done**

- [x] QFileSystemWatcher wrapper

### Phase 5 — GUI application — **done** (MVP+)

- [x] MainWindow, dual views, zoom, filter, history
- [x] Location line edit + **LocationButtonBar** breadcrumbs
- [x] Context menus, properties, keyboard shortcuts
- [x] Clipboard / DND / TransferWorker / TransferDialog
- [x] Menus: File / Edit / View / Go / Help

### Phase 6 — Thumbnails — **done** (basic)

- [x] Thumbnailer1 D-Bus client; icon view requests
- [ ] Optional: local freedesktop cache path fallback without D-Bus
- [x] Optional: cancel in-flight requests on directory change

### Phase 7 — Archives — **done** (read-only)

- [x] TOC listing (`bsdtar`/`unzip`/`tar`); member extract on open
- [x] Full extract fallback; nested archives
- [x] Mutations blocked inside archives
- [ ] Optional: progress dialog for large TOC / extract (status only today)
- [ ] Optional: libarchive in-process listing (no external process)

### Phase 8 — Search / filter — **partial**

- [x] Name substring filter
- [x] Glob patterns
- [ ] Expression language (Python `filter/` / `expr/`) — defer
- [ ] Recursive content search — defer

### Phase 9 — Polish & packaging — **mostly done**

- [x] Desktop file, icons, About, settings, README/STATUS
- [x] Escape clears filter (not only focus quirks)
- [ ] Middle-click breadcrumb → open in new window (Python had this)
- [x] DnD onto breadcrumb segments (Python LocationButton)
- [ ] Audit README build instructions vs current flake
- [ ] Optional: AppStream metainfo

---

## Near-term queue (priority order)

1. [x] Escape key clears name filter
2. [x] DnD files onto breadcrumb buttons (drop into that directory)
3. [x] Cancel thumbnail requests when leaving a directory
4. [x] Glob filter (`*.png`)
5. [x] Breadcrumb middle-click / open in new window
6. [x] AppStream metainfo

---

## Explicitly out of scope / ignore

| Item | Reason |
|------|--------|
| Write into archives | Read-only by design for now |
| Most `programs/*` Python CLIs | dirops tools suffice |
| experiments/, face detect, QML, UDisks demos | Not MVP |
| Pixel-perfect Python UI | Functional modular UI |
| Cloning Python bugs | Fix by design |
| Windows/macOS first | Linux-first |

---

## Working process

- After each change series, suggest a detailed git commit message.
- Prefer small commits: library → tests → UI.
- Keep `dirtoo-py/` intact as reference.
