# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status (revised)

Critical freezes, DnD/Link, content-filter offload, Graphics reuse, and core
parity features are in place. Build/tests green (50/50 as of last Nix check).

**Residual focus:** Detail-view virtualization; inotify per-entry events (optional)
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
| Content filter I/O on GUI thread | **Mitigated** — `FilterWorker` + `replace_visible`; soft reload keeps list; sort uses `sort_items_only` + re-filter |
| Graphics full scene rebuild | **Mitigated** — item reuse; softer `layoutChanged` refresh |
| Thumbnails for every visible row | **Mitigated** — viewport batch (cap 64), scroll-driven |
| Watcher full rescan storms | **Mitigated** — debounce + soft reload + **merge_items** + cancel in-flight; **no emit on start**; hard nav cancels pending soft timer |
| Double paint unsorted→sorted | **Mitigated** for soft watcher reloads (skip intermediate paint; refresh after sort) |
| Full-range dataChanged on every refresh | **Mitigated** — `FileListModel::refresh` uses layoutChanged only |
| No list virtualization | **Mitigated** for Icons/Graphics — viewport window + precomputed slots; selection persists off-window; Detail/Small still full model |

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
| Small-icons grid uneven | **Mitigated** — fixed IconMode grid cells |
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
| Select All (Ctrl+A) | **done** (Graphics: all model rows, not only viewport) |
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
| True incremental FS watcher deltas | **Partial** — merge_items after soft rescan; still O(n) readdir (no inotify names) |
| List / Graphics virtualization | **Graphics viewport window done**; Detail/Small still full QAbstractItemView |
| Rubber-band vs item drag | **done** |
| Nested drop into own selection | **done** |
| Small-icons grid metrics | **done** (fixed grid) |
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
3. [x] Graphics viewport windowing — only tiles in view (± margin) exist as QGraphicsItems
4. [partial] Soft watcher merge via FileCollection::merge_items (still full readdir; no inotify deltas)
5. [x] Small-icons grid polish — fixed IconMode grid cells

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

---

## Bugs and Issues from User Testing

- [x] Home/End should jump to the top/bottom of the file view
- [x] type-ahead in the file view should jump to a file matching the name, currently does nothing
- [x] folders with thumbnails should still be recognizable as folders,
      use them as background for a normal folder icon, see dirtoo-py/.
      (folder emblem overlaid on directory montages)
- [x] a dedicated reload button to reload a folder, inotify can't
      always be dependend up on or isn't available sometimes
- [x] thumbnail generation isn't starting
      (extension-based MIME for Thumbnailer1; octet-stream was often ignored)
- [ ] new files where marked with a little sticker in dirtoo-py, they aren't anymore
      (code path exists via `IsNewRole` / `mark_new` on watcher soft reload — needs verification)
- [x] no debug messages make it hard to lack what is going on, add a
      --verbose (regular events) and --debug (extreme debug messages)
      flag for more verbose messages
- [x] if a file is drag&dropped and hovering over a folder, the folder
      should indicate that it is the drog target at the moment
      (Graphics view drop-target highlight)
- [ ] folders should show how many files they contain (i.e. filecount, non-recursive)
- [x] right click menu should follow normal conventions, currently looks very unorganized
      (grouped Open / Clipboard / Edit / Create / Thumbnails sections)
- [ ] proper mime-type handling is missing, no "Open With...", no
      default mime-apps listed in context menu, see dirtoo-py/
      ("Open with…" exists; default mime-app list still missing)
- [x] the chunky button in the LocationBar in dirtoo-py/ looked
      better, "Location:" label didn't hurd either.
- [x] filters should reset when changing directories, unless Pin filter is active
- [ ] the Location syntax of dirtoo-py/ was better than the new JAR inspired one
- [ ] filecount (recursive here) isn't displayed for archives
- [x] Right click seems to cancel the selection
- [x] Properties dialog doesn't open up
      (was likely selection-clear on right-click; preserved multi-select + explicit Properties)
- [x] Rename dialog doesn't open up
      (same selection fix as Properties)
- [ ] dirtoo-py/ had a much better
- [x] sort and group-by used to have buttons in the toolbar
- [ ] the rendering of group-by looks like a hack, if there isn't an
      obviously batter way, leave that
- [x] history isn't persistent between restarts
- [ ] filenames still disappear and don't render properly
