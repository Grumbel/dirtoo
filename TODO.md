# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status (revised)

Critical freezes, DnD/Link, content-filter offload, Graphics reuse, and core
parity features are in place. Build/tests green (50/50 as of last Nix check).

**Residual focus:** operations history log (see below); inotify per-entry events
(optional); transfer dedicated error dialog; remaining DnD edge cases.

Source audit: **`AUDIT.md`** (file inventory + parity notes, 2026-08).

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
| No list virtualization | **Mitigated** for Icons/Graphics — viewport window; Detail: uniform heights + no GUI-thread mtime stat; model is still full (Qt paints visible rows only) |

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
| Dual icon paths (Graphics + QListView) | **Acceptable** — Small Icons is compact IconMode grid; full Icons use Graphics view |

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
| Operations history log | **done** (log + dialog; no rollback) |

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
| Operations history log | **done** (log + dialog; no rollback) |

---

## Operations history (replaces Undo)

**Do not implement full Undo/rollback** for complex filesystem ops (cross-device
moves, partial tree copies, permission batches are not reliably reversible).

Instead, track an **operations history log**:

| Field | Content |
|-------|---------|
| Timestamp | When the op started / finished |
| Operation | rename, move, copy, delete, mkdir, mkfile, symlink, swap, permissions (when editable) |
| Sources | Path(s) involved |
| Destination | Target path when applicable |
| Outcome | success / skipped / failed (+ short error) |
| App context | Optional: window location, user-visible label |

**UI:** **Edit → Operations History…** — filterable tree (per-item source→dest),
Go to Folder, Clear. Storage: SQLite at `$XDG_STATE_HOME/dirtoo/operations-history.sqlite`
(fallback `~/.local/state/dirtoo/…`). JSON columns for sources/items.

**Wiring:** record from `TransferWorker` / paste / rename / dirops call sites
(and future permission changes). CLI `dt-*` tools may log optionally later.

---

## Residual / optional polish

| Item | Notes |
|------|--------|
| Operations history log | **done** — SQLite under `$XDG_STATE_HOME/dirtoo/`; full sources+items; no rollback |
| Location URL encoding | Only `%20` today; non-ASCII / reserved chars |
| Archive member thumbnails | Weak; needs extract path or special URI |
| Watcher richness | QFileSystemWatcher only; no archive extract-dir watch |
| MainWindow factoring | **Started** — NavigationHistory, SearchController, TransferController extracted |
| True incremental FS watcher deltas | **Partial** — merge_items after soft rescan; still O(n) readdir (no inotify names) |
| List / Graphics virtualization | **Graphics viewport window done**; Detail uses uniform row heights when no group/time-gap; Qt paints only visible rows |
| Rubber-band vs item drag | **done** |
| Nested drop into own selection | **done** |
| Small-icons grid metrics | **done** (fixed grid) |
| Caption shadow/outline | **done** — soft outline on icon captions |
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

---

## Planned: directory tree sidebar + UDisks2 volumes

**Status:** **Phases 1–5 done** — sidebar tree, Places+bookmarks, UDisks2 mounted+unmounted volumes, Mount/Unmount/Eject + live updates. Phase 6 polish remaining.

**User goals**

1. Left **directory tree** sidebar for hierarchical navigation (like classic file managers).
2. Surface **disks / partitions / mounts** via **UDisks2** D-Bus (not only `$HOME` paths).
3. **Toolbar/menu control** to show/hide the tree sidebar (persisted).

Python reference: experimental `dirtoo-py/experiments/udisks/` (`udisksqt.py`); main GUI does **not** ship a production tree+volumes UI — C++ can design cleanly rather than clone incomplete experiment code.

### Architecture sketch

```
┌──────────── MainWindow ─────────────────────────────────────────┐
│  Toolbar … [Toggle Tree] …                                      │
│  Location bar / breadcrumbs                                     │
│ ┌─ QSplitter ─────────────────────────────────────────────────┐ │
│ │ Sidebar          │  Existing central column                 │ │
│ │ ┌──────────────┐ │  (location chrome, message area,         │ │
│ │ │ Places/Vols  │ │   view_stack_ Detail|Icons|Small,        │ │
│ │ │ (UDisks2 +   │ │   filter bar)                            │ │
│ │ │ bookmarks)   │ │                                          │ │
│ │ ├──────────────┤ │                                          │ │
│ │ │ Dir tree     │ │                                          │ │
│ │ │ QTreeView    │ │                                          │ │
│ │ └──────────────┘ │                                          │ │
│ └──────────────────┴──────────────────────────────────────────┘ │
│  Status bar                                                     │
└─────────────────────────────────────────────────────────────────┘
```

**Layout**

- Wrap the current central content in a horizontal **`QSplitter`**.
- Left pane: sidebar widget (min width ~160–200px, default ~220px).
- Right pane: existing vertical layout (location, views, filter).
- Sidebar visibility toggled by hiding/showing the left widget (or collapsing splitter sizes to 0); **do not** destroy the tree model on hide so expand state can survive short toggles.

