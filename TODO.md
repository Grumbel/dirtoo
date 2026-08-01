# TODO — dirtoo C++ port (UI parity & remaining work)

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status

Core file manager MVP is **done**. Filter DSL, multi-window, dirops, archives
(read-only), and packaging are in place.

---

## Python UI analysis → C++ parity

### Navigation & mouse

| Python behaviour | C++ status |
|------------------|------------|
| Middle-click breadcrumb / directory / archive → new window | **done** |
| Middle-click Parent toolbar | **done** |
| Middle-click History menu entries → new window | **done** (`HistoryMenu`) |
| Location bar ↔ line edit, path completer | **done** |
| Location history menu | **done** |
| Bookmarks menu (file store, middle-click) | **done** |

### Filter & search

| Python behaviour | C++ status |
|------------------|------------|
| Filter show/hide + pin, history Up/Down, Escape | **done** |
| Filter DSL (`and`/`or`/`not`/`()`, glob, regex, size, type) | **done** (`dirtoo-filter`) |
| Filter help | **done** (Help → Filter expression help) |
| CLI filter tool | **done** (`dt-filter`) |
| Content / recursive search | partial: recursive name/filter search done; content `contains:` still open |
| Media metadata predicates | **done** (ffprobe: width/height/duration/framerate) |

### View & chrome

| Python behaviour | C++ status |
|------------------|------------|
| Detail + icons, zoom, leap widget | **done** |
| Message area | **done** (transient banner) |
| Preferences, transfers, properties, About | **done** |
| Async path completion worker | **done** |
| Graphics View icon scene | deferred (may revisit for flexibility) |

### Ops

| Item | Status |
|------|--------|
| dirops + tools | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus | **done** |

---

## Near-term queue

1. [x] Leap widget
2. [x] History menu + middle-click new window
3. [x] Middle-click Parent
4. [x] Filter show/hide + pin
5. [x] Preferences dialog
6. [x] Optional: async path completion worker
7. [x] Filter DSL + `()` grouping
8. [x] `dt-filter` CLI
9. [x] Message area
10. [x] Bookmarks menu
11. [ ] Optional: `contains:` content match (careful with large files)
12. [x] Optional: recursive directory search UI/CLI (`dt-filter -r`, View → Recursive Search)

---


---

## Responsive UI & metadata architecture (in progress)

**Guiding rule:** the GUI thread must not perform filesystem or network I/O.
That keeps the UI fluid today and leaves room for virtual backends later
(archive views, SFTP, other VFS). Listing, probing, thumbnails, and cache
reads/writes belong on workers; the UI only applies results and paints from
in-memory state.

Python used per-file XML metadata sidecars — too slow at scale. C++ will use a
**single SQLite database** for durable media (and later general) metadata.

### SQLite metadata cache

| Topic | Decision |
|-------|----------|
| Store | `$XDG_CACHE_HOME/dirtoo/meta.sqlite` (fallback `~/.cache/dirtoo/`) |
| Key | Absolute path + `mtime_ns` + `size` (invalidate when either changes) |
| Columns (v1) | path, mtime_ns, size, width, height, duration_ms, framerate, probed_at |
| Later | mime/type, pages, file_count, checksum hooks, backend id for non-file URIs |
| Writers | Worker threads only; one connection per worker or serialized write queue |
| Readers | Workers hydrate an **in-memory** map; GUI reads memory only |
| Probe | ffprobe (override `DIRTOO_FFPROBE`); skip non-media extensions |
| CLI | `dt-filter` / tools may use the same cache on their own threads |

Schema evolution via `PRAGMA user_version` migrations.

### Async layers (ordered)

1. **MediaMetaCache (memory + SQLite + worker pool)** — done/in progress  
   - `try_get(path)` → optional in-memory hit (no I/O)  
   - `request(path, gen)` → enqueue; worker: SQLite → maybe ffprobe → memory → notify  
   - Paint/sort/filter **never** call ffprobe or SQLite on the GUI thread  

2. **FileItemDelegate** — paint from `try_get` only; request missing rows for viewport  

3. **DirectoryLoadWorker** — `list_directory` / archive TOC off UI thread; generation token  

4. **Sort worker** — name/size/mtime off-thread; media sorts wait on meta coverage or use cached fields only  

5. **Filter** — cheap predicates immediate; media predicates match against memory cache / worker  

