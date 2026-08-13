# TODO — dirtoo C++ port

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status (revised)

Critical freezes, DnD/Link, content-filter offload, Graphics reuse, and core
parity features are in place. Catch suite: 57 cases (Overwrite-directory test
aligned with into-dir resolution + `remove_for_overwrite` safety).

**Residual focus:** Audit 2026-08-13 (see bottom): archive TOC off GUI, SQLite WAL,
concurrent TagJobs, `tag://` Location, MainWindow gravity, quick-hash productization.
Prior: QuickFilter polish, list virtualization optional.

Source audit: **`AUDIT.md`** (inventory + deep review passes 2–2h, 2026-08).

### Parity freeze (still intentionally out of scope)

- Archive **write** / modify
- Remote / VFS backends (gio, KIO, …)
- Full Python `programs/*` CLI surface
- Debug mode action
- Kinetic / animated Graphics layout

---


### Recent polish (agent, 2026-08)

- [x] Context menu with large selection: no MatchContent / full-path Open With scan;
      unique MIME sample + lazy Open with…; selectedRows(0) not selectedIndexes
- [x] Ctrl+T / Tools → Tag… opens tag name dialog for selection
- [x] Multi-file tagging: progress dialog + processEvents (avoids hard hang;
      hashing still on GUI thread — residual vs “no hash on GUI” goal)
- [x] Startup opens cwd or CLI path, not `session/last_location`
- [x] Group by Day/Directory CPU spin: cache section labels at rebuild
      (`apply_grouping` + index-based `IsGroupStartRole` / `GroupLabelRole`)
- [x] Group by Session (10h mtime gap clusters; View → Group By → Session)
- [x] Tag archive members (extract+hash; Location URL as checksum key; badges/filter)

- [x] Icon/badge load failures always on stderr; qrc fallback for badges
- [x] Directory montage failures report concrete reasons on stderr
- [x] Startup under-development warning with permanent dismiss checkbox
- [x] Properties dialog shows file thumbnail / system icon preview
- [x] Read/write-protected file badges (`badge-readonly` / `badge-nowrite`)
- [x] Directory montages use XDG cached thumbs for videos/non-images
- [x] Directory montages are XDG-thumb-only; generate missing child thumbs first

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
| Inclusive numeric ranges | **Done** — `lo-hi` / `lo..hi` (duration unit inheritance) |

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
| Filter show/hide + pin, Escape | **done** (Ctrl+K = show+focus; Escape restores view focus) |
| Filter history | **done** |
| Filter DSL + contains/date/length/time/weekday/fuzzy/media | **done** |
| Inclusive ranges (`size:1M..50M`, `duration:3-10m`, …) | **done** |
| Content filters off GUI thread | **done** (`FilterWorker`) |
| Filter help + `dt-filter` + recursive search | **done** |
| QuickFilter bar (type/tag chips + pinned filters) | **done** (rebuild-on-keystroke polish open) |

### View & chrome

| Behaviour | Status |
|-----------|--------|
| Detail + Icons (Graphics) + Small icons + zoom + leap | **done** |
| Group by day / directory / duration (headers) | **done** (list + Graphics) |
| Message area / async path completion / save file list | **done** |
| Select all / time gaps / directory thumbnails | **done** |
| List virtualization | **Partial** — viewport row sampling for thumbs; model still full; Graphics windowed |

### Ops & dialogs