**Toggle UX**

- Toolbar button (icon `view-sidetree` / `sidebar-show-left` / `view-list-tree`) checkable.
- View menu item **“Show Directory Tree”** (checkable), shortcut e.g. **F9** (common in file managers; confirm no conflict).
- Persist in `AppSettings`: `ui/sidebar_visible` (bool), `ui/sidebar_width` (int).

### Directory tree (phase 1 — local FS only)

| Piece | Responsibility |
|-------|----------------|
| `DirectoryTreeModel` | Lazy `QAbstractItemModel`: roots = configured places; children = subdirs only (not files). Populate on `canFetchMore` / `fetchMore`. |
| `DirectoryTreeView` | `QTreeView`, single selection, expand on double-click or single-click policy TBD. |
| Integration | Activate path → `MainWindow::open_location`. Current location → expand+select matching node (best-effort, async if deep). |

**Constraints (match project rules)**

- **No directory listing on the GUI thread** for large trees: use a small worker or Qt concurrent for `directory_iterator` of one parent; cache results.
- Ignore non-directories; respect “show hidden” only if we later sync that preference (default: hide dot-dirs in the tree).
- Archives: **out of scope for v1** of the tree (opening an archive stays in the main view only). Optional later: synthetic archive nodes.

**Sync rules**

- Navigating from the tree opens that folder in the main view.
- Navigating from the main view (breadcrumb, double-click folder) updates tree selection/expansion when the sidebar is visible.
- Watcher: optional soft refresh of the expanded parent only (v1 can rescan one directory node on inotify for that path).

### UDisks2 (phase 2 — volumes / disks)

| Piece | Responsibility |
|-------|----------------|
| `libs/dirtoo-volumes` (new) or `apps/dirtoo/udisks_client.*` | Thin Qt D-Bus client for `org.freedesktop.UDisks2`. Prefer a small library if CLIs or tests need it later; start in-app if faster. |
| ObjectManager | Subscribe to `InterfacesAdded` / `InterfacesRemoved` / property changes. |
| Models | Drive / Block / Filesystem / Partition tables as needed. |

**Data to expose (minimum useful set)**

- Drive: model, vendor, size, connection bus (USB/SATA), media removable.
- Block: device file (`/dev/…`), size, id label, id UUID, hint name.
- Filesystem: mount points, type (ext4, vfat, …), size.
- Partition: number, type UUID (optional).

**UI placement**

- Top of sidebar: **“Devices”** section (or separate small list above the tree).
- Rows: label or device node + mount point if mounted; icon by bus/media (disk, usb, optical).
- Click mounted volume → `open_location(mount_point)`.
- Context menu (phase 2b): Mount / Unmount / Eject via UDisks2 methods (`Filesystem.Mount`, `Filesystem.Unmount`, `Drive.Eject`) with error reporting in the message area — **requires** careful async D-Bus calls and polkit interaction (user may see system auth dialogs).

**Graceful degradation**

- If the session bus or UDisks2 service is missing: hide Devices section or show “Disks unavailable”; tree still works for local paths.
- No hard dependency at build time beyond Qt6 DBus (already used for thumbnails).

**Security / safety**

- Read-only property queries first; mount/unmount only from explicit user action.
- Never run privileged helpers ourselves; always go through UDisks2.

### Places / bookmarks (phase 1.5)

- Fixed roots: Home, Filesystem root `/`, optional XDG user dirs (Desktop, Documents, Downloads) if they exist.
- Reuse existing bookmarks store if present for user “Places”.
- Devices from UDisks2 appear above or below Places, clearly grouped.

### Settings & accessibility

- `AppSettings`: sidebar visible, width, maybe “tree show hidden directories”.
- Keyboard: tree view participates in tab order; arrows expand/collapse; Enter opens.
- High-contrast: use palette, not hard-coded sidebar colors.

### Implementation phases (recommended order)

1. **Splitter + sidebar shell + toggle + settings** — **done** (F9, toolbar, View menu, `ui/show_sidebar`, `ui/sidebar_width`).
2. **`DirectoryTreeModel` lazy local dirs + navigation sync** — **done** (QtConcurrent fetch; click opens; best-effort path highlight).
3. **Places roots** — **done** (Home, `/`, XDG dirs, bookmarks).
4. **UDisks2 client** + Devices list (mounted volumes only) — **done** (read-only; live refresh on interface changes best-effort).
5. **UDisks2** unmounted volumes + Mount/Unmount/Eject actions + live updates — **done**
   (async Mount/Unmount/Eject; ObjectManager + PropertiesChanged debounce; devices context menu).
6. **Polish**: icons, context menus, hidden-dir preference, watcher refresh of expanded nodes.

### Explicit non-goals (for this feature track)