6. **Thumbnails** — keep D-Bus async; debounce + viewport priority; shared concurrency budget with meta pool  

7. **VFS readiness** — `Location` already abstracts path vs archive; future SFTP provider implements the same async list/stat/read APIs with no GUI I/O  

### Viewport & generation

- Only enqueue meta/thumbnails for **visible** rows (+ small margin).  
- Bump `generation` on navigate / filter / sort change; drop stale callbacks.  
- Bound concurrency (e.g. 2–4 ffprobe jobs).

### Success criteria

- Large directories: chrome stays responsive while listing fills in.  
- Icon scroll over videos: no stalls; badges appear progressively.  
- Revisit folder: meta loads from SQLite without re-ffprobe when mtime/size match.  
- No `stat` / `popen` / SQLite from paint or other GUI-thread slots.

### Implementation checklist

- [x] SQLite schema + open/migrate helpers  
- [x] Disk cache get/put keyed by path+mtime+size  
- [x] In-memory layer + worker queue (`request` / `try_get`)  
- [x] Wire FileItemDelegate to memory-only + viewport requests (paint path)  
- [x] Stop sync `probe_media` from sorter (memory only); filter uses cache then sync resolve for CLI  
- [x] Async directory load (`DirectoryLoadWorker` + generation)  
- [x] Async sort (`SortWorker` + generation; show unsorted then refine)  
- [x] Debounced thumbnails (80ms singleShot)  
- [ ] Optional: persist more FileInfo fields; multi-backend location keys  


## Explicitly defer / ignore

