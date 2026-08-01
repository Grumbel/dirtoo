# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status (revised)

Critical freezes, DnD/Link, content-filter offload, Graphics reuse, and core
parity features are in place. Build/tests green (50/50 as of last Nix check).

**Residual focus:** list/icon virtualization; true incremental FS deltas
(add/remove without full rescan); DnD edge cases; small-icons layout polish.

### Parity freeze (still intentionally out of scope)

- Archive **write** / modify
- Remote / VFS backends (gio, KIO, …)
- Full Python `programs/*` CLI surface
- Debug mode action
- Kinetic / animated Graphics layout

---

## Critical / high-priority defect queue

### 1. UI freezes / jank on large directories

| Cause | Status |
|-------|--------|
| Extra stats / `weakly_canonical` per entry | **Mitigated** — `directory_entry` + `from_path_unchecked` |
| Full listing cost | **Partial** — async + cancellable; still O(n) stats |
| Content filter I/O on GUI thread | **Mitigated** — `FilterWorker` + `replace_visible` |
| Graphics full scene rebuild | **Mitigated** — item reuse; softer `layoutChanged` refresh |
| Thumbnails for every visible row | **Mitigated** — viewport batch (cap 64), scroll-driven |
| Watcher full rescan storms | **Mitigated** — 200ms debounce + soft reload + cancel in-flight |
| Double paint unsorted→sorted | **Open** — two refreshes after load (acceptable; not a freeze) |
| No list virtualization | **Open** — all rows still materialized |

### 2. Drag & drop

| Issue | Status |
|-------|--------|
| Copy-biased Graphics drag | **Fixed** — Shift=Move, Ctrl=Copy, Alt/Ctrl+Shift=Link |
| Folder drop only in Graphics | **Fixed** — list/tree resolve directory row |
| No Link / symlink DnD | **Fixed** — `LinkAction` + `dirops::create_symlink` |
| Rubber-band vs item drag | **Fixed** — item press → NoDrag + item drag; empty → rubber-band |
| Nested same-app drop checks | **Fixed** — refuse drop onto self / into selected folder |

### 3. Icon layout and look

| Issue | Status |
|-------|--------|
| Hardcoded scene background | **Fixed** — `palette().base()` |
| Group headers in Graphics | **Fixed** — labels + row break at group start |
| Directory composite thumbs | **Fixed** — up to 9-tile montage (explicit action) |
| Caption / tile geometry | **Partial** — themed captions; no shadow/outline renderer |
| Small-icons grid uneven | **Open** |
| Dual icon paths (Graphics + QListView) | **Acceptable** — Small Icons uses list mode by design |

### 4. Filter correctness

| Issue | Status |
|-------|--------|
| Case-sensitive `Contains`/`Containsre`/… | **Fixed** |
| Charset predicate limited | **Documented** (ascii / utf-8 / latin1) |
| Content filters on GUI thread | **Mitigated** via `FilterWorker` |

---

## Python features → C++ parity

### User-visible local FS

| Feature | Status |
|---------|--------|
| Select All (Ctrl+A) | **done** |
| Create empty file | **done** |
| Symlink / Link paste & DnD | **done** |
| Directory thumbnails | **done** (Make Directory Thumbnails) |
| Swap Names | **done** |
| Show abspath vs basename | **done** (Show Full Paths) |
| Time gaps | **done** (≥6h separator) |
| Filter line history | **done** (Up/Down) |
| Transfer error / request dialogs | **partial** (folded into transfer/conflict) |
| Undo menu | **missing** (Python may be stub) |

### Acceptable gaps / out of scope

| Feature | Notes |
|---------|------|
| `programs/*` CLIs | Out of scope except library test helpers |
| Archive write | Out of scope |
| Remote VFS | Out of scope |
| Kinetic layout animation | Out of scope |
| Face-detect / experiment code | Not product |

---

## Python UI → C++ parity (checklist)

### Navigation & mouse