- Full network VFS in the tree (SMB/SFTP) — still out of scope.
- Partition editing / format / resize.
- Optical burn / RAID management.
- Replacing the main Detail/Icons/Small stack with the sidebar tree.

### Files likely touched

- `apps/dirtoo/main_window.cpp/.hpp` — splitter, toggle action, settings.
- New: `directory_tree_model.*`, `directory_tree_view.*`, `sidebar_widget.*` (or similar).
- New: `udisks_client.*` or `libs/dirtoo-volumes/`.
- `app_settings.*` — sidebar keys.
- `resources/` — optional icons.
- `flake.nix` / CMake — only if a new library target is added.
- Tests: model unit tests with a temp directory tree; UDisks2 mocked or optional integration.

### Risks

- Lazy tree + current-location sync can be racy on deep paths — expand step-by-step with generation tokens.
- UDisks2 property variants (`ay` mount points) need careful QDBus decoding.
- Mount/Unmount without blocking the GUI (async pending calls).

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
- [x] new files where marked with a little sticker in dirtoo-py, they aren't anymore
      (fixed: thumbnail ready no longer clears IsNewRole; marks persist until reload)
- [x] no debug messages make it hard to lack what is going on, add a
      --verbose (regular events) and --debug (extreme debug messages)
      flag for more verbose messages
- [x] if a file is drag&dropped and hovering over a folder, the folder
      should indicate that it is the drog target at the moment
      (Graphics view drop-target highlight)
- [x] folders should show how many files they contain (i.e. filecount, non-recursive)
      (async ChildCountRole; badge + Size column "N items")
- [x] right click menu should follow normal conventions, currently looks very unorganized
      (grouped Open / Clipboard / Edit / Create / Thumbnails sections)
- [x] proper mime-type handling is missing, no "Open With...", no
      default mime-apps listed in context menu, see dirtoo-py/
      (Open with submenu from mimeapps.list + desktop files; Other Application…)
- [x] the chunky button in the LocationBar in dirtoo-py/ looked
      better, "Location:" label didn't hurd either.
- [x] filters should reset when changing directories, unless Pin filter is active
- [x] the Location syntax of dirtoo-py/ was better than the new JAR inspired one
      (as_url uses file://…//archive[:entry]; JAR archive://…!/… still accepted)
- [x] filecount (recursive here) isn't displayed for archives
      (archive directory child counts from index; disk archives via media meta)
- [x] Right click seems to cancel the selection
- [x] Properties dialog doesn't open up
      (was likely selection-clear on right-click; preserved multi-select + explicit Properties)
- [x] Rename dialog doesn't open up
      (same selection fix as Properties)
- [x] sort and group-by used to have buttons in the toolbar
- [ ] the rendering of group-by looks like a hack, if there isn't an
      obviously better way, leave it like that
- [x] history isn't persistent between restarts
- [x] filenames still disappear and don't render properly
      (basename fallback when DisplayRole empty; caption height floor)
- [x] folders show up with size 0, not their actual size
      (FileInfo reads st_size for directories; Size column still shows "N items")
- [x] size show up as MiB, but should be base 1000 MB
      (SI KB/MB/GB in list + conflict dialog)
- [x] use Ctrl-k for Filter
      (Ctrl+K; Search uses Ctrl+F)
- [x] move filter bar to the bottom
- [x] change background color when filter bar is active, see dirtoo-py
      (view + filter row tint rgb(220,220,255))
- [x] only the first now in "Small Icons" has a filename, filename is
      invisible everywhere else
      (icon-style delegate kept enabled; taller grid for caption)
- [ ] files in archives don't get thumbnails
- [partial] drag&drop of files from inside an archive just gives a filename
      pointing to the archive
      (mimeData emits Location URL file://…//archive:entry; external extract-on-drop optional)
- [x] use human-friendly ISO date/time: "2011-12-21 16:14"
- [x] in About page, use actual URL, not "Project Page" text on link
- [x] Segfault on "Open Containing Folder"
      (context menu captured FileInfo* into local vector; capture Location by value)
- [x] Detail View should show file metadata (fps, dimension, duration, ...)
      (Dimensions + Duration columns via MediaMetaCache; fps still in Properties)
- [x] Small Icon view is only showing one item per line, it should
      show multiple in a grid style view
      (renamed to List; ListMode + TopToBottom + wrapping = Win95 columns;
       icon left of filename via icon_style=false + QStyledItemDelegate)
- [x] --help options of some tools/ look small and miserable, should be long and detailed
      (dt-move/copy/rename/rm/mkdir/mkfile/swap/symlink expanded)
- [x] remove dt-move <from> <to>, `-t` should be required
- [x] document rename conflict resolution
      (dt-rename --help + dirops ConflictPolicy docs; rename → stem (N).ext)
- [x] conflict dialog should show thumbnails of the affected files
      (cache lookup + QFileIconProvider fallback)