| Item | Reason |
|------|--------|
| Write into archives | Read-only by design |
| Graphics View icon scene | May revisit; Model/View for now |
| Face detect / experiments / most programs/* | Out of scope |
| Pixel-perfect Python layout | Functional parity only |
| Archive write support | Read-only by design |

---

## Filter DSL notes (`dirtoo-filter`)

- Parentheses grouping fixed vs Python.
- Modular Qt-free library; `dt-filter '<expr>' [dir]` for CLI testing.
- Still open vs Python: `contains:`. Media width/height/duration/framerate via ffprobe; fuzzy n-gram done.

---


---

## C++ vs Python — detailed feature comparison

Reference: `dirtoo-py/`. Implementation: `dirtoo/`. Status reflects the C++ port
as of the responsive-UI / media-cache work.

### Architecture

| Area | Python | C++ | Notes |
|------|--------|-----|-------|
| Language / UI | Python 3 + PyQt6 | C++23 + Qt6 Widgets | |
| View tech | `QGraphicsView` + custom `FileItem` scene | `QTreeView` / `QListView` + model/delegate | Graphics scene deferred |
| FS abstraction | `virtual_filesystem`, Location URLs | `dirtoo-fs::Location` (file + archive) | SFTP/other VFS not started |
| File ops | In-app + scripts | **`dirops`** lib + `dt-*` CLIs | Stronger separation in C++ |
| Filtering | `filter/` + pyparsing | **`dirtoo-filter`** (hand parser, Qt-free) | |
| Collection | SortedList + Sorter + Grouper | **`dirtoo-collection`** + Sorter | Grouper missing |
| Metadata cache | Per-file XML sidecars | **SQLite** `meta.sqlite` + memory + workers | C++ direction is better for scale |
| Thumbnails | D-Bus thumbnailer | **`dirtoo-thumbnail`** D-Bus | |
| Watcher | inotify-style | **`dirtoo-watcher`** | |
| Archives | ArchiveInfo / extract tools | **`dirtoo-archive`** read-only | Write into archives not planned |
| Packaging | pip / Nix | CMake libs + Nix flake multi-output | |

### GUI features present in both (done or close)

| Feature | C++ status |
|---------|------------|
| Multi-window | done |
| Detail + icon views | done (no separate “small icon / sequence” mode) |
| Zoom in/out (+ large zoom steps) | done |
| Crop thumbnails (cover vs letterbox) | done |
| Icon caption LOD (name / size / date) | done |
| Media overlays (WxH, duration, fps) + type badges | done (async meta + Python PNGs) |
| Location bar ↔ breadcrumbs, trail keep on back | done |
| Path completion (async) | done |
| History menu + middle-click new window | done |
| Bookmarks (file store, middle-click) | done |
| Parent / Home / Back / Forward | done |
| Middle-click open in new window | done |
| Filter show/hide, pin, history | done |
| Filter DSL + help + `dt-filter` | done (subset of predicates) |
| Recursive search UI + CLI `-r` | done |
| Clipboard cut/copy/paste + conflict UI | done |
| Background transfers | done |
| Mkdir / rename / delete / properties | done |
| Open with / terminal | done |
| Preferences / About / QSettings | done |
| Message area | done |
| Leap (type-ahead jump) | done |
| Show hidden | done |
| Directory watcher refresh | done |
| Sort: name (natural), size, ext, date, type, media keys, permissions, random | done (async sort worker) |
| Directories first / reverse | done |
| Toolbar theme icons | done |
| Async directory load | done |

### Missing or incomplete in C++ (priority roughly high → low)

#### View & layout
| Missing | Python behaviour |
|---------|------------------|
| **Small icon / sequence mode** | Third view style (wide rows / list-like icons) |
| **Graphics View icon scene** | Freer layout, hover overlays, per-item animation |
| **Group by** (day, directory, duration, none) | `Grouper` + section headers in layout |
| **Time gaps** in icon layout | Visual spacing by mtime gaps |
| **Show abspath vs basename** toggle | Caption shows full path |
| **Show filtered** (keep non-matches greyed?) | Separate from hide |
| **Hover highlight / overlay on thumbnails** | CompositionMode overlay on hover |
| **Prepare / reload thumbnails** toolbar actions | Force thumbnail + metadata refresh |
| **Loading / error / locked / new badge pixmaps** | Assets copied; not fully wired like Python |
| **Undo / redo** | Present in actions but commented out in Python toolbar too |

#### Filter DSL gaps
| Predicate | Python | C++ |
|-----------|--------|-----|
| `contains:` / `Contains:` (file content) | yes | **no** |
| `containsre:` content regex | yes | **no** |
| `containsfuzzy:` | yes | **no** |
| `date:` / `time:` / `weekday:` | yes | **no** |
| `length:` / `len:` (name length) | yes | **no** |
| `charset:` / `encoding:` | yes | **no** |
| `pages:` (PDF) | yes | **no** |
| `filecount:` (dir/archive) | yes | **no** |
| `random:` | yes | **no** |
| fuzzy / glob / regex / size / type / media | yes | **yes** |

#### Sort / metadata
| Missing | Notes |
|---------|--------|
| Sort by **user / group** | Actions exist in Python; not fully implemented there either |
| Sort waits for **full media coverage** | C++ sorts on cached meta only (unknowns sort as 0) |
| PDF pages / archive file_count in meta DB | Schema ready to extend; collectors not written |
| pymediainfo parity fields (bitrate, channels, …) | C++ uses ffprobe subset |

#### Filesystem & VFS
| Missing | Notes |
|---------|--------|
| **SFTP / remote Location** | Python experiments / VFS hooks; C++ file+archive only |
| Non-file protocols in Location | Extensible design, no backends yet |
| Archive **write** / modify | Explicitly deferred both sides |

#### Ops & tools (`programs/*`)
Python ships many CLIs under `programs/` (find expr engine, fsck, shuffle, desktop, mime, …). C++ has focused **`dt-copy/move/mkdir/rename/swap/filter`** via dirops. Most Python one-offs are **out of scope** unless needed for GUI parity.

#### UX polish
| Missing | Notes |
|---------|--------|
| Richer preferences (all Python settings keys) | C++ has a subset |
| Context menu parity (every Python item action) | Basic set present |
| DnD cursor themed pixmaps | Python `dnd-*.png`; C++ uses Qt defaults |
| Save file list as | Python action |
| Debug mode action | Python only |

### C++ advantages (not in Python)
- Modular installable libraries and Nix flake outputs
- SQLite metadata cache + explicit **no GUI-thread I/O** architecture
- Async directory load + async sort workers
- `dirops` as a reusable Qt-free ops library
- Hand-written filter parser (no pyparsing runtime)

### Recommended next parity work
1. Filter: `contains:` (bounded size) + `date:` / `length:`  
2. Group by day/directory (collection + UI headers)  
3. Small-icon / compact list mode  
4. Wire remaining badge assets (loading/error/locked)  
5. Preferences coverage + thumbnail prepare/reload actions  


## Working process

- Suggest a detailed commit message after each change series.
- Keep `dirtoo-py/` as reference only.