| Behaviour | Status |
|-----------|--------|
| Middle-click breadcrumb / directory / archive → new window | **done** |
| Middle-click Parent toolbar | **done** |
| Middle-click History menu → new window | **done** |
| Location bar ↔ line edit, path completer | **done** |
| Location history / bookmarks | **done** |

### Filter & search

| Behaviour | Status |
|-----------|--------|
| Filter show/hide + pin, Escape | **done** |
| Filter history | **done** |
| Filter DSL + contains/date/length/time/weekday/fuzzy/media | **done** |
| Content filters off GUI thread | **done** (`FilterWorker`) |
| Filter help + `dt-filter` + recursive search | **done** |

### View & chrome

| Behaviour | Status |
|-----------|--------|
| Detail + Icons (Graphics) + Small icons + zoom + leap | **done** |
| Group by day / directory / duration (headers) | **done** (list + Graphics) |
| Message area / async path completion / save file list | **done** |
| Select all / time gaps / directory thumbnails | **done** |
| List virtualization | **open** |

### Ops & dialogs

| Item | Status |
|------|--------|
| dirops + clipboard cut/copy/paste/**link** | **done** |
| Conflict / transfer / properties / about / preferences | **done** |
| Rename / New Folder / New File / Swap Names | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus + status badges + dir montage | **done** |
| Editable permissions | **open** (display-only) |
| Undo | **open** |

---

## Residual / optional polish

| Item | Notes |
|------|--------|
| True incremental FS watcher deltas | Soft full rescan today |
| List / Graphics virtualization | Large dirs still allocate all items |
| Rubber-band vs item drag | **done** |
| Nested drop into own selection | **done** |
| Small-icons grid metrics | SequenceMode parity |
| Caption shadow/outline | Python renderer polish |
| Transfer dedicated error dialog | Optional UX split |

---

## Architecture notes (must keep)

**GUI thread must not perform filesystem or network I/O** for listings,
metadata probes, bulk transfers, or **content-filter evaluation**.

Use workers (`DirectoryLoadWorker`, `SortWorker`, `FilterWorker`, search,
transfer, `DirectoryThumbnailWorker`), `MediaMetaCache`, and viewport-scoped
thumbnail work. `dirops` remains Qt-free.

### View tech

| Mode | Implementation |
|------|----------------|
| Detail | `QTreeView` + `FileListModel` + `FileItemDelegate` |
| Icons | `GraphicsFileView` + `GraphicsFileItem` |
| Small icons | `QListView` list mode |

### Architecture violation status

1. ~~Content predicates on GUI thread~~ → `FilterWorker`
2. ~~mime probing on GUI thread~~ → avoided
3. ~~Watcher without debounce~~ → 200ms + soft reload
4. ~~Graphics mass allocation~~ → item reuse + softer refresh

---

## Suggested work order (next)

1. [x] Graphics: item-press → drag, empty → rubber-band
2. [x] Guard drop into own selected folder / nested selection
3. [ ] List virtualization or windowed Graphics population for huge dirs
4. [ ] Incremental watcher (inotify event → add/remove single entries)
5. [ ] Small-icons grid polish

### Completed work order

1. [x] Listing cost (`directory_entry` / `from_path_unchecked`)
2. [x] Watcher debounce + soft reload
3. [x] Graphics item reuse
4. [x] Viewport thumbnails
5. [x] FilterWorker for content predicates
6. [x] DnD modifiers + folder drop + Link
7. [x] Icons theme/group headers
8. [x] Select All, New File, Link paste, dir montages, time gaps, full paths, swap names

---

## Working process

- Suggest a detailed commit message after each change series.
- Keep `dirtoo-py/` as reference only.
- Prefer small, reviewable commits; do not bulk-reformat unrelated code.
- When fixing freezes, measure with directories of 10k+ entries.
- **Always update `TODO.md` and `AGENTS.md`** when closing/opening items or
  changing user-visible behavior (see AGENTS.md → Documentation).