| Item | Status |
|------|--------|
| dirops + clipboard cut/copy/paste/**link** | **done** |
| Conflict / transfer / properties / about / preferences | **done** |
| Rename / New Folder / New File / Swap Names | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus + status badges + dir montage | **done** |
| Editable permissions | **done** — Properties OK → `dirops::set_permissions`; ops history |
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

### Session notes (2026-08-12)

Handoff bundles **dirtoo-003** … **dirtoo-015** (tip after 015: filter ranges).

#### Done this session

**Background activity toolbar**
- [x] Toolbar **Activity** indicator (Idle / task headline); click → task list + log ring
- [x] `ActivityMonitor` fed from status heuristics, thumbnail counts, transfers, TagJob,
      ChecksumDialog, Qt message handler (warnings+)

**MainWindow gravity (R6 remainder + collaborators)**
- [x] `FilterSearchChrome` — filter + recursive-search row chrome
- [x] `DevicesController::attach` — devices list wiring as value member
- [x] `ThumbnailCoordinator::request_rows` — viewport queue / archive extract path
- [x] `TagController` — Tag… dialog, progress, `TagJob` lifecycle

**Tags**
- [x] **Tag Manager** (Tools → Tag Manager…): list definitions, rename, delete,
      edit **label / color / badge**; chip paint uses those fields
- [x] Tag chips drawn **above** bottom-left meta (Width×Height)
- [x] `TagStore::delete_tag` (CASCADE on `file_tags`)
- [x] TagJob cancel via shared `atomic_bool` (no `invokeMethod` on dead worker)

**Thumbnails**
- [x] **Reload Thumbnails** deletes XDG cache (normal/large/x-large/xx-large/fail)
      and force-queues Thumbnailer1 (`Thumbnailer::remove_cache_for` + `force`)

**QuickFilter bar**
- [x] Auto chips from listing: `type:image|video|…`, `tag:…` (checksum-cache hits only)
- [x] Pin current filter; persist pins in `~/.config/dirtoo/quick_filters.ini`
- [x] Pin context menu: Edit filter…, Set label…, Directories… (`;`-separated paths),
      scope (everywhere / this directory / subtree), Remove
- [x] Visibility filtered by current location vs pin directory list

**Filter language**
- [x] Inclusive ranges: `lo-hi` or `lo..hi` on size, duration, width, height, fps,
      length, pages, filecount (e.g. `duration:3-10m`, `size:1M..50M`)
- [x] Duration: unit on high side only applies to both ends (`3-10m` = 3–10 minutes)

**Bugfixes**
- [x] **dt-search** positionals are QUERY (not directories); parity with Python simple mode
- [x] **Open with…** submenu fill: no `Qt::UniqueConnection` on lambda

#### Open issues / residuals from 2026-08-12 review

- [x] **QuickFilter rebuild cost (medium)** — `set_active_expression` only updates
      checked state (no widget teardown on keystroke)
- [x] **New pin default scope (UX)** — new pins default to *subtree* of current location
- [x] **Reload Thumbnails + archive members (medium)** — unextracted members now go
      through `ensure_archive_member_extracted` (same as request_rows) then force
      Thumbnailer1 on the real path; no more bare archive URL queue.
- [x] **Tag Manager file counts (low)** — `TagStore::count_files_for_tag` (SQL COUNT);
      Tag Manager reload no longer expands all paths via `files_for_tag`.
- [x] **Filter range unit tests (P3)** — size lo-hi / lo..hi match tests; duration
      range parse + unit inheritance smoke tests; width/height/length/fps/pages
- [ ] **MainWindow gravity (ongoing)** — see Refactoring track; chrome/controllers
      improved; `ThumbnailCoordinator::force_regenerate` took Reload Thumbnails
      off MainWindow. ops/nav/load still large

#### Open issues from 2026-08-13 (user report / feature requests)

**Activity / background work**
- [x] **Thumbnail activity counter only goes up, never down (high)** — Fixed:
      `on_thumbnail_ready`/`failed` always call `update_status_selection` so the
      last pending→ready/failed clears ActivityMonitor (previously skipped when
      pending hit 0 after the status change).
- [x] **Show which tags are currently being processed (nice)** — Activity label is
      now `Tagging <name>` (seeded at job start; updated on progress).
- [x] **Progress indicator shows “idle” while still at ~100% of one CPU (high)** —
      Root cause: `FileListModel::data` Duration column did
      `if (!meta || !meta->duration_ms) request(...)`. Cached image meta (no
      duration) is not negative, so `MediaMetaCache::request` hit memory and
      synchronously invoked the ready callback → `notify_row_changed` Queued →
      data() again → infinite loop with ActivityMonitor Idle. Fixed to only
      request when `!meta` (same pattern as Graphics/delegate).

**Tag UI / apply**
- [x] **Tag dialog: clickable list of existing tags (medium)** — TagNameDialog lists
      known tags; click appends to the field.
- [x] **Apply multiple tags at once (medium)** — Space/comma-separated names; TagJob
      applies the full list per file.
- [x] **Auto-complete for tag names (nice)** — QCompleter over TagStore list_tags().
- [x] **Click tag chip → filter for that tag (medium)** — Graphics + Small Icons
      (`FileItemDelegate::editorEvent`); Detail column icons are too small for chips.

**Tag model / filter language**
- [x] **`tag:` glob patterns (medium)** — Trailing `*` supported (`tag:location-*`).
- [x] **Allow `:` in tag names as namespace separator (medium)** — `:` allowed in
      normalize; chips show local part; `tag:doom` matches any `*:doom`;
      `tag:game:doom` exact.


#### Agent session 2026-08-13 (continued)

- [x] **Tag hyphens preserved** — `normalize_tag_name` no longer collapses `-` to
      `_` (`location-paris` stays hyphenated; globs still work).
- [x] **Reload Thumbnails → ThumbnailCoordinator::force_regenerate** — archive
      extract + force queue moved off MainWindow (gravity: thumbs surface).
- [x] **Tag dialog: remove current tags** — dialog lists tags already on the
      selection (checksum-cache hits); multi-select + Remove / double-click runs
      TagJob in Remove mode (`remove_tag_from_file`).
- [x] **QuickFilter: Untagged / Untagged images chips** — when the listing has
      images and/or a tags DB, auto-chips offer `type:image tagged:no` and
      `tagged:no` (checksum-cache semantics; no hashing on filter).
- [x] **normalize_tag_name leading `:` / `::`** — reject leading separators and
      empty namespaces (fixes Catch test).
- [x] **Tests via `nix flake check`** — `doCheck = false` on packages.dirtoo;
      tests installed to `$out/libexec/dirtoo/dirtoo-tests`; `checks.dirtoo-tests`
      runs that binary so check does not recompile when the package is cached.
- [x] **Keep selection visible after reorder** — snapshot paths before sort
      refresh; restore multi-selection by path; scroll `last_selected_path_`
      (cursor / current index) into view with priority over other selected rows.
- [x] **Places: Bookmarks section heading** — user bookmarks listed under a
      non-interactive bold "Bookmarks" row; no longer mixed with Home/Filesystem
      and XDG standard locations.

### Review residuals (earlier 2026-08)

Prioritized issues from a multi-day change review.

- [x] **Thumbs: request set ≠ on-screen layout (high)**  
  `viewport_model_rows()` from `slot_pos_`; no lowest-index truncate on viewport
  ranges (soft middle-biased cap 256). Landed in `676022a`.
- [x] **Tagging hashes on GUI thread (high)**  
  `TagJob` worker thread; progress dialog on GUI; cancel via shared atomic
  (not invokeMethod on worker).
- [x] **Tag chips hit SQLite during paint (medium)**  
  Path/URL → chips cache; clear after Tag… and on hard directory reload
  (navigation/refresh). Residual: no live watch on tags.sqlite from other processes.
- [x] **Filter FocusOut hide is fragile (low–medium)**  
  Qt6 has no `QFocusEvent::relatedWidget()`; deferred `focusWidget()` check
  (singleShot 0) so filter-row controls keep the bar open.
- [x] **Archive tag: extract-per-member cost (medium)**  
  TagJob reuses `dirtoo-archive-thumbs` member extract cache (shared with thumbs).
- [x] **Thumbnail `Pending` can stick (low)**  
  `clear_pending_thumbnails()` on cancel_all; `clear_stale_pending_thumbnails(60s)`
  on each viewport thumb request so lost D-Bus jobs can re-queue.
- [ ] **MainWindow gravity (ongoing)**  
  Hash/tag/thumb policy still grows MainWindow TUs despite R2 collaborators.
  Prefer new orchestration helpers over more `main_window_*.cpp` surface.
  Progress: `TagJob` + `TagController`; R6 `FilterSearchChrome` +
  `SidebarController::create`; `DevicesController::attach`;
  `ThumbnailCoordinator::request_rows` (viewport row discovery still on MainWindow);
  QuickFilterBar is separate chrome (not MainWindow methods).


| Item | Notes |
|------|--------|
| Operations history log | **done** — SQLite under `$XDG_STATE_HOME/dirtoo/`; full sources+items; no rollback |
| Location URL encoding | **Promoted** — see *Audit findings* (systemic) |
| Archive member thumbnails | **Improved** — shared `dirtoo-archive-thumbs` cache; prefer cache hit before extract |
| Watcher richness | **Improved** — multi-path + Linux inotify names; TOC `refresh_if_stale` |
| MainWindow factoring | **Improved** — 9 TUs incl. nav; setup/view/filter/thumbs/transfer/events/settings |
| True incremental FS watcher deltas | **Improved** — Linux inotify name deltas + small-set incremental collection patch; large bursts soft-merge |
| List / Graphics virtualization | **Graphics viewport window done**; Detail uses uniform row heights when no group/time-gap; Qt paints only visible rows |
| Rubber-band vs item drag | **done** |
| Nested drop into own selection | **done** |
| Small-icons grid metrics | **done** (fixed grid) |
| Caption shadow/outline | **done** — soft outline on icon captions |
| Transfer dedicated error dialog | Optional UX split |

---

## Audit findings (2026-08 deep review) — action queue

From `AUDIT.md` passes 2–2h. Core FM is usable; these are the concrete defects,
smells, and gaps worth scheduling. Not every item is a user-visible crash.

### P0 — correctness / data safety

| ID | Issue | Why | Direction |
|----|-------|-----|-----------|
| A1 | **Location URL encoding incomplete** | Only `%20`. `#`, `?`, non-ASCII break `as_url` round-trips, bookmark identity, thumbnail MD5 keys | **Done** — general percent encode/decode in `location.cpp`; Catch tests for space/`#`/`?`/archive entry |
| A2 | **Archive extract cache can go stale** | `.dirtoo-extracted` marker matches path only, not archive mtime/size | **Done** — cache dir stamp is mtime+size; Ready state revalidated against current stamp on `open()` |
| A3 | **Conflict Overwrite uses `remove_all` on existing destinations** | `dirops` Overwrite deletes the whole existing tree before write/rename. Overwriting a **directory** is catastrophic; dialog does not strongly distinguish file vs dir | **Done** — `remove_for_overwrite` refuses directories; conflict dialog disables Replace for dirs; Catch tests |
| A4 | **Transfer conflict CV lifecycle** | Worker blocks until UI `resolve_conflict`; shutdown without notify can hang the thread | **Mostly done already** — `cancel()` clears pending + notifies; dialog reject → cancel; re-verify on next transfer hang |

### P1 — architecture smells (not “bugs”, but wrong long-term shape)

| ID | Smell | Why it hurts | Direction |
|----|-------|--------------|-----------|
| S1 | **Archive listing/extract shells out to `bsdtar` / `tar` / `unzip` / `7z`** | Fragile verbose-text parsers | **Done** — **libarchive required** (CMake `REQUIRED`, flake); no CLI fallback (see AGENTS.md) |
| S2 | **`std::filesystem::remove_all` as the Overwrite primitive** | Same as A3 — policy API looks like “replace file” but implementation is “delete subtree” | **Done (refuse path)** — Overwrite is file/symlink only; dirs rejected at API + UI |
| S3 | **MainWindow multi-TU / god-header** | Core still large; header owns everything | **In progress** — R1 ops TU done (`main_window_ops.cpp`); see **Refactoring track** |
| S4 | **Dual icon paint paths** | `GraphicsFileItem` and `FileItemDelegate` must stay twin for montage/badges | **Done** — `icon_tile_paint.hpp` (badge, directory montage, status stickers) shared by both |

### P2 — reliability / scale / UX

| ID | Issue | Direction |
|----|-------|-----------|
| B1 | Archive member **size 0** when verbose parse fails | Fixed properly by S1 (libarchive sizes); until then, fixture tests for `parse_tv_lines` / `unzip -l` |
| B2 | Search jank on huge result sets | **Done** — batched `append_visible_items` + `notify_rows_appended` (32); Graphics `on_rows_inserted` relayouts without clearing tiles/selection |
| B3 | Watcher: directory-only events, full soft rescan | **Improved** — inotify names + incremental patch (≤48); else soft merge |
| B4 | Icon dir discovery | **Mostly done** — CMake installs full icon set to `share/dirtoo/icons`; runtime probes that path |
| B5 | Bookmarks sorted by URL (order lost) | **Done** — load preserves file order; dedup keeps first |
| B6 | Open With incomplete Desktop Entry / empty menu on mixed MIME | Document limits; improve intersection messaging |
| B7 | Conflict dialog wording for directories | **Done** — header says folder when dest is dir; Replace disabled + tooltip; body explains Rename/Skip only |

### P3 — tests to add (cheap, high value)

- [x] `parse_filter("type:video")` / `type:image` / `type:archive` matches expected extensions (`test_filter`)
- [x] Verbose archive listing fixtures → non-zero sizes (`test_archive_index` or new parse unit tests)
- [x] Location round-trip with space, `#`, non-ASCII (after A1)
- [x] dirops: Overwrite of **file** OK; Overwrite of **directory** refused (after A3)
      (`remove_for_overwrite`; Catch covers rename + copy/move when resolved dest is a dir)

### Explicit non-bugs (OOS / intentional)

- Archive **write**, remote VFS, full Python `programs/*`
- Full undo (operations history is log-only)
- Python `expr/` language (filter DSL is separate)
- `search://` Location protocol (search is a GUI mode)

### Suggested order

1. **A3 / S2** — stop treating directory overwrite like file replace (safety)
2. **A1** — Location encoding
3. **S1** — libarchive for TOC/extract (kills size parse fragility)
4. **A2** — extract cache invalidation
5. **A4** — transfer shutdown
6. **P3 tests** interleaved with the above

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

1. QuickFilter: avoid full chip rebuild on every filter keystroke; pin default scope
2. Reload Thumbnails for unextracted archive members via extract path
3. MainWindow gravity residuals (ops/nav/load surface)
4. Filter range unit tests; Tag Manager COUNT optimization (optional)
5. Transfer dedicated error dialog; remaining UX polish from user testing


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
6. **Polish**: icons, context menus, hidden-dir preference, watcher refresh of expanded nodes —
   **partial** ( + soft refresh on watcher
   create/remove parents and full dir change).

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

## Refactoring track (oversized units)

Goal: smaller translation units and eventually fewer responsibilities on
`MainWindow` itself — prefer extracting **collaborators** over endless
`main_window_*.cpp` dumps of the same class.

### Priority order

- [x] **R1** `main_window_ops.cpp` — clipboard + FS mutations + read-only guards
      (cut/copy/paste/mkdir/mkfile/rename/delete/swap; pure move, no behavior change)
- [x] **R2** Shrink `MainWindow` via collaborators (header members ↓):
      - [x] `ListPipelineWorkers` — dir-load / sort / filter threads + generations
      - [x] `PathCompletionService` — location-bar completion worker/completer
      - [x] `ThumbnailCoordinator` — Thumbnailer + aliases + dir montages
      - [x] `SidebarPlaces` — places model + rebuild/sync
      - [x] `ViewZoom` + `ViewMode` header + `FilterHistory`
      - [x] `DirectorySession` + `SearchSession` value types
      - [x] `LocationChrome` — breadcrumb / line edit / path completion
        (`location_chrome` owns bar widgets + signals; PathCompletionService nested)
      - [x] `SidebarController` — tree + places (devices already partly extracted)
        (`sidebar_controller.{hpp,cpp}`; devices remain DevicesController)
      - [x] `ThumbnailCoordinator` — owns Thumbnailer, aliases, dir-montage worker,
        `request_rows` (`thumbnail_coordinator.{hpp,cpp}`); viewport row discovery
        still on MainWindow
- [x] **R3** Split `libs/dirtoo-filter/src/predicates.cpp` (~1.8k) by domain
      (`_name` / `_media` / `_fuzzy` / `_meta` / `_content` / `_misc` + `predicates_detail.hpp`)
- [x] **R4** `GraphicsFileView` impl split — layout/windowing, selection/cursor, DnD
      (`graphics_file_view.cpp` core+layout; `_selection.cpp`; `_dnd.cpp`)
- [x] **R5** `FileListModel` — extract thumbnail/new-mark state helper; thin `data()`
      (`file_list_model_thumbs.cpp`: thumbs, new marks, child counts, icon_for)
- [x] **R6** `setup_central_ui` — after R2, build chrome in owners
      (setup split into `_central` / `_toolbar` / `_menus` + `_menus_{file,edit,view,sort,go}`;
       sidebar shell `SidebarController::create()`; filter + search rows
       `FilterSearchChrome::create_*()`; QuickFilterBar above filter row —
       MainWindow keeps event filters / actions / load wiring)

### Explicit non-goals (this track)

- PIMPL-only MainWindow with no real ownership change
- Fourth relative-size view / group-by polish mid-refactor
- Virtual hierarchies for filter matchers (anonymous classes are fine)


### Files removed in this track (zip clients must delete)

- `libs/dirtoo-filter/src/predicates.cpp` — old monolith (delete if overlay transfer keeps it)
- `libs/dirtoo-filter/src/predicates_rest.cpp` — intermediate split file; replaced by
  `predicates_meta.cpp`, `predicates_content.cpp`, `predicates_misc.cpp`

Keep: `predicates_name/media/fuzzy/meta/content/misc.cpp` and `predicates_detail.hpp`.

### Notes

- Mechanical TU moves first; collaborator extraction second.
- `main_window_common.hpp` supplies complete Qt types for all MainWindow TUs.

- Do not bulk-reformat unrelated code in the same commit as a move.

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
- [x] the rendering of group-by looks like a hack, if there isn't an
      obviously better way, leave it like that
      (shared paint_group_header + Graphics full-width drawForeground bands)
- [x] history isn't persistent between restarts
- [x] filenames still disappear and don't render properly
      (basename fallback when DisplayRole empty; caption height floor)
- [x] folders show up with size 0, not their actual size
      (FileInfo reads st_size for directories; Size column still shows "N items")
- [x] size show up as MiB, but should be base 1000 MB
      (SI KB/MB/GB in list + conflict dialog)
- [x] use Ctrl-k for Filter
      (Ctrl+K always shows + focuses filter edit via on_focus_filter; does not
       toggle. View menu "Show Filter" remains checkable; WidgetShortcut on the
       action so the global binding owns the key. Search uses Ctrl+F)
- [x] move filter bar to the bottom
- [x] change background color when filter bar is active, see dirtoo-py
      (view + filter row tint rgb(220,220,255))
- [x] only the first now in "Small Icons" has a filename, filename is
      invisible everywhere else
      (icon-style delegate kept enabled; taller grid for caption)
- [x] files in archives don't get thumbnails
      (unique path key via location URL; extract member off-thread then Thumbnailer1;
       map extracted path → archive model key)
- [x] drag&drop of files from inside an archive just gives a filename
      pointing to the archive
      (same-app drop resolves Location URL / extract cache; drag mimeData extracts
       members to cache and sets file:// for external apps + x-dirtoo-locations)
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
- [x] caution icon shows, unclear exactly when (thumbnail isn't ready?)
      (error badge only for image/video/pdf/office; other thumbnail failures
       clear to system icon without sticker)
- [x] 16:56:18.357 [warning] QGraphicsView::dragLeaveEvent: drag leave received before drag enter
      (track drag_entered_; only forward leave to QGraphicsView after accept)
- [x] ensure that filter language has all the dirtoo-py features, e.g. type:video
      (type:video|image|archive|audio via extension regex; help text updated)
- [x] file size doesn't show for archives
      (bsdtar/tar -tvf + unzip -l parsers populate ArchiveEntry.size)
- [x] Search across big directory can still lock the UI
      (synthetic hits; batch append 48 + 50ms deferred flush; status throttle;
       notify_rows_appended — Detail virtualization still optional)
- [x] implement mount and eject for udisks
      (UDisksClient::mount/unmount/eject + DevicesController context menu;
       async D-Bus; already in Phases 4–5)
- [x] give icons for hidden files a different background color, see dirtoo-py/
      (IsHiddenRole; muted gray tile bg + dimmed foreground in Detail/List/
       Icons/Graphics when basename starts with '.')
- [x] indicate when files are opened/closed (if that information comes
      via inotify for free, otherwise ignore for now)
      (IN_CLOSE_WRITE treated as modify for content-change detection; IN_OPEN
       omitted as too noisy — no separate open/close UI indicator)
- [x] directory thumbnails don't update when the directory content
      changed, or maybe it's the thumbnailing in general getting
      stuck?
      (clear montage status on create/remove/modify of dirs; low-priority regen)
- [x] directory thumbnails should be created automatically, though
      with very low priority only after everything else is done, as
      they might get expensive
      (schedule_directory_thumbnails_low_priority: 2.5s delay, max 12 dirs)
- [x] "Transfering files" dialog only shows progress for file count,
      should show the byte count transfer for the currently
      transfering file
      (dirops streams with on_progress mid-file; dialog shows bytes + %)
- [x] a fourth file view that shows icons at their relative size
      (ViewMode::RelativeIcons — Graphics flow layout; tile scale log2(size),
       clamp ~0.55–1.85; toolbar/menu "Relative Size")
      compared to what else is in the directory (do deffer for now)
- [x] thumbnail generation doesn't start for new files, "Reload Thumbnails" doesn't make them show up either
      (on_entries_changed requests thumbs for created regular files by path)
- [x] add button that switches the whole file manager into read-only
      mode, no file system manupulation should be possible when that
      switch is active
      (toolbar+View menu Ctrl+Shift+R; gates delete/rename/paste/mkdir/DnD/…)
- [x] add support for more times: atime, ctime, birth
      (FileInfo POSIX times; Detail columns Accessed/Changed/Created, off by default)
- [x] pressing shift selects/unselects the current item
      (Shift+Space toggles cursor selection in Icons; Ctrl+Space unchanged)
- [x] the type ahead quick search should be closable via the Escape key
      (LeapWidget Escape clears + hides + returns focus to parent; global Escape
       in on_clear_filter also dismisses leap first and restores file-view focus)
- [x] the fileview should have focus after startup
      (showEvent focuses current_view / graphics_view)
- [x] directory tree panel hide/show should be persistent
      (restore_settings applies ui/show_sidebar + ui/sidebar_width; persist
       skips width=0 while hidden so the last real width is kept)
- [x] move "block all filesystem modifications" button to the very
      right edge of the toolbar
      (expanding spacer + read_only_act_ at end of main toolbar)
- [x] give better indication when IO is busy and doing something in the background
      (status-bar busy_label_ with badge-loading.png + tooltip of current activity;
       driven by set_status ellipsis/known verbs and transfer_controller_.busy())
- [x] errors should print something to stdout/stderr, --verbose/--debug should be reserved for noisy messages
      (MessageArea::show_error always qWarning; default g_min_level remains QtWarningMsg)
- [x] leap search triggers when trying to do keyword search sometimes
      (when filter bar is visible, type-ahead inserts into filter_edit_ instead of leap)
- [x] Ctrl-k shouldn't toggle the filter bar, but always activate it
      (on_focus_filter; menu action stays checkable for View → Show Filter)
- [x] type ahead search should disappear when it no longer has focus
      (LeapWidget FocusOut hides + closed; closed restores file-view focus)
- [x] Escape after hiding the filter bar should return focus to the file view
      so leap/type-ahead can be activated again
      (on_clear_filter restores focus on graphics_view_ / current_view())
- [x] when resizing the window, thumbnails only showed for the old window size
      (viewport_model_rows from slot layout — not scene()->items; no kMaxBatch
       truncate on viewport rows; visible_window_changed; Detail/List Resize)
- [x] filter bar should disappear when focus returns to the file view and the
      bar is empty (FocusOut on filter_edit_; skip if pinned or focus stays in
      filter_row_; unchecks show_filter_act_)
- [x] slow IO (USB HDD that has to spin up) still locks up the GUI
      (mitigated: watcher create/modify deltas stat via QThreadPool +
       apply_watcher_upserts; removals stay on GUI with no FS I/O. Residual:
       rare single-path exists checks in thumbs/ops/conflict dialog; full
       directory load already off-thread)
- [x] clicking on folders in filter view might open the wrong folder
      (on_item_activated copies FileInfo by value before navigate; avoids race
       when async filter/sort replaces visible between click and slot)
- [x] status for how many files have been thumbnailed and metadata
      identified and how many are still to go
      (status_info shows "thumbs ready/tracked" while pending > 0; updated on
       thumbnail_ready/failed; FileListModel::thumbnail_counts())
- [x] filter should give results for the data currently available and
      update when new comes in, it currently just stalls when the
      directory hasn't been fully processed
      (async filter keeps previous visible by default; status shows count still
       shown; name filter applied immediately; re-applied on directory_loaded)
- [x] add a spinning busy icon when the browser is doing something,
      tooltip should show what it is doing
      (status-bar busy_label_ shows badge-loading.png while activity detected;
       tooltip carries the current status / transfer text)
- [x] filter should have a history showing past filters
      (FilterHistory + Up/Down in filter_edit_; Enter pushes)
- [x] aspect ration is missing from filter
      (aspect:/ar:/ratio: — 16:9, >1.5, =4:3; uses media width/height)
- [x] selection on drag&drop can end up with unrelated additional files
      (Graphics start_drag uses selected_row_set_ only; mouseMove clears to
       pressed row unless it was already in the multi-selection / Ctrl|Shift)
- [x] Icons view selection/cursor thorough review: click could deselect multi
      selection before drag, cursor could jump on (0,0) moves, Shift-click had
      no range, clear/select_row did not always emit, QGraphicsItem default
      press fought our logic.
      Intended behaviour documented on GraphicsFileView (header). Fixes:
      deferred single-select on press-of-selected (collapse on release without
      drag); view-owned Ctrl/Shift/plain click selection + anchor range;
      payload still only from selected_row_set_; cursor_move(0,0) seed-only;
      materialize_row helper; selection_changed emitted from clear/select_row.
- [x] when the keyboard controlled file select cursor in File View is
      out of screen, instead of scrolling the screen back to the
      cursor, leave the scroll untouched and warp the cursor to a
      position that is on screen. The screen should only scroll if the
      cursor is visible and would leave the screen with the next move
      (e.g. going up or down through the file list while the view
      follows)
      (GraphicsFileView: is_row_on_screen + warp_cursor_to_visible;
       scroll only when previous cursor was on-screen — follow mode)
- [x] Update AGENTS.md, we are working with git, git repository can be
      found at https://github.com/Grumbel/dirtoo.git every change
      should be committed and at the end a `git format-patch --stdout > changes.mbox`
      patch file shall be produced. If multiple changes are made in one go, each shall be
      a separate commit, all merged into one file with `git am` at the end.
      (AGENTS.md: new "Git workflow" section; `git format-patch --stdout <base>..HEAD > changes.mbox`)

- [x] QGraphicsView::dragLeaveEvent: drag leave received before drag enter
      (spam during DnD). Caused by calling QGraphicsView::dragLeaveEvent without
      going through base dragEnter (lastDragDropEvent unset). Leave handler now
      only clears drop-target highlights and accepts — does not call base.


---

## File checksums + tags (planned)

Checksums and tags are **separate**. Tagging never hashes files; it only
consumes digests from the checksum cache.

### Checksums (`dirtoo-hash` + `dt-checksum`)

- [x] **`dirtoo-hash` library** — one sequential read → CRC32, MD5, SHA-1, SHA-256
- [x] **`ChecksumStore`** — SQLite cache (`$XDG_CACHE_HOME/dirtoo/checksums.sqlite`);
      valid while size+mtime match; path keyed by absolute path / Location URL
- [x] **`dt-checksum`** — coreutils-like CLI (`-a` algo, `--refresh`, `--cached-only`,
      `-a all`, optional `--check`); uses cache by default
- [x] **GUI** — Tools → “Checksums…” dialog (selection, compute/refresh,
      table of digests, progress/cancel); Preferences button opens same dialog
- [x] Segfault fixes: checksum dialog no longer captures stack locals after
      modeless `show()`; tag filter TagLookup mutex; sqlite NULL text guards
- [x] Context menu: “Checksums…” (+ Copy SHA-256 inside dialog)

Primary algo for identity: **SHA-256**. Always compute the set in one pass.

### Tags (`dirtoo-tags`, later — depends on checksum cache)

- [x] SQLite under `$XDG_DATA_HOME/dirtoo/tags.sqlite` (tag defs + file_tags;
      file identity by sha256 from checksum store, path aliases)
- [x] **`dt-tag`** add/remove/list/files/def/rename — refuse if checksum unknown
      (optional `--hash-if-needed` calls checksum API, does not reimplement hash)
- [x] Tag indirection: `file_tags.tag_id` → `tag_defs.id`; rename does not retouch files
- [x] Filter / `dt-search`: `tag:name` / `tagged:yes|no` predicate
- [x] GUI: tag context menu (“Tag…”) + Tools menu + Ctrl+T
- [x] Multi-file Tag… uses progress dialog + TagJob worker (hash off GUI thread)
- [x] Badges from tag_defs (icon/color/label/optional image badge) in icon view
- [x] **Tag Manager** GUI — list / rename / delete / edit label·color·badge
- [x] Tag chips above Width×Height meta row

### Explicit non-goals (v1)

- No xattrs / no in-tree sidecars that modify the file tree
- No hashing on the GUI thread
- No tag match that re-hashes the whole directory on each filter keystroke
- Skip pure Unix-replacements already noted: `dt-shuffle`→`shuf`, chomp/glob

### Phasing

1. `dirtoo-hash` compute API + tests  
2. `ChecksumStore` + `dt-checksum`  
3. Checksum GUI (Tools + Preferences entry)  
4. `dirtoo-tags` + `dt-tag`  
5. `tag:` filter + badges


#### Agent session 2026-08-13 (checksummed / tag view / progress)

- [x] **`checksummed:yes|no` predicate** — cache hit only (no hashing on filter);
      aliases `hashed:`, `csum:`.
- [x] **Tag Manager Edit Tag dialog sizing** — min 520×320, ExpandingFieldsGrow,
      wider line edits.
- [x] **Progress reporting polish** — task summary includes percent when total known;
      headline elide at 72 chars.
- [x] **Show files for a tag in main view** — Tag Manager **Show files** loads matching
      paths into a search-like session; location chrome shows `tag://name`.
      Full `Location` protocol still deferred (see Feature ideas).

## Feature ideas (backlog — not committed work)

Useful product ideas gathered during the 2026-08-13 sessions. None are required
for MVP parity; pick when polishing tagging / large-library workflows.

| Idea | Notes |
|------|--------|
| **QuickFilter untagged chips** | Done: `Untagged` / `Untagged images` auto-chips. |
| **`checksummed:yes\|no` predicate** | Done. |
| **Pin “Untagged images” by default** | Optional first-run pin with subtree scope. |
| **Tag Manager → show tag files** | Done: **Show files** → virtual `tag://name` listing (search-session style). Full Location protocol later. |
| **Bulk checksum-then-tag** | From Tag… when cache misses, offer “Hash selection first” with progress (TagJob already hashes; expose as explicit step). |
| **Live tags.sqlite watch** | Chip cache invalidation when another process tags files (inotify on the DB file). |
| **Tag stats in Tag Manager** | Sort by count; histogram; last-used (needs `tagged_at` aggregate). |
| **Namespaces browser** | Group Tag Manager by `ns:` prefix; filter chips by namespace. |
| **Operations History → re-open paths** | Jump to dest dir of a logged op (dialog may already support; verify UX). |
| **Dual-pane / compare** | Out of current modular focus; large feature. |
| **Detail list virtualization** | Optional for 100k+ dirs; model still full. |
| **Smart albums** | Saved QuickFilter pins with labels are already a light form of this. |


---

## Audit findings (2026-08-13)

Full file inventory + notes: **`AUDIT.md`** (section *Full source inventory + audit pass (2026-08-13)*).  
~234 `.cpp`/`.hpp` files under `dirtoo/`. Tip reviewed: post–quick-hash / non-modal tag.

### Bugs / risks (do these)

- [x] **A1 — Archive TOC on GUI thread** — async `QtConcurrent` + generation; status/Activity “Indexing archive…”. Extract fallback still uses WaitCursor until ArchiveManager signals ready.
- [x] **A2 — `weakly_canonical` on navigate** — `normalize_file_path` uses absolute + lexically_normal only (no `weakly_canonical`). Symlink targets are not resolved into Location keys.
- [x] **A3 — SQLite busy / no WAL** — TagStore + ChecksumStore: `busy_timeout=5000`, `journal_mode=WAL`, `synchronous=NORMAL`. Single-writer queue still desirable under heavy load.
- [x] **A4 — Concurrent TagJobs** — FIFO queue in TagController; one active job; per-job ActivityMonitor id `tag-N`.
- [x] **A5 — Watcher start on slow mounts** — `is_directory` + `inotify_add_watch` on QtConcurrent; generation-cancelled on stop. Residual: `QFileSystemWatcher::addPath` still on GUI for non-inotify paths (e.g. archive file).
- [ ] **B7 — Search hit metadata** — Synthetic search `FileInfo`s skip stat; Detail columns (size already set; mtime/type/media) may stay blank until refresh/stat.
- [ ] **B8 — Open-from-archive temp files** — Extract under temp for open; define cleanup (age/size) so `/tmp/dirtoo-open` does not grow forever.
- [ ] **C8 — Launch flash timers** — Nested `QTimer`s not cancelled on navigate-away; low risk of painting wrong row after list replace (path key mitigates).

### Missing features / product

- [x] **B2 — Real `tag://` Location** — `Location::from_tag` / `is_tag` / `tag_query`; `open_location` loads union for `tag://a,b`; history + location bar; no search_session hack; F5 reloads tag listing.
- [x] **B3 — Quick hash productization** — Quick samples stored under `quick:` path keys; filter `checksummed:quick`; full `checksummed:yes` / TagJob / ensure() ignore samples.
- [x] **B5 — Icon spacing control** — Preferences → Appearance: Icon spacing + Icon label width; applied in Icons/RelativeIcons grid.
- [ ] **B10 — Preferences gaps** — Thumbnail crop/zoom ok; no spacing; no “default open app”; no hash policy (full vs prompt on large files).
- [ ] **B11 — Undo** — Operations history is log-only; no restore.
- [ ] **B12 — RelativeIcons polish** — Mode present; document in UI/help; verify layout quality.
- [ ] **C2 — Devices without UDisks** — Clear empty state / install hint.
- [ ] **C7 — Network FS watcher** — Poll fallback when inotify unreliable.
- [ ] **Filter: untagged in recursive search** — Works if expression supports `tagged:no`; confirm search path uses same predicates.
- [ ] **Tag autocomplete in filter bar** — Chip/list exists in Tag dialog; filter bar still plain text.
- [ ] **Background full checksum queue** — Idle hashing of visible folder for faster `checksummed:` / tagging later.

### Architecture / cleanup

- [ ] **D1 — MainWindow gravity** — Continue extracting collaborators; `main_window_ops.cpp` / `actions.cpp` / `settings.cpp` still large.
- [ ] **D2 — ActivityMonitor job tokens** — Per-job ids so concurrent tag/checksum/dir-load compose in the busy badge.
- [ ] **D4 — Shared HashService** — One queue for full/quick hash used by TagJob, ChecksumDialog, future idle scanner.
- [ ] **D5 — Store writer queue** — Serialize SQLite writes for tags + checksums.
- [ ] **D6 — Dual icon paint stacks** — GraphicsFileItem vs FileItemDelegate divergence (flash, tags, captions); share more via `icon_tile_paint`.
- [ ] **D7 — Fewer modal error boxes** — Batch tag/checksum failures → status + Activity details dialog.
- [ ] **C4 — Tests** — `hash_file` / `hash_file_quick` Catch cases done; still open: TagStore concurrent open smoke; DirectoryLoadWorker cancel; filter `checksummed`/`tagged` already partly covered.

### Intentionally out of scope (still)

- Archive write/modify
- Remote VFS (SMB/SFTP as first-class Location)
- Full multi-step undo
- Nested multi-payload Location stacks / `search://` as Location (unless we choose B2-style protocols)

